/* =====================================================================
 * x16clib :: test_vbcc/runner3.c -- the four new bitmap engines
 * =====================================================================
 * Split out of runner.c for size, exactly as runner2.c was: bitmap4l
 * (320x240@4bpp), bitmap2l (320x240@2bpp), and the VERA_2 SDRAM pair
 * bitmap4h/8h (640x480). The low-res engines draw into stock VRAM, so
 * every effect is t_vpeek-checkable; the VERA_2 pair only runs where
 * the hardware answers x16_gfx*h_has() -- always absent under the
 * emulator, so those checks skip there.
 *
 * 4bpp: a pixel byte is at y*160 + (x>>1), left pixel high nibble.
 * 2bpp: a pixel byte is at y*80 + (x>>2), MSB-first pairs of bits.
 * ===================================================================== */

#include "testlib.h"
#include <x16/vera.h>
#include <x16/bitmap2l.h>
#include <x16/bitmap4l.h>
#include <x16/bitmap4h.h>
#include <x16/bitmap8h.h>
#include <x16/verafx.h>
#include <x16/verafx_utils.h>
#include <x16/dos.h>
#include <x16/bmx.h>
#include <x16/load.h>

#define P4L(x, y)   ((unsigned long)(y) * 160 + ((x) >> 1))
#define P2L(x, y)   ((unsigned long)(y) * 80 + ((x) >> 2))
#define L0_CONFIG   (*(volatile unsigned char *)0x9F2DU)
#define L0_TILEBASE (*(volatile unsigned char *)0x9F2FU)
#define DC_HSCALE   (*(volatile unsigned char *)0x9F2AU)
#define DC_VSCALE   (*(volatile unsigned char *)0x9F2BU)
#define TESTVRAM    0x08000UL

static void test_g4l_init(void)
{
    x16_gfx4l_init();
    /* 320-wide: TILEBASE bit 0 clear, and the scale doubles every
    ** pixel so the framebuffer covers the whole 640x480 display.
    */
    t_check(L0_CONFIG == 0x06 &&        /* bitmap | 4bpp */
            L0_TILEBASE == 0x00 &&
            DC_HSCALE == 0x40 && DC_VSCALE == 0x40,
            "G4L_INIT");
}

/* 38,400 bytes fits vera_fill's 16-bit count, but check the LAST byte
** anyway: a stride slip shows up at the far end first.
*/
static void test_g4l_clear(void)
{
    t_vpoke(0x00, P4L(0, 0));
    t_vpoke(0x00, P4L(318, 239));
    x16_gfx4l_clear(5);
    t_check(t_vpeek(P4L(0, 0)) == 0x55 &&
            t_vpeek(P4L(318, 239)) == 0x55,
            "G4L_CLEAR");
}

static void test_g4l_pset(void)
{
    t_vpoke(0x00, P4L(10, 7));
    x16_gfx4l_pset(11, 7, 9);           /* odd x: low nibble */
    x16_gfx4l_pset(10, 7, 3);           /* even x: high nibble */
    t_check(t_vpeek(P4L(10, 7)) == 0x39, "G4L_PSET");
}

/* Unclipped, (320,0) would land on byte 160 (row 1) and (0,240) at
** offset 38,400.
*/
static void test_g4l_clip(void)
{
    t_vpoke(0x11, P4L(0, 1));
    t_vpoke(0x22, 38400UL);
    x16_gfx4l_pset(320, 0, 15);
    x16_gfx4l_pset(0, 240, 15);
    t_check(t_vpeek(P4L(0, 1)) == 0x11 && t_vpeek(38400UL) == 0x22,
            "G4L_CLIP");
}

static void test_g4l_read(void)
{
    t_vpoke(0x8E, P4L(20, 12));
    t_check(x16_gfx4l_read(20, 12) == 8 &&
            x16_gfx4l_read(21, 12) == 14 &&
            x16_gfx4l_read(320, 12) == 0xFF,
            "G4L_READ");
}

/* x=3 len=6: head = low nibble of byte 1, middle bytes 2-3, tail =
** high nibble of byte 4. The bytes either side must survive.
*/
static void test_g4l_hline(void)
{
    unsigned char i;
    for (i = 0; i < 6; ++i) {
        t_vpoke(0x00, P4L(0, 40) + i);
    }
    x16_gfx4l_hline(3, 40, 6, 4);
    t_check(t_vpeek(P4L(0, 40) + 0) == 0x00 &&
            t_vpeek(P4L(0, 40) + 1) == 0x04 &&
            t_vpeek(P4L(0, 40) + 2) == 0x44 &&
            t_vpeek(P4L(0, 40) + 3) == 0x44 &&
            t_vpeek(P4L(0, 40) + 4) == 0x40 &&
            t_vpeek(P4L(0, 40) + 5) == 0x00,
            "G4L_HLINE");
}

/* Colour 7 into poisoned $0C bytes: even x, so the high nibble; the
** low nibble must survive as read-modify-write.
*/
static void test_g4l_vline(void)
{
    unsigned char i;
    for (i = 50; i <= 53; ++i) {
        t_vpoke(0x0C, (unsigned long)i * 160 + 3);
    }
    x16_gfx4l_vline(6, 50, 3, 7);
    t_check(t_vpeek(50UL * 160 + 3) == 0x7C &&
            t_vpeek(51UL * 160 + 3) == 0x7C &&
            t_vpeek(52UL * 160 + 3) == 0x7C &&
            t_vpeek(53UL * 160 + 3) == 0x0C,    /* len 3 stops here */
            "G4L_VLINE");
}

static void test_g4l_rect(void)
{
    unsigned char i;
    for (i = 0; i < 2; ++i) {
        t_vpoke(0x00, P4L(4, 60 + i));
        t_vpoke(0x00, P4L(6, 60 + i));
    }
    x16_gfx4l_rect(4, 60, 4, 2, 6);
    t_check(t_vpeek(P4L(4, 60)) == 0x66 && t_vpeek(P4L(6, 60)) == 0x66 &&
            t_vpeek(P4L(4, 61)) == 0x66 && t_vpeek(P4L(6, 61)) == 0x66,
            "G4L_RECT");
}

/* The middle row keeps only its edge pixels: (4,71) in a high nibble,
** (9,71) in a low nibble.
*/
static void test_g4l_frame(void)
{
    unsigned char i, j;
    for (i = 0; i < 3; ++i) {
        for (j = 2; j <= 4; ++j) {
            t_vpoke(0x00, (unsigned long)(70 + i) * 160 + j);
        }
    }
    x16_gfx4l_frame(4, 70, 6, 3, 2);
    t_check(t_vpeek(P4L(4, 70)) == 0x22 &&
            t_vpeek(P4L(6, 70)) == 0x22 &&
            t_vpeek(P4L(8, 70)) == 0x22 &&
            t_vpeek(P4L(4, 71)) == 0x20 &&
            t_vpeek(P4L(6, 71)) == 0x00 &&
            t_vpeek(P4L(8, 71)) == 0x02 &&
            t_vpeek(P4L(4, 72)) == 0x22 &&
            t_vpeek(P4L(8, 72)) == 0x22,
            "G4L_FRAME");
}

/* Asymmetric endpoints: a shim that swapped x1 (P3/P4) with y1 (P5)
** or the colour (P6) could not light both ends.
*/
static void test_g4l_line(void)
{
    t_vpoke(0x00, P4L(3, 80));
    t_vpoke(0x00, P4L(11, 84));
    x16_gfx4l_line(3, 80, 11, 84, 5);
    t_check(x16_gfx4l_read(3, 80) == 5 &&
            x16_gfx4l_read(11, 84) == 5,
            "G4L_LINE");
}

static void test_g4l_char(void)
{
    unsigned char i, set = 0;

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 8000);          /* rows 0-49, 4bpp */

    x16_gfx4l_char(40, 8, 5, 1);        /* screen code 1 = 'A' */

    for (i = 0; i < 8; ++i) {
        unsigned char j;
        for (j = 0; j < 8; ++j) {
            if (x16_gfx4l_read(40 + j, 8 + i) == 5) ++set;
        }
    }
    t_check(set > 4 && set < 64 && x16_gfx4l_read(48, 8) == 0,
            "G4L_CHAR");
}

static void test_g4l_text(void)
{
    unsigned char i, a = 0, b = 0;

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 8000);

    x16_gfx4l_text(100, 8, 9, "AB");

    for (i = 0; i < 8; ++i) {
        unsigned char j;
        for (j = 0; j < 8; ++j) {
            if (x16_gfx4l_read(100 + j, 8 + i) == 9) ++a;
            if (x16_gfx4l_read(108 + j, 8 + i) == 9) ++b;
        }
    }
    t_check(a > 4 && b > 4 && a != b, "G4L_TEXT");
}

/* Rows of $F0 tile from the origin: pixels 0-3 foreground, 4-7
** background.
*/
static void test_g4l_pattern(void)
{
    static const unsigned char rows[8] = {
        0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0
    };
    unsigned char i;
    for (i = 0; i < 4; ++i) {
        t_vpoke(0xFF, P4L(0, 100) + i);
    }
    x16_gfx4l_pattern_set(rows, 1, 5);
    x16_gfx4l_pattern_rect(0, 100, 8, 1);
    t_check(t_vpeek(P4L(0, 100) + 0) == 0x55 &&
            t_vpeek(P4L(0, 100) + 1) == 0x55 &&
            t_vpeek(P4L(0, 100) + 2) == 0x11 &&
            t_vpeek(P4L(0, 100) + 3) == 0x11,
            "G4L_PATTERN");
}

/* Copy, then XOR with itself: the second pass must erase the first. */
static void test_g4l_blit(void)
{
    static const unsigned char img[2] = { 0xAB, 0xCD };
    t_vpoke(0x00, P4L(6, 110));
    t_vpoke(0x00, P4L(8, 110));
    x16_gfx4l_blit(6, 110, 4, 1, img, 0);
    t_check(t_vpeek(P4L(6, 110)) == 0xAB && t_vpeek(P4L(8, 110)) == 0xCD,
            "G4L_BLIT");
    x16_gfx4l_blit(6, 110, 4, 1, img, 3);
    t_check(t_vpeek(P4L(6, 110)) == 0x00 && t_vpeek(P4L(8, 110)) == 0x00,
            "G4L_BLIT_XOR");
}

/* Colour 0 pixels leave the framebuffer nibble alone. */
static void test_g4l_blitm(void)
{
    static const unsigned char img[2] = { 0xA0, 0x0D };
    t_vpoke(0x99, P4L(6, 111));
    t_vpoke(0x99, P4L(8, 111));
    x16_gfx4l_blitm(6, 111, 4, 1, img);
    t_check(t_vpeek(P4L(6, 111)) == 0xA9 && t_vpeek(P4L(8, 111)) == 0x9D,
            "G4L_BLITM");
}

static void test_g2l_init(void)
{
    x16_gfx2l_init();
    t_check(L0_CONFIG == 0x05 &&        /* bitmap | 2bpp */
            L0_TILEBASE == 0x00 &&      /* base $00000, 320 wide */
            DC_HSCALE == 0x40 && DC_VSCALE == 0x40,
            "G2L_INIT");
}

static void test_g2l_pset(void)
{
    t_vpoke(0x00, P2L(5, 10));
    x16_gfx2l_pset(5, 10, 2);           /* byte 1, pixel 1 */
    t_check(t_vpeek(10UL * 80 + 1) == 0x20, "G2L_PSET");
}

/* Unclipped, (320,0) would land on byte 80 and (0,240) at 19,200. */
static void test_g2l_clip(void)
{
    t_vpoke(0x11, 80UL);
    t_vpoke(0x22, 19200UL);
    x16_gfx2l_pset(320, 0, 3);
    x16_gfx2l_pset(0, 240, 3);
    t_check(t_vpeek(80UL) == 0x11 && t_vpeek(19200UL) == 0x22, "G2L_CLIP");
}

static void test_g2l_read(void)
{
    t_vpoke(0x1B, 12UL * 80);           /* pixels 0,1,2,3 left to right */
    t_check(x16_gfx2l_read(0, 12) == 0 &&
            x16_gfx2l_read(1, 12) == 1 &&
            x16_gfx2l_read(2, 12) == 2 &&
            x16_gfx2l_read(3, 12) == 3 &&
            x16_gfx2l_read(320, 12) == 0xFF,
            "G2L_READ");
}

/* x=5 len=13: head = byte 1 pixels 1-3, middle bytes 2-3, tail = byte 4
** pixels 0-1.
*/
static void test_g2l_hline(void)
{
    unsigned char i;
    for (i = 0; i < 6; ++i) {
        t_vpoke(0x00, 20UL * 80 + i);
    }
    x16_gfx2l_hline(5, 20, 13, 3);
    t_check(t_vpeek(20UL * 80 + 0) == 0x00 &&
            t_vpeek(20UL * 80 + 1) == 0x3F &&
            t_vpeek(20UL * 80 + 2) == 0xFF &&
            t_vpeek(20UL * 80 + 3) == 0xFF &&
            t_vpeek(20UL * 80 + 4) == 0xF0 &&
            t_vpeek(20UL * 80 + 5) == 0x00,
            "G2L_HLINE");
}

/* Colour 0 ink onto $FF: proves the column really is read-modify-write. */
static void test_g2l_vline(void)
{
    unsigned char i;
    for (i = 30; i <= 34; ++i) {
        t_vpoke(0xFF, (unsigned long)i * 80 + 1);
    }
    x16_gfx2l_vline(6, 30, 4, 0);       /* byte 1, pixel 2 */
    t_check(t_vpeek(30UL * 80 + 1) == 0xF3 &&
            t_vpeek(33UL * 80 + 1) == 0xF3 &&
            t_vpeek(34UL * 80 + 1) == 0xFF,     /* len 4 stops here */
            "G2L_VLINE");
}

static void test_g2l_rect(void)
{
    t_vpoke(0x00, 100UL * 80 + 2);
    t_vpoke(0x00, 100UL * 80 + 3);
    t_vpoke(0x00, 101UL * 80 + 2);
    t_vpoke(0x00, 101UL * 80 + 3);
    x16_gfx2l_rect(8, 100, 8, 2, 1);
    t_check(t_vpeek(100UL * 80 + 2) == 0x55 &&
            t_vpeek(100UL * 80 + 3) == 0x55 &&
            t_vpeek(101UL * 80 + 2) == 0x55 &&
            t_vpeek(101UL * 80 + 3) == 0x55,
            "G2L_RECT");
}

/* Middle row keeps only its edge pixels: 12 leads byte 3, 19 ends
** byte 4.
*/
static void test_g2l_frame(void)
{
    unsigned char i, j;
    for (i = 110; i <= 112; ++i) {
        for (j = 3; j <= 4; ++j) {
            t_vpoke(0x00, (unsigned long)i * 80 + j);
        }
    }
    x16_gfx2l_frame(12, 110, 8, 3, 2);
    t_check(t_vpeek(110UL * 80 + 3) == 0xAA &&
            t_vpeek(110UL * 80 + 4) == 0xAA &&
            t_vpeek(111UL * 80 + 3) == 0x80 &&
            t_vpeek(111UL * 80 + 4) == 0x02 &&
            t_vpeek(112UL * 80 + 3) == 0xAA &&
            t_vpeek(112UL * 80 + 4) == 0xAA,
            "G2L_FRAME");
}

/* A horizontal walk fills whole bytes through pset; the diagonal's
** endpoints prove the argument order end to end.
*/
static void test_g2l_line(void)
{
    t_vpoke(0x00, 120UL * 80);
    t_vpoke(0x00, 120UL * 80 + 1);
    x16_gfx2l_line(0, 120, 7, 120, 3);
    t_check(t_vpeek(120UL * 80) == 0xFF && t_vpeek(120UL * 80 + 1) == 0xFF,
            "G2L_LINE");

    t_vpoke(0x00, P2L(0, 130));
    t_vpoke(0x00, P2L(7, 137));
    x16_gfx2l_line(0, 130, 7, 137, 1);
    t_check(x16_gfx2l_read(0, 130) == 1 && x16_gfx2l_read(7, 137) == 1,
            "G2L_LINE_DIAG");
}

/* Rows of $F0 tile from the origin: pixels 0-3 foreground (11), 4-7
** background (01).
*/
static void test_g2l_pattern(void)
{
    static const unsigned char rows[8] = {
        0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0
    };
    t_vpoke(0x00, 140UL * 80);
    t_vpoke(0x00, 140UL * 80 + 1);
    x16_gfx2l_pattern_set(rows, (1 << 2) | 3);
    x16_gfx2l_pattern_rect(0, 140, 8, 1);
    t_check(t_vpeek(140UL * 80) == 0xFF && t_vpeek(140UL * 80 + 1) == 0x55,
            "G2L_PATTERN");
}

/* Copy, then XOR with itself: the second pass must erase the first. */
static void test_g2l_blit(void)
{
    static const unsigned char img[2] = { 0xE4, 0x1B };
    t_vpoke(0x00, 150UL * 80 + 2);
    t_vpoke(0x00, 150UL * 80 + 3);
    x16_gfx2l_blit(8, 150, 2, 1, img, 0);
    t_check(t_vpeek(150UL * 80 + 2) == 0xE4 &&
            t_vpeek(150UL * 80 + 3) == 0x1B,
            "G2L_BLIT");
    x16_gfx2l_blit(8, 150, 2, 1, img, 3);
    t_check(t_vpeek(150UL * 80 + 2) == 0x00 &&
            t_vpeek(150UL * 80 + 3) == 0x00,
            "G2L_BLIT_XOR");
}

/* fb' = (fb AND mask) OR data, one column, one row. */
static void test_g2l_blitm(void)
{
    static const unsigned char img[2] = { 0x3F, 0x40 };
    t_vpoke(0xFF, 160UL * 80);
    x16_gfx2l_blitm(0, 160, 1, 1, img);
    t_check(t_vpeek(160UL * 80) == 0x7F, "G2L_BLITM");
}

/* 19,200 bytes via the FX cache write; check both ends. */
static void test_g2l_clear(void)
{
    t_vpoke(0x00, 0UL);
    t_vpoke(0x00, 19199UL);
    x16_gfx2l_clear(2);
    t_check(t_vpeek(0UL) == 0xAA && t_vpeek(19199UL) == 0xAA, "G2L_CLEAR");
}

/* The VERA_2 SDRAM engines. On stock hardware and the emulator the
** ID register reads open bus, has() answers 0, and the drawing
** routines must not run (they would write into open bus). has() may
** only ever answer 0 or 1 -- anything else is a broken shim.
*/
static void test_g8h(void)
{
    unsigned char has = x16_gfx8h_has();
    t_check(has <= 1, "G8H_HAS_SANE");
    if (!has) {
        t_skip("G8H_ROUNDTRIP");
        t_skip("G8H_CLEAR");
        t_skip("G8H_COPY");
        return;
    }
    x16_gfx8h_init();
    x16_gfx8h_clear(0x11);
    t_check(x16_gfx8h_read(0, 0) == 0x11 &&
            x16_gfx8h_read(639, 479) == 0x11,
            "G8H_CLEAR");
    x16_gfx8h_pset(3, 2, 0xAB);
    t_check(x16_gfx8h_read(3, 2) == 0xAB &&
            x16_gfx8h_read(640, 2) == 0xFFFFU,
            "G8H_ROUNDTRIP");
    x16_gfx8h_pset(1, 0, 0x5A);
    x16_gfx8h_copy(1UL, 5UL, 1UL);      /* offset (1,0) -> (5,0) */
    t_check(x16_gfx8h_read(5, 0) == 0x5A, "G8H_COPY");
    x16_gfx8h_off();
}

static void test_g4h(void)
{
    unsigned char has = x16_gfx4h_has();
    t_check(has <= 1, "G4H_HAS_SANE");
    if (!has) {
        t_skip("G4H_ROUNDTRIP");
        t_skip("G4H_CLEAR");
        return;
    }
    x16_gfx4h_init();
    x16_gfx4h_clear(0x3);
    t_check(x16_gfx4h_read(0, 0) == 3 &&
            x16_gfx4h_read(639, 479) == 3,
            "G4H_CLEAR");
    x16_gfx4h_pset(3, 2, 0xA);
    t_check(x16_gfx4h_read(3, 2) == 0xA &&
            x16_gfx4h_read(2, 2) == 3 &&
            x16_gfx4h_read(640, 2) == 0xFF,
            "G4H_ROUNDTRIP");
    x16_gfx4h_off();
}

/* ------------------------------------------------------------------ */
/* The FX affine sampler, and the raw FX register knobs                */
/* ------------------------------------------------------------------ */

/* An 8x8 tile whose texel (x,y) holds y*16+x, behind a 2x2 map of tile
** 0: row 0 reads back 0,1,2..., column 0 reads back 0,16,32... The two
** rays are deliberately asymmetric -- a shim that swapped dx (P4/P5)
** with dy (P6/P7) would hand each ray the other one's walk.
*/
static void test_affine(void)
{
    unsigned char x, y, i, ok;

    i = 0;
    for (y = 0; y < 8; ++y) {           /* tile data, 2 KB aligned */
        for (x = 0; x < 8; ++x) {
            t_vpoke(y * 16 + x, 0x10000UL + i);
            ++i;
        }
    }
    for (i = 0; i < 4; ++i) {           /* a 2x2 map, all tile 0 */
        t_vpoke(0, 0x10800UL + i);
    }

    x16_fx_affine_on(0x10000UL, 0x10800UL, 0, 1);

    /* One texel per read along +x: the tile's row 0. */
    x16_fx_affine_ray(0, 0, 512, 0);
    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_fx_affine_span(8);
    ok = 1;
    for (i = 0; i < 8; ++i) {
        if (t_vpeek(TESTVRAM + i) != i) ok = 0;
    }
    t_check(ok, "AFFINE_SPAN_ROW");

    /* The same walk along +y: column 0, so 0,16,32... */
    x16_fx_affine_ray(0, 0, 0, 512);
    x16_vera_addr0(X16_INC_1, TESTVRAM + 16);
    x16_fx_affine_span(8);
    ok = 1;
    for (i = 0; i < 8; ++i) {
        if (t_vpeek(TESTVRAM + 16 + i) != i * 16) ok = 0;
    }
    t_check(ok, "AFFINE_SPAN_COL");

    /* Affine mode deliberately stays on between rays; switching it off
    ** must hand port 1 -- and ordinary addressing -- back to everyone.
    */
    x16_fx_off();
    t_vpoke(0x77, TESTVRAM + 32);
    t_check(t_vpeek(TESTVRAM + 32) == 0x77, "AFFINE_OFF_CLEAN");
}

/* Set, OR in, read back, AND out: the ctrl knobs end to end. Bit 2 is
** the 4-bit flag -- harmless while nothing is being drawn.
*/
static void test_fxu_ctrl(void)
{
    unsigned char on, off;

    x16_fxu_set_ctrl(0);
    x16_fxu_ctrl_on(4);
    on = x16_fxu_get_ctrl();
    x16_fxu_ctrl_off(4);
    off = x16_fxu_get_ctrl();
    x16_fxu_off();

    t_check(on == 4 && off == 0, "FXU_CTRL");
}

/* The poly-fill readback must return -- its value is only defined
** mid-polygon, but reading it idle may not hang or derail DCSEL.
*/
static void test_fxu_poly(void)
{
    /* The idle value is undefined, so only returning proves anything;
    ** folding v into the check (harmlessly) keeps vbcc from warning
    ** about a value-discarding statement.
    */
    unsigned int v = x16_fxu_get_poly_fill();
    x16_fxu_off();
    t_vpoke(0x66, TESTVRAM + 33);
    t_check((v | 1u) != 0 && t_vpeek(TESTVRAM + 33) == 0x66, "FXU_POLY");
}

/* ------------------------------------------------------------------ */
/* the lasterr getters, and the hi-res BMX loader                      */
/* ------------------------------------------------------------------ */

/* After a failing call the getter re-reads that exact code -- 62, FILE
** NOT FOUND, not merely "some error" -- and after a succeeding one it
** re-reads the success code.
*/
static void test_dos_lasterr(void)
{
    static const char name[] = "NOSUCH.BIN";
    unsigned char code, ok;

    code = x16_dos_delete(name, sizeof name - 1);
    ok = (code == 62) && (x16_dos_lasterr() == code);

    code = x16_dos_status();            /* the 62 was consumed: now OK */
    ok = ok && (code < X16_DOS_OK_BELOW) && (x16_dos_lasterr() == code);

    t_check(ok, "DOS_LASTERR");
}

/* A file that starts with a PRG load address instead of "BMX" (the
** cc65 suite 2 technique, minimally): the load returns ERR_FORMAT and
** the getter agrees.
*/
static void test_bmx_lasterr(void)
{
    static const char name[] = "NOTBMX3.BIN";
    static unsigned char junk[20];
    unsigned char i, code, ok;

    for (i = 0; i < sizeof junk; ++i) {
        junk[i] = i;
    }
    x16_fs_save(name, sizeof name - 1, X16_DEVICE_SD,
                junk, junk + sizeof junk);

    code = x16_bmx_load(name, sizeof name - 1, X16_DEVICE_SD, TESTVRAM);
    ok = (code == X16_BMX_ERR_FORMAT) && (x16_bmx_lasterr() == code);

    x16_dos_delete(name, sizeof name - 1);
    t_check(ok, "BMX_LASTERR");
}

/* KERNAL wrappers for the raw file write below, declared as inline-asm
** functions exactly as vbcc's own libsys.h does. t_put_raw (testlib.h)
** already wraps CHROUT.
*/
__regsused("a/x/y") void t_setnam(__reg("r0/r1") const char *name,
                                  __reg("r2") unsigned char len) = "\tlda\tr2\n\tldx\tr0\n\tldy\tr1\n\tjsr\t$ffbd";
__regsused("a/x/y") void t_setlfs(__reg("r0") unsigned char lfn,
                                  __reg("r1") unsigned char dev,
                                  __reg("r2") unsigned char sa) = "\tlda\tr0\n\tldx\tr1\n\tldy\tr2\n\tjsr\t$ffba";
__regsused("a/x/y") signed char t_open(void) = "\tinline\n\tjsr\t$ffc0\n\tbcs\t.l1\n\tlda\t#0\n.l1:\n\teinline";
__regsused("a/x/y") signed char t_chkout(__reg("r0") unsigned char lfn) = "\tinline\n\tldx\tr0\n\tjsr\t$ffc9\n\tbcs\t.l1\n\tlda\t#0\n.l1:\n\teinline";
__regsused("a/x/y") void t_kclose(__reg("a") unsigned char lfn) = "\tjsr\t$ffc3";
__regsused("a/x/y") void t_clrchn(void) = "\tjsr\t$ffcc";

/* Write exactly these bytes and nothing else -- x16_fs_save() would
** prepend a load address. Secondary address 1 = write, as the cc65
** suite's write_raw.
*/
static unsigned char write_raw3(const char *name, unsigned char namelen,
                                const unsigned char *data, unsigned char len)
{
    unsigned char i;

    t_setnam(name, namelen);
    t_setlfs(2, X16_DEVICE_SD, 1);
    if (t_open() != 0 || t_chkout(2) != 0) {
        t_kclose(2);
        return 1;
    }
    for (i = 0; i < len; ++i) {
        t_put_raw(data[i]);
    }
    t_clrchn();
    t_kclose(2);
    return 0;
}

/* On hardware without the VERA_2 layer -- this emulator -- the hi-res
** loader still parses the header and streams the pixels into open bus.
** It must come back (no hang, no crash) with a sane code, and the
** getter must agree. A complete, valid one-row BMX makes success the
** expected answer.
**
** The magic bytes are 0x42,0x4D,0x58 ('B','M','X' in ASCII) written as
** values: -cbmascii would store the character literals as PETSCII.
*/
static void test_bmx_hires_absent(void)
{
    static const char name[] = "HIRES.BMX";
    static const unsigned char file[] = {
        0x42, 0x4D, 0x58, 1,    /* magic "BMX", version                 */
        8, 3,                   /* bits per pixel, VERA depth code      */
        4, 0,                   /* width                                */
        1, 0,                   /* height: one row                      */
        1, 0,                   /* one palette entry, from index 0      */
        18, 0,                  /* pixel data offset: 16 + 1*2, no gap  */
        0, 0,                   /* not compressed, border 0             */
        0x0F, 0x00,             /* the palette entry                    */
        1, 2, 3, 4              /* the row                              */
    };
    unsigned char code, ok;

    if (write_raw3(name, sizeof name - 1, file, sizeof file)) {
        t_check(0, "BMX_HIRES_ABSENT");
        return;
    }
    code = x16_bmx_load_hires(name, X16_DEVICE_SD);
    ok = (code <= 3) && (x16_bmx_lasterr() == code);

    x16_dos_delete(name, sizeof name - 1);
    t_check(ok, "BMX_HIRES_ABSENT");
}

int main(void)
{
    t_init();

    test_g4l_init();
    test_g4l_clear();
    test_g4l_pset();
    test_g4l_clip();
    test_g4l_read();
    test_g4l_hline();
    test_g4l_vline();
    test_g4l_rect();
    test_g4l_frame();
    test_g4l_line();
    test_g4l_char();
    test_g4l_text();
    test_g4l_pattern();
    test_g4l_blit();
    test_g4l_blitm();

    test_g2l_init();
    test_g2l_pset();
    test_g2l_clip();
    test_g2l_read();
    test_g2l_hline();
    test_g2l_vline();
    test_g2l_rect();
    test_g2l_frame();
    test_g2l_line();
    test_g2l_pattern();
    test_g2l_blit();
    test_g2l_blitm();
    if (x16_vera_has_fx()) {
        test_g2l_clear();
    } else {
        t_skip("G2L_CLEAR");
    }

    test_g8h();
    test_g4h();

    if (x16_vera_has_fx()) {
        test_affine();
        test_fxu_ctrl();
        test_fxu_poly();
    } else {
        t_skip("AFFINE_SPAN_ROW");
        t_skip("AFFINE_SPAN_COL");
        t_skip("AFFINE_OFF_CLEAN");
        t_skip("FXU_CTRL");
        t_skip("FXU_POLY");
    }

    test_dos_lasterr();
    test_bmx_lasterr();
    test_bmx_hires_absent();

    t_done();
    return 0;
}
