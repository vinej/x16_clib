/* =====================================================================
 * x16clib :: test_kickc/runner.c -- on-machine regression suite
 * =====================================================================
 * Runs under `.\build_kickc.ps1 -Test`, which boots x16emu headless
 * (-testbench -warp -echo) and fails the build on any FAIL, a pass count
 * that disagrees with the total, a missing DONE line, or a timeout.
 *
 * The tests are ports of the util-module cases in test_ca65/runner.c,
 * adjusted only for KickC's dialect: no `static`, struct locals become
 * globals initialised field by field, and the expected values are
 * written in decimal where cc65 needed casts of hex literals.
 *
 * The cc65 suite's ABI_* tests guard shims that pop arguments off a
 * stack in a hand-written order. KickC's asm reads each parameter BY
 * NAME, so that failure mode does not exist here; what stands in for it
 * is that every multi-argument test below uses distinct values per
 * argument, so a swapped name still turns the test red.
 * =====================================================================
 */

#include "testlib.h"
#include <x16/x16.h>

/* ------------------------------------------------------------------ */
/* VRAM access for verification, independent of the library's path.
**
** The cc65 suite verified through cc65's vpeek()/vpoke(); KickC has no
** equivalent worth trusting, so these do the same job: point port 0
** with NO increment and touch the data register once. None of the
** library's increment/DECR marshalling is involved, so an
** address-plumbing bug in vera.c cannot hide behind itself.
**
** VRAM addresses are 17-bit; these take (bank, low 16 bits) so the
** tests never do 32-bit arithmetic at run time.
** ------------------------------------------------------------------ */

#define TESTVRAM        0x4000          /* bank 0: clear of the text map */

void t_vsetaddr(unsigned char bank, unsigned int addr) {
    __asm {
        lda 0x9f25
        and #0xfe
        sta 0x9f25
        lda addr
        sta 0x9f20
        lda addr+1
        sta 0x9f21
        lda bank
        and #0x01
        sta 0x9f22                       /* increment index 0 */
    }
}

unsigned char t_vpeek(unsigned char bank, unsigned int addr) {
    t_vsetaddr(bank, addr);
    return __asm {
        lda 0x9f23
        sta accu
    };
}

void t_vpoke(unsigned char bank, unsigned int addr, unsigned char v) {
    t_vsetaddr(bank, addr);
    __asm {
        lda v
        sta 0x9f23
    }
}

/* Write `n` copies of `v` through the independent path. */
void vram_poison(unsigned char bank, unsigned int base, unsigned int n,
                 unsigned char v) {
    unsigned int i;
    for (i = 0; i < n; i++) {
        t_vpoke(bank, base + i, v);
    }
}

/* 1 if VRAM[base .. base+n-1] all equal v. */
unsigned char vram_all(unsigned char bank, unsigned int base, unsigned int n,
                       unsigned char v) {
    unsigned int i;
    for (i = 0; i < n; i++) {
        if (t_vpeek(bank, base + i) != v) {
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* VERA data ports                                                     */
/* ------------------------------------------------------------------ */

void test_fill(void) {
    vram_poison(0, TESTVRAM, 24, 0x00);

    x16_vera_addr0(X16_INC_1, 0x04000);
    x16_vera_fill(0xAA, 16);

    t_check((vram_all(0, TESTVRAM, 16, 0xAA) &&
            t_vpeek(0, TESTVRAM + 16) == 0x00) ? 1 : 0,  /* stayed in bounds */
            "FILL");
}

void test_fill_zero(void) {
    vram_poison(0, TESTVRAM, 4, 0x5C);

    x16_vera_addr0(X16_INC_1, 0x04000);
    x16_vera_fill(0x11, 0);

    /* A count of 0 means zero bytes, not 65536. */
    t_check(vram_all(0, TESTVRAM, 4, 0x5C), "FILL_ZERO");
}

void test_fill_stride(void) {
    unsigned char i;
    unsigned char want;
    unsigned char ok = 1;

    vram_poison(0, TESTVRAM, 16, 0x00);

    x16_vera_addr0(X16_INC_2, 0x04000);
    x16_vera_fill(0xBB, 8);

    for (i = 0; i < 16; i++) {
        want = (i & 1) ? 0x00 : 0xBB;
        if (t_vpeek(0, TESTVRAM + i) != want) {
            ok = 0;
        }
    }
    t_check(ok, "FILL_STRIDE");
}

/* A count above 255 exercises vera_fill's high-byte path, where the
** partial-page correction lives.
*/
void test_fill_16bit(void) {
    vram_poison(0, TESTVRAM + 300, 2, 0x00);

    x16_vera_addr0(X16_INC_1, 0x04000);
    x16_vera_fill(0xD7, 300);

    t_check((t_vpeek(0, TESTVRAM) == 0xD7 &&
            t_vpeek(0, TESTVRAM + 299) == 0xD7 &&
            t_vpeek(0, TESTVRAM + 300) == 0x00) ? 1 : 0,
            "FILL_16BIT");
}

void test_copy(void) {
    unsigned char i;
    unsigned char ok = 1;

    /* Lay down a ramp through the test path, copy it through ours. */
    for (i = 0; i < 100; i++) {
        t_vpoke(0, TESTVRAM + i, i);
    }
    vram_poison(0, TESTVRAM + 256, 100, 0x00);

    x16_vera_addr0(X16_INC_1, 0x04000);         /* source */
    x16_vera_addr1(X16_INC_1, 0x04100);         /* destination */
    x16_vera_copy(100);

    for (i = 0; i < 100; i++) {
        if (t_vpeek(0, TESTVRAM + 256 + i) != i) {
            ok = 0;
        }
    }
    t_check(ok, "COPY");
}

void test_has_fx(void) {
    /* The r49 emulator reports FX. A 0 here means either the probe
    ** broke or someone pointed the build at an older ROM.
    */
    t_check((x16_vera_has_fx() == 1) ? 1 : 0, "HAS_FX");
}

/* ------------------------------------------------------------------ */
/* screen                                                              */
/* ------------------------------------------------------------------ */

/* Leave a hostile DCSEL behind: screen_border must select bank 0
** itself before it can even see DC_BORDER.
*/
void test_border(void) {
    unsigned char got;

    *(char*)0x9F25 = 2 << 1;   /* DCSEL = 2, the FX bank */
    x16_screen_border(7);

    *(char*)0x9F25 = 0;        /* DCSEL = 0 */
    got = *(char*)0x9F2C;      /* DC_BORDER */

    x16_screen_border(6);               /* restore the default */
    t_check((got == 7) ? 1 : 0, "SCREEN_BORDER");
}

void test_set_mode(void) {
    unsigned char ok = x16_screen_set_mode(X16_MODE_80x60);
    unsigned char mode = x16_screen_get_mode();

    t_check((ok == 1 && mode == X16_MODE_80x60) ? 1 : 0, "SCREEN_SET_MODE");
}

/* 0x7F is not a mode. The KERNAL reports that in the carry, and the
** wrapper must turn a set carry into 0, not into 1.
*/
void test_set_mode_bad(void) {
    unsigned char ok = x16_screen_set_mode(0x7F);

    t_check((ok == 0 && x16_screen_get_mode() == X16_MODE_80x60) ? 1 : 0,
            "SCREEN_SET_MODE_BAD");
}

/* Enter with ADDRSEL = 1, the state x16_vera_addr1() and
** x16_vera_copy() leave behind. The KERNAL's screen code writes VERA's
** address registers before selecting a port, so cls corrupts the
** display unless it clears ADDRSEL first. Remove that guard from
** screen.c and this fails.
*/
void test_cls_clears(void) {
    t_vpoke(1, 0xB000 + 10 * 2, 0xAA);          /* sentinel at column 10 */

    *(char*)0x9F25 = 1;                /* hostile: ADDRSEL = 1 */
    x16_screen_cls();

    t_check((t_vpeek(1, 0xB000 + 10 * 2) == 0x20) ? 1 : 0, "CLS_CLEARS");
}

/* x16_screen_color must change what CHROUT actually puts in VRAM, not
** merely poke a KERNAL variable. Verify through the tilemap attribute
** byte, which is the odd byte of each cell.
*/
void test_color_reaches_vram(void) {
    unsigned char attr;

    *(char*)0x9F25 = 1;                /* hostile: ADDRSEL = 1 */
    x16_screen_cls();
    x16_screen_color(1, 6);                     /* white on blue */
    x16_screen_locate(0, 0);
    x16_screen_chrout('X');

    attr = t_vpeek(1, 0xB001);                  /* fg | bg<<4 */

    x16_screen_color(1, 6);
    x16_screen_cls();
    t_check((attr == 0x61) ? 1 : 0, "COLOR_REACHES_VRAM");
}

/* ------------------------------------------------------------------ */
/* palette                                                             */
/* ------------------------------------------------------------------ */

/* A 12-bit 0RGB colour stores little-endian into an entry: low byte is
** Green<<4 | Blue, high byte is Red. The palette is at $1FA00: bank 1.
*/
void test_palette(void) {
    unsigned char lo;
    unsigned char hi;

    x16_pal_set(1, 0x0F00);                     /* pure red */
    lo = t_vpeek(1, 0xFA02);
    hi = t_vpeek(1, 0xFA03);

    x16_pal_set(1, 0x0FFF);                     /* entry 1 back to white */
    t_check((lo == 0x00 && hi == 0x0F) ? 1 : 0, "PALETTE");
}

const unsigned int pal_ramp[3] = { 0x0F00, 0x00F0, 0x000F };

void test_pal_load(void) {
    unsigned char ok;

    x16_pal_load(pal_ramp, 4, 3);

    ok = (t_vpeek(1, 0xFA08) == 0x00 &&         /* entry 4 lo */
          t_vpeek(1, 0xFA09) == 0x0F &&         /* entry 4 hi */
          t_vpeek(1, 0xFA0A) == 0xF0 &&         /* entry 5 lo */
          t_vpeek(1, 0xFA0B) == 0x00 &&
          t_vpeek(1, 0xFA0C) == 0x0F &&         /* entry 6 lo */
          t_vpeek(1, 0xFA0D) == 0x00) ? 1 : 0;
    t_check(ok, "PAL_LOAD");
}

/* A count of 0 must load nothing. Get the guard wrong and the loop
** runs 256 times and shreds the whole palette.
*/
const unsigned int pal_one[1] = { 0x0ABC };

void test_pal_load_zero(void) {
    x16_pal_set(8, 0x0123);
    x16_pal_load(pal_one, 8, 0);

    t_check((t_vpeek(1, 0xFA10) == 0x23 &&
            t_vpeek(1, 0xFA11) == 0x01) ? 1 : 0,
            "PAL_LOAD_ZERO");
}

/* ------------------------------------------------------------------ */
/* tilemap and layers                                                  */
/* ------------------------------------------------------------------ */

/* Where layer 1's cell (col,row) really lives, derived from VERA's own
** registers rather than from the same arithmetic tile_setptr uses.
** Returns bank via tl_bank, the low 16 bits via tl_addr.
*/
unsigned char tl_bank;
unsigned int tl_addr;

void tile_addr(unsigned char col, unsigned char row) {
    unsigned char cfg = *(char*)0x9F34;        /* L1_CONFIG */
    unsigned char mb = *(char*)0x9F35;         /* L1_MAPBASE */
    unsigned char shift = 6 + ((cfg >> 4) & 3);         /* *mapwidth*2 */
    unsigned int off = ((unsigned int)row << shift) + ((unsigned int)col << 1);
    /* base = (mb & 0x7F) << 9, built a byte at a time: the low byte is
    ** always zero and the high byte is (mb & 0x7F) << 1.
    */
    unsigned char baseh = (mb & 0x7F) << 1;
    unsigned int base = ((unsigned int)baseh) << 8;

    tl_bank = mb >> 7;                                  /* address bit 16 */
    tl_addr = base + off;
}

void test_tile_addr(void) {
    x16_tile_put(5, 3, 0x41, 0x61);

    tile_addr(5, 3);
    t_check((t_vpeek(tl_bank, tl_addr) == 0x41 &&
            t_vpeek(tl_bank, tl_addr + 1) == 0x61) ? 1 : 0,
            "TILE_ADDR");
}

void test_tile_roundtrip(void) {
    unsigned int cell;

    x16_tile_put(7, 2, 0x53, 0x1E);
    cell = x16_tile_get(7, 2);

    t_check((X16_TILE_CODE(cell) == 0x53 && X16_TILE_ATTR(cell) == 0x1E) ? 1 : 0,
            "TILE_ROUNDTRIP");
}

/* Scrolling layer 1 must not move layer 0, and the value is 12-bit. */
void test_layer_scroll(void) {
    unsigned char h1l;
    unsigned char h1h;
    unsigned char v1l;
    unsigned char v1h;
    unsigned char h0l;
    unsigned char h0h;

    x16_layer_scroll_x(0, 0);
    x16_layer_scroll_y(0, 0);
    x16_layer_scroll_x(1, 0x123);
    x16_layer_scroll_y(1, 0x0AB);

    h1l = *(char*)0x9F37;      /* L1_HSCROLL_L */
    h1h = *(char*)0x9F38;
    v1l = *(char*)0x9F39;
    v1h = *(char*)0x9F3A;
    h0l = *(char*)0x9F30;      /* L0_HSCROLL_L */
    h0h = *(char*)0x9F31;

    t_check((h1l == 0x23 && (h1h & 0x0F) == 0x01 &&
            v1l == 0xAB && (v1h & 0x0F) == 0x00 &&
            h0l == 0 && (h0h & 0x0F) == 0) ? 1 : 0,
            "LAYER_SCROLL");

    x16_layer_scroll_x(1, 0);
    x16_layer_scroll_y(1, 0);
}

void test_layer_enable(void) {
    unsigned char both;
    unsigned char off0;

    *(char*)0x9F25 = 0;        /* DCSEL = 0: DC_VIDEO visible */
    x16_layer_on(0);
    x16_layer_on(1);
    both = *(char*)0x9F29 & 0x30;      /* DC_VIDEO layer bits */

    x16_layer_off(0);
    off0 = *(char*)0x9F29 & 0x30;

    x16_layer_on(0);                    /* put the text screen back */
    t_check((both == 0x30 && off0 == 0x20) ? 1 : 0, "LAYER_ENABLE");
}

/* ------------------------------------------------------------------ */
/* sprites                                                             */
/* ------------------------------------------------------------------ */

/* The attribute records live at $1FC00: bank 1, $FC00 + sprite*8. */

/* Sprite 3's record bytes 2-5 are the position: x low, x high (2
** bits), y low, y high (2 bits).
*/
void test_sprite_pos(void) {
    unsigned int sx = 0;
    unsigned int sy = 0;

    x16_sprite_pos(3, 0x123, 0x2AB);
    x16_sprite_get_pos(3, &sx, &sy);

    t_check((t_vpeek(1, 0xFC00 + 3 * 8 + 2) == 0x23 &&
            t_vpeek(1, 0xFC00 + 3 * 8 + 3) == 0x01 &&
            t_vpeek(1, 0xFC00 + 3 * 8 + 4) == 0xAB &&
            t_vpeek(1, 0xFC00 + 3 * 8 + 5) == 0x02 &&
            sx == 0x123 && sy == 0x2AB) ? 1 : 0,
            "SPRITE_POS");
}

/* The record stores image address bits 16:5. For $13000 in 8bpp:
**   byte 0 = addr 12:5           = 0x80
**   byte 1 = mode | addr 16:13   = 0x80 | 0x09 = 0x89
*/
void test_sprite_image(void) {
    x16_sprite_image(2, X16_SPRITE_8BPP, 0x13000);

    t_check((t_vpeek(1, 0xFC00 + 2 * 8 + 0) == 0x80 &&
            t_vpeek(1, 0xFC00 + 2 * 8 + 1) == 0x89) ? 1 : 0,
            "SPRITE_IMAGE");
}

/* x16_sprite_z is a read-modify-write on write-only VRAM: it only works
** because the host wrote the record first. It must leave the flip bits
** and collision mask alone.
*/
void test_sprite_z(void) {
    x16_sprite_flags(5, 0x30 | X16_SPRITE_Z_BEHIND | X16_SPRITE_HFLIP);
    x16_sprite_z(5, X16_SPRITE_Z_FRONT);

    t_check((t_vpeek(1, 0xFC00 + 5 * 8 + 6) ==
            (0x30 | X16_SPRITE_Z_FRONT | X16_SPRITE_HFLIP)) ? 1 : 0,
            "SPRITE_Z");
}

/* Byte 7: height in 7:6, width in 5:4, palette offset in 3:0. */
void test_sprite_size(void) {
    x16_sprite_size(6, X16_SPRITE_SIZE_16, X16_SPRITE_SIZE_64, 9);

    t_check((t_vpeek(1, 0xFC00 + 6 * 8 + 7) == (0xC0 | 0x10 | 0x09)) ? 1 : 0,
            "SPRITE_SIZE");
}

/* The palette offset is or'd into byte 7, so an out-of-range value
** would set the size bits too -- 0xFF would turn a 16-wide sprite into
** a 64-wide one. The routine masks it to four bits. Without the mask
** this reads back 0xFF instead of 0xDF.
*/
void test_sprite_size_pal_mask(void) {
    x16_sprite_size(7, X16_SPRITE_SIZE_16, X16_SPRITE_SIZE_64, 0xFF);

    t_check((t_vpeek(1, 0xFC00 + 7 * 8 + 7) == (0xC0 | 0x10 | 0x0F)) ? 1 : 0,
            "SPRITE_SIZE_PAL_MASK");
}

/* init_all zeroes all 128 records -- check the first and last byte of
** the kilobyte, having poisoned them through the independent path.
*/
void test_sprite_init_all(void) {
    t_vpoke(1, 0xFC00, 0x5A);
    t_vpoke(1, 0xFFFF, 0xA5);

    x16_sprite_init_all();

    t_check((t_vpeek(1, 0xFC00) == 0x00 &&
            t_vpeek(1, 0xFFFF) == 0x00) ? 1 : 0,
            "SPRITE_INIT_ALL");
}

/* ------------------------------------------------------------------ */
/* bitmap drawing                                                      */
/* ------------------------------------------------------------------ */

/* Pixel (x,y) of the framebuffer at VRAM 0. Bank 0 for every row these
** tests draw in; the two bank-1 cells are spelled out where they occur.
*/
#define PX(x, y) ((unsigned int)(y) * 320 + (x))

/* 320*240 bytes does not fit a 16-bit fill count: a naive clear does
** the top 35 rows and stops. So check the LAST pixel ($12BFF: bank 1),
** which only a two-half clear reaches.
*/
void test_gfx_clear(void) {
    t_vpoke(0, PX(0, 0), 0x00);
    t_vpoke(0, PX(0, 36), 0x00);        /* just past a truncated fill */
    t_vpoke(1, 0x2BFF, 0x00);           /* the very last pixel */

    x16_gfx_clear(0x77);

    t_check((t_vpeek(0, PX(0, 0)) == 0x77 &&
            t_vpeek(0, PX(0, 36)) == 0x77 &&
            t_vpeek(1, 0x2BFF) == 0x77) ? 1 : 0,
            "GFX_CLEAR");
}

void test_gfx_pset(void) {
    t_vpoke(0, PX(10, 5), 0x00);
    x16_gfx_pset(10, 5, 0x42);

    t_check((t_vpeek(0, PX(10, 5)) == 0x42) ? 1 : 0, "GFX_PSET");
}

/* x >= 320 and y >= 240 are off screen. Unclipped, pset(320,0) would
** land on pixel (0,1) and pset(0,240) at offset $12C00 -- so poison
** both and check they stayed clean.
*/
void test_gfx_clip(void) {
    t_vpoke(0, PX(0, 1), 0x00);
    t_vpoke(1, 0x2C00, 0x00);           /* 240 * 320 */

    x16_gfx_pset(320, 0, 0x99);
    x16_gfx_pset(0, 240, 0x99);

    t_check((t_vpeek(0, PX(0, 1)) == 0x00 &&
            t_vpeek(1, 0x2C00) == 0x00) ? 1 : 0,
            "GFX_CLIP");
}

void test_gfx_hline(void) {
    t_vpoke(0, PX(20, 8), 0x00);
    t_vpoke(0, PX(25, 8), 0x00);

    x16_gfx_hline(20, 8, 5, 0x33);

    t_check((t_vpeek(0, PX(20, 8)) == 0x33 &&
            t_vpeek(0, PX(24, 8)) == 0x33 &&
            t_vpeek(0, PX(25, 8)) == 0x00) ? 1 : 0, /* one past the end */
            "GFX_HLINE");
}

void test_gfx_vline(void) {
    unsigned char i;

    for (i = 2; i <= 6; i++) {
        t_vpoke(0, PX(3, i), 0x00);
    }
    t_vpoke(0, PX(4, 2), 0x00);

    x16_gfx_vline(3, 2, 4, 0x55);

    t_check((t_vpeek(0, PX(3, 2)) == 0x55 &&
            t_vpeek(0, PX(3, 5)) == 0x55 &&
            t_vpeek(0, PX(3, 6)) == 0x00 &&     /* one past the end */
            t_vpeek(0, PX(4, 2)) == 0x00) ? 1 : 0, /* not sideways */
            "GFX_VLINE");
}

void test_gfx_line(void) {
    /* Four distinct coordinates: a transposed argument lands elsewhere. */
    t_vpoke(0, PX(3, 7), 0x00);
    t_vpoke(0, PX(11, 5), 0x00);

    x16_gfx_line(3, 7, 11, 5, 0x77);

    t_check((t_vpeek(0, PX(3, 7)) == 0x77 &&
            t_vpeek(0, PX(11, 5)) == 0x77) ? 1 : 0,
            "GFX_LINE");
}

void test_gfx_circle(void) {
    x16_vera_addr0(X16_INC_1, 0x00000);
    x16_vera_fill(0x00, 12800);         /* rows 0..39 */

    x16_gfx_circle(50, 20, 10, 0x91);

    t_check((t_vpeek(0, PX(60, 20)) == 0x91 &&  /* east */
            t_vpeek(0, PX(40, 20)) == 0x91 &&   /* west */
            t_vpeek(0, PX(50, 10)) == 0x91 &&   /* north */
            t_vpeek(0, PX(50, 30)) == 0x91 &&   /* south */
            t_vpeek(0, PX(50, 20)) == 0x00 &&   /* hollow */
            t_vpeek(0, PX(61, 20)) == 0x00) ? 1 : 0, /* nothing outside */
            "GFX_CIRCLE");
}

void test_gfx_disc(void) {
    x16_vera_addr0(X16_INC_1, 0x00000);
    x16_vera_fill(0x00, 12800);

    x16_gfx_disc(50, 20, 10, 0x93);

    t_check((t_vpeek(0, PX(50, 20)) == 0x93 &&  /* centre filled */
            t_vpeek(0, PX(55, 20)) == 0x93 &&
            t_vpeek(0, PX(60, 20)) == 0x93 &&   /* rim */
            t_vpeek(0, PX(61, 20)) == 0x00 &&   /* one past */
            t_vpeek(0, PX(50, 31)) == 0x00) ? 1 : 0,
            "GFX_DISC");
}

void test_gfx_char(void) {
    unsigned char i;
    unsigned char j;
    unsigned char nset = 0;

    x16_vera_addr0(X16_INC_1, 0x00000);
    x16_vera_fill(0x00, 12800);

    x16_gfx_char(40, 8, 0x95, 1);       /* screen code 1 = 'A' */

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            if (t_vpeek(0, PX(40 + j, 8 + i)) == 0x95) nset++;
        }
    }
    /* Some pixels lit, but not the whole 8x8 cell: it is a glyph, and
    ** the clear bits were left transparent.
    */
    t_check((nset > 4 && nset < 64 && t_vpeek(0, PX(48, 8)) == 0x00) ? 1 : 0,
            "GFX_CHAR");
}

void test_gfx_flood(void) {
    unsigned char ok;

    x16_vera_addr0(X16_INC_1, 0x00000);
    x16_vera_fill(0x00, 12800);

    x16_gfx_frame(200, 4, 20, 20, 0x97);        /* a hollow box */
    ok = x16_gfx_flood(205, 10, 0x98);          /* seed inside it */

    t_check((ok == 1 &&
            t_vpeek(0, PX(205, 10)) == 0x98 &&  /* the seed */
            t_vpeek(0, PX(218, 22)) == 0x98 &&  /* the far corner */
            t_vpeek(0, PX(200, 4)) == 0x97 &&   /* the frame survived */
            t_vpeek(0, PX(199, 10)) == 0x00) ? 1 : 0, /* did not leak */
            "GFX_FLOOD");
}

/* ------------------------------------------------------------------ */
/* VERA FX                                                             */
/* ------------------------------------------------------------------ */

void test_fx_mult(void) {
    t_check((x16_fx_mult(1000, 1000) == 1000000) ? 1 : 0, "FX_MULT");
}

void test_fx_mult_signed(void) {
    t_check((x16_fx_mult(-1000, 1000) == -1000000 &&
            x16_fx_mult(-3, -5) == 15) ? 1 : 0,
            "FX_MULT_SIGNED");
}

void test_fx_fill(void) {
    vram_poison(0, TESTVRAM, 12, 0x00);

    x16_fx_fill(0xA5, 10, 0x04000);

    t_check((vram_all(0, TESTVRAM, 10, 0xA5) &&
            t_vpeek(0, TESTVRAM + 10) == 0x00 && /* tail stopped on time */
            t_vpeek(0, TESTVRAM + 11) == 0x00) ? 1 : 0,
            "FX_FILL");
}

void test_fx_line(void) {
    unsigned char i;
    unsigned char ok = 1;

    x16_vera_addr0(X16_INC_1, 0x00000);
    x16_vera_fill(0x00, 2560);          /* rows 0..7 */

    x16_fx_line(0, 0, 7, 0, 0xC1);      /* horizontal */
    x16_fx_line(20, 0, 27, 7, 0xC2);    /* diagonal */
    x16_fx_line(60, 0, 53, 7, 0xC3);    /* anti-diagonal */
    x16_fx_line(100, 0, 103, 7, 0xC4);  /* y-major slant */

    for (i = 0; i < 8; i++) {
        if (t_vpeek(0, PX(i, 0)) != 0xC1) ok = 0;       /* the whole run */
        if (t_vpeek(0, PX(20 + i, i)) != 0xC2) ok = 0;  /* every step */
        if (t_vpeek(0, PX(60 - i, i)) != 0xC3) ok = 0;
    }
    if (t_vpeek(0, PX(8, 0)) != 0x00) ok = 0;           /* stopped on time */
    if (t_vpeek(0, PX(21, 0)) != 0x00) ok = 0;          /* not a transpose */

    /* A y-major slant draws one pixel per row; only the endpoints are
    ** pinned, because the interior is VERA rounding, not ours.
    */
    if (t_vpeek(0, PX(100, 0)) != 0xC4) ok = 0;
    if (t_vpeek(0, PX(103, 7)) != 0xC4) ok = 0;

    t_check(ok, "FX_LINE");
}

x16_point tri_a;
x16_point tri_b;
x16_point tri_c;

void test_fx_triangle(void) {
    unsigned char ok;

    tri_a.x = 10;
    tri_a.y = 5;
    tri_b.x = 30;
    tri_b.y = 5;
    tri_c.x = 10;
    tri_c.y = 25;

    x16_vera_addr0(X16_INC_1, 0x00000);
    x16_vera_fill(0x00, 9600);          /* rows 0..29 */

    x16_fx_triangle(&tri_a, &tri_b, &tri_c, 0xAA);

    ok = (t_vpeek(0, PX(9, 5)) == 0x00 &&       /* left of it */
          t_vpeek(0, PX(10, 5)) == 0xAA &&      /* top row runs 10..29 */
          t_vpeek(0, PX(29, 5)) == 0xAA &&
          t_vpeek(0, PX(30, 5)) == 0x00 &&      /* (30,5) is outside */
          t_vpeek(0, PX(19, 15)) == 0xAA &&     /* row 15 runs 10..19 */
          t_vpeek(0, PX(20, 15)) == 0x00 &&
          t_vpeek(0, PX(10, 24)) == 0xAA &&     /* last row: one pixel */
          t_vpeek(0, PX(11, 24)) == 0x00 &&
          t_vpeek(0, PX(10, 25)) == 0x00) ? 1 : 0; /* half-open row */

    t_check(ok, "FX_TRIANGLE");
}

void test_fx_copy(void) {
    unsigned char i;
    unsigned char ok = 1;

    for (i = 0; i < 10; i++) {
        t_vpoke(0, TESTVRAM + i, 0xA0 + i);     /* source: a ramp */
    }
    vram_poison(0, TESTVRAM + 0x100, 12, 0x00); /* aligned destination */

    x16_fx_copy(0x04000, 0x04100, 10);

    for (i = 0; i < 10; i++) {
        if (t_vpeek(0, TESTVRAM + 0x100 + i) != 0xA0 + i) ok = 0;
    }
    t_check((ok && t_vpeek(0, TESTVRAM + 0x100 + 10) == 0x00) ? 1 : 0,
            "FX_COPY");
}

void test_fx_transparency(void) {
    vram_poison(0, TESTVRAM, 4, 0x77);

    x16_fx_transp_on();
    x16_vera_addr0(X16_INC_1, 0x04000);
    x16_vera_fill(0x00, 2);             /* zeros: must be dropped */
    x16_vera_addr0(X16_INC_1, 0x04002);
    x16_vera_fill(0x5B, 2);             /* nonzero: must land */
    x16_fx_transp_off();

    t_check((t_vpeek(0, TESTVRAM + 0) == 0x77 &&  /* transparent */
            t_vpeek(0, TESTVRAM + 1) == 0x77 &&
            t_vpeek(0, TESTVRAM + 2) == 0x5B &&   /* opaque */
            t_vpeek(0, TESTVRAM + 3) == 0x5B) ? 1 : 0,
            "FX_TRANSPARENCY");
}

/* ------------------------------------------------------------------ */
/* interrupts                                                          */
/* ------------------------------------------------------------------ */

/* Headless x16emu raises no VSYNC, so nothing callback-shaped can be
** asserted here; -Windowed exercises the live paths. What CAN be
** checked headless is the hooking itself: install must change CINV,
** remove must restore it exactly, and the sprite-collision accumulator
** must read empty and clear. This also forces irq.c through the
** compiler, static function-pointer initialisers and all.
*/
void test_irq_hook(void) {
    unsigned int before;
    unsigned int hooked;
    unsigned int after;
    unsigned char m;

    before = *(unsigned int*)0x0314;    /* CINV */
    x16_irq_install();
    hooked = *(unsigned int*)0x0314;
    x16_irq_sprcol_install(0);          /* NULL handler: poll-only */
    m = x16_sprite_collisions();
    x16_irq_sprcol_remove();
    x16_irq_remove();
    after = *(unsigned int*)0x0314;

    t_check((hooked != before && after == before && m == 0) ? 1 : 0,
            "IRQ_HOOK");
}

/* ------------------------------------------------------------------ */
/* PSG                                                                 */
/* ------------------------------------------------------------------ */

/* The voices are at $1F9C0: bank 1, $F9C0 + voice*4. The volume byte
** carries pan in 7:6 and volume in 5:0.
*/
#define PSG_VOL_OF(v)   (t_vpeek(1, 0xF9C0 + (v) * 4 + 2) & 0x3F)
#define PSG_PAN_OF(v)   (t_vpeek(1, 0xF9C0 + (v) * 4 + 2) & 0xC0)

void test_psg_regs(void) {
    x16_psg_init();
    x16_psg_set_freq(3, 0x04A5);
    x16_psg_set_vol(3, 63, X16_PSG_PAN_BOTH);
    x16_psg_set_wave(3, X16_PSG_WAVE_TRIANGLE, 32);

    t_check((t_vpeek(1, 0xF9C0 + 3 * 4 + 0) == 0xA5 &&
            t_vpeek(1, 0xF9C0 + 3 * 4 + 1) == 0x04 &&
            t_vpeek(1, 0xF9C0 + 3 * 4 + 2) == 0xFF &&  /* both | vol 63 */
            t_vpeek(1, 0xF9C0 + 3 * 4 + 3) == 0xA0) ? 1 : 0, /* tri | 32 */
            "PSG_REGS");
}

/* X16_PSG_HZ scales by 175922>>16 with a rounded shift. It must land on
** the exact step for the musical pitches and must not overflow 32 bits
** at the top of the audible range. The pitches go through variables so
** the 32-bit multiply actually runs on target instead of folding.
*/
unsigned int hz_a4 = 440;
unsigned int hz_a5 = 880;
unsigned int hz_one = 1;
unsigned int hz_top = 20000;

unsigned int psg_hz_of(unsigned int hz) {
    /* X16_PSG_HZ's arithmetic, in steps: the one-expression macro form
    ** trips a KickC 0.8.6 internal error ("Cannot cast declared type")
    ** when its argument is a variable. Literal arguments fold fine.
    */
    unsigned long t = (unsigned long)hz;
    t = t * 175922;
    t = t + 32768;
    t = t >> 16;
    return (unsigned int)t;
}

void test_psg_hz(void) {
    t_check((psg_hz_of(hz_a4) == 1181 &&        /* A4, exact */
            psg_hz_of(hz_a5) == 2362 &&         /* A5, exact */
            psg_hz_of(hz_one) == 3 &&           /* rounds up, not down */
            psg_hz_of(hz_top) == 53687 &&       /* no 32-bit overflow */
            X16_PSG_HZ(440) == 1181 &&          /* the macro, folded */
            X16_PSG_HZ(880) == 2362) ? 1 : 0,
            "PSG_HZ");
}

void test_psg_note_off(void) {
    x16_psg_set_vol(5, 40, X16_PSG_PAN_RIGHT);
    x16_psg_note_off(5);

    t_check((t_vpeek(1, 0xF9C0 + 5 * 4 + 2) == X16_PSG_PAN_RIGHT) ? 1 : 0,
            "PSG_NOTE_OFF");
}

void test_psg_env_attack(void) {
    unsigned char v0;
    unsigned char v1;
    unsigned char v2;
    unsigned char peak;

    x16_psg_init();
    x16_psg_set_vol(1, 0, X16_PSG_PAN_RIGHT);   /* establish the panning */
    x16_psg_env_start(1, 30, 10, 255, 5);       /* peak 30, +10/tick */

    v0 = PSG_VOL_OF(1);                         /* not written yet */
    x16_psg_env_tick();
    v1 = PSG_VOL_OF(1);                         /* 10 */
    x16_psg_env_tick();
    v2 = PSG_VOL_OF(1);                         /* 20 */
    x16_psg_env_tick();                         /* 30: clamps at the peak */
    x16_psg_env_tick();
    peak = PSG_VOL_OF(1);

    t_check((v0 == 0 && v1 == 10 && v2 == 20 && peak == 30 &&
            PSG_PAN_OF(1) == X16_PSG_PAN_RIGHT) ? 1 : 0,
            "PSG_ENV_ATTACK");
}

/* attack = 0 jumps to the peak and writes it immediately, without
** waiting for a tick.
*/
void test_psg_env_instant(void) {
    x16_psg_init();
    x16_psg_set_vol(2, 0, X16_PSG_PAN_BOTH);
    x16_psg_env_start(2, 45, 0, 255, 5);

    t_check((PSG_VOL_OF(2) == 45) ? 1 : 0, "PSG_ENV_INSTANT");
}

/* Release fades in steps of 12 from 40: 28, 16, 4, 0-clamped. The step
** deliberately does not divide the peak, so the clamp is exercised.
*/
void test_psg_env_release(void) {
    unsigned char held;
    unsigned char r1;
    unsigned char i;

    x16_psg_init();
    x16_psg_set_vol(3, 0, X16_PSG_PAN_BOTH);
    x16_psg_env_start(3, 40, 0, 255, 12);       /* instant peak, hold */

    x16_psg_env_tick();
    x16_psg_env_tick();
    held = PSG_VOL_OF(3);                       /* still 40 */

    x16_psg_env_release(3);
    x16_psg_env_tick();
    r1 = PSG_VOL_OF(3);                         /* 28 */

    for (i = 0; i < 8; i++) {
        x16_psg_env_tick();                     /* ...down to silence */
    }
    t_check((held == 40 && r1 == 28 &&
            PSG_VOL_OF(3) == 0 &&
            PSG_PAN_OF(3) == X16_PSG_PAN_BOTH) ? 1 : 0,
            "PSG_ENV_RELEASE");
}

/* A finite sustain counts down and then releases on its own. */
void test_psg_env_sustain(void) {
    unsigned char after2;
    unsigned char i;

    x16_psg_init();
    x16_psg_set_vol(4, 0, X16_PSG_PAN_BOTH);
    x16_psg_env_start(4, 20, 0, 2, 6);          /* hold 2 ticks, then fade */

    x16_psg_env_tick();
    x16_psg_env_tick();
    after2 = PSG_VOL_OF(4);                     /* still at the peak */

    for (i = 0; i < 5; i++) {
        x16_psg_env_tick();
    }
    t_check((after2 == 20 && PSG_VOL_OF(4) == 0) ? 1 : 0, "PSG_ENV_SUSTAIN");
}

void test_psg_env_stop(void) {
    x16_psg_init();
    x16_psg_set_vol(5, 0, X16_PSG_PAN_LEFT);
    x16_psg_env_start(5, 50, 0, 255, 0);

    x16_psg_env_stop(5);
    x16_psg_env_tick();                         /* disarmed: stays silent */

    t_check((PSG_VOL_OF(5) == 0 && PSG_PAN_OF(5) == X16_PSG_PAN_LEFT) ? 1 : 0,
            "PSG_ENV_STOP");
}

/* ------------------------------------------------------------------ */
/* YM2151                                                              */
/* ------------------------------------------------------------------ */

/* ym_write must complete rather than time out on the busy flag. */
void test_ym_write(void) {
    /* $20 is RL/FB/CONNECT for channel 0. */
    t_check((x16_ym_write(0x20, 0x00) == 1) ? 1 : 0, "YM_WRITE");
}

/* THE channel-in-A test.
**
** The ROM driver takes the FM channel in .A and the payload in .X, the
** reverse of x16_ym_write. A transposed call would send pan=5 to
** channel 2 instead of pan=2 to channel 5 -- a valid-looking call that
** plays on the wrong channel and never reports an error. So check
** both: that the setting reached channel 5, AND that it did not land
** on channel 2.
*/
void test_ym_channel_in_a(void) {
    unsigned char ok;

    ok = (x16_ym_pan(5, X16_YM_PAN_RIGHT) == 1 &&
          x16_ym_get_pan(5) == X16_YM_PAN_RIGHT &&
          x16_ym_get_pan(2) != X16_YM_PAN_RIGHT) ? 1 : 0;

    /* Attenuation travels the same way round. */
    ok = (ok && x16_ym_vol(5, 40) == 1 && x16_ym_get_vol(5) == 40) ? 1 : 0;

    t_check(ok, "YM_CHANNEL_IN_A");
}

/* ------------------------------------------------------------------ */
/* input                                                               */
/* ------------------------------------------------------------------ */

/* Joystick 0 is the keyboard shadow: always present, all bits high
** (active low) when nothing is held.
*/
void test_joy_get(void) {
    unsigned char present = 0xAA;
    unsigned int buttons;

    x16_joy_scan();
    buttons = x16_joy_get(0, &present);

    t_check((buttons == 0xFFFF && (present == 0 || present == 1)) ? 1 : 0,
            "JOY_GET");
}

/* Joystick 4 is certainly not plugged in, so the wrapper must turn the
** KERNAL's $FF into a clean 0 rather than passing it through.
*/
void test_joy_absent(void) {
    unsigned char present = 0xAA;

    x16_joy_scan();
    x16_joy_get(4, &present);

    t_check((present == 0) ? 1 : 0, "JOY_ABSENT");
}

/* Drained, the queue reads empty: get is 0 and peek's depth is 0. The
** drain matters -- the boot sequence's RUN line can still be queued.
*/
void test_key_empty(void) {
    unsigned int p;
    unsigned char guard = 0;

    while (x16_key_get() != 0 && guard < 32) {
        guard++;                /* drain whatever boot left queued */
    }
    p = x16_key_peek();

    t_check((x16_key_get() == 0 && X16_KEY_COUNT(p) == 0) ? 1 : 0,
            "KEY_EMPTY");
}

/* ------------------------------------------------------------------ */

int main(void) {
    t_init();

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

    test_tile_addr();
    test_tile_roundtrip();
    test_layer_scroll();
    test_layer_enable();

    test_sprite_pos();
    test_sprite_image();
    test_sprite_z();
    test_sprite_size();
    test_sprite_size_pal_mask();
    test_sprite_init_all();

    test_irq_hook();

    test_gfx_clear();
    test_gfx_pset();
    test_gfx_clip();
    test_gfx_hline();
    test_gfx_vline();
    test_gfx_line();
    test_gfx_circle();
    test_gfx_disc();
    test_gfx_char();
    test_gfx_flood();

    /* On a VERA without the FX register set these would write to
    ** registers that do not exist. Skipping is honest; the capability
    ** was independently probed, not assumed absent.
    */
    if (x16_vera_has_fx()) {
        test_fx_mult();
        test_fx_mult_signed();
        test_fx_fill();
        test_fx_line();
        test_fx_triangle();
        test_fx_copy();
        test_fx_transparency();
    } else {
        t_skip("FX_MULT");
        t_skip("FX_MULT_SIGNED");
        t_skip("FX_FILL");
        t_skip("FX_LINE");
        t_skip("FX_TRIANGLE");
        t_skip("FX_COPY");
        t_skip("FX_TRANSPARENCY");
    }

    test_psg_regs();
    test_psg_hz();
    test_psg_note_off();
    test_psg_env_attack();
    test_psg_env_instant();
    test_psg_env_release();
    test_psg_env_sustain();
    test_psg_env_stop();

    test_joy_get();
    test_joy_absent();
    test_key_empty();

    test_ym_write();
    /* x16_ym_init() reports whether this machine has a YM2151 at all.
    ** Skipping on a machine without one is honest; failing would not be.
    */
    if (x16_ym_init()) {
        test_ym_channel_in_a();
    } else {
        t_skip("YM_CHANNEL_IN_A");
    }

    t_done();
    return 0;
}
