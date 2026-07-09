/* =====================================================================
 * x16clib :: test/runner.c -- on-machine regression suite
 * =====================================================================
 * Runs under `.\build.ps1 -Test`, which boots x16emu headless
 * (-testbench -warp -echo) and fails the build on any FAIL, a pass count
 * that disagrees with the total, a missing DONE line, or a timeout.
 *
 * Independent-path principle, inherited from the assembly suite: drive
 * the library one way and verify the other way -- write through a library
 * call on port 0, read back with cc65's vpeek(). An address-plumbing bug
 * then cannot hide behind itself.
 *
 * The ABI_* tests have no counterpart in the assembly suite because
 * there was no C boundary there. They exist because a shim that pops its
 * arguments in the wrong order, or in the wrong width, does not crash --
 * it quietly does the wrong thing. Each one is built so that the answer
 * changes if any single argument is misplaced.
 * =====================================================================
 */

#include "testlib.h"
#include <cx16.h>
#include <x16/x16.h>
#include <x16/vera.h>
#include <x16/screen.h>
#include <x16/palette.h>
#include <x16/sprite.h>
#include <x16/bitmap.h>
#include <x16/collide.h>
#include <x16/fixed.h>
#include <x16/tile.h>
#include <x16/verafx.h>
#include <x16/input.h>
#include <x16/irq.h>
#include <x16/psg.h>
#include <x16/pcm.h>
#include <x16/ym.h>
#include <x16/bank.h>
#include <x16/load.h>
#include <x16/float.h>
#include <time.h>

/* Scratch VRAM, well clear of the text screen ($1B000), the default
** bitmap ($00000) and VERA's own registers ($1F9C0+).
*/
#define TESTVRAM        0x04000UL

/* Bank-1 scratch: bit 16 set. Between the sprite images ($13000) and the
** charset ($1F000) there is nothing the ROM cares about.
*/
#define TESTVRAM_HI     0x10000UL

/* ------------------------------------------------------------------ */

/* Write `n` copies of `v` with cc65's vpoke -- an address path entirely
** independent of the library's.
*/
static void vram_poison(unsigned long base, unsigned int n, unsigned char v)
{
    unsigned int i;

    for (i = 0; i < n; ++i) {
        vpoke(v, base + i);
    }
}

/* 1 if VRAM[base .. base+n-1] all equal v. */
static unsigned char vram_all(unsigned long base, unsigned int n, unsigned char v)
{
    unsigned int i;

    for (i = 0; i < n; ++i) {
        if (vpeek(base + i) != v) {
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */

static void test_zp_in_window(void)
{
    unsigned char base = x16_zp_base();

    /* The block must land in the $22-$7F user window: below $22 are the
    ** KERNAL's r0..r15, at $80 and up is KERNAL/BASIC/DOS. cc65's
    ** cx16.cfg maps ZEROPAGE onto exactly that window, so this really
    ** checks that x16zp.s went into ZEROPAGE and not, say, BSS -- which
    ** would still assemble, and would silently corrupt whatever page-zero
    ** byte the truncated address happened to name. 16 bytes wide, so the
    ** last must also fit under $80.
    */
    t_check(base >= 0x22 && base <= 0x80 - 16, "ZP_IN_WINDOW");
}

static void test_fill(void)
{
    vram_poison(TESTVRAM, 24, 0x00);

    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0xAA, 16);

    t_check(vram_all(TESTVRAM, 16, 0xAA) &&
            vpeek(TESTVRAM + 16) == 0x00,       /* did not run past the end */
            "FILL");
}

static void test_fill_zero(void)
{
    vram_poison(TESTVRAM, 4, 0x5C);

    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0x11, 0);

    /* A count of 0 means zero bytes, not 65536. */
    t_check(vram_all(TESTVRAM, 4, 0x5C), "FILL_ZERO");
}

static void test_fill_stride(void)
{
    unsigned char i, ok = 1;

    vram_poison(TESTVRAM, 16, 0x00);

    x16_vera_addr0(X16_INC_2, TESTVRAM);
    x16_vera_fill(0xBB, 8);

    for (i = 0; i < 16; ++i) {
        unsigned char want = (i & 1) ? 0x00 : 0xBB;
        if (vpeek(TESTVRAM + i) != want) {
            ok = 0;
        }
    }
    t_check(ok, "FILL_STRIDE");
}

/* A count above 255 exercises vera_fill's high-byte path, where the
** partial-page correction lives.
*/
static void test_fill_16bit(void)
{
    vram_poison(TESTVRAM + 300, 2, 0x00);

    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0xD7, 300);

    t_check(vpeek(TESTVRAM) == 0xD7 &&
            vpeek(TESTVRAM + 299) == 0xD7 &&
            vpeek(TESTVRAM + 300) == 0x00,
            "FILL_16BIT");
}

static void test_copy(void)
{
    unsigned char i, ok = 1;

    /* Lay down a ramp through cc65's path, copy it through ours. */
    for (i = 0; i < 100; ++i) {
        vpoke(i, TESTVRAM + i);
    }
    vram_poison(TESTVRAM + 256, 100, 0x00);

    x16_vera_addr0(X16_INC_1, TESTVRAM);        /* source */
    x16_vera_addr1(X16_INC_1, TESTVRAM + 256);  /* destination */
    x16_vera_copy(100);

    for (i = 0; i < 100; ++i) {
        if (vpeek(TESTVRAM + 256 + i) != i) {
            ok = 0;
        }
    }
    t_check(ok, "COPY");
}

static void test_has_fx(void)
{
    /* The r49 emulator reports FX. A 0 here means either the probe broke
    ** or someone pointed the build at an older ROM.
    */
    t_check(x16_vera_has_fx() == 1, "HAS_FX");
}

/* ------------------------------------------------------------------ */
/* screen                                                              */
/* ------------------------------------------------------------------ */

/* Leave a hostile DCSEL behind: screen_border must select bank 0 itself
** before it can even see DC_BORDER.
*/
static void test_border(void)
{
    unsigned char got;

    VERA.control = 2 << 1;              /* DCSEL = 2, the FX bank */
    x16_screen_border(7);

    VERA.control = 0;                   /* DCSEL = 0 */
    got = VERA.display.border;

    x16_screen_border(6);               /* restore the default */
    t_check(got == 7, "SCREEN_BORDER");
}

static void test_set_mode(void)
{
    unsigned char ok = x16_screen_set_mode(X16_MODE_80x60);
    unsigned char mode = x16_screen_get_mode();

    t_check(ok == 1 && mode == X16_MODE_80x60, "SCREEN_SET_MODE");
}

/* $7F is not a mode. The KERNAL reports that in the carry, and the shim
** must turn a set carry into 0, not into 1.
*/
static void test_set_mode_bad(void)
{
    unsigned char ok = x16_screen_set_mode(0x7F);

    t_check(ok == 0 && x16_screen_get_mode() == X16_MODE_80x60,
            "SCREEN_SET_MODE_BAD");
}

/* Enter with ADDRSEL = 1, the state x16_vera_addr1() and x16_vera_copy()
** leave behind. The KERNAL's screen code writes VERA's address registers
** before selecting a port, so cls corrupts the display unless it clears
** ADDRSEL first. Remove that guard from screen.s and this fails.
*/
static void test_cls_clears(void)
{
    vpoke(0xAA, X16_VRAM_TEXT + 10 * 2);        /* sentinel at column 10 */

    VERA.control |= 1;                          /* hostile: ADDRSEL = 1 */
    x16_screen_cls();

    t_check(vpeek(X16_VRAM_TEXT + 10 * 2) == 0x20, "CLS_CLEARS");
}

/* x16_screen_color must change what CHROUT actually puts in VRAM, not
** merely poke a KERNAL variable. Verify through the tilemap attribute
** byte, which is the odd byte of each cell.
*/
static void test_color_reaches_vram(void)
{
    unsigned char attr;

    VERA.control |= 1;                          /* hostile: ADDRSEL = 1 */
    x16_screen_cls();
    x16_screen_color(1, 6);                     /* white on blue */
    x16_screen_locate(0, 0);
    x16_screen_chrout('X');

    attr = vpeek(X16_VRAM_TEXT + 1);            /* fg | bg<<4 */

    x16_screen_color(1, 6);
    x16_screen_cls();
    t_check(attr == 0x61, "COLOR_REACHES_VRAM");
}

/* ------------------------------------------------------------------ */
/* palette                                                             */
/* ------------------------------------------------------------------ */

/* A 12-bit 0RGB colour stores little-endian into an entry: low byte is
** Green<<4 | Blue, high byte is Red.
*/
static void test_palette(void)
{
    unsigned char lo, hi;

    x16_pal_set(1, 0x0F00);                     /* pure red */
    lo = vpeek(X16_VRAM_PALETTE + 2);
    hi = vpeek(X16_VRAM_PALETTE + 3);

    x16_pal_set(1, 0x0FFF);                     /* put entry 1 back to white */
    t_check(lo == 0x00 && hi == 0x0F, "PALETTE");
}

static void test_pal_load(void)
{
    static const unsigned int ramp[3] = { 0x0F00, 0x00F0, 0x000F };
    unsigned char ok;

    x16_pal_load(ramp, 4, 3);

    ok = vpeek(X16_VRAM_PALETTE + 8) == 0x00 &&   /* entry 4 lo */
         vpeek(X16_VRAM_PALETTE + 9) == 0x0F &&   /* entry 4 hi */
         vpeek(X16_VRAM_PALETTE + 10) == 0xF0 &&  /* entry 5 lo */
         vpeek(X16_VRAM_PALETTE + 11) == 0x00 &&
         vpeek(X16_VRAM_PALETTE + 12) == 0x0F &&  /* entry 6 lo */
         vpeek(X16_VRAM_PALETTE + 13) == 0x00;
    t_check(ok, "PAL_LOAD");
}

/* A count of 0 must load nothing. Get the guard wrong and the loop runs
** 256 times and shreds the whole palette.
*/
static void test_pal_load_zero(void)
{
    static const unsigned int one[1] = { 0x0ABC };

    x16_pal_set(8, 0x0123);
    x16_pal_load(one, 8, 0);

    t_check(vpeek(X16_VRAM_PALETTE + 16) == 0x23 &&
            vpeek(X16_VRAM_PALETTE + 17) == 0x01,
            "PAL_LOAD_ZERO");
}

/* ------------------------------------------------------------------ */
/* fixed point                                                         */
/* ------------------------------------------------------------------ */

static void test_mul88(void)
{
    /* 1.5 * 2.0 == 3.0 */
    t_check(x16_mul88(0x0180, 0x0200) == 0x0300, "MUL88");
}

static void test_mul88_negative(void)
{
    /* -1.5 * 2.0 == -3.0, and -1.5 * -2.0 == 3.0 */
    t_check(x16_mul88((int)0xFE80, 0x0200) == (int)0xFD00 &&
            x16_mul88((int)0xFE80, (int)0xFE00) == 0x0300,
            "MUL88_NEGATIVE");
}

static void test_umul16(void)
{
    t_check(x16_umul16(1000, 1000) == 1000000UL &&
            x16_umul16(0xFFFF, 0xFFFF) == 0xFFFE0001UL,
            "UMUL16");
}

/* ------------------------------------------------------------------ */
/* collision                                                           */
/* ------------------------------------------------------------------ */

static void test_collide_overlap(void)
{
    t_check(x16_collide8(0, 0, 10, 10, 5, 5, 10, 10) == 1, "COLLIDE_OVERLAP");
}

static void test_collide_apart(void)
{
    t_check(x16_collide8(0, 0, 10, 10, 20, 20, 5, 5) == 0, "COLLIDE_APART");
}

/* Edges that merely touch do not overlap. Box A spans x[0,10), box B
** starts at x=10: adjacent, not colliding. Same on the y axis.
*/
static void test_collide_touching(void)
{
    t_check(x16_collide8(0, 0, 10, 10, 10, 0, 10, 10) == 0 &&
            x16_collide8(0, 0, 10, 10, 0, 10, 10, 10) == 0,
            "COLLIDE_TOUCHING");
}

static void test_collide16(void)
{
    /* Beyond x=255, where collide8 cannot reach. */
    static const x16_box16 a = { 600, 400, 20, 4 };
    static const x16_box16 b = { 618, 370, 2, 35 };
    static const x16_box16 away = { 1000, 400, 4, 4 };

    t_check(x16_collide16(&a, &b) == 1 &&
            x16_collide16(&a, &away) == 0,
            "COLLIDE16");
}

/* ------------------------------------------------------------------ */
/* sprites                                                             */
/* ------------------------------------------------------------------ */

/* Sprite 3's record starts at $1FC00 + 3*8. Bytes 2-5 are the position:
** x low, x high (2 bits), y low, y high (2 bits).
*/
static void test_sprite_pos(void)
{
    unsigned int x = 0, y = 0;

    x16_sprite_pos(3, 0x123, 0x2AB);
    x16_sprite_get_pos(3, &x, &y);

    t_check(vpeek(X16_VRAM_SPRITE_ATTR + 3 * 8 + 2) == 0x23 &&
            vpeek(X16_VRAM_SPRITE_ATTR + 3 * 8 + 3) == 0x01 &&
            vpeek(X16_VRAM_SPRITE_ATTR + 3 * 8 + 4) == 0xAB &&
            vpeek(X16_VRAM_SPRITE_ATTR + 3 * 8 + 5) == 0x02 &&
            x == 0x123 && y == 0x2AB,
            "SPRITE_POS");
}

/* The record stores image address bits 16:5. For $13000 in 8bpp:
**   byte 0 = addr 12:5           = 0x80
**   byte 1 = mode | addr 16:13   = 0x80 | 0x09 = 0x89
*/
static void test_sprite_image(void)
{
    x16_sprite_image(2, X16_SPRITE_8BPP, 0x13000UL);

    t_check(vpeek(X16_VRAM_SPRITE_ATTR + 2 * 8 + 0) == 0x80 &&
            vpeek(X16_VRAM_SPRITE_ATTR + 2 * 8 + 1) == 0x89,
            "SPRITE_IMAGE");
}

/* x16_sprite_z is a read-modify-write on write-only VRAM: it only works
** because the host wrote the record first. It must leave the flip bits
** and collision mask alone.
*/
static void test_sprite_z(void)
{
    x16_sprite_flags(5, 0x30 | X16_SPRITE_Z_BEHIND | X16_SPRITE_HFLIP);
    x16_sprite_z(5, X16_SPRITE_Z_FRONT);

    t_check(vpeek(X16_VRAM_SPRITE_ATTR + 5 * 8 + 6) ==
            (0x30 | X16_SPRITE_Z_FRONT | X16_SPRITE_HFLIP),
            "SPRITE_Z");
}

/* Byte 7: height in 7:6, width in 5:4, palette offset in 3:0. */
static void test_sprite_size(void)
{
    x16_sprite_size(6, X16_SPRITE_SIZE_16, X16_SPRITE_SIZE_64, 9);

    t_check(vpeek(X16_VRAM_SPRITE_ATTR + 6 * 8 + 7) == (0xC0 | 0x10 | 0x09),
            "SPRITE_SIZE");
}

/* The palette offset is or'd into byte 7, so an out-of-range value would
** set the size bits too -- 0xFF would turn a 16-wide sprite into a
** 64-wide one. The routine masks it to four bits. Without the mask this
** reads back 0xFF instead of 0xDF.
*/
static void test_sprite_size_pal_mask(void)
{
    x16_sprite_size(7, X16_SPRITE_SIZE_16, X16_SPRITE_SIZE_64, 0xFF);

    t_check(vpeek(X16_VRAM_SPRITE_ATTR + 7 * 8 + 7) == (0xC0 | 0x10 | 0x0F),
            "SPRITE_SIZE_PAL_MASK");
}

/* ------------------------------------------------------------------ */
/* bitmap                                                              */
/* ------------------------------------------------------------------ */

#define PIXEL(x, y)     (X16_VRAM_BITMAP + (unsigned long)(y) * 320 + (x))

/* The framebuffer is 320*240 = 76800 bytes, which does not fit
** vera_fill's 16-bit count. Pass it naively and it truncates to $2C00 --
** the top 35 rows -- and the rest of the screen keeps whatever was there.
** So check the LAST pixel, not the first: only a two-half clear reaches
** it. Runs before the other bitmap tests, which poison their own cells.
*/
static void test_gfx_clear(void)
{
    vpoke(0x00, PIXEL(0, 0));
    vpoke(0x00, PIXEL(0, 36));          /* just past a truncated fill */
    vpoke(0x00, PIXEL(319, 239));       /* the very last pixel */

    x16_gfx_clear(0x77);

    t_check(vpeek(PIXEL(0, 0)) == 0x77 &&
            vpeek(PIXEL(0, 36)) == 0x77 &&
            vpeek(PIXEL(319, 239)) == 0x77,
            "GFX_CLEAR");
}

static void test_gfx_pset(void)
{
    vpoke(0x00, PIXEL(10, 5));
    x16_gfx_pset(10, 5, 0x42);

    t_check(vpeek(PIXEL(10, 5)) == 0x42, "GFX_PSET");
}

/* x >= 320 and y >= 240 are off screen. Unclipped, pset(320, 0) would
** land on pixel (0,1) and pset(0, 240) at offset 76800 -- so poison both
** and check they stayed clean.
*/
static void test_gfx_clip(void)
{
    vpoke(0x00, PIXEL(0, 1));
    vpoke(0x00, 0x12C00UL);             /* 240 * 320 */

    x16_gfx_pset(320, 0, 0x99);
    x16_gfx_pset(0, 240, 0x99);

    t_check(vpeek(PIXEL(0, 1)) == 0x00 &&
            vpeek(0x12C00UL) == 0x00,
            "GFX_CLIP");
}

static void test_gfx_hline(void)
{
    vpoke(0x00, PIXEL(20, 8));
    vpoke(0x00, PIXEL(25, 8));

    x16_gfx_hline(20, 8, 5, 0x33);

    t_check(vpeek(PIXEL(20, 8)) == 0x33 &&
            vpeek(PIXEL(24, 8)) == 0x33 &&
            vpeek(PIXEL(25, 8)) == 0x00,        /* one past the end */
            "GFX_HLINE");
}

/* A vertical line steps the data port by 320, one of VERA's odd
** increments, so it is the same tight loop as a horizontal one.
*/
static void test_gfx_vline(void)
{
    unsigned char i;

    /* Poison every cell this test reads. VRAM starts out holding whatever
    ** the ROM left there, so an unpoisoned "must still be zero" cell tests
    ** nothing -- or fails at random.
    */
    for (i = 2; i <= 6; ++i) {
        vpoke(0x00, PIXEL(3, i));
    }
    vpoke(0x00, PIXEL(4, 2));

    x16_gfx_vline(3, 2, 4, 0x55);

    t_check(vpeek(PIXEL(3, 2)) == 0x55 &&
            vpeek(PIXEL(3, 5)) == 0x55 &&
            vpeek(PIXEL(3, 6)) == 0x00 &&       /* one past the end */
            vpeek(PIXEL(4, 2)) == 0x00,         /* did not go sideways */
            "GFX_VLINE");
}

static void test_gfx_line(void)
{
    /* Four distinct coordinates: a transposed shim lands elsewhere. */
    vpoke(0x00, PIXEL(3, 7));
    vpoke(0x00, PIXEL(11, 5));

    x16_gfx_line(3, 7, 11, 5, 0x77);

    t_check(vpeek(PIXEL(3, 7)) == 0x77 &&
            vpeek(PIXEL(11, 5)) == 0x77,
            "GFX_LINE");
}

static void test_gfx_frame(void)
{
    vpoke(0x00, PIXEL(41, 31));

    x16_gfx_frame(40, 30, 6, 4, 0x66);

    t_check(vpeek(PIXEL(40, 30)) == 0x66 &&     /* top left */
            vpeek(PIXEL(45, 30)) == 0x66 &&     /* top right */
            vpeek(PIXEL(40, 33)) == 0x66 &&     /* bottom left */
            vpeek(PIXEL(45, 33)) == 0x66 &&     /* bottom right */
            vpeek(PIXEL(41, 31)) == 0x00,       /* hollow */
            "GFX_FRAME");
}

/* ------------------------------------------------------------------ */
/* tiles and layers                                                    */
/* ------------------------------------------------------------------ */

/* Where layer 1's cell (col,row) really lives, derived from VERA's own
** registers rather than from the same arithmetic tile_setptr uses.
*/
static unsigned long tile_addr(unsigned char col, unsigned char row)
{
    unsigned int mapw = 32 << ((VERA.layer1.config >> 4) & 3);
    unsigned long base = (unsigned long)VERA.layer1.mapbase << 9;

    return base + ((unsigned long)row * mapw + col) * 2;
}

static void test_tile_addr(void)
{
    x16_tile_put(5, 3, 0x41, 0x61);

    t_check(vpeek(tile_addr(5, 3)) == 0x41 &&
            vpeek(tile_addr(5, 3) + 1) == 0x61,
            "TILE_ADDR");
}

static void test_tile_roundtrip(void)
{
    unsigned int cell;

    x16_tile_put(7, 2, 0x53, 0x1E);
    cell = x16_tile_get(7, 2);

    t_check(X16_TILE_CODE(cell) == 0x53 && X16_TILE_ATTR(cell) == 0x1E,
            "TILE_ROUNDTRIP");
}

/* Scrolling layer 1 must not move layer 0, and the value is 12-bit. */
static void test_layer_scroll(void)
{
    x16_layer_scroll_x(0, 0);
    x16_layer_scroll_y(0, 0);
    x16_layer_scroll_x(1, 0x123);
    x16_layer_scroll_y(1, 0x0AB);

    t_check((VERA.layer1.hscroll & 0x0FFF) == 0x123 &&
            (VERA.layer1.vscroll & 0x0FFF) == 0x0AB &&
            (VERA.layer0.hscroll & 0x0FFF) == 0,
            "LAYER_SCROLL");

    x16_layer_scroll_x(1, 0);
    x16_layer_scroll_y(1, 0);
}

/* layer_on/off must touch only the named layer's enable bit. */
static void test_layer_enable(void)
{
    unsigned char both, off0;

    VERA.control = 0;                   /* DCSEL = 0, so DC_VIDEO is visible */
    x16_layer_on(0);
    x16_layer_on(1);
    both = VERA.display.video & 0x30;

    x16_layer_off(0);
    off0 = VERA.display.video & 0x30;

    x16_layer_on(0);                    /* put the text screen back */
    t_check(both == 0x30 && off0 == 0x20, "LAYER_ENABLE");
}

/* ------------------------------------------------------------------ */
/* VERA FX                                                             */
/* ------------------------------------------------------------------ */

static void test_fx_mult(void)
{
    t_check(x16_fx_mult(1000, 1000) == 1000000L, "FX_MULT");
}

/* The multiplier is signed, and the sign has to survive into all four
** result bytes: -1000 * 1000 is 0xFFF0BDC0, not 0x000F4240.
*/
static void test_fx_mult_signed(void)
{
    t_check(x16_fx_mult(-1000, 1000) == -1000000L &&
            x16_fx_mult(-3, -5) == 15L,
            "FX_MULT_SIGNED");
}

/* The multiplier feeds an accumulator that only a *read* of
** FX_ACCUM_RESET clears. Drop that read and the second multiply comes
** back polluted by the first.
*/
static void test_fx_accum_dirty(void)
{
    x16_fx_mult(1000, 1000);
    t_check(x16_fx_mult(2, 3) == 6L, "FX_ACCUM_DIRTY");
}

/* A count that is not a multiple of four exercises the tail path, where
** FX is switched off and the last bytes are written singly.
*/
static void test_fx_fill(void)
{
    vram_poison(TESTVRAM, 12, 0x00);

    x16_fx_fill(0xA5, 10, TESTVRAM);

    t_check(vram_all(TESTVRAM, 10, 0xA5) &&
            vpeek(TESTVRAM + 10) == 0x00 &&     /* tail stopped on time */
            vpeek(TESTVRAM + 11) == 0x00,
            "FX_FILL");
}

static void test_fx_clear(void)
{
    vram_poison(TESTVRAM, 9, 0xFF);

    x16_fx_clear(8, TESTVRAM);

    t_check(vram_all(TESTVRAM, 8, 0x00) &&
            vpeek(TESTVRAM + 8) == 0xFF,
            "FX_CLEAR");
}

/* Every FX routine must leave FX_CTRL and DCSEL at 0. Leaving Addr1 Mode
** set silently changes VRAM addressing for everyone downstream, so this
** checks a plain fill still works right after a multiply.
*/
static void test_fx_leaves_clean(void)
{
    unsigned char ctrl;

    x16_fx_mult(7, 9);

    ctrl = VERA.control;                /* DCSEL back at 0? */
    vram_poison(TESTVRAM, 4, 0x00);
    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0x5E, 4);

    t_check((ctrl & 0x7E) == 0 && vram_all(TESTVRAM, 4, 0x5E),
            "FX_LEAVES_CLEAN");
}

/* ------------------------------------------------------------------ */
/* IRQ                                                                 */
/* ------------------------------------------------------------------ */

#define CINV    (*(volatile unsigned int *)0x0314)

/* Install must redirect CINV, and must be idempotent. The idempotency
** check needs no access to the module's internals: install twice, remove
** once. A second install that re-saved CINV would have stashed our own
** handler as the "previous" vector, and the remove would restore that
** instead of the KERNAL's.
*/
static void test_irq_hook(void)
{
    unsigned int old = CINV;
    unsigned char hooked, restored;

    x16_irq_install();
    hooked = (CINV != old);

    x16_irq_install();          /* second install must change nothing */
    x16_irq_remove();
    restored = (CINV == old);

    t_check(hooked && restored, "IRQ_HOOK");
}

static unsigned char status_after_install;

/* irq_install brackets its work in php/sei/plp, not sei/.../cli. A blind
** cli would hand interrupts back on to a caller that had deliberately
** turned them off -- silently, and only sometimes fatally.
**
** Enter with interrupts masked and check the I flag survived. The php/pla
** pair reads the status register straight after the call returns.
*/
static void test_irq_preserves_iflag(void)
{
    __asm__("sei");
    x16_irq_install();
    __asm__("php");
    __asm__("pla");
    __asm__("sta %v", status_after_install);
    __asm__("cli");

    x16_irq_remove();

    t_check((status_after_install & 0x04) != 0, "IRQ_PRESERVES_IFLAG");
}

/* The VSYNC hook must actually tick -- where VSYNC exists at all.
**
** x16emu's -testbench mode is headless: it runs no video, so VERA never
** raises a VSYNC interrupt and the KERNAL's jiffy clock stands still.
** That is a property of the harness, not a bug in irq.s, so use the jiffy
** clock (cc65's clock(), which reads a counter only the KERNAL's IRQ
** advances) as an INDEPENDENT oracle:
**
**   frames advanced           -> pass
**   frames stuck, jiffy stuck -> no interrupts here at all -> skip
**   frames stuck, jiffy moved -> interrupts ran and our counter did not
**                                -> a real bug -> fail
**
** A bounded spin, not a blocking x16_vsync_wait(): a broken counter must
** report a failure, not hang the harness until its timeout.
*/
static void test_vsync_counter(void)
{
    clock_t jiffy0;
    unsigned char start;
    unsigned int spin;
    unsigned char ticked = 0;

    x16_irq_install();
    jiffy0 = clock();
    start = x16_irq_frames();

    for (spin = 0; spin < 20000 && !ticked; ++spin) {
        if ((unsigned char)(x16_irq_frames() - start) >= 2) {
            ticked = 1;
        }
    }

    x16_irq_remove();

    if (ticked) {
        t_check(1, "VSYNC_COUNTER");
    } else if (clock() == jiffy0) {
        t_skip("VSYNC_COUNTER");        /* headless: no interrupts at all */
    } else {
        t_check(0, "VSYNC_COUNTER");    /* the IRQ ran; we just missed it */
    }
}

/* ------------------------------------------------------------------ */
/* input                                                               */
/* ------------------------------------------------------------------ */

/* Nothing is plugged in under the emulator, so every button reads
** released -- which, active low, means all ones. And `present` must come
** back as a clean boolean, never the 0xAA sentinel and never the KERNAL's
** raw 0xFF.
*/
static void test_joy_get(void)
{
    unsigned char present = 0xAA;
    unsigned int buttons;

    x16_joy_scan();
    buttons = x16_joy_get(0, &present);

    t_check(buttons == 0xFFFF && (present == 0 || present == 1), "JOY_GET");
}

/* Joystick 4 is certainly not plugged in, so the shim must turn the
** KERNAL's $FF into a clean 0 rather than passing it through.
*/
static void test_joy_absent(void)
{
    unsigned char present = 0xAA;

    x16_joy_scan();
    x16_joy_get(4, &present);

    t_check(present == 0, "JOY_ABSENT");
}

static unsigned char joy_raw_n, joy_raw_y;

/* JOYSTICK_GET straight from the KERNAL, bypassing the shim. */
static void joy_raw(unsigned char n)
{
    joy_raw_n = n;                      /* %v needs a static, not a param */
    __asm__("lda %v", joy_raw_n);
    __asm__("jsr $FF56");
    __asm__("sty %v", joy_raw_y);
}

/* The shim maps the KERNAL's Y ($00 present, $FF absent) onto 1/0.
** JOY_ABSENT covers the $FF branch. The $00 branch needs a joystick that
** actually reports present -- and under x16emu's headless testbench none
** does, not even the keyboard: JOYSTICK_GET answers $FF for all five.
**
** So ask the KERNAL directly and skip if it says nothing is attached,
** exactly as VSYNC_COUNTER does. On real hardware this runs.
*/
static void test_joy_present(void)
{
    unsigned char n, present = 0xAA;

    x16_joy_scan();
    for (n = 0; n <= 4; ++n) {
        joy_raw(n);
        if (joy_raw_y == 0x00) {
            x16_joy_get(n, &present);
            t_check(present == 1, "JOY_PRESENT");
            return;
        }
    }
    t_skip("JOY_PRESENT");
}

/* On an empty buffer only the COUNT is meaningful. KBDBUF_PEEK leaves a
** stale byte in A -- 0x0A here, from the boot sequence -- so a test that
** expected a 0 character would be asserting something the KERNAL never
** promised.
*/
static void test_key_peek_empty(void)
{
    unsigned int p;
    unsigned char guard = 0;

    while (x16_key_get() != 0 && ++guard < 32) {
        /* drain whatever the boot sequence left queued */
    }
    p = x16_key_peek();

    t_check(X16_KEY_COUNT(p) == 0, "KEY_PEEK_EMPTY");
}

/* ------------------------------------------------------------------ */
/* audio                                                               */
/* ------------------------------------------------------------------ */

/* Voice 3's four bytes are at $1F9C0 + 3*4. The PSG is write-only, so
** vpeek reads back the host's own shadow -- which is exactly what we are
** checking: that the right bytes went to the right places.
*/
static void test_psg_regs(void)
{
    unsigned long v = X16_VRAM_PSG + 3 * 4;

    x16_psg_init();
    x16_psg_set_freq(3, 0x04A5);
    x16_psg_set_vol(3, 63, X16_PSG_PAN_BOTH);
    x16_psg_set_wave(3, X16_PSG_WAVE_TRIANGLE, 32);

    t_check(vpeek(v + 0) == 0xA5 &&
            vpeek(v + 1) == 0x04 &&
            vpeek(v + 2) == 0xFF &&     /* pan both | volume 63 */
            vpeek(v + 3) == 0xA0,       /* triangle | width 32 */
            "PSG_REGS");
}

/* X16_PSG_HZ scales by 175922>>16 with a rounded shift, instead of
** multiplying by 2.68435456, because cc65 has no floating point. It must
** still land on the exact step for the musical pitches, and it must not
** overflow 32 bits at the top of the audible range.
**
** The pitches go through variables, not literals. With literals cc65
** folds the whole expression and the comparison becomes a compile-time
** constant -- the test would then check nothing at run time, and would
** never exercise the 32-bit multiply on a non-constant argument.
*/
static unsigned int hz_a4 = 440, hz_a5 = 880, hz_one = 1, hz_top = 20000;

static void test_psg_hz(void)
{
    t_check(X16_PSG_HZ(hz_a4) == 1181 &&        /* A4, exact */
            X16_PSG_HZ(hz_a5) == 2362 &&        /* A5, exact */
            X16_PSG_HZ(hz_one) == 3 &&          /* rounds up, not down */
            X16_PSG_HZ(hz_top) == 53687u,       /* no 32-bit overflow */
            "PSG_HZ");
}

/* Silencing a voice must keep its panning. */
static void test_psg_note_off(void)
{
    unsigned long v = X16_VRAM_PSG + 5 * 4;

    x16_psg_set_vol(5, 40, X16_PSG_PAN_RIGHT);
    x16_psg_note_off(5);

    t_check(vpeek(v + 2) == X16_PSG_PAN_RIGHT, "PSG_NOTE_OFF");
}

/* AUDIO_RATE above 128 is invalid, so the rate is clamped. Checked on
** the returned value rather than by reading the register back, because
** that register is not readable.
*/
static void test_pcm_rate_clamp(void)
{
    unsigned char over = x16_pcm_rate(200);
    unsigned char under = x16_pcm_rate(64);

    x16_pcm_rate(0);                    /* stop playback again */

    t_check(over == 128 && under == 64 && x16_pcm_empty() == 1,
            "PCM_RATE_CLAMP");
}

/* ym_write must complete rather than time out on the busy flag. */
static void test_ym_write(void)
{
    /* $20 is RL/FB/CONNECT for channel 0. */
    t_check(x16_ym_write(0x20, 0x00) == 1, "YM_WRITE");
}

/* THE channel-in-A test.
**
** The ROM driver takes the FM channel in .A and the payload in .X, the
** reverse of x16_ym_write. A transposed shim would send pan=5 to channel
** 2 instead of pan=2 to channel 5 -- a valid-looking call that plays on
** the wrong channel and never reports an error. So check both: that the
** setting reached channel 5, AND that it did not land on channel 2.
*/
static void test_ym_channel_in_a(void)
{
    unsigned char ok;

    ok = x16_ym_pan(5, X16_YM_PAN_RIGHT) == 1 &&
         x16_ym_get_pan(5) == X16_YM_PAN_RIGHT &&
         x16_ym_get_pan(2) != X16_YM_PAN_RIGHT;

    /* Attenuation travels the same way round. */
    ok = ok && x16_ym_vol(5, 40) == 1 && x16_ym_get_vol(5) == 40;

    t_check(ok, "YM_CHANNEL_IN_A");
}

/* ------------------------------------------------------------------ */
/* banked RAM                                                          */
/* ------------------------------------------------------------------ */

/* A poke must land in the named bank, and must leave the caller's bank
** mapped exactly as it found it.
*/
static void test_bank_roundtrip(void)
{
    unsigned char before;

    x16_bank_set(7);
    before = x16_bank_get();

    x16_bank_poke(2, 100, 0x5A);
    x16_bank_poke(3, 100, 0xA5);

    t_check(x16_bank_peek(2, 100) == 0x5A &&
            x16_bank_peek(3, 100) == 0xA5 &&
            x16_bank_get() == before &&
            before == 7,
            "BANK_ROUNDTRIP");

    x16_bank_set(1);
}

/* The window pointer must snap from $BFFF back to $A000 and step the
** bank. Write four bytes starting two from the end of bank 1: two land
** in bank 1, two in bank 2.
*/
static void test_bank_boundary(void)
{
    static const unsigned char src[4] = { 0x11, 0x22, 0x33, 0x44 };

    x16_bank_poke(2, 0, 0x00);
    x16_bank_poke(2, 1, 0x00);

    x16_mem_to_bank(src, 1, X16_BANK_SIZE - 2, 4);

    t_check(x16_bank_peek(1, X16_BANK_SIZE - 2) == 0x11 &&
            x16_bank_peek(1, X16_BANK_SIZE - 1) == 0x22 &&
            x16_bank_peek(2, 0) == 0x33 &&
            x16_bank_peek(2, 1) == 0x44,
            "BANK_BOUNDARY");
}

static void test_bank_to_mem(void)
{
    static const unsigned char src[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    static unsigned char dst[4];

    x16_mem_to_bank(src, 4, 512, 4);
    dst[0] = dst[1] = dst[2] = dst[3] = 0;
    x16_bank_to_mem(4, 512, dst, 4);

    t_check(dst[0] == 0xDE && dst[1] == 0xAD &&
            dst[2] == 0xBE && dst[3] == 0xEF,
            "BANK_TO_MEM");
}

/* ------------------------------------------------------------------ */
/* file I/O                                                            */
/* ------------------------------------------------------------------ */

/* Save a block, load it back somewhere else, compare. Runs against the
** emulator's -fsroot directory, which build.ps1 empties first, so a
** stale file cannot make a broken save look like a working one.
*/
static void test_fs_roundtrip(void)
{
    static const char name[] = "TESTDATA.BIN";
    static unsigned char out[16];
    static unsigned char in[16];
    unsigned int end = 0;
    unsigned char i, err, ok;

    for (i = 0; i < 16; ++i) {
        out[i] = 0xF0 ^ i;
        in[i] = 0x00;
    }

    err = x16_fs_save(name, sizeof name - 1, X16_DEVICE_SD,
                      out, out + sizeof out);
    if (err) {
        t_check(0, "FS_ROUNDTRIP");
        return;
    }

    err = x16_fs_load(name, sizeof name - 1, X16_DEVICE_SD, X16_SA_ADDR,
                      in, &end);

    ok = (err == 0);
    for (i = 0; i < 16; ++i) {
        if (in[i] != (unsigned char)(0xF0 ^ i)) {
            ok = 0;
        }
    }
    /* The load reports one past the last byte written. */
    ok = ok && (end == (unsigned int)in + sizeof in);

    t_check(ok, "FS_ROUNDTRIP");
}

/* A NULL `end` must be tolerated, not written through. */
static void test_fs_load_null_end(void)
{
    static const char name[] = "TESTDATA.BIN";
    static unsigned char in[16];

    t_check(x16_fs_load(name, sizeof name - 1, X16_DEVICE_SD, X16_SA_ADDR,
                        in, (unsigned int *)0) == 0,
            "FS_LOAD_NULL_END");
}

/* A file that is not there must report an error, not succeed silently. */
static void test_fs_load_missing(void)
{
    static const char name[] = "NOSUCH.BIN";
    static unsigned char in[4];

    t_check(x16_fs_load(name, sizeof name - 1, X16_DEVICE_SD, X16_SA_ADDR,
                        in, (unsigned int *)0) != 0,
            "FS_LOAD_MISSING");
}

/* Straight into VRAM, then read back through vpeek. */
static void test_fs_vload(void)
{
    static const char name[] = "TESTDATA.BIN";
    unsigned char err;

    vram_poison(TESTVRAM, 16, 0x00);
    err = x16_fs_vload(name, sizeof name - 1, X16_DEVICE_SD, TESTVRAM);

    t_check(err == 0 &&
            vpeek(TESTVRAM) == 0xF0 &&
            vpeek(TESTVRAM + 15) == (0xF0 ^ 15),
            "FS_VLOAD");
}

/* ------------------------------------------------------------------ */
/* floating point                                                      */
/* ------------------------------------------------------------------ */

static x16_float fa, fb;
static char fbuf[X16_FP_STRLEN];

/* String compare. The ROM emits digits, '.', '-' and 'E', all of which
** have the same code in ASCII and PETSCII -- and this file is compiled
** with the ASCII charmap anyway (see testlib.h).
*/
static unsigned char streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

static void test_f_roundtrip(void)
{
    x16_f_from_s16(1234);
    x16_f_store(fa);
    x16_f_zero();
    x16_f_load(fa);

    t_check(x16_f_to_s16() == 1234, "F_ROUNDTRIP");
}

/* fp_negfac, despite its name, is an internal helper of the ROM's
** add/subtract path: it two's-complements the mantissa in place and
** denormalises a normal FAC. fp_negop is the real unary minus. Call the
** wrong one and 5.0 comes back as garbage reading about -2.5.
*/
static void test_f_neg(void)
{
    x16_f_from_s16(5);
    x16_f_neg();

    t_check(x16_f_to_s16() == -5, "F_NEG");
}

/* The ROM's fp_float converts a SIGNED byte, so a naive f_from_u8 turns
** 200 into -56. The binding goes through givayf with a zero high byte.
*/
static void test_f_from_u8(void)
{
    x16_f_from_u8(200);

    t_check(x16_f_to_s16() == 200, "F_FROM_U8");
}

/* The ROM's fp_fsub computes mem - FAC, not FAC - mem. f_sub corrects
** that; f_rsub deliberately does not.
*/
static void test_f_sub_order(void)
{
    x16_f_from_s16(4);
    x16_f_store(fb);

    x16_f_from_s16(10);
    x16_f_sub(fb);                      /* 10 - 4 */
    if (x16_f_to_s16() != 6) {
        t_check(0, "F_SUB_ORDER");
        return;
    }

    x16_f_from_s16(10);
    x16_f_rsub(fb);                     /* 4 - 10 */
    t_check(x16_f_to_s16() == -6, "F_SUB_ORDER");
}

/* Likewise fp_fdiv is mem / FAC. 10/4 is 2.5 and 4/10 is 0.4 -- checked
** through the string, because to_s16 would round both away.
*/
static void test_f_div_order(void)
{
    x16_f_from_s16(4);
    x16_f_store(fb);

    x16_f_from_s16(10);
    x16_f_div(fb);                      /* 10 / 4 */
    x16_f_to_str_trim(fbuf);
    if (!streq(fbuf, "2.5")) {
        t_check(0, "F_DIV_ORDER");
        return;
    }

    x16_f_from_s16(10);
    x16_f_rdiv(fb);                     /* 4 / 10 */
    x16_f_to_str_trim(fbuf);
    t_check(streq(fbuf, ".4"), "F_DIV_ORDER");
}

static void test_f_sqrt(void)
{
    x16_f_from_s16(16);
    x16_f_sqrt();

    t_check(x16_f_to_s16() == 4, "F_SQRT");
}

/* to_str keeps BASIC's leading space before a positive number; the trim
** form drops it. A negative number has no space to drop.
*/
static void test_f_str(void)
{
    unsigned char ok;

    x16_f_from_s16(42);
    x16_f_to_str(fbuf);
    ok = streq(fbuf, " 42");

    x16_f_from_s16(42);
    x16_f_to_str_trim(fbuf);
    ok = ok && streq(fbuf, "42");

    x16_f_from_s16(-3);
    x16_f_to_str_trim(fbuf);
    ok = ok && streq(fbuf, "-3");

    t_check(ok, "F_STR");
}

static void test_f_from_str(void)
{
    static const char s[] = "2.5";

    x16_f_from_str(s, sizeof s - 1);
    x16_f_store(fa);

    x16_f_from_s16(2);
    x16_f_store(fb);
    x16_f_load(fa);

    /* 2.5 > 2, and 2.5 * 2 == 5. */
    t_check(x16_f_cmp(fb) == 1 && (x16_f_mul(fb), x16_f_to_s16() == 5),
            "F_FROM_STR");
}

static void test_f_cmp(void)
{
    x16_f_from_s16(3);
    x16_f_store(fb);

    x16_f_from_s16(2);
    if (x16_f_cmp(fb) != -1) {
        t_check(0, "F_CMP");
        return;
    }
    x16_f_from_s16(3);
    if (x16_f_cmp(fb) != 0) {
        t_check(0, "F_CMP");
        return;
    }
    x16_f_from_s16(4);
    t_check(x16_f_cmp(fb) == 1, "F_CMP");
}

/* ------------------------------------------------------------------ */
/* ABI shim tests                                                      */
/* ------------------------------------------------------------------ */

/* x16_vera_fill(value, count). Swap the two and the fill writes 2 copies
** of 0x37 as 0x37 copies of 2 -- so byte 0 alone cannot tell them apart,
** but byte 2 can.
*/
static void test_abi_fill_argorder(void)
{
    vram_poison(TESTVRAM, 8, 0x00);

    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0x37, 2);

    t_check(vpeek(TESTVRAM + 0) == 0x37 &&
            vpeek(TESTVRAM + 1) == 0x37 &&
            vpeek(TESTVRAM + 2) == 0x00,
            "ABI_FILL_ARGORDER");
}

/* Bit 16 of the address is the only part of the high half that reaches
** ADDR_H. Drop it and the write lands at $00000 instead of $10000.
*/
static void test_abi_addr_bank(void)
{
    vram_poison(TESTVRAM_HI, 4, 0x00);
    vram_poison(0x00000UL, 4, 0x00);

    x16_vera_addr0(X16_INC_1, TESTVRAM_HI);
    x16_vera_fill(0x5A, 4);

    t_check(vram_all(TESTVRAM_HI, 4, 0x5A) &&
            vram_all(0x00000UL, 4, 0x00),       /* did not alias down */
            "ABI_ADDR_BANK");
}

/* X16_DECR walks the port backwards. Filling 3 bytes down from +7 must
** touch 5, 6, 7 and nothing else -- an ascending fill would have hit
** 7, 8, 9 instead, which is why the poisoned window extends past the end.
*/
static void test_abi_addr_decr(void)
{
    vram_poison(TESTVRAM, 12, 0x00);

    x16_vera_addr0(X16_INC_1 | X16_DECR, TESTVRAM + 7);
    x16_vera_fill(0xC3, 3);

    t_check(vram_all(TESTVRAM + 5, 3, 0xC3) &&
            vram_all(TESTVRAM, 5, 0x00) &&
            vram_all(TESTVRAM + 8, 4, 0x00),
            "ABI_ADDR_DECR");
}

/* x16_screen_color(fg, bg) packs to fg | bg<<4. Swap the two and 1,6
** becomes 0x16 instead of 0x61 -- both plausible-looking bytes.
*/
static void test_abi_color_argorder(void)
{
    x16_screen_color(1, 6);
    t_check(*(unsigned char *)0x0376 == 0x61, "ABI_COLOR_ARGORDER");
}

/* x16_screen_locate(row, col) with row != col, read back through the
** other entry point. A transposed shim passes its own round trip, so
** the check is against the KERNAL's cursor, not against ourselves.
*/
static void test_abi_locate_argorder(void)
{
    unsigned char row = 0xFF, col = 0xFF;

    x16_screen_locate(3, 11);
    x16_screen_get_cursor(&row, &col);

    t_check(row == 3 && col == 11, "ABI_LOCATE_ARGORDER");
}

/* x16_pal_set(index, color). Index 3 and colour 0x0F00 are both small
** and both plausible; if they swap, entry 0 gets written instead.
*/
static void test_abi_pal_argorder(void)
{
    x16_pal_set(0, 0x0000);                     /* poison entry 0 */
    x16_pal_set(3, 0x0F00);

    t_check(vpeek(X16_VRAM_PALETTE + 6) == 0x00 &&      /* entry 3 lo */
            vpeek(X16_VRAM_PALETTE + 7) == 0x0F &&      /* entry 3 hi */
            vpeek(X16_VRAM_PALETTE + 1) == 0x00,        /* entry 0 untouched */
            "ABI_PAL_ARGORDER");
}

/* Eight one-byte arguments, the widest shim in the library. The geometry
** is picked so that transposing ANY adjacent pair -- ax/ay, aw/ah, bx/by,
** bw/bh -- flips the answer from 1 to 0, and so does reversing the whole
** pop order:
**
**      A spans x[10,30) y[100,104)
**      B spans x[28,30) y[70,105)
**
** They overlap by two columns and four rows. Make A tall instead of wide,
** or B wide instead of tall, and the overlap vanishes.
*/
static void test_abi_collide8_argorder(void)
{
    t_check(x16_collide8(10, 100, 20, 4, 28, 70, 2, 35) == 1,
            "ABI_COLLIDE8_ARGORDER");
}

/* x16_gfx_line(x0, y0, x1, y1, color): five arguments of mixed width.
** All four coordinates differ, and the line is drawn between two points
** that a swapped x/y would miss entirely. Checking the midpoint as well
** as the endpoints catches a shim that got only the first pop right.
*/
static void test_abi_gfx_line_argorder(void)
{
    vpoke(0x00, PIXEL(3, 7));
    vpoke(0x00, PIXEL(7, 3));           /* where a transposed line would go */
    vpoke(0x00, PIXEL(11, 5));

    x16_gfx_line(3, 7, 11, 5, 0x77);

    t_check(vpeek(PIXEL(3, 7)) == 0x77 &&
            vpeek(PIXEL(11, 5)) == 0x77 &&
            vpeek(PIXEL(7, 3)) == 0x00,
            "ABI_GFX_LINE_ARGORDER");
}

/* x16_gfx_rect(x, y, w, h, color) with w != h. Transpose them and the
** far corner is never painted.
*/
static void test_abi_gfx_rect_wh(void)
{
    vpoke(0x00, PIXEL(9, 4));
    vpoke(0x00, PIXEL(4, 5));

    x16_gfx_rect(4, 3, 6, 2, 0x88);     /* x[4,10) y[3,5) */

    t_check(vpeek(PIXEL(9, 4)) == 0x88 &&       /* far corner painted */
            vpeek(PIXEL(4, 5)) == 0x00,         /* one row past the end */
            "ABI_GFX_RECT_WH");
}

/* A shim that returns a boolean in A but forgets `ldx #0` works until a
** caller promotes the result to int and reads X as the high byte.
*/
static void test_abi_bool_return(void)
{
    int v = x16_vera_has_fx();
    int m = x16_screen_set_mode(X16_MODE_80x60);
    int c = x16_collide8(0, 0, 10, 10, 5, 5, 10, 10);

    t_check(v == 1 && m == 1 && c == 1, "ABI_BOOL_RETURN");
}

/* The mirror image: a shim that sets A but forgets X (16-bit) or sreg
** (32-bit) is right for results under 256 and wrong above.
*/
static void test_abi_int_return_hi(void)
{
    int m = x16_mul88(0x0180, 0x0200);          /* 0x0300, needs X */
    unsigned long p = x16_umul16(1000, 1000);   /* 0x000F4240, needs sreg */

    t_check(m == 0x0300 && p == 1000000UL, "ABI_INT_RETURN_HI");
}

/* Out-params through pointers. Both bytes of both words differ, so a
** swapped pointer or a dropped high byte shows up.
*/
static void test_abi_out_params(void)
{
    unsigned int x = 0xFFFF, y = 0xFFFF;
    unsigned char row = 0xFF, col = 0xFF;

    x16_sprite_pos(7, 0x123, 0x2AB);
    x16_sprite_get_pos(7, &x, &y);

    x16_screen_locate(4, 9);
    x16_screen_get_cursor(&row, &col);

    t_check(x == 0x123 && y == 0x2AB && row == 4 && col == 9,
            "ABI_OUT_PARAMS");
}

/* x16_bank_poke(bank, offset, value). Bank, both offset bytes and the
** value are all distinct, so any transposition writes somewhere else --
** and the offset is deliberately >255 so a shim that dropped its high
** byte would land at 0x02 instead of 0x0102.
*/
static void test_abi_bank_argorder(void)
{
    x16_bank_poke(3, 0x0102, 0x7F);

    t_check(x16_bank_peek(3, 0x0102) == 0x7F &&
            x16_bank_peek(3, 0x0002) != 0x7F &&  /* high byte not dropped */
            x16_bank_peek(0x7F, 0x0102) != 0x7F, /* bank and value not swapped */
            "ABI_BANK_ARGORDER");
}

/* x16_tile_put(col, row, code, attr): four one-byte arguments, and the
** two that address the cell are the ones the shim juggles across the
** hardware stack. col != row and code != attr, so any transposition
** lands somewhere visible.
*/
static void test_abi_tile_argorder(void)
{
    unsigned int cell;

    x16_tile_put(9, 4, 0x2A, 0x5B);
    cell = x16_tile_get(9, 4);

    t_check(vpeek(tile_addr(9, 4)) == 0x2A &&
            vpeek(tile_addr(9, 4) + 1) == 0x5B &&
            X16_TILE_CODE(cell) == 0x2A &&
            X16_TILE_ATTR(cell) == 0x5B &&
            vpeek(tile_addr(4, 9)) != 0x2A,     /* not at the transpose */
            "ABI_TILE_ARGORDER");
}

/* x16_fx_fill(value, count, addr): value and count would both be small
** and plausible if swapped. Fill 3 bytes with 0x0C, not 12 bytes with 3.
*/
static void test_abi_fx_fill_argorder(void)
{
    vram_poison(TESTVRAM, 16, 0x00);

    x16_fx_fill(0x0C, 3, TESTVRAM);

    t_check(vram_all(TESTVRAM, 3, 0x0C) &&
            vpeek(TESTVRAM + 3) == 0x00 &&
            vpeek(TESTVRAM + 12) == 0x00,
            "ABI_FX_FILL_ARGORDER");
}

/* Bit 16 of a sprite's image address lives in byte 1 of the record.
** $13000 has it set, $03000 does not; everything else about the two is
** identical, so only that bit distinguishes them.
*/
static void test_abi_sprite_image_bank(void)
{
    unsigned char hi_bank, lo_bank;

    x16_sprite_image(4, X16_SPRITE_8BPP, 0x13000UL);
    hi_bank = vpeek(X16_VRAM_SPRITE_ATTR + 4 * 8 + 1);

    x16_sprite_image(4, X16_SPRITE_8BPP, 0x03000UL);
    lo_bank = vpeek(X16_VRAM_SPRITE_ATTR + 4 * 8 + 1);

    t_check(hi_bank == 0x89 && lo_bank == 0x81, "ABI_SPRITE_IMAGE_BANK");
}

/* The float binding crosses a ROM bank on every call, through the
** KERNAL's jsrfar. This proves the whole path survives cc65's register
** usage -- and that x16_f_to_str() copied the result out of the stack
** page before the C call frames trampled it.
*/
static void test_abi_float_through_c(void)
{
    x16_f_from_s16(4);
    x16_f_store(fb);
    x16_f_from_s16(10);
    x16_f_div(fb);
    x16_f_to_str_trim(fbuf);

    t_check(streq(fbuf, "2.5"), "ABI_FLOAT_THROUGH_C");
}

/* Every shim pops its stack arguments itself. Pop one too few or too
** many and cc65's software stack pointer drifts; locals are addressed
** off it, so a sentinel that survives a few hundred calls proves the pop
** counts match the declared argument widths. Without this, a wrong count
** surfaces as a crash several calls later, nowhere near the cause.
**
** Exercise the widest shims: eight bytes, five mixed, four with pointers.
*/
static void test_abi_stack_balance(void)
{
    unsigned int guard = 0x1234;
    unsigned int sx, sy;
    unsigned char i;

    for (i = 0; i < 20; ++i) {
        x16_vera_addr0(X16_INC_1, TESTVRAM);
        x16_vera_fill(0x00, 1);
        x16_vera_addr1(X16_INC_1, TESTVRAM + 8);
        x16_vera_addr0(X16_INC_1, TESTVRAM);
        x16_vera_copy(1);
        x16_collide8(1, 2, 3, 4, 5, 6, 7, 8);
        x16_gfx_line(0, 0, 2, 2, 0);
        x16_gfx_rect(0, 0, 2, 2, 0);
        x16_sprite_size(1, 0, 0, 0);
        x16_sprite_pos(1, 1, 1);
        x16_sprite_get_pos(1, &sx, &sy);
        x16_sprite_image(1, X16_SPRITE_4BPP, 0x13000UL);
        x16_mul88(0x0100, 0x0100);
    }

    t_check(guard == 0x1234, "ABI_STACK_BALANCE");
}

/* ------------------------------------------------------------------ */

int main(void)
{
    t_init();

    test_zp_in_window();

    test_fill();
    test_fill_zero();
    test_fill_stride();
    test_fill_16bit();
    test_copy();
    test_has_fx();

    test_border();
    test_set_mode();
    test_set_mode_bad();
    test_cls_clears();
    test_color_reaches_vram();

    test_palette();
    test_pal_load();
    test_pal_load_zero();

    test_mul88();
    test_mul88_negative();
    test_umul16();

    test_collide_overlap();
    test_collide_apart();
    test_collide_touching();
    test_collide16();

    x16_sprite_init_all();      /* give the write-only attr RAM a shadow */
    test_sprite_pos();
    test_sprite_image();
    test_sprite_z();
    test_sprite_size();
    test_sprite_size_pal_mask();

    test_gfx_clear();           /* first: it repaints the whole framebuffer */
    test_gfx_pset();
    test_gfx_clip();
    test_gfx_hline();
    test_gfx_vline();
    test_gfx_line();
    test_gfx_frame();

    test_tile_addr();
    test_tile_roundtrip();
    test_layer_scroll();
    test_layer_enable();

    /* On a VERA without the FX register set these routines would write to
    ** registers that do not exist. Skipping is honest; the capability was
    ** independently probed, not assumed absent because something failed.
    */
    if (x16_vera_has_fx()) {
        test_fx_mult();
        test_fx_mult_signed();
        test_fx_accum_dirty();
        test_fx_fill();
        test_fx_clear();
        test_fx_leaves_clean();
    } else {
        t_skip("FX_MULT");
        t_skip("FX_MULT_SIGNED");
        t_skip("FX_ACCUM_DIRTY");
        t_skip("FX_FILL");
        t_skip("FX_CLEAR");
        t_skip("FX_LEAVES_CLEAN");
    }

    test_irq_hook();
    test_irq_preserves_iflag();
    test_vsync_counter();

    test_joy_get();
    test_joy_absent();
    test_joy_present();
    test_key_peek_empty();

    test_psg_regs();
    test_psg_hz();
    test_psg_note_off();
    test_pcm_rate_clamp();
    test_ym_write();

    /* x16_ym_init() reports whether this machine has a YM2151 at all.
    ** Skipping on a machine without one is honest; failing would not be.
    */
    if (x16_ym_init()) {
        test_ym_channel_in_a();
    } else {
        t_skip("YM_CHANNEL_IN_A");
    }

    test_bank_roundtrip();
    test_bank_boundary();
    test_bank_to_mem();

    test_fs_roundtrip();
    test_fs_load_null_end();
    test_fs_load_missing();
    test_fs_vload();

    test_f_roundtrip();
    test_f_neg();
    test_f_from_u8();
    test_f_sub_order();
    test_f_div_order();
    test_f_sqrt();
    test_f_str();
    test_f_from_str();
    test_f_cmp();

    test_abi_fill_argorder();
    test_abi_addr_bank();
    test_abi_addr_decr();
    test_abi_bool_return();
    test_abi_int_return_hi();
    test_abi_color_argorder();
    test_abi_locate_argorder();
    test_abi_pal_argorder();
    test_abi_collide8_argorder();
    test_abi_gfx_line_argorder();
    test_abi_gfx_rect_wh();
    test_abi_out_params();
    test_abi_sprite_image_bank();
    test_abi_tile_argorder();
    test_abi_bank_argorder();
    test_abi_float_through_c();
    if (x16_vera_has_fx()) {
        test_abi_fx_fill_argorder();
    } else {
        t_skip("ABI_FX_FILL_ARGORDER");
    }
    test_abi_stack_balance();

    t_done();
    return 0;
}
