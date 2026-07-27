/* =====================================================================
 * x16clib :: test_llvm/runner3.c -- the four new bitmap engines
 * =====================================================================
 * The third slice of the llvm-mos suite: bitmap4l (320x240@4bpp),
 * bitmap2l (320x240@2bpp), and the VERA_2 SDRAM pair bitmap4h/8h
 * (640x480). RUNNER2.PRG already sits near the PRG ceiling, so these
 * live in their own PRG, exactly as the cc65 suite grew runner3.c.
 *
 * Same discipline as the other two runners: drive the library one way,
 * verify through an INDEPENDENT path. Writes go through a library call;
 * fixtures are poisoned with t_vpoke() (testlib.h -- the SDK's vpoke()
 * is broken) and read back with the SDK's own vpeek().
 *
 * The low-res engines draw into stock VRAM, so every effect is
 * vpeek-checkable; the VERA_2 pair only runs where the hardware answers
 * x16_gfx*h_has() -- on the emulator it never does, so those skip.
 *
 * 4bpp: a pixel byte is at y*160 + (x>>1), left pixel high nibble.
 * 2bpp: a pixel byte is at y*80 + (x>>2), MSB-first pairs of bits.
 * ===================================================================== */

#include "testlib.h"
#include <cx16.h>
#include <x16/x16.h>

#define P4L(x, y)   ((unsigned long)(y) * 160 + ((x) >> 1))
#define P2L(x, y)   ((unsigned long)(y) * 80 + ((x) >> 2))
#define L0_CONFIG   (*(volatile unsigned char *)0x9F2DU)
#define L0_TILEBASE (*(volatile unsigned char *)0x9F2FU)

static void test_g4l_init(void)
{
    x16_gfx4l_init();
    /* The TILEBASE value mirrors upstream bitmap4l exactly. */
    t_check(L0_CONFIG == 0x06 &&        /* bitmap | 4bpp */
            L0_TILEBASE == 0x01,
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
    t_check(vpeek(P4L(0, 0)) == 0x55 &&
            vpeek(P4L(318, 239)) == 0x55,
            "G4L_CLEAR");
}

static void test_g4l_pset(void)
{
    t_vpoke(0x00, P4L(10, 7));
    x16_gfx4l_pset(11, 7, 9);           /* odd x: low nibble */
    x16_gfx4l_pset(10, 7, 3);           /* even x: high nibble */
    t_check(vpeek(P4L(10, 7)) == 0x39, "G4L_PSET");
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
    t_check(vpeek(P4L(0, 1)) == 0x11 && vpeek(38400UL) == 0x22,
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
    t_check(vpeek(P4L(0, 40) + 0) == 0x00 &&
            vpeek(P4L(0, 40) + 1) == 0x04 &&
            vpeek(P4L(0, 40) + 2) == 0x44 &&
            vpeek(P4L(0, 40) + 3) == 0x44 &&
            vpeek(P4L(0, 40) + 4) == 0x40 &&
            vpeek(P4L(0, 40) + 5) == 0x00,
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
    t_check(vpeek(50UL * 160 + 3) == 0x7C &&
            vpeek(51UL * 160 + 3) == 0x7C &&
            vpeek(52UL * 160 + 3) == 0x7C &&
            vpeek(53UL * 160 + 3) == 0x0C,      /* len 3 stops here */
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
    t_check(vpeek(P4L(4, 60)) == 0x66 && vpeek(P4L(6, 60)) == 0x66 &&
            vpeek(P4L(4, 61)) == 0x66 && vpeek(P4L(6, 61)) == 0x66,
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
    t_check(vpeek(P4L(4, 70)) == 0x22 &&
            vpeek(P4L(6, 70)) == 0x22 &&
            vpeek(P4L(8, 70)) == 0x22 &&
            vpeek(P4L(4, 71)) == 0x20 &&
            vpeek(P4L(6, 71)) == 0x00 &&
            vpeek(P4L(8, 71)) == 0x02 &&
            vpeek(P4L(4, 72)) == 0x22 &&
            vpeek(P4L(8, 72)) == 0x22,
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
    t_check(vpeek(P4L(0, 100) + 0) == 0x55 &&
            vpeek(P4L(0, 100) + 1) == 0x55 &&
            vpeek(P4L(0, 100) + 2) == 0x11 &&
            vpeek(P4L(0, 100) + 3) == 0x11,
            "G4L_PATTERN");
}

/* Copy, then XOR with itself: the second pass must erase the first. */
static void test_g4l_blit(void)
{
    static const unsigned char img[2] = { 0xAB, 0xCD };
    t_vpoke(0x00, P4L(6, 110));
    t_vpoke(0x00, P4L(8, 110));
    x16_gfx4l_blit(6, 110, 4, 1, img, 0);
    t_check(vpeek(P4L(6, 110)) == 0xAB && vpeek(P4L(8, 110)) == 0xCD,
            "G4L_BLIT");
    x16_gfx4l_blit(6, 110, 4, 1, img, 3);
    t_check(vpeek(P4L(6, 110)) == 0x00 && vpeek(P4L(8, 110)) == 0x00,
            "G4L_BLIT_XOR");
}

/* Colour 0 pixels leave the framebuffer nibble alone. */
static void test_g4l_blitm(void)
{
    static const unsigned char img[2] = { 0xA0, 0x0D };
    t_vpoke(0x99, P4L(6, 111));
    t_vpoke(0x99, P4L(8, 111));
    x16_gfx4l_blitm(6, 111, 4, 1, img);
    t_check(vpeek(P4L(6, 111)) == 0xA9 && vpeek(P4L(8, 111)) == 0x9D,
            "G4L_BLITM");
}

static void test_g2l_init(void)
{
    x16_gfx2l_init();
    t_check(L0_CONFIG == 0x05 &&        /* bitmap | 2bpp */
            L0_TILEBASE == 0x00,        /* base $00000, 320 wide */
            "G2L_INIT");
}

static void test_g2l_pset(void)
{
    t_vpoke(0x00, P2L(5, 10));
    x16_gfx2l_pset(5, 10, 2);           /* byte 1, pixel 1 */
    t_check(vpeek(10UL * 80 + 1) == 0x20, "G2L_PSET");
}

/* Unclipped, (320,0) would land on byte 80 and (0,240) at 19,200. */
static void test_g2l_clip(void)
{
    t_vpoke(0x11, 80UL);
    t_vpoke(0x22, 19200UL);
    x16_gfx2l_pset(320, 0, 3);
    x16_gfx2l_pset(0, 240, 3);
    t_check(vpeek(80UL) == 0x11 && vpeek(19200UL) == 0x22, "G2L_CLIP");
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
    t_check(vpeek(20UL * 80 + 0) == 0x00 &&
            vpeek(20UL * 80 + 1) == 0x3F &&
            vpeek(20UL * 80 + 2) == 0xFF &&
            vpeek(20UL * 80 + 3) == 0xFF &&
            vpeek(20UL * 80 + 4) == 0xF0 &&
            vpeek(20UL * 80 + 5) == 0x00,
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
    t_check(vpeek(30UL * 80 + 1) == 0xF3 &&
            vpeek(33UL * 80 + 1) == 0xF3 &&
            vpeek(34UL * 80 + 1) == 0xFF,       /* len 4 stops here */
            "G2L_VLINE");
}

static void test_g2l_rect(void)
{
    t_vpoke(0x00, 100UL * 80 + 2);
    t_vpoke(0x00, 100UL * 80 + 3);
    t_vpoke(0x00, 101UL * 80 + 2);
    t_vpoke(0x00, 101UL * 80 + 3);
    x16_gfx2l_rect(8, 100, 8, 2, 1);
    t_check(vpeek(100UL * 80 + 2) == 0x55 &&
            vpeek(100UL * 80 + 3) == 0x55 &&
            vpeek(101UL * 80 + 2) == 0x55 &&
            vpeek(101UL * 80 + 3) == 0x55,
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
    t_check(vpeek(110UL * 80 + 3) == 0xAA &&
            vpeek(110UL * 80 + 4) == 0xAA &&
            vpeek(111UL * 80 + 3) == 0x80 &&
            vpeek(111UL * 80 + 4) == 0x02 &&
            vpeek(112UL * 80 + 3) == 0xAA &&
            vpeek(112UL * 80 + 4) == 0xAA,
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
    t_check(vpeek(120UL * 80) == 0xFF && vpeek(120UL * 80 + 1) == 0xFF,
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
    t_check(vpeek(140UL * 80) == 0xFF && vpeek(140UL * 80 + 1) == 0x55,
            "G2L_PATTERN");
}

/* Copy, then XOR with itself: the second pass must erase the first. */
static void test_g2l_blit(void)
{
    static const unsigned char img[2] = { 0xE4, 0x1B };
    t_vpoke(0x00, 150UL * 80 + 2);
    t_vpoke(0x00, 150UL * 80 + 3);
    x16_gfx2l_blit(8, 150, 2, 1, img, 0);
    t_check(vpeek(150UL * 80 + 2) == 0xE4 &&
            vpeek(150UL * 80 + 3) == 0x1B,
            "G2L_BLIT");
    x16_gfx2l_blit(8, 150, 2, 1, img, 3);
    t_check(vpeek(150UL * 80 + 2) == 0x00 &&
            vpeek(150UL * 80 + 3) == 0x00,
            "G2L_BLIT_XOR");
}

/* fb' = (fb AND mask) OR data, one column, one row. */
static void test_g2l_blitm(void)
{
    static const unsigned char img[2] = { 0x3F, 0x40 };
    t_vpoke(0xFF, 160UL * 80);
    x16_gfx2l_blitm(0, 160, 1, 1, img);
    t_check(vpeek(160UL * 80) == 0x7F, "G2L_BLITM");
}

/* 19,200 bytes via the FX cache write; check both ends. */
static void test_g2l_clear(void)
{
    t_vpoke(0x00, 0UL);
    t_vpoke(0x00, 19199UL);
    x16_gfx2l_clear(2);
    t_check(vpeek(0UL) == 0xAA && vpeek(19199UL) == 0xAA, "G2L_CLEAR");
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

    t_done();
    return 0;
}
