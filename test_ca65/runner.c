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

/* ---------------------------------------------------------------------
 * THE SUITE IS BUILT TWICE.
 *
 * All 28 library modules plus 150-odd test functions no longer fit in the
 * X16's 38.6 KB of program RAM: the code alone reaches 33 KB, leaving no
 * room for BSS. So runner.c compiles into two PRGs -- test/runner2.c is
 * three lines that set SUITE to 2 and include this file -- and build.ps1
 * runs both, summing the results.
 *
 * Helpers below stay outside the guards and compile into both. Only the
 * test functions and their calls are split.
 * ------------------------------------------------------------------ */
#ifndef SUITE
#define SUITE 1
#endif

#include "testlib.h"
#include <cbm.h>
#include <cx16.h>
#include <x16/x16.h>
#include <x16/vera.h>
#include <x16/screen.h>
#include <x16/palette.h>
#include <x16/sprite.h>
#include <x16/bitmap.h>
#include <x16/bitmap2.h>
#include <x16/shapes.h>
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
#include <x16/mem.h>
#include <x16/math.h>
#include <x16/clip.h>
#include <x16/buffers.h>
#include <x16/dos.h>
#include <x16/adpcm.h>
#include <x16/zx0.h>
#include <x16/bmx.h>
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

#if SUITE == 2

/* ------------------------------------------------------------------ */
/* game math                                                           */
/* ------------------------------------------------------------------ */

/* The tables are precomputed literals, because ca65 has no assembly-time
** floating point and cannot evaluate ACME's `!for ... sin()`. So pin the
** values that would move if anyone regenerated them wrong: the four
** quarter points of the sine, and the ends of the arctangent table.
*/
static void test_math_tables(void)
{
    t_check(x16_sin8(0) == 0 &&
            x16_sin8(64) == 127 &&              /* +90 degrees */
            x16_sin8(128) == 0 &&
            x16_sin8(192) == -127 &&            /* -90 degrees */
            x16_cos8(0) == 127 &&               /* cos(a) = sin(a+64) */
            x16_cos8(64) == 0 &&
            x16_sin8u(64) == 255 &&             /* biased by 128 */
            x16_sin8u(192) == 1,
            "MATH_TABLES");
}

/* xorshift has period 65535 and one fixed point at zero, which the seed
** routine must avoid. The same seed must replay the same sequence.
*/
static void test_rnd(void)
{
    unsigned int a, b, c;

    x16_rnd_seed(0x1234);
    a = x16_rnd16();
    b = x16_rnd16();

    x16_rnd_seed(0x1234);
    c = x16_rnd16();

    t_check(a == c && a != b, "RND_REPEATABLE");
}

static void test_rnd_zero_seed(void)
{
    unsigned int a, b;

    x16_rnd_seed(0);                    /* must be nudged off the fixed point */
    a = x16_rnd16();
    b = x16_rnd16();

    t_check(a != 0 && b != 0 && a != b, "RND_ZERO_SEED");
}

/* Angle 0 is east (+x); 64 is south (+y), because the screen's y axis
** points down. A byte angle wraps for free, so 192 is north.
*/
static void test_atan2(void)
{
    t_check(x16_atan2(100, 0) == 0 &&           /* east */
            x16_atan2(0, 100) == 64 &&          /* south, down-screen */
            x16_atan2(-100, 0) == 128 &&        /* west */
            x16_atan2(0, -100) == 192 &&        /* north */
            x16_atan2(50, 50) == 32 &&          /* exactly 45 degrees */
            x16_atan2(0, 0) == 0,               /* degenerate: call it east */
            "ATAN2");
}

/* atan2 must agree with the sine table it shares a convention with:
** walking one step along the returned heading moves toward the target.
*/
static void test_atan2_roundtrip(void)
{
    unsigned char a = x16_atan2(-60, 60);       /* south-west: 96 = 135 deg */

    t_check(a == 96 && x16_cos8(a) < 0 && x16_sin8(a) > 0,
            "ATAN2_ROUNDTRIP");
}

/* Exact at both ends; the interior is a /256 approximation of /255. */
static void test_lerp8(void)
{
    t_check(x16_lerp8(10, 200, 0) == 10 &&
            x16_lerp8(10, 200, 255) == 200 &&
            x16_lerp8(200, 10, 0) == 200 &&     /* descending */
            x16_lerp8(200, 10, 255) == 10 &&
            x16_lerp8(0, 100, 128) == 50 &&     /* halfway, within one */
            x16_lerp8(77, 77, 128) == 77,       /* a == b */
            "LERP8");
}

#endif /* SUITE == 2 */

#if SUITE == 2

/* ------------------------------------------------------------------ */
/* ring buffer and stack                                               */
/* ------------------------------------------------------------------ */

static void test_ringbuffer(void)
{
    x16_rb_init();

    t_check(x16_rb_get() == -1 &&               /* empty reads -1 */
            x16_rb_count() == 0 &&
            x16_rb_put(0x11) == 1 &&
            x16_rb_put(0x22) == 1 &&
            x16_rb_count() == 2 &&
            x16_rb_get() == 0x11 &&             /* FIFO order */
            x16_rb_get() == 0x22 &&
            x16_rb_get() == -1 &&
            x16_rb_count() == 0,
            "RINGBUFFER");
}

/* Capacity is 255, and the 8-bit head/tail wrap for free -- so pushing
** and draining twice as many bytes as the buffer holds must still come
** out in order.
*/
static void test_ringbuffer_wrap(void)
{
    unsigned int i;
    unsigned char ok = 1;

    x16_rb_init();
    for (i = 0; i < 255; ++i) {
        if (!x16_rb_put((unsigned char)i)) ok = 0;
    }
    if (x16_rb_put(0xFF) != 0) ok = 0;          /* the 256th is refused */
    if (x16_rb_count() != 255) ok = 0;

    for (i = 0; i < 200; ++i) {                 /* drain most of it... */
        if (x16_rb_get() != (int)(unsigned char)i) ok = 0;
    }
    for (i = 0; i < 100; ++i) {                 /* ...and refill past the wrap */
        if (!x16_rb_put((unsigned char)(0xC0 + i))) ok = 0;
    }
    for (i = 200; i < 255; ++i) {
        if (x16_rb_get() != (int)(unsigned char)i) ok = 0;
    }
    for (i = 0; i < 100; ++i) {
        if (x16_rb_get() != (int)(unsigned char)(0xC0 + i)) ok = 0;
    }
    t_check(ok && x16_rb_get() == -1, "RINGBUFFER_WRAP");
}

static void test_stack(void)
{
    x16_stk_init();

    t_check(x16_stk_pop() == -1 &&
            x16_stk_depth() == 0 &&
            x16_stk_push(0xAA) == 1 &&
            x16_stk_push(0xBB) == 1 &&
            x16_stk_depth() == 2 &&
            x16_stk_pop() == 0xBB &&            /* LIFO order */
            x16_stk_pop() == 0xAA &&
            x16_stk_pop() == -1,
            "STACK");
}

/* A byte of 0xFF must not be mistaken for the -1 that means "empty". */
static void test_buffers_ff_is_not_empty(void)
{
    x16_rb_init();
    x16_stk_init();
    x16_rb_put(0xFF);
    x16_stk_push(0xFF);

    t_check(x16_rb_get() == 0x00FF && x16_stk_pop() == 0x00FF,
            "BUFFERS_FF_NOT_EMPTY");
}

#endif /* SUITE == 2 */

#if SUITE == 2

/* ------------------------------------------------------------------ */
/* line clipping                                                       */
/* ------------------------------------------------------------------ */

/* Wholly inside: unchanged, and accepted. */
static void test_clip_inside(void)
{
    x16_line s = { 10, 20, 300, 200 };

    t_check(x16_clip_line(&s) == 1 &&
            s.x0 == 10 && s.y0 == 20 && s.x1 == 300 && s.y1 == 200,
            "CLIP_INSIDE");
}

/* Both endpoints share an outside half-plane: rejected without work. */
static void test_clip_reject(void)
{
    x16_line above = { -50, -10, 400, -5 };
    x16_line right = { 400, 10, 500, 200 };

    t_check(x16_clip_line(&above) == 0 && x16_clip_line(&right) == 0,
            "CLIP_REJECT");
}

/* A horizontal line crossing both vertical edges: y is unchanged, and
** the ends land exactly on xmin and xmax (inclusive: 0 and 319).
*/
static void test_clip_horizontal(void)
{
    x16_line s = { -100, 120, 500, 120 };

    t_check(x16_clip_line(&s) == 1 &&
            s.x0 == 0 && s.x1 == 319 && s.y0 == 120 && s.y1 == 120,
            "CLIP_HORIZONTAL");
}

/* A 45-degree diagonal from (-40,-40): it must enter the rectangle
** exactly at the origin, because x and y cross zero together.
*/
static void test_clip_diagonal(void)
{
    x16_line s = { -40, -40, 100, 100 };

    t_check(x16_clip_line(&s) == 1 &&
            s.x0 == 0 && s.y0 == 0 && s.x1 == 100 && s.y1 == 100,
            "CLIP_DIAGONAL");
}

/* A user rectangle, and a segment clipped to it on all four sides. */
static void test_clip_set(void)
{
    x16_line s = { 0, 50, 319, 50 };
    unsigned char ok;

    x16_clip_set(100, 40, 200, 60);
    ok = x16_clip_line(&s) == 1 && s.x0 == 100 && s.x1 == 200;

    x16_clip_set(0, 0, 319, 239);       /* back to the full bitmap */
    t_check(ok, "CLIP_SET");
}

/* Reversing the endpoints must clip to the same segment, reversed --
** the algorithm moves whichever end is outside, and both can be.
*/
static void test_clip_symmetry(void)
{
    x16_line a = { -100, 120, 500, 120 };
    x16_line b = { 500, 120, -100, 120 };

    t_check(x16_clip_line(&a) == 1 && x16_clip_line(&b) == 1 &&
            a.x0 == b.x1 && a.x1 == b.x0,
            "CLIP_SYMMETRY");
}

#endif /* SUITE == 2 */

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

#if SUITE == 2

/* A midpoint circle: the four cardinal points are exact, the centre is
** hollow, and nothing lands one pixel outside the radius.
*/
static void test_gfx_circle(void)
{
    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 12800);         /* rows 0..39 */

    x16_gfx_circle(50, 20, 10, 0x91);

    t_check(vpeek(PIXEL(60, 20)) == 0x91 &&     /* east */
            vpeek(PIXEL(40, 20)) == 0x91 &&     /* west */
            vpeek(PIXEL(50, 10)) == 0x91 &&     /* north */
            vpeek(PIXEL(50, 30)) == 0x91 &&     /* south */
            vpeek(PIXEL(50, 20)) == 0x00 &&     /* hollow */
            vpeek(PIXEL(61, 20)) == 0x00,       /* nothing outside */
            "GFX_CIRCLE");
}

/* Radius 0 is a single point, not nothing. */
static void test_gfx_circle_r0(void)
{
    vpoke(0x00, PIXEL(100, 20));
    vpoke(0x00, PIXEL(101, 20));

    x16_gfx_circle(100, 20, 0, 0x92);

    t_check(vpeek(PIXEL(100, 20)) == 0x92 && vpeek(PIXEL(101, 20)) == 0x00,
            "GFX_CIRCLE_R0");
}

/* A disc is solid to the rim and empty past it. */
static void test_gfx_disc(void)
{
    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 12800);

    x16_gfx_disc(50, 20, 10, 0x93);

    t_check(vpeek(PIXEL(50, 20)) == 0x93 &&     /* centre filled */
            vpeek(PIXEL(55, 20)) == 0x93 &&
            vpeek(PIXEL(60, 20)) == 0x93 &&     /* rim */
            vpeek(PIXEL(61, 20)) == 0x00 &&     /* one past */
            vpeek(PIXEL(50, 31)) == 0x00,
            "GFX_DISC");
}

/* Circle OUTLINES clip: the midpoint walk plots through the clipping
** x16_gfx_pset, so an outline centred off the left edge draws only its
** on-screen pixels and never wraps to the far side of a row. (Disc FILLS
** are unclipped by module policy -- keep a disc on screen -- so this tests
** the outline, whose west extreme at x=-8 must simply vanish.)
*/
static void test_gfx_circle_clips(void)
{
    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 12800);

    x16_gfx_circle(2, 20, 10, 0x94);            /* west extreme off-screen */

    t_check(vpeek(PIXEL(12, 20)) == 0x94 &&     /* east extreme, on screen */
            vpeek(PIXEL(2, 10)) == 0x94 &&      /* north extreme, on screen */
            vpeek(PIXEL(319, 20)) == 0x00 &&    /* west did not wrap */
            vpeek(PIXEL(319, 19)) == 0x00,
            "GFX_CIRCLE_CLIPS");
}

/* A glyph from the KERNAL's charset. Screen code 1 is 'A'; its top row
** is blank and it has set pixels in the middle rows. Clear bits stay
** transparent, so the background shows through.
*/
static void test_gfx_char(void)
{
    unsigned char i, set = 0;

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 12800);

    x16_gfx_char(40, 8, 0x95, 1);       /* screen code 1 = 'A' */

    for (i = 0; i < 8; ++i) {
        unsigned char j;
        for (j = 0; j < 8; ++j) {
            if (vpeek(PIXEL(40 + j, 8 + i)) == 0x95) ++set;
        }
    }
    /* Some pixels lit, but not the whole 8x8 cell: it is a glyph, and
    ** the clear bits were left transparent.
    */
    t_check(set > 4 && set < 64 && vpeek(PIXEL(48, 8)) == 0x00,
            "GFX_CHAR");
}

/* x16_gfx_text advances the pen 8 pixels per character, so the second
** glyph of "AB" starts 8 to the right of the first, and the two differ.
*/
static void test_gfx_text(void)
{
    unsigned char i, a = 0, b = 0;

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 12800);

    x16_gfx_text(100, 8, 0x96, "AB");

    for (i = 0; i < 8; ++i) {
        unsigned char j;
        for (j = 0; j < 8; ++j) {
            if (vpeek(PIXEL(100 + j, 8 + i)) == 0x96) ++a;
            if (vpeek(PIXEL(108 + j, 8 + i)) == 0x96) ++b;
        }
    }
    t_check(a > 4 && b > 4 && a != b, "GFX_TEXT");
}

/* Flood a rectangle bounded by a frame: the interior fills, the frame
** survives, and the pixel just outside is untouched.
*/
static void test_gfx_flood(void)
{
    unsigned char ok;

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 12800);

    x16_gfx_frame(200, 4, 20, 20, 0x97);        /* a hollow box */
    ok = x16_gfx_flood(205, 10, 0x98);          /* seed inside it */

    t_check(ok == 1 &&
            vpeek(PIXEL(205, 10)) == 0x98 &&    /* the seed */
            vpeek(PIXEL(218, 22)) == 0x98 &&    /* the far interior corner */
            vpeek(PIXEL(200, 4)) == 0x97 &&     /* the frame survived */
            vpeek(PIXEL(199, 10)) == 0x00,      /* did not leak out */
            "GFX_FLOOD");
}

/* Filling with the colour already under the seed is a no-op, not an
** infinite loop.
*/
static void test_gfx_flood_noop(void)
{
    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x33, 640);           /* rows 0..1 */

    t_check(x16_gfx_flood(10, 1, 0x33) == 1 && vpeek(PIXEL(10, 1)) == 0x33,
            "GFX_FLOOD_NOOP");
}

#endif /* SUITE == 2 */

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

/* Hardware Bresenham. Both endpoints must be exact, and nothing may land
** at the transpose of the start point -- which is where a swapped x/y
** shim would put the line.
*/
static void test_fx_line(void)
{
    unsigned char i, ok = 1;

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 2560);          /* rows 0..7 */

    x16_fx_line(0, 0, 7, 0, 0xC1);      /* horizontal */
    x16_fx_line(20, 0, 27, 7, 0xC2);    /* diagonal */
    x16_fx_line(60, 0, 53, 7, 0xC3);    /* anti-diagonal */
    x16_fx_line(100, 0, 103, 7, 0xC4);  /* y-major slant */

    for (i = 0; i < 8; ++i) {
        if (vpeek(PIXEL(i, 0)) != 0xC1) ok = 0;         /* the whole run */
        if (vpeek(PIXEL(20 + i, i)) != 0xC2) ok = 0;    /* every step */
        if (vpeek(PIXEL(60 - i, i)) != 0xC3) ok = 0;
    }
    ok = ok && vpeek(PIXEL(8, 0)) == 0x00;              /* stopped on time */
    ok = ok && vpeek(PIXEL(21, 0)) == 0x00;             /* not a transpose */

    /* A y-major slant draws one pixel per row; only the endpoints are
    ** pinned, because the interior is VERA's rounding, not ours.
    */
    ok = ok && vpeek(PIXEL(100, 0)) == 0xC4 &&
               vpeek(PIXEL(103, 7)) == 0xC4;

    t_check(ok, "FX_LINE");
}

/* THE CARRY TRAP.
**
** Writing the increment seeds the subpixel accumulator to half a pixel,
** but leaves the position's carry bit alone -- whatever the FX reference
** implies. So a line drawn straight after another whose slope did not
** land on a whole pixel inherits that carry, and its FIRST minor-axis
** step is eaten: the line starts one pixel off the diagonal and never
** recovers.
**
** What actually leaves a carry is the POLYGON filler, not another line:
** it steps two edge accumulators twice per row. A line after a line does
** not reproduce it, which is why FX_LINE above stays green even with the
** guard removed. So: a slant, then a triangle, then a clean 45-degree
** diagonal whose every pixel is checked.
**
** Remove the `stz VERA_FX_X_POS_*` from fx_line and this is the one test
** that goes red.
*/
static void test_fx_line_carry(void)
{
    static const x16_point ta = { 200, 0 };
    static const x16_point tb = { 213, 0 };
    static const x16_point tc = { 203, 6 };
    unsigned char i, ok = 1;

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 2560);

    /* Three ways to leave the accumulator dirty, then a clean diagonal. */
    x16_fx_line(100, 0, 103, 7, 0xE1);          /* slope 3/7 */
    x16_fx_triangle(&ta, &tb, &tc, 0xE3);       /* poly: two edge accumulators */
    x16_fx_line(140, 0, 147, 7, 0xE2);          /* now a clean 1:1 diagonal */

    for (i = 0; i < 8; ++i) {
        if (vpeek(PIXEL(140 + i, i)) != 0xE2) ok = 0;
    }
    t_check(ok, "FX_LINE_CARRY");
}

/* A vertical and a single-pixel line: the minor delta is zero, so the FX
** helper is bypassed and port 1's increment does the walking.
*/
static void test_fx_line_axis(void)
{
    unsigned char i, ok = 1;

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 2560);

    x16_fx_line(10, 0, 10, 7, 0xD1);    /* vertical */
    x16_fx_line(200, 3, 200, 3, 0xD2);  /* a point */

    for (i = 0; i < 8; ++i) {
        if (vpeek(PIXEL(10, i)) != 0xD1) ok = 0;
    }
    ok = ok && vpeek(PIXEL(11, 0)) == 0x00 &&
               vpeek(PIXEL(200, 3)) == 0xD2;

    t_check(ok, "FX_LINE_AXIS");
}

/* Spans verified against the emulator. Right triangle, flat top edge. */
static void test_fx_triangle(void)
{
    static const x16_point a = { 10, 5 };
    static const x16_point b = { 30, 5 };
    static const x16_point c = { 10, 25 };
    unsigned char ok;

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 9600);          /* rows 0..29 */

    x16_fx_triangle(&a, &b, &c, 0xAA);

    ok = vpeek(PIXEL(9, 5)) == 0x00 &&          /* left of it */
         vpeek(PIXEL(10, 5)) == 0xAA &&         /* top row runs 10..29 */
         vpeek(PIXEL(29, 5)) == 0xAA &&
         vpeek(PIXEL(30, 5)) == 0x00 &&         /* (30,5) is outside */
         vpeek(PIXEL(19, 15)) == 0xAA &&        /* row 15 runs 10..19 */
         vpeek(PIXEL(20, 15)) == 0x00 &&
         vpeek(PIXEL(10, 24)) == 0xAA &&        /* last drawn row: one pixel */
         vpeek(PIXEL(11, 24)) == 0x00 &&
         vpeek(PIXEL(10, 25)) == 0x00;          /* half-open: row y2 empty */

    t_check(ok, "FX_TRIANGLE");
}

/* A general triangle with the vertices deliberately out of order: the
** bottom one first, then the top, then the middle. fx_triangle sorts.
*/
static void test_fx_triangle_unsorted(void)
{
    static const x16_point bottom = { 45, 20 };
    static const x16_point top = { 40, 0 };
    static const x16_point middle = { 60, 10 };
    unsigned char ok;

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 9600);

    x16_fx_triangle(&bottom, &top, &middle, 0xAB);

    ok = vpeek(PIXEL(40, 0)) == 0xAB &&         /* apex */
         vpeek(PIXEL(41, 0)) == 0x00 &&
         vpeek(PIXEL(41, 9)) == 0x00 &&         /* row 9 runs 42..58 */
         vpeek(PIXEL(42, 9)) == 0xAB &&
         vpeek(PIXEL(58, 9)) == 0xAB &&
         vpeek(PIXEL(59, 9)) == 0x00 &&
         vpeek(PIXEL(43, 13)) == 0xAB &&        /* row 13 runs 43..54 */
         vpeek(PIXEL(54, 13)) == 0xAB &&
         vpeek(PIXEL(55, 13)) == 0x00 &&
         vpeek(PIXEL(45, 19)) == 0xAB &&        /* last row: one pixel */
         vpeek(PIXEL(45, 20)) == 0x00;          /* half-open bottom */

    t_check(ok, "FX_TRIANGLE_UNSORTED");
}

#if SUITE == 2

/* Cached VRAM-to-VRAM. The destination must be 4-byte aligned; a count
** that is not a multiple of four exercises the single-byte tail.
**
** Both long arguments come off the C stack as two popax pairs each, so a
** wrong pop order would corrupt an address rather than a value: hence
** distinct bytes and a guard one past the end.
*/
static void test_fx_copy(void)
{
    unsigned char i, ok = 1;

    for (i = 0; i < 10; ++i) {
        vpoke(0xA0 + i, TESTVRAM + i);          /* source: a ramp */
    }
    vram_poison(TESTVRAM + 0x100, 12, 0x00);    /* aligned destination */

    x16_fx_copy(TESTVRAM, TESTVRAM + 0x100, 10);

    for (i = 0; i < 10; ++i) {
        if (vpeek(TESTVRAM + 0x100 + i) != 0xA0 + i) ok = 0;
    }
    t_check(ok && vpeek(TESTVRAM + 0x100 + 10) == 0x00, "FX_COPY");
}

/* Bit 16 of each address must survive: copy across the VRAM bank
** boundary, from bank 0 to bank 1.
*/
static void test_fx_copy_bank(void)
{
    unsigned char i, ok = 1;

    for (i = 0; i < 8; ++i) {
        vpoke(0x50 + i, TESTVRAM + i);
    }
    vram_poison(TESTVRAM_HI, 8, 0x00);

    x16_fx_copy(TESTVRAM, TESTVRAM_HI, 8);

    for (i = 0; i < 8; ++i) {
        if (vpeek(TESTVRAM_HI + i) != 0x50 + i) ok = 0;
    }
    t_check(ok, "FX_COPY_BANK");
}

/* With transparency on, a zero byte written to a data port leaves the
** target alone. Enable, write, disable -- and check the enable actually
** reached FX_CTRL rather than being reset by the previous fx_* call.
*/
static void test_fx_transparency(void)
{
    vram_poison(TESTVRAM, 4, 0x77);

    x16_fx_transp_on();
    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0x00, 2);             /* zeros: must be dropped */
    x16_vera_addr0(X16_INC_1, TESTVRAM + 2);
    x16_vera_fill(0x5B, 2);             /* nonzero: must land */
    x16_fx_transp_off();

    t_check(vpeek(TESTVRAM + 0) == 0x77 &&      /* transparent */
            vpeek(TESTVRAM + 1) == 0x77 &&
            vpeek(TESTVRAM + 2) == 0x5B &&      /* opaque */
            vpeek(TESTVRAM + 3) == 0x5B,
            "FX_TRANSPARENCY");
}

/* ...and once off, a zero write is opaque again. */
static void test_fx_transparency_off(void)
{
    vram_poison(TESTVRAM, 2, 0x77);

    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0x00, 2);

    t_check(vpeek(TESTVRAM) == 0x00 && vpeek(TESTVRAM + 1) == 0x00,
            "FX_TRANSPARENCY_OFF");
}

#endif /* SUITE == 2 */

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

/* ------------------------------------------------------------------ */
/* raster and sprite-collision interrupts                              */
/* ------------------------------------------------------------------ */

/* VERA's base is $9F20. IEN is $9F26; $9F28 is IRQ_LINE on WRITE but
** SCANLINE on READ, so the line you programmed can never be read back.
**
** IEN is two registers in one, too: bit 7 is IRQ_LINE's bit 8 (read and
** write), while bit 6 is SCANLINE's bit 8 (read-only). That is why tsb
** and trb on IEN are safe -- their read-modify-write only ever puts bit 6
** back, and bit 6 ignores writes.
*/
#define VERA_IEN_REG      (*(volatile unsigned char *)0x9F26)
#define VERA_SCANLINE_L   (*(volatile unsigned char *)0x9F28)
#define VERA_IEN_LINE8    0x80
#define VERA_IEN_SCAN8    0x40

static void dummy_line_handler(void) { }
static void dummy_sprcol_handler(unsigned char groups) { (void)groups; }

/* Line 300 is $12C, so its bit 8 is set and rides in IEN bit 7. Line 100
** has bit 8 clear, so installing it must PUT THAT BIT BACK -- an install
** that only ever set it would leave line 100 firing at line 356.
**
** The low byte cannot be checked here (write-only). IRQ_LINE_AT_SCANLINE
** below proves it landed, by asking the beam.
*/
static void test_irq_line_regs(void)
{
    unsigned char ien_hi, ien_lo, after;

    x16_irq_line_install(300, dummy_line_handler);
    ien_hi = VERA_IEN_REG;

    x16_irq_line_install(100, dummy_line_handler);
    ien_lo = VERA_IEN_REG;

    x16_irq_line_remove();
    after = VERA_IEN_REG;

    x16_irq_remove();

    t_check((ien_hi & VERA_IEN_LINE8) != 0 && (ien_hi & 0x02) != 0 &&
            (ien_lo & VERA_IEN_LINE8) == 0 && (ien_lo & 0x02) != 0 &&
            (after & 0x02) == 0,        /* LINE disabled again */
            "IRQ_LINE_REGS");
}

static void test_sprcol_regs(void)
{
    unsigned char on, off;

    x16_irq_sprcol_install(dummy_sprcol_handler);
    on = VERA_IEN_REG;
    x16_irq_sprcol_remove();
    off = VERA_IEN_REG;

    x16_irq_remove();

    t_check((on & 0x04) != 0 && (off & 0x04) == 0, "SPRCOL_REGS");
}

/* A NULL handler is poll mode: the groups still accumulate, nothing is
** called. With no sprites rendered there are none, so the accumulator
** reads 0 -- and reads 0 again, because the read clears it.
*/
static void test_sprcol_poll(void)
{
    unsigned char first, second;

    x16_irq_sprcol_install((x16_sprcol_handler)0);
    first = x16_sprite_collisions();
    second = x16_sprite_collisions();
    x16_irq_sprcol_remove();
    x16_irq_remove();

    t_check(first == 0 && second == 0, "SPRCOL_POLL");
}

static unsigned char line_fired;

static void counting_line_handler(void)
{
    ++line_fired;
}

/* Does a raster interrupt actually reach the handler? Needs video, so it
** uses the same three-way jiffy oracle as VSYNC_COUNTER: frames stuck +
** jiffy stuck means the harness has no interrupts at all (skip); frames
** stuck + jiffy moving means the IRQ ran and our handler did not (fail).
**
** This is the test that catches a shim clobbering the handler address:
** the vector is only dereferenced when the interrupt fires, so no
** register check could ever see it.
*/
static void test_irq_line_fires(void)
{
    clock_t jiffy0;
    unsigned int spin;

    line_fired = 0;
    jiffy0 = clock();
    x16_irq_line_install(240, counting_line_handler);

    for (spin = 0; spin < 30000 && !line_fired; ++spin) {
        /* bounded: a broken handler must fail, not hang the harness */
    }

    x16_irq_line_remove();
    x16_irq_remove();

    if (line_fired) {
        t_check(1, "IRQ_LINE_FIRES");
    } else if (clock() == jiffy0) {
        t_skip("IRQ_LINE_FIRES");       /* headless: no interrupts at all */
    } else {
        t_check(0, "IRQ_LINE_FIRES");
    }
}

static unsigned int line_scan;

/* Read the 9-bit scanline: low byte from $9F28, bit 8 from IEN bit 6. */
static void scanline_probe_handler(void)
{
    line_scan = VERA_SCANLINE_L;
    if (VERA_IEN_REG & VERA_IEN_SCAN8) {
        line_scan |= 0x100;
    }
    ++line_fired;
}

/* Did the interrupt fire WHERE it was asked to?
**
** IRQ_LINE is write-only, so the only way to know the line number landed
** -- low byte and bit 8 both -- is to ask the beam once the handler runs.
** Line 300 has bit 8 set; line 100 does not. Drop bit 8 and 300 becomes
** 44, drop the low byte and 300 becomes 256: either way, far outside the
** window.
**
** VERA renders a line ahead of scanout, and the KERNAL's IRQ stub runs
** before our handler, so allow some slack.
*/
static void check_line_at(unsigned int want, const char *name)
{
    clock_t jiffy0 = clock();
    unsigned int spin;
    int delta;

    line_fired = 0;
    line_scan = 0xFFFF;
    x16_irq_line_install(want, scanline_probe_handler);

    for (spin = 0; spin < 30000 && !line_fired; ++spin) {
    }

    x16_irq_line_remove();
    x16_irq_remove();

    if (!line_fired) {
        if (clock() == jiffy0) {
            t_skip(name);               /* headless: no video, no raster IRQ */
        } else {
            t_check(0, name);
        }
        return;
    }
    delta = (int)line_scan - (int)want;
    t_check(delta >= -4 && delta <= 12, name);
}

static void test_irq_line_at_scanline(void)
{
    check_line_at(100, "IRQ_LINE_AT_100");     /* bit 8 clear */
    check_line_at(300, "IRQ_LINE_AT_300");     /* bit 8 set */
}

static unsigned char zp_fired;

/* Runs inside the interrupt and deliberately uses the library's scratch
** block: x16_bank_peek() writes X16_P0/P1 and X16_T0/T1.
*/
static void zp_clobbering_handler(void)
{
    (void)x16_bank_peek(2, 0x0102);
    ++zp_fired;
}

/* Does the handler wrapper really preserve the zero page?
**
** Park a sentinel in X16_P0 -- found through x16_zp_base(), so nothing is
** hardcoded -- then spin until a raster interrupt has run a handler that
** overwrites that very byte. If save_zp/restore_zp work, the sentinel is
** still there. If they do not, X16_P0 holds 0x02, the low byte of the
** offset the handler passed to x16_bank_peek().
**
** Deterministic: the interrupt is guaranteed to land inside the spin.
** The same wrapper saves cc65's own runtime zero page by the same means,
** which is what lets a handler be written in C at all.
*/
static void test_irq_zp_preserved(void)
{
    volatile unsigned char *p0 = (volatile unsigned char *)(unsigned int)x16_zp_base();
    clock_t jiffy0;
    unsigned int spin;
    unsigned char survived;

    zp_fired = 0;
    jiffy0 = clock();

    /* Install FIRST: the installer itself passes the scanline through
    ** X16_P0/P1, so a sentinel written before it would not survive.
    */
    x16_irq_line_install(240, zp_clobbering_handler);
    *p0 = 0x5A;

    for (spin = 0; spin < 30000 && !zp_fired; ++spin) {
    }
    survived = (*p0 == 0x5A);

    x16_irq_line_remove();
    x16_irq_remove();

    if (zp_fired) {
        t_check(survived, "IRQ_ZP_PRESERVED");
    } else if (clock() == jiffy0) {
        t_skip("IRQ_ZP_PRESERVED");
    } else {
        t_check(0, "IRQ_ZP_PRESERVED");
    }
}

static unsigned char vreg_fired;

/* Runs inside the interrupt and clobbers the KERNAL's virtual registers:
** x16_mem_copy() loads r0, r1 and r2 before calling MEMORY_COPY.
*/
static void vreg_clobbering_handler(void)
{
    static unsigned char a[2], b[2];

    x16_mem_copy(a, b, 2);
    ++vreg_fired;
}

/* r0-r15 at $02-$21 are ordinary zero page, and the KERNAL does not
** preserve them across an interrupt. The foreground's x16_mem_copy()
** sets r0/r1/r2 and then runs MEMORY_COPY *with interrupts enabled*, so
** a callback that touches them corrupts the interrupted copy on resume.
**
** Park a sentinel in r0 and let a handler run x16_mem_copy(). Without
** the vreg half of save_zp/restore_zp, r0 comes back as a pointer.
*/
static void test_irq_vregs_preserved(void)
{
    volatile unsigned char *r0 = (volatile unsigned char *)0x02;
    clock_t jiffy0;
    unsigned int spin;
    unsigned char survived;

    vreg_fired = 0;
    jiffy0 = clock();

    x16_irq_line_install(240, vreg_clobbering_handler);
    *r0 = 0xA7;

    for (spin = 0; spin < 30000 && !vreg_fired; ++spin) {
    }
    survived = (*r0 == 0xA7);

    x16_irq_line_remove();
    x16_irq_remove();

    if (vreg_fired) {
        t_check(survived, "IRQ_VREGS_PRESERVED");
    } else if (clock() == jiffy0) {
        t_skip("IRQ_VREGS_PRESERVED");
    } else {
        t_check(0, "IRQ_VREGS_PRESERVED");
    }
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

#if SUITE == 2

/* The volume byte of a voice: pan bits in 7:6, volume in 5:0. */
#define PSG_VOL_OF(v)   (vpeek(X16_VRAM_PSG + (v) * 4 + 2) & 0x3F)
#define PSG_PAN_OF(v)   (vpeek(X16_VRAM_PSG + (v) * 4 + 2) & 0xC0)

/* Attack ramps up by `attack` per tick, stops at the peak, and the pan
** bits survive: the envelope drives only the volume.
*/
static void test_psg_env_attack(void)
{
    unsigned char v0, v1, v2, peak;

    x16_psg_init();
    x16_psg_set_vol(1, 0, X16_PSG_PAN_RIGHT);   /* establish the panning */
    x16_psg_env_start(1, 30, 10, 255, 5);       /* peak 30, +10/tick */

    v0 = PSG_VOL_OF(1);                         /* not written yet */
    x16_psg_env_tick(); v1 = PSG_VOL_OF(1);     /* 10 */
    x16_psg_env_tick(); v2 = PSG_VOL_OF(1);     /* 20 */
    x16_psg_env_tick();                         /* 30: clamps at the peak */
    x16_psg_env_tick();
    peak = PSG_VOL_OF(1);

    t_check(v0 == 0 && v1 == 10 && v2 == 20 && peak == 30 &&
            PSG_PAN_OF(1) == X16_PSG_PAN_RIGHT,
            "PSG_ENV_ATTACK");
}

/* attack = 0 jumps to the peak and writes it immediately, without
** waiting for a tick.
*/
static void test_psg_env_instant(void)
{
    x16_psg_init();
    x16_psg_set_vol(2, 0, X16_PSG_PAN_BOTH);
    x16_psg_env_start(2, 45, 0, 255, 5);

    t_check(PSG_VOL_OF(2) == 45, "PSG_ENV_INSTANT");
}

/* A sustain count of 255 holds until release is asked for; then the
** volume ramps down and the voice disarms at zero.
**
** The release step deliberately does NOT divide the peak: 40 steps down
** by 12 to 28, 16, 4, and the next step would go below zero. A step that
** divided evenly (10 into 40) would land on zero exactly and never
** exercise the underflow clamp -- and an unclamped volume does not merely
** read back wrong, it wraps to 248 and its high bits overwrite the pan
** field, because env_write ORs the volume in unmasked.
*/
static void test_psg_env_release(void)
{
    unsigned char held, r1, i;

    x16_psg_init();
    x16_psg_set_vol(3, 0, X16_PSG_PAN_BOTH);
    x16_psg_env_start(3, 40, 0, 255, 12);       /* instant peak, hold */

    x16_psg_env_tick();
    x16_psg_env_tick();
    held = PSG_VOL_OF(3);                       /* still 40 */

    x16_psg_env_release(3);
    x16_psg_env_tick();
    r1 = PSG_VOL_OF(3);                         /* 28 */

    for (i = 0; i < 8; ++i) {
        x16_psg_env_tick();                     /* ...down to silence */
    }
    t_check(held == 40 && r1 == 28 &&
            PSG_VOL_OF(3) == 0 &&
            PSG_PAN_OF(3) == X16_PSG_PAN_BOTH,  /* the clamp kept pan intact */
            "PSG_ENV_RELEASE");
}

/* A finite sustain counts down and then releases on its own. Again the
** step (6) does not divide the peak (20): 14, 8, 2, then underflow.
*/
static void test_psg_env_sustain(void)
{
    unsigned char after2, i;

    x16_psg_init();
    x16_psg_set_vol(4, 0, X16_PSG_PAN_BOTH);
    x16_psg_env_start(4, 20, 0, 2, 6);          /* hold 2 ticks, then fade */

    x16_psg_env_tick();
    x16_psg_env_tick();
    after2 = PSG_VOL_OF(4);                     /* still at the peak */

    for (i = 0; i < 5; ++i) {
        x16_psg_env_tick();
    }
    t_check(after2 == 20 && PSG_VOL_OF(4) == 0, "PSG_ENV_SUSTAIN");
}

static void test_psg_env_stop(void)
{
    x16_psg_init();
    x16_psg_set_vol(5, 0, X16_PSG_PAN_LEFT);
    x16_psg_env_start(5, 50, 0, 255, 0);

    x16_psg_env_stop(5);
    x16_psg_env_tick();                         /* disarmed: stays silent */

    t_check(PSG_VOL_OF(5) == 0 && PSG_PAN_OF(5) == X16_PSG_PAN_LEFT,
            "PSG_ENV_STOP");
}

#endif /* SUITE == 2 */

/* AFLOW streaming, primed at rate 0 so nothing actually plays -- which is
** what makes this runnable headless.
**
** Two shapes matter. A buffer bigger than the 4 KB FIFO must leave the
** stream active with the refill interrupt armed. One that fits outright
** must leave it inactive with AFLOW disabled: enabling AFLOW with no data
** behind it would storm, because nothing acknowledges AFLOW except
** refilling the FIFO.
*/
static void test_pcm_stream(void)
{
    unsigned char big_full, big_active, big_ien;
    unsigned char sml_active, sml_ien, sml_queued;
    unsigned char stopped_ien, stopped_active;

    x16_pcm_rate(0);
    x16_pcm_ctrl(X16_PCM_VOLUME(15));   /* 8-bit mono, full volume */
    x16_pcm_reset();

    /* Any readable RAM does as sample data. */
    x16_pcm_stream_start((const void *)0x2000, 5120, 0);   /* > 4 KB FIFO */
    big_full = x16_pcm_full();
    big_active = x16_pcm_stream_active();
    big_ien = VERA_IEN_REG & 0x08;      /* AFLOW enable */

    x16_pcm_stream_stop();
    stopped_ien = VERA_IEN_REG & 0x08;
    stopped_active = x16_pcm_stream_active();

    x16_pcm_reset();
    x16_pcm_stream_start((const void *)0x2000, 64, 0);      /* fits outright */
    sml_active = x16_pcm_stream_active();
    sml_ien = VERA_IEN_REG & 0x08;
    sml_queued = !x16_pcm_empty();

    x16_pcm_reset();
    x16_irq_remove();

    t_check(big_full == 1 && big_active == 1 && big_ien != 0 &&
            stopped_ien == 0 && stopped_active == 0 &&
            sml_active == 0 && sml_ien == 0 && sml_queued,
            "PCM_STREAM");
}

/* Play a real stream to the end, and check the refill interrupt turned
** itself off.
**
** This is the only test that reaches the ISR's exhaustion path: with the
** small buffer above, the data runs out while priming, before AFLOW was
** ever enabled. Here the buffer outlives the FIFO, so AFLOW is armed, the
** interrupt refills, and eventually the last byte goes in.
**
** AFLOW cannot be acknowledged -- it clears only by refilling the FIFO --
** so a refiller that finishes without disabling it in IEN storms forever
** and the machine livelocks. Remove the `trb VERA_IEN` from psf_exhausted
** and this run hangs rather than fails.
**
** Needs interrupts, so headless it skips, exactly like the raster tests.
*/
static void test_pcm_stream_exhaust(void)
{
    clock_t jiffy0 = clock();
    unsigned long spin;

    x16_pcm_rate(0);
    x16_pcm_ctrl(X16_PCM_VOLUME(1));    /* quiet: this really does play */
    x16_pcm_reset();

    x16_pcm_stream_start((const void *)0x2000, 5120, 128);

    for (spin = 0; spin < 200000UL && x16_pcm_stream_active(); ++spin) {
    }

    if (x16_pcm_stream_active()) {
        x16_pcm_stream_stop();
        if (clock() == jiffy0) {
            t_skip("PCM_STREAM_EXHAUST");       /* no interrupts here at all */
        } else {
            t_check(0, "PCM_STREAM_EXHAUST");   /* the IRQ ran; refill did not */
        }
    } else {
        t_check((VERA_IEN_REG & 0x08) == 0, "PCM_STREAM_EXHAUST");
    }

    x16_pcm_rate(0);
    x16_pcm_reset();
    x16_irq_remove();
}

/* A zero-length buffer must start nothing at all. */
static void test_pcm_stream_empty(void)
{
    x16_pcm_rate(0);
    x16_pcm_reset();
    x16_pcm_stream_start((const void *)0x2000, 0, 0);

    t_check(x16_pcm_stream_active() == 0 && (VERA_IEN_REG & 0x08) == 0,
            "PCM_STREAM_EMPTY");

    x16_pcm_reset();
    x16_irq_remove();
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

/* Banked -> banked, straddling a bank edge on BOTH sides. Four bytes
** from (5,8190) land at (8,8190): two in each source bank, two in each
** destination bank. A copy that forgot to roll either side would put the
** last two bytes at offset 8192 of the same bank -- which is $C000, RAM
** that is not even mapped.
*/
static void test_bank_copy_far(void)
{
    unsigned char before;

    x16_bank_set(11);
    before = x16_bank_get();

    x16_bank_poke(5, 8190, 0x11);
    x16_bank_poke(5, 8191, 0x22);
    x16_bank_poke(6, 0, 0x33);
    x16_bank_poke(6, 1, 0x44);

    x16_bank_poke(8, 8190, 0x00);
    x16_bank_poke(8, 8191, 0x00);
    x16_bank_poke(9, 0, 0x00);
    x16_bank_poke(9, 1, 0x00);

    x16_bank_copy_far(5, 8190, 8, 8190, 4);

    t_check(x16_bank_peek(8, 8190) == 0x11 &&
            x16_bank_peek(8, 8191) == 0x22 &&
            x16_bank_peek(9, 0) == 0x33 &&
            x16_bank_peek(9, 1) == 0x44 &&
            x16_bank_get() == before,           /* caller's bank restored */
            "BANK_COPY_FAR");

    x16_bank_set(1);
}

/* More than the 128-byte bounce buffer, so the loop runs several chunks. */
static void test_bank_copy_far_long(void)
{
    unsigned int i;
    unsigned char ok = 1;

    for (i = 0; i < 200; ++i) {
        x16_bank_poke(5, 100 + i, (unsigned char)(i ^ 0x5A));
        x16_bank_poke(8, 300 + i, 0x00);
    }
    x16_bank_poke(8, 500, 0x00);        /* the one-past-the-end guard */

    x16_bank_copy_far(5, 100, 8, 300, 200);

    for (i = 0; i < 200; ++i) {
        if (x16_bank_peek(8, 300 + i) != (unsigned char)(i ^ 0x5A)) ok = 0;
    }
    t_check(ok && x16_bank_peek(8, 500) == 0x00, "BANK_COPY_FAR_LONG");
}

/* ------------------------------------------------------------------ */
/* bank allocator                                                      */
/* ------------------------------------------------------------------ */

static void test_bank_alloc(void)
{
    unsigned char a, b, c, d;

    x16_bank_alloc_init(1, 3);
    a = x16_bank_alloc();               /* lowest first */
    b = x16_bank_alloc();
    c = x16_bank_alloc();
    d = x16_bank_alloc();               /* exhausted */

    t_check(a == 1 && b == 2 && c == 3 && d == 0, "BANK_ALLOC");
}

static void test_bank_free(void)
{
    x16_bank_alloc_init(1, 3);
    x16_bank_alloc();                   /* 1 */
    x16_bank_alloc();                   /* 2 */
    x16_bank_alloc();                   /* 3 */
    x16_bank_free(2);

    t_check(x16_bank_alloc() == 2 && x16_bank_alloc() == 0, "BANK_FREE");
}

static void test_bank_reserve(void)
{
    unsigned char first, again, after_free, outside;

    x16_bank_alloc_init(4, 6);
    first = x16_bank_reserve(5);        /* free -> claimed */
    again = x16_bank_reserve(5);        /* already taken */
    x16_bank_free(5);
    after_free = x16_bank_reserve(5);
    outside = x16_bank_reserve(9);      /* never in the pool */

    t_check(first == 1 && again == 0 && after_free == 1 && outside == 0,
            "BANK_RESERVE");
}

/* Before init, nothing is allocatable: a forgotten init must fail
** cleanly rather than hand out bank 0, which is the KERNAL's.
*/
static void test_bank_alloc_uninit(void)
{
    x16_bank_alloc_init(1, 1);
    x16_bank_alloc();                   /* drain it */

    t_check(x16_bank_alloc() == 0, "BANK_ALLOC_UNINIT");
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

#if SUITE == 2

/* ------------------------------------------------------------------ */
/* the DOS command channel                                             */
/* ------------------------------------------------------------------ */

/* A status read always answers with a parsable line: two digits, a
** comma, then text. The code is those digits.
*/
static void test_dos_status(void)
{
    unsigned char code = x16_dos_status();
    const char *msg = x16_dos_msg();

    t_check(code != X16_DOS_NO_CHANNEL &&
            code <= 99 &&
            msg[0] >= '0' && msg[0] <= '9' &&
            msg[1] >= '0' && msg[1] <= '9' &&
            msg[2] == ',',
            "DOS_STATUS");
}

/* Deleting a file that is not there is an ERROR, not a silent success --
** which is the whole reason this module exists. CBM DOS answers 62,
** FILE NOT FOUND.
**
** Assert that exact code, not merely "some error": a command channel that
** is being fed mistranslated bytes answers 30, SYNTAX ERROR, which is
** also an error and would slip past a looser check.
*/
static void test_dos_delete_missing(void)
{
    static const char name[] = "NOSUCH.BIN";
    unsigned char code = x16_dos_delete(name, sizeof name - 1);

    t_check(code == 62, "DOS_DELETE_MISSING");
}

/* Save a file, delete it, and prove it is gone: the load that worked
** before now fails.
*/
static void test_dos_delete(void)
{
    static const char name[] = "DOSTEST.BIN";
    static unsigned char buf[4] = { 1, 2, 3, 4 };
    unsigned char saved, deleted, loaded;

    saved = x16_fs_save(name, sizeof name - 1, X16_DEVICE_SD,
                        buf, buf + sizeof buf);
    deleted = x16_dos_delete(name, sizeof name - 1);
    loaded = x16_fs_load(name, sizeof name - 1, X16_DEVICE_SD,
                         X16_SA_ADDR, buf, (unsigned int *)0);

    t_check(saved == 0 &&
            deleted < X16_DOS_OK_BELOW &&       /* the drive said yes */
            loaded != 0,                        /* ...and it really is gone */
            "DOS_DELETE");
}

/* Rename builds "R:new=old" on the command channel. Prove it by loading
** the file back under its new name.
*/
static void test_dos_rename(void)
{
    static const char oldname[] = "DOSOLD.BIN";
    static const char newname[] = "DOSNEW.BIN";
    static unsigned char out[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    static unsigned char in[4];
    unsigned char code, loaded;

    x16_fs_save(oldname, sizeof oldname - 1, X16_DEVICE_SD,
                out, out + sizeof out);

    code = x16_dos_rename(oldname, sizeof oldname - 1,
                          newname, sizeof newname - 1);

    in[0] = 0;
    loaded = x16_fs_load(newname, sizeof newname - 1, X16_DEVICE_SD,
                         X16_SA_ADDR, in, (unsigned int *)0);

    x16_dos_delete(newname, sizeof newname - 1);

    t_check(code < X16_DOS_OK_BELOW && loaded == 0 && in[0] == 0xDE,
            "DOS_RENAME");
}

#endif /* SUITE == 2 */

#if SUITE == 2

/* ------------------------------------------------------------------ */
/* BMX, the X16's native bitmap format                                 */
/* ------------------------------------------------------------------ */

/* Save a small image out of VRAM and load it back somewhere else. The
** palette goes with it, and the header round-trips.
**
** stride is set to the image width so the pixels are contiguous; the
** default 320 would stamp each row a full screen-line apart.
*/
static void test_bmx_roundtrip(void)
{
    static const char name[] = "PIC.BMX";
    x16_bmx_info info, back;
    unsigned char i, ok;

    /* An 8x4 image of a colour ramp at TESTVRAM. */
    for (i = 0; i < 32; ++i) {
        vpoke(0x40 + i, TESTVRAM + i);
    }
    x16_pal_set(200, 0x0F0F);           /* two palette entries to carry */
    x16_pal_set(201, 0x00F0);

    info.width = 8;
    info.height = 4;
    info.bpp = 8;
    info.palstart = 200;
    info.palcount = 2;
    info.border = 5;
    info.stride = 8;                    /* contiguous, not a screen row */
    x16_bmx_set_info(&info);

    if (x16_bmx_save(name, sizeof name - 1, X16_DEVICE_SD, TESTVRAM) != 0) {
        t_check(0, "BMX_ROUNDTRIP");
        return;
    }

    vram_poison(TESTVRAM_HI, 32, 0x00);
    x16_pal_set(200, 0x0000);           /* wipe the palette we saved */
    x16_pal_set(201, 0x0000);

    if (x16_bmx_load(name, sizeof name - 1, X16_DEVICE_SD, TESTVRAM_HI) != 0) {
        t_check(0, "BMX_ROUNDTRIP");
        return;
    }

    x16_bmx_get_info(&back);
    ok = back.width == 8 && back.height == 4 && back.bpp == 8 &&
         back.palstart == 200 && back.palcount == 2 && back.border == 5;

    for (i = 0; i < 32; ++i) {          /* pixels, across the VRAM bank */
        if (vpeek(TESTVRAM_HI + i) != 0x40 + i) ok = 0;
    }
    /* ...and the palette came back: entry 200 = 0x0F0F, 201 = 0x00F0 */
    ok = ok && vpeek(X16_VRAM_PALETTE + 400) == 0x0F &&
               vpeek(X16_VRAM_PALETTE + 401) == 0x0F &&
               vpeek(X16_VRAM_PALETTE + 402) == 0xF0 &&
               vpeek(X16_VRAM_PALETTE + 403) == 0x00;

    x16_dos_delete(name, sizeof name - 1);
    t_check(ok, "BMX_ROUNDTRIP");
}

/* A file that is not a BMX must be rejected on its magic, not loaded as
** garbage. Write one here rather than borrowing a file some other test
** happens to leave behind: a PRG starts with a load address, not "BMX".
*/
static void test_bmx_bad_format(void)
{
    static const char name[] = "NOTABMX.BIN";
    static unsigned char junk[20];
    unsigned char i, code;

    for (i = 0; i < sizeof junk; ++i) {
        junk[i] = i;
    }
    x16_fs_save(name, sizeof name - 1, X16_DEVICE_SD, junk, junk + sizeof junk);

    code = x16_bmx_load(name, sizeof name - 1, X16_DEVICE_SD, TESTVRAM);
    x16_dos_delete(name, sizeof name - 1);

    t_check(code == X16_BMX_ERR_FORMAT, "BMX_BAD_FORMAT");
}

static void test_bmx_missing(void)
{
    static const char name[] = "NOSUCH.BMX";

    t_check(x16_bmx_load(name, sizeof name - 1, X16_DEVICE_SD, TESTVRAM) ==
            X16_BMX_ERR_IO,
            "BMX_MISSING");
}

/* Write exactly these bytes and nothing else. x16_fs_save() cannot build
** a fixture that begins with a magic number, because the KERNAL's SAVE
** prepends a two-byte load address.
*/
static unsigned char write_raw (const char *name, const unsigned char *data,
                                unsigned char len)
{
    if (cbm_open(2, X16_DEVICE_SD, 1, name) != 0) {
        return 1;
    }
    cbm_write(2, data, len);
    cbm_close(2);
    return 0;
}

/* A file that stops in the middle of the image is an I/O error, not a
** success. The header is entirely valid and promises four rows of four
** pixels; the file carries two. Without a status check the remaining
** CHRINs return junk, the load reports success, and half the image is
** whatever VRAM held before -- a silent wrong answer.
*/
static void test_bmx_truncated(void)
{
    static const char name[] = "TRUNC.BMX";
    static const unsigned char file[] = {
        'B', 'M', 'X', 1,       /* magic, version                       */
        8, 3,                   /* bits per pixel, VERA depth code      */
        4, 0,                   /* width                                */
        4, 0,                   /* height                               */
        1, 0,                   /* one palette entry, from index 0      */
        18, 0,                  /* pixel data offset: 16 + 1*2, no gap  */
        0, 0,                   /* not compressed, border 0             */
        0x0F, 0x00,             /* the palette entry                    */
        1, 2, 3, 4,             /* row 0                                */
        5, 6, 7, 8              /* row 1 -- and here the file stops     */
    };
    unsigned char code;

    if (write_raw(name, file, sizeof file)) {
        t_check(0, "BMX_TRUNCATED");
        return;
    }
    code = x16_bmx_load(name, sizeof name - 1, X16_DEVICE_SD, TESTVRAM);
    x16_dos_delete(name, sizeof name - 1);

    t_check(code == X16_BMX_ERR_IO, "BMX_TRUNCATED");
}

/* ...and a file that stops inside the PALETTE, before a single pixel.
**
** This one exists because the per-row check cannot see it. The image is
** one row tall, so that check never runs: it deliberately tolerates EOF
** after the last row, and here the last row is the first. Only the status
** test between the palette and the pixels catches this, which is why that
** test is not redundant. The header asks for four palette entries --
** eight bytes -- and the file supplies two.
*/
static void test_bmx_short_pal(void)
{
    static const char name[] = "SHORTPAL.BMX";
    static const unsigned char file[] = {
        'B', 'M', 'X', 1,
        8, 3,
        4, 0,                   /* width                                */
        1, 0,                   /* height: ONE row, so no per-row check */
        4, 0,                   /* four palette entries = eight bytes...*/
        24, 0,                  /* pixel data offset: 16 + 4*2          */
        0, 0,
        0x0F, 0x00              /* ...of which the file holds two       */
    };
    unsigned char code;

    if (write_raw(name, file, sizeof file)) {
        t_check(0, "BMX_SHORT_PAL");
        return;
    }
    code = x16_bmx_load(name, sizeof name - 1, X16_DEVICE_SD, TESTVRAM);
    x16_dos_delete(name, sizeof name - 1);

    t_check(code == X16_BMX_ERR_IO, "BMX_SHORT_PAL");
}

#endif /* SUITE == 2 */

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
/* KERNAL block memory operations                                      */
/* ------------------------------------------------------------------ */

/* "X16LIB-DECOMPRESS-TEST!!" four times over, as a raw LZSA2 block --
** the output of `lzsa -r -f2`. Written as hex, not a string literal,
** so the ASCII charmap cannot touch it.
*/
static const unsigned char lzsa_packed[] = {
    0x3f, 0xf4, 0x06, 0x58, 0x31, 0x36, 0x4c, 0x49, 0x42, 0x2d, 0x44, 0x45,
    0x43, 0x4f, 0x4d, 0x50, 0x52, 0x45, 0x53, 0x53, 0x2d, 0x54, 0x45, 0x53,
    0x54, 0x21, 0x21, 0xff, 0x30, 0xe7, 0xe8
};
static const unsigned char lzsa_phrase[24] = {
    'X','1','6','L','I','B','-','D','E','C','O','M','P','R','E','S','S',
    '-','T','E','S','T','!','!'
};
static unsigned char unpacked[97];

static void test_mem_fill(void)
{
    static unsigned char buf[8];
    unsigned char i, ok = 1;

    for (i = 0; i < 8; ++i) {
        buf[i] = 0x11;
    }
    x16_mem_fill(buf, 5, 0xC3);
    x16_mem_fill(buf + 6, 0, 0x99);     /* a zero count fills nothing */

    for (i = 0; i < 5; ++i) {
        if (buf[i] != 0xC3) ok = 0;
    }
    t_check(ok && buf[5] == 0x11 && buf[6] == 0x11, "MEM_FILL");
}

/* The target does not increment when it is in $9F00-$9FFF, so a fill
** through VERA_DATA0 paints VRAM at the data port's own increment.
*/
static void test_mem_fill_vram(void)
{
    vram_poison(TESTVRAM, 6, 0x00);

    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_mem_fill(X16_VERA_DATA0, 4, 0x7E);

    t_check(vram_all(TESTVRAM, 4, 0x7E) && vpeek(TESTVRAM + 4) == 0x00,
            "MEM_FILL_VRAM");
}

static void test_mem_copy(void)
{
    static const unsigned char src[6] = { 1, 2, 3, 4, 5, 6 };
    static unsigned char dst[8];
    unsigned char i, ok = 1;

    for (i = 0; i < 8; ++i) {
        dst[i] = 0xEE;
    }
    x16_mem_copy(src, dst, 6);
    x16_mem_copy(src, dst + 7, 0);      /* a zero count copies nothing */

    for (i = 0; i < 6; ++i) {
        if (dst[i] != i + 1) ok = 0;
    }
    t_check(ok && dst[6] == 0xEE && dst[7] == 0xEE, "MEM_COPY");
}

/* Upload to VRAM and download back, both through the non-incrementing
** data-port address.
*/
static void test_mem_copy_vram(void)
{
    static const unsigned char src[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    static unsigned char back[4];

    vram_poison(TESTVRAM, 4, 0x00);
    back[0] = back[1] = back[2] = back[3] = 0;

    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_mem_copy(src, X16_VERA_DATA0, 4);       /* upload */

    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_mem_copy(X16_VERA_DATA0, back, 4);      /* download */

    t_check(vpeek(TESTVRAM) == 0xDE && vpeek(TESTVRAM + 3) == 0xEF &&
            back[0] == 0xDE && back[3] == 0xEF,
            "MEM_COPY_VRAM");
}

/* 0x29B1 over "123456789" is the published check value for
** CRC-16/IBM-3740, so this pins the algorithm, not just our plumbing.
*/
static void test_mem_crc(void)
{
    static const unsigned char check[9] = { '1','2','3','4','5','6','7','8','9' };

    t_check(x16_mem_crc(check, 9) == 0x29B1 &&
            x16_mem_crc(check, 0) == 0xFFFF,    /* empty: the init value */
            "MEM_CRC");
}

static void test_mem_decompress(void)
{
    unsigned char *end;
    unsigned char i, r, ok = 1;

    unpacked[96] = 0x77;                /* guard, one past the output */
    end = x16_mem_decompress(lzsa_packed, unpacked);

    if (end != unpacked + 96 || unpacked[96] != 0x77) {
        t_check(0, "MEM_DECOMPRESS");
        return;
    }
    for (r = 0; r < 4; ++r) {
        for (i = 0; i < 24; ++i) {
            if (unpacked[r * 24 + i] != lzsa_phrase[i]) ok = 0;
        }
    }
    t_check(ok, "MEM_DECOMPRESS");
}

/* The flagship trick: unpack an asset straight into video memory, with
** no staging buffer. VERA_DATA0 does not increment, so the data port's
** own increment walks the output.
*/
static void test_mem_decompress_vram(void)
{
    unsigned char i, r, ok = 1;

    vram_poison(TESTVRAM, 4, 0x00);

    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_mem_decompress(lzsa_packed, X16_VERA_DATA0);

    for (r = 0; r < 4 && ok; ++r) {
        for (i = 0; i < 24; ++i) {
            if (vpeek(TESTVRAM + r * 24 + i) != lzsa_phrase[i]) ok = 0;
        }
    }
    t_check(ok, "MEM_DECOMPRESS_VRAM");
}

#if SUITE == 2

/* ------------------------------------------------------------------ */
/* ZX0 and IMA ADPCM                                                   */
/* ------------------------------------------------------------------ */

/* The same phrase as the LZSA2 test, four times over, packed by
** `salvador`. ZX0 v2, not -classic.
*/
static const unsigned char zx0_packed[] = {
    0x15, 0xb8, 0x58, 0x31, 0x36, 0x4c, 0x49, 0x42, 0x2d, 0x44, 0x45, 0x43,
    0x4f, 0x4d, 0x50, 0x52, 0x45, 0x53, 0x53, 0x2d, 0x54, 0x45, 0x53, 0x54,
    0x21, 0xd0, 0x15, 0xd5, 0x55, 0x60
};

static void test_zx0(void)
{
    unsigned char *end;
    unsigned char i, r, ok = 1;

    unpacked[96] = 0x77;                /* guard, one past the output */
    end = x16_zx0_decompress(zx0_packed, unpacked);

    if (end != unpacked + 96 || unpacked[96] != 0x77) {
        t_check(0, "ZX0");
        return;
    }
    for (r = 0; r < 4; ++r) {
        for (i = 0; i < 24; ++i) {
            if (unpacked[r * 24 + i] != lzsa_phrase[i]) ok = 0;
        }
    }
    /* Aside: 30 bytes here against LZSA2's 31 for the same payload --
    ** ZX0 packs tighter, which is the reason to carry this code at all.
    */
    t_check(ok, "ZX0");
}

/* Eight ADPCM bytes decode to sixteen signed 16-bit samples. The
** expected values came from Python's audioop, so this pins the algorithm
** -- the saturation, the index clamp, the low-nibble-first order -- not
** just our plumbing.
*/
static void test_adpcm(void)
{
    static const unsigned char packed[8] = {
        0x17, 0x28, 0x93, 0x4C, 0xE5, 0x0A, 0x71, 0xBF
    };
    static const int expect[16] = {
        0x000b, 0x0011, 0x0010, 0x0017, 0x0021, 0x001e, 0x0013, 0x0020,
        0x0032, 0x0011, -5,     -1,     0x0009, 0x003d, -51,    -164
    };
    static int out[16];
    unsigned char i, ok = 1;

    x16_adpcm_init();
    x16_adpcm_block(packed, out, sizeof packed);

    for (i = 0; i < 16; ++i) {
        if (out[i] != expect[i]) ok = 0;
    }
    /* The decoder state must end where the reference says it does. */
    ok = ok && x16_adpcm_predictor() == -164 && x16_adpcm_index() == 29;

    t_check(ok, "ADPCM");
}

/* State carries across calls, so decoding a block in slices gives the
** same answer as decoding it in one go. That is what makes it stream.
*/
static void test_adpcm_sliced(void)
{
    static const unsigned char packed[8] = {
        0x17, 0x28, 0x93, 0x4C, 0xE5, 0x0A, 0x71, 0xBF
    };
    static int whole[16], sliced[16];
    unsigned char i, ok = 1;

    x16_adpcm_init();
    x16_adpcm_block(packed, whole, 8);

    x16_adpcm_init();
    x16_adpcm_block(packed, sliced, 3);
    x16_adpcm_block(packed + 3, sliced + 6, 5);

    for (i = 0; i < 16; ++i) {
        if (whole[i] != sliced[i]) ok = 0;
    }
    t_check(ok, "ADPCM_SLICED");
}

/* An IMA WAV block header carries the initial predictor and step index. */
static void test_adpcm_state(void)
{
    x16_adpcm_set_state(-1000, 42);

    t_check(x16_adpcm_predictor() == -1000 && x16_adpcm_index() == 42,
            "ADPCM_STATE");
}

#endif /* SUITE == 2 */

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


#if SUITE == 2

/* ------------------------------------------------------------------ */
/* gfx2: 640x480 at 2bpp -- 4 pixels per byte, MSB-first, rows of 160
** bytes. A pixel byte sits at y*160 + (x>>2). These tests run LAST:
** x16_gfx2_init() reprograms the display and palette entries 0-3.
*/

#define G2_L0_CONFIG    (*(volatile unsigned char *)0x9F2DU)
#define G2_L0_TILEBASE  (*(volatile unsigned char *)0x9F2FU)

static void test_g2_init(void)
{
    x16_gfx2_init();
    t_check(G2_L0_CONFIG == 0x05 &&     /* bitmap | 2bpp */
            G2_L0_TILEBASE == 0x01,     /* base $00000, 640 wide */
            "G2_INIT");
}

static void test_g2_pset(void)
{
    vpoke(0x00, 10UL * 160 + 1);
    x16_gfx2_pset(5, 10, 2);            /* byte 1, pixel 1 */
    t_check(vpeek(10UL * 160 + 1) == 0x20, "G2_PSET");
}

/* Unclipped, (640,0) would land at byte 160 and (0,480) at 76,800. */
static void test_g2_clip(void)
{
    vpoke(0x11, 160UL);
    vpoke(0x22, 76800UL);
    x16_gfx2_pset(640, 0, 3);
    x16_gfx2_pset(0, 480, 3);
    t_check(vpeek(160UL) == 0x11 && vpeek(76800UL) == 0x22, "G2_CLIP");
}

static void test_g2_read(void)
{
    vpoke(0x1B, 12UL * 160);            /* pixels 0,1,2,3 left to right */
    t_check(x16_gfx2_read(0, 12) == 0 &&
            x16_gfx2_read(1, 12) == 1 &&
            x16_gfx2_read(2, 12) == 2 &&
            x16_gfx2_read(3, 12) == 3 &&
            x16_gfx2_read(640, 12) == 0xFF,
            "G2_READ");
}

/* x=5 len=13: head = byte 1 pixels 1-3, middle bytes 2-3, tail = byte 4
** pixels 0-1. The bytes either side must survive.
*/
static void test_g2_hline(void)
{
    unsigned char i;
    for (i = 0; i < 6; ++i) {
        vpoke(0x00, 20UL * 160 + i);
    }
    x16_gfx2_hline(5, 20, 13, 3);
    t_check(vpeek(20UL * 160 + 0) == 0x00 &&
            vpeek(20UL * 160 + 1) == 0x3F &&
            vpeek(20UL * 160 + 2) == 0xFF &&
            vpeek(20UL * 160 + 3) == 0xFF &&
            vpeek(20UL * 160 + 4) == 0xF0 &&
            vpeek(20UL * 160 + 5) == 0x00,
            "G2_HLINE");
}

/* Colour 0 ink onto $FF: proves the column really is read-modify-write. */
static void test_g2_vline(void)
{
    unsigned char i;
    for (i = 30; i <= 34; ++i) {
        vpoke(0xFF, (unsigned long)i * 160 + 1);
    }
    x16_gfx2_vline(6, 30, 4, 0);        /* byte 1, pixel 2 */
    t_check(vpeek(30UL * 160 + 1) == 0xF3 &&
            vpeek(33UL * 160 + 1) == 0xF3 &&
            vpeek(34UL * 160 + 1) == 0xFF,
            "G2_VLINE");
}

static void test_g2_rect(void)
{
    unsigned char i;
    for (i = 0; i < 4; ++i) {
        vpoke(0x00, 40UL * 160 + i);
        vpoke(0x00, 41UL * 160 + i);
        vpoke(0x00, 42UL * 160 + i);
    }
    x16_gfx2_rect(4, 40, 8, 2, 1);
    t_check(vpeek(40UL * 160 + 0) == 0x00 &&
            vpeek(40UL * 160 + 1) == 0x55 &&
            vpeek(40UL * 160 + 2) == 0x55 &&
            vpeek(40UL * 160 + 3) == 0x00 &&
            vpeek(41UL * 160 + 1) == 0x55 &&
            vpeek(42UL * 160 + 1) == 0x00,
            "G2_RECT");
}

static void test_g2_frame(void)
{
    unsigned char i;
    for (i = 0; i < 4; ++i) {
        vpoke(0x00, 50UL * 160 + i);
        vpoke(0x00, 51UL * 160 + i);
        vpoke(0x00, 52UL * 160 + i);
    }
    x16_gfx2_frame(0, 50, 16, 3, 3);
    t_check(vpeek(50UL * 160 + 0) == 0xFF &&    /* top edge */
            vpeek(50UL * 160 + 3) == 0xFF &&
            vpeek(51UL * 160 + 0) == 0xC0 &&    /* left edge only */
            vpeek(51UL * 160 + 1) == 0x00 &&    /* hollow */
            vpeek(51UL * 160 + 3) == 0x03 &&    /* right edge only */
            vpeek(52UL * 160 + 2) == 0xFF,      /* bottom edge */
            "G2_FRAME");
}

static void test_g2_line(void)
{
    unsigned char i;
    for (i = 60; i <= 67; ++i) {
        vpoke(0x00, (unsigned long)i * 160);
        vpoke(0x00, (unsigned long)i * 160 + 1);
    }
    x16_gfx2_line(0, 60, 7, 67, 3);     /* the 45-degree diagonal */
    t_check(vpeek(60UL * 160) == 0xC0 &&
            vpeek(63UL * 160) == 0x03 &&
            vpeek(64UL * 160 + 1) == 0xC0 &&
            vpeek(67UL * 160 + 1) == 0x03,
            "G2_LINE");
}

/* Pattern $F0 (left half ink): even bytes $FF, odd bytes $00. Patterns
** anchor to the screen, so an x=2 fill gets a phase head and the even
** byte again in its tail.
*/
static const unsigned char g2_pat_half[8] = {
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0
};

static void test_g2_pattern(void)
{
    unsigned char i;
    for (i = 0; i < 5; ++i) {
        vpoke(0x55, 70UL * 160 + i);
    }
    for (i = 0; i < 4; ++i) {
        vpoke(0x00, 74UL * 160 + i);
    }
    x16_gfx2_pattern_set(g2_pat_half, 0x03);    /* bg 0, fg 3 */
    x16_gfx2_pattern_rect(0, 70, 16, 1);
    x16_gfx2_pattern_rect(2, 74, 8, 1);
    t_check(vpeek(70UL * 160 + 0) == 0xFF &&
            vpeek(70UL * 160 + 1) == 0x00 &&
            vpeek(70UL * 160 + 2) == 0xFF &&
            vpeek(70UL * 160 + 3) == 0x00 &&
            vpeek(70UL * 160 + 4) == 0x55 &&    /* untouched */
            vpeek(74UL * 160 + 0) == 0x0F &&    /* phase-2 head */
            vpeek(74UL * 160 + 1) == 0x00 &&
            vpeek(74UL * 160 + 2) == 0xF0,      /* tail */
            "G2_PATTERN");
}

static const unsigned char g2_img[4] = { 0xDE, 0xAD, 0xBE, 0xEF };

static void test_g2_blit(void)
{
    vpoke(0x00, 80UL * 160 + 2);
    vpoke(0x00, 80UL * 160 + 3);
    vpoke(0x00, 81UL * 160 + 2);
    vpoke(0x00, 81UL * 160 + 3);
    x16_gfx2_blit(8, 80, 2, 2, g2_img, 0);      /* copy */
    if (vpeek(80UL * 160 + 2) != 0xDE || vpeek(80UL * 160 + 3) != 0xAD ||
        vpeek(81UL * 160 + 2) != 0xBE || vpeek(81UL * 160 + 3) != 0xEF) {
        t_check(0, "G2_BLIT");
        return;
    }
    x16_gfx2_blit(8, 80, 2, 2, g2_img, 3);      /* XOR it away again */
    t_check(vpeek(80UL * 160 + 2) == 0x00 &&
            vpeek(81UL * 160 + 3) == 0x00,
            "G2_BLIT");
}

/* Onto a solid $FF background: keep pixels 2-3 (mask $0F), ink pixels
** 0-1 with colour 1 (data $50) -> every touched byte reads $5F.
*/
static const unsigned char g2_mcol[8] = {   /* (mask,data) x 4 rows */
    0x0F, 0x50, 0x0F, 0x50, 0x0F, 0x50, 0x0F, 0x50
};

static void test_g2_blitm(void)
{
    unsigned char i;
    for (i = 90; i <= 94; ++i) {
        vpoke(0xFF, (unsigned long)i * 160 + 3);
    }
    x16_gfx2_blitm(12, 90, 4, 1, g2_mcol);
    t_check(vpeek(90UL * 160 + 3) == 0x5F &&
            vpeek(93UL * 160 + 3) == 0x5F &&
            vpeek(94UL * 160 + 3) == 0xFF,      /* one past the end */
            "G2_BLITM");
}

/* Exactly the 76,800 framebuffer bytes and not one more. */
static void test_g2_clear(void)
{
    vpoke(0x77, 76800UL);
    x16_gfx2_clear(2);
    t_check(vpeek(0UL) == 0xAA &&
            vpeek(38400UL) == 0xAA &&           /* the second fill half */
            vpeek(76799UL) == 0xAA &&
            vpeek(76800UL) == 0x77,
            "G2_CLEAR");
}

/* <x16/shapes.h> bound to the 2bpp module. The same algorithm the 8bpp
** tests above exercise, now plotting through gfx2_pset/hline/read: a disc
** inks its centre and rim and leaves the outside clear, and a flood fills
** a framed interior up to (but not over) the border. VRAM is cleared with
** a direct fill (not x16_gfx2_clear, which wants VERA FX).
*/
static void test_g2_disc(void)
{
    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 160UL * 41);            /* rows 0..40 */

    x16_gfx2_disc(40, 30, 8, 3);

    t_check(x16_gfx2_read(40, 30) == 3 &&       /* centre */
            x16_gfx2_read(47, 30) == 3 &&       /* inside the rim */
            x16_gfx2_read(60, 30) == 0,         /* well outside */
            "G2_DISC");
}

static void test_g2_flood(void)
{
    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 160UL * 25);            /* rows 0..24 */

    x16_gfx2_frame(20, 4, 20, 20, 3);           /* colour-3 border, 0 inside */

    t_check(x16_gfx2_flood(25, 10, 2) == 1 &&   /* fills completely */
            x16_gfx2_read(25, 10) == 2 &&       /* interior painted */
            x16_gfx2_read(38, 22) == 2 &&       /* far interior corner */
            x16_gfx2_read(20, 4) == 3 &&        /* border not crossed */
            x16_gfx2_read(19, 10) == 0,         /* nothing leaked out */
            "G2_FLOOD");
}

#endif /* SUITE == 2 */

int main(void)
{
    t_init();

#if SUITE == 1

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
        test_fx_line();
        test_fx_line_carry();
        test_fx_line_axis();
        test_fx_triangle();
        test_fx_triangle_unsorted();
        test_fx_leaves_clean();
    } else {
        t_skip("FX_MULT");
        t_skip("FX_MULT_SIGNED");
        t_skip("FX_ACCUM_DIRTY");
        t_skip("FX_FILL");
        t_skip("FX_CLEAR");
        t_skip("FX_LINE");
        t_skip("FX_LINE_CARRY");
        t_skip("FX_LINE_AXIS");
        t_skip("FX_TRIANGLE");
        t_skip("FX_TRIANGLE_UNSORTED");
        t_skip("FX_LEAVES_CLEAN");
    }

    test_irq_hook();
    test_irq_preserves_iflag();
    test_irq_line_regs();
    test_sprcol_regs();
    test_sprcol_poll();
    test_vsync_counter();
    test_irq_line_fires();
    test_irq_line_at_scanline();
    test_irq_zp_preserved();
    test_irq_vregs_preserved();

    test_joy_get();
    test_joy_absent();
    test_joy_present();
    test_key_peek_empty();

    test_psg_regs();
    test_psg_hz();
    test_psg_note_off();
    test_pcm_rate_clamp();
    test_pcm_stream();
    test_pcm_stream_empty();
    test_pcm_stream_exhaust();
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
    test_bank_copy_far();
    test_bank_copy_far_long();

    test_bank_alloc();
    test_bank_free();
    test_bank_reserve();
    test_bank_alloc_uninit();

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

    test_mem_fill();
    test_mem_fill_vram();
    test_mem_copy();
    test_mem_copy_vram();
    test_mem_crc();
    test_mem_decompress();
    test_mem_decompress_vram();

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

#else   /* SUITE == 2: the Prog8-inspired batch */

    test_math_tables();
    test_rnd();
    test_rnd_zero_seed();
    test_atan2();
    test_atan2_roundtrip();
    test_lerp8();

    test_ringbuffer();
    test_ringbuffer_wrap();
    test_stack();
    test_buffers_ff_is_not_empty();

    test_clip_inside();
    test_clip_reject();
    test_clip_horizontal();
    test_clip_diagonal();
    test_clip_set();
    test_clip_symmetry();

    test_gfx_circle();
    test_gfx_circle_r0();
    test_gfx_disc();
    test_gfx_circle_clips();
    test_gfx_char();
    test_gfx_text();
    test_gfx_flood();
    test_gfx_flood_noop();

    if (x16_vera_has_fx()) {
        test_fx_copy();
        test_fx_copy_bank();
        test_fx_transparency();
        test_fx_transparency_off();
    } else {
        t_skip("FX_COPY");
        t_skip("FX_COPY_BANK");
        t_skip("FX_TRANSPARENCY");
        t_skip("FX_TRANSPARENCY_OFF");
    }

    test_psg_env_attack();
    test_psg_env_instant();
    test_psg_env_release();
    test_psg_env_sustain();
    test_psg_env_stop();

    test_dos_status();
    test_dos_delete_missing();
    test_dos_delete();
    test_dos_rename();

    test_bmx_roundtrip();
    test_bmx_bad_format();
    test_bmx_missing();
    test_bmx_truncated();
    test_bmx_short_pal();

    test_zx0();
    test_adpcm();
    test_adpcm_sliced();
    test_adpcm_state();

    /* Last: x16_gfx2_init() reprograms the display and palette 0-3. */
    test_g2_init();
    test_g2_pset();
    test_g2_clip();
    test_g2_read();
    test_g2_hline();
    test_g2_vline();
    test_g2_rect();
    test_g2_frame();
    test_g2_line();
    test_g2_pattern();
    test_g2_blit();
    test_g2_blitm();
    if (x16_vera_has_fx()) {
        test_g2_clear();
    } else {
        t_skip("G2_CLEAR");
    }
    test_g2_disc();
    test_g2_flood();

#endif

    t_done();
    return 0;
}
