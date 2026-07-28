/* =====================================================================
 * x16clib :: test_kickc/runner5.c -- the four new bitmap engines
 * =====================================================================
 * The fifth PRG of the suite: bitmap4l (320x240@4bpp), bitmap2l
 * (320x240@2bpp) and the VERA_2 SDRAM pair bitmap4h/8h (640x480).
 * The low-res engines draw into stock VRAM below 64K, so every effect
 * is vpeek-checkable on bank 0; the VERA_2 pair only runs where the
 * hardware answers x16_gfx*h_has() -- never on the emulator.
 *
 * 4bpp: a pixel byte is at y*160 + (x>>1), left pixel high nibble.
 * 2bpp: a pixel byte is at y*80 + (x>>2), MSB-first pairs of bits.
 * =====================================================================
 */

#include "testlib.h"
#include <x16/x16.h>

/* The same independent VRAM path as runner.c: written by hand here so a
 * bug in the library cannot hide behind itself.
 */
void t_vsetaddr(unsigned char bank, unsigned int addr) {
    asm {
        lda #$01
        trb $9f25
        lda addr
        sta $9f20
        lda addr+1
        sta $9f21
        lda bank
        and #$01
        sta $9f22
    }
}

unsigned char t_vpeek(unsigned char bank, unsigned int addr) {
    char r;
    t_vsetaddr(bank, addr);
    asm { lda $9f23 sta r }
    return r;
}

void t_vpoke(unsigned char bank, unsigned int addr, unsigned char v) {
    t_vsetaddr(bank, addr);
    asm { lda v sta $9f23 }
}

#define ROW4(y)  ((unsigned int)(y) * 160)
#define ROW2(y)  ((unsigned int)(y) * 80)

const unsigned char pat_half[8] = {
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0
};
const unsigned char img4[2] = { 0xAB, 0xCD };
const unsigned char imgm4[2] = { 0xA0, 0x0D };
const unsigned char img2[2] = { 0xE4, 0x1B };
const unsigned char imgm2[2] = { 0x3F, 0x40 };

/* ------------------------------------------------------------------ */
/* bitmap4l                                                            */
/* ------------------------------------------------------------------ */

void test_g4l_init(void) {
    x16_gfx4l_init();
    /* 320-wide: TILEBASE bit 0 clear, and the scale doubles every
     * pixel so the framebuffer covers the whole 640x480 display.
     */
    t_check((*((char *)0x9f2d) == 0x06 &&        /* L0_CONFIG: bitmap|4bpp */
            *((char *)0x9f2a) == 0x40 && *((char *)0x9f2b) == 0x40 &&
            *((char *)0x9f2f) == 0x00) ? 1 : 0,
            "G4L_INIT");
}

void test_g4l_clear(void) {
    t_vpoke(0, 0, 0x00);
    t_vpoke(0, 38399U, 0x00);
    x16_gfx4l_clear(5);
    t_check((t_vpeek(0, 0) == 0x55 &&
            t_vpeek(0, 38399U) == 0x55) ? 1 : 0,
            "G4L_CLEAR");
}

void test_g4l_pset(void) {
    t_vpoke(0, ROW4(7) + 5, 0x00);
    x16_gfx4l_pset(11, 7, 9);                   /* odd x: low nibble */
    x16_gfx4l_pset(10, 7, 3);                   /* even x: high nibble */
    t_check((t_vpeek(0, ROW4(7) + 5) == 0x39) ? 1 : 0, "G4L_PSET");
}

/* Unclipped, (320,0) would land on byte 160 (row 1) and (0,240) at
 * offset 38,400.
 */
void test_g4l_clip(void) {
    t_vpoke(0, ROW4(1), 0x11);
    t_vpoke(0, 38400U, 0x22);
    x16_gfx4l_pset(320, 0, 15);
    x16_gfx4l_pset(0, 240, 15);
    t_check((t_vpeek(0, ROW4(1)) == 0x11 &&
            t_vpeek(0, 38400U) == 0x22) ? 1 : 0,
            "G4L_CLIP");
}

void test_g4l_read(void) {
    t_vpoke(0, ROW4(12) + 10, 0x8E);
    t_check((x16_gfx4l_read(20, 12) == 8 &&
            x16_gfx4l_read(21, 12) == 14 &&
            x16_gfx4l_read(320, 12) == 0xFF) ? 1 : 0,
            "G4L_READ");
}

/* x=3 len=6: head = low nibble of byte 1, middle bytes 2-3, tail =
 * high nibble of byte 4.
 */
void test_g4l_hline(void) {
    unsigned char i;
    for (i = 0; i < 6; i++) {
        t_vpoke(0, ROW4(40) + i, 0x00);
    }
    x16_gfx4l_hline(3, 40, 6, 4);
    t_check((t_vpeek(0, ROW4(40) + 0) == 0x00 &&
            t_vpeek(0, ROW4(40) + 1) == 0x04 &&
            t_vpeek(0, ROW4(40) + 2) == 0x44 &&
            t_vpeek(0, ROW4(40) + 3) == 0x44 &&
            t_vpeek(0, ROW4(40) + 4) == 0x40 &&
            t_vpeek(0, ROW4(40) + 5) == 0x00) ? 1 : 0,
            "G4L_HLINE");
}

/* Colour 7 into $0C bytes: even x is the high nibble; the low nibble
 * must survive as read-modify-write.
 */
void test_g4l_vline(void) {
    unsigned char i;
    for (i = 50; i <= 53; i++) {
        t_vpoke(0, ROW4(i) + 3, 0x0C);
    }
    x16_gfx4l_vline(6, 50, 3, 7);
    t_check((t_vpeek(0, ROW4(50) + 3) == 0x7C &&
            t_vpeek(0, ROW4(51) + 3) == 0x7C &&
            t_vpeek(0, ROW4(52) + 3) == 0x7C &&
            t_vpeek(0, ROW4(53) + 3) == 0x0C) ? 1 : 0,
            "G4L_VLINE");
}

void test_g4l_rect(void) {
    unsigned char i;
    for (i = 0; i < 2; i++) {
        t_vpoke(0, ROW4(60 + i) + 2, 0x00);
        t_vpoke(0, ROW4(60 + i) + 3, 0x00);
    }
    x16_gfx4l_rect(4, 60, 4, 2, 6);
    t_check((t_vpeek(0, ROW4(60) + 2) == 0x66 &&
            t_vpeek(0, ROW4(60) + 3) == 0x66 &&
            t_vpeek(0, ROW4(61) + 2) == 0x66 &&
            t_vpeek(0, ROW4(61) + 3) == 0x66) ? 1 : 0,
            "G4L_RECT");
}

void test_g4l_frame(void) {
    unsigned char i, j;
    for (i = 0; i < 3; i++) {
        for (j = 2; j <= 4; j++) {
            t_vpoke(0, ROW4(70 + i) + j, 0x00);
        }
    }
    x16_gfx4l_frame(4, 70, 6, 3, 2);
    t_check((t_vpeek(0, ROW4(70) + 2) == 0x22 &&
            t_vpeek(0, ROW4(70) + 3) == 0x22 &&
            t_vpeek(0, ROW4(70) + 4) == 0x22 &&
            t_vpeek(0, ROW4(71) + 2) == 0x20 &&
            t_vpeek(0, ROW4(71) + 3) == 0x00 &&
            t_vpeek(0, ROW4(71) + 4) == 0x02 &&
            t_vpeek(0, ROW4(72) + 2) == 0x22 &&
            t_vpeek(0, ROW4(72) + 4) == 0x22) ? 1 : 0,
            "G4L_FRAME");
}

/* Asymmetric endpoints: swapped arguments could not light both ends. */
void test_g4l_line(void) {
    t_vpoke(0, ROW4(80) + 1, 0x00);
    t_vpoke(0, ROW4(84) + 5, 0x00);
    x16_gfx4l_line(3, 80, 11, 84, 5);
    t_check((x16_gfx4l_read(3, 80) == 5 &&
            x16_gfx4l_read(11, 84) == 5) ? 1 : 0,
            "G4L_LINE");
}

void test_g4l_char(void) {
    unsigned char i, j, set;

    x16_vera_addr0(X16_INC_1, 0x00000);
    x16_vera_fill(0x00, 8000);          /* rows 0-49, 4bpp */

    x16_gfx4l_char(40, 8, 5, 1);        /* screen code 1 = 'A' */

    set = 0;
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            if (x16_gfx4l_read(40 + j, (unsigned char)(8 + i)) == 5) {
                set++;
            }
        }
    }
    t_check((set > 4 && set < 64 &&
            x16_gfx4l_read(48, 8) == 0) ? 1 : 0,
            "G4L_CHAR");
}

void test_g4l_text(void) {
    unsigned char i, j, a, b;

    x16_vera_addr0(X16_INC_1, 0x00000);
    x16_vera_fill(0x00, 8000);

    x16_gfx4l_text(100, 8, 9, "AB");

    a = 0;
    b = 0;
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            if (x16_gfx4l_read(100 + j, (unsigned char)(8 + i)) == 9) {
                a++;
            }
            if (x16_gfx4l_read(108 + j, (unsigned char)(8 + i)) == 9) {
                b++;
            }
        }
    }
    t_check((a > 4 && b > 4 && a != b) ? 1 : 0, "G4L_TEXT");
}

/* Rows of $F0 tile from the origin: pixels 0-3 fg, 4-7 bg. */
void test_g4l_pattern(void) {
    unsigned char i;
    for (i = 0; i < 4; i++) {
        t_vpoke(0, ROW4(100) + i, 0xFF);
    }
    x16_gfx4l_pattern_set(pat_half, 1, 5);
    x16_gfx4l_pattern_rect(0, 100, 8, 1);
    t_check((t_vpeek(0, ROW4(100) + 0) == 0x55 &&
            t_vpeek(0, ROW4(100) + 1) == 0x55 &&
            t_vpeek(0, ROW4(100) + 2) == 0x11 &&
            t_vpeek(0, ROW4(100) + 3) == 0x11) ? 1 : 0,
            "G4L_PATTERN");
}

/* Copy, then XOR with itself: the second pass must erase the first. */
void test_g4l_blit(void) {
    t_vpoke(0, ROW4(110) + 3, 0x00);
    t_vpoke(0, ROW4(110) + 4, 0x00);
    x16_gfx4l_blit(6, 110, 4, 1, img4, 0);
    t_check((t_vpeek(0, ROW4(110) + 3) == 0xAB &&
            t_vpeek(0, ROW4(110) + 4) == 0xCD) ? 1 : 0,
            "G4L_BLIT");
    x16_gfx4l_blit(6, 110, 4, 1, img4, 3);
    t_check((t_vpeek(0, ROW4(110) + 3) == 0x00 &&
            t_vpeek(0, ROW4(110) + 4) == 0x00) ? 1 : 0,
            "G4L_BLIT_XOR");
}

/* Colour 0 pixels leave the framebuffer nibble alone. */
void test_g4l_blitm(void) {
    t_vpoke(0, ROW4(111) + 3, 0x99);
    t_vpoke(0, ROW4(111) + 4, 0x99);
    x16_gfx4l_blitm(6, 111, 4, 1, imgm4);
    t_check((t_vpeek(0, ROW4(111) + 3) == 0xA9 &&
            t_vpeek(0, ROW4(111) + 4) == 0x9D) ? 1 : 0,
            "G4L_BLITM");
}

/* ------------------------------------------------------------------ */
/* bitmap2l                                                            */
/* ------------------------------------------------------------------ */

void test_g2l_init(void) {
    x16_gfx2l_init();
    t_check((*((char *)0x9f2d) == 0x05 &&        /* L0_CONFIG: bitmap|2bpp */
            *((char *)0x9f2a) == 0x40 && *((char *)0x9f2b) == 0x40 &&
            *((char *)0x9f2f) == 0x00) ? 1 : 0,  /* base $00000, 320 wide */
            "G2L_INIT");
}

void test_g2l_pset(void) {
    t_vpoke(0, ROW2(10) + 1, 0x00);
    x16_gfx2l_pset(5, 10, 2);                    /* byte 1, pixel 1 */
    t_check((t_vpeek(0, ROW2(10) + 1) == 0x20) ? 1 : 0, "G2L_PSET");
}

/* Unclipped, (320,0) would land on byte 80 and (0,240) at 19,200. */
void test_g2l_clip(void) {
    t_vpoke(0, 80, 0x11);
    t_vpoke(0, 19200U, 0x22);
    x16_gfx2l_pset(320, 0, 3);
    x16_gfx2l_pset(0, 240, 3);
    t_check((t_vpeek(0, 80) == 0x11 &&
            t_vpeek(0, 19200U) == 0x22) ? 1 : 0,
            "G2L_CLIP");
}

void test_g2l_read(void) {
    t_vpoke(0, ROW2(12), 0x1B);                 /* pixels 0,1,2,3 */
    t_check((x16_gfx2l_read(0, 12) == 0 &&
            x16_gfx2l_read(1, 12) == 1 &&
            x16_gfx2l_read(2, 12) == 2 &&
            x16_gfx2l_read(3, 12) == 3 &&
            x16_gfx2l_read(320, 12) == 0xFF) ? 1 : 0,
            "G2L_READ");
}

/* x=5 len=13: head = byte 1 pixels 1-3, middle bytes 2-3, tail = byte 4
 * pixels 0-1.
 */
void test_g2l_hline(void) {
    unsigned char i;
    for (i = 0; i < 6; i++) {
        t_vpoke(0, ROW2(20) + i, 0x00);
    }
    x16_gfx2l_hline(5, 20, 13, 3);
    t_check((t_vpeek(0, ROW2(20) + 0) == 0x00 &&
            t_vpeek(0, ROW2(20) + 1) == 0x3F &&
            t_vpeek(0, ROW2(20) + 2) == 0xFF &&
            t_vpeek(0, ROW2(20) + 3) == 0xFF &&
            t_vpeek(0, ROW2(20) + 4) == 0xF0 &&
            t_vpeek(0, ROW2(20) + 5) == 0x00) ? 1 : 0,
            "G2L_HLINE");
}

/* Colour 0 ink onto $FF: proves the column really is read-modify-write. */
void test_g2l_vline(void) {
    unsigned char i;
    for (i = 30; i <= 34; i++) {
        t_vpoke(0, ROW2(i) + 1, 0xFF);
    }
    x16_gfx2l_vline(6, 30, 4, 0);               /* byte 1, pixel 2 */
    t_check((t_vpeek(0, ROW2(30) + 1) == 0xF3 &&
            t_vpeek(0, ROW2(33) + 1) == 0xF3 &&
            t_vpeek(0, ROW2(34) + 1) == 0xFF) ? 1 : 0,
            "G2L_VLINE");
}

void test_g2l_rect(void) {
    t_vpoke(0, ROW2(100) + 2, 0x00);
    t_vpoke(0, ROW2(100) + 3, 0x00);
    t_vpoke(0, ROW2(101) + 2, 0x00);
    t_vpoke(0, ROW2(101) + 3, 0x00);
    x16_gfx2l_rect(8, 100, 8, 2, 1);
    t_check((t_vpeek(0, ROW2(100) + 2) == 0x55 &&
            t_vpeek(0, ROW2(100) + 3) == 0x55 &&
            t_vpeek(0, ROW2(101) + 2) == 0x55 &&
            t_vpeek(0, ROW2(101) + 3) == 0x55) ? 1 : 0,
            "G2L_RECT");
}

void test_g2l_frame(void) {
    unsigned char i, j;
    for (i = 110; i <= 112; i++) {
        for (j = 3; j <= 4; j++) {
            t_vpoke(0, ROW2(i) + j, 0x00);
        }
    }
    x16_gfx2l_frame(12, 110, 8, 3, 2);
    t_check((t_vpeek(0, ROW2(110) + 3) == 0xAA &&
            t_vpeek(0, ROW2(110) + 4) == 0xAA &&
            t_vpeek(0, ROW2(111) + 3) == 0x80 &&
            t_vpeek(0, ROW2(111) + 4) == 0x02 &&
            t_vpeek(0, ROW2(112) + 3) == 0xAA &&
            t_vpeek(0, ROW2(112) + 4) == 0xAA) ? 1 : 0,
            "G2L_FRAME");
}

void test_g2l_line(void) {
    t_vpoke(0, ROW2(120), 0x00);
    t_vpoke(0, ROW2(120) + 1, 0x00);
    x16_gfx2l_line(0, 120, 7, 120, 3);
    t_check((t_vpeek(0, ROW2(120)) == 0xFF &&
            t_vpeek(0, ROW2(120) + 1) == 0xFF) ? 1 : 0,
            "G2L_LINE");

    t_vpoke(0, ROW2(130), 0x00);
    t_vpoke(0, ROW2(137) + 1, 0x00);
    x16_gfx2l_line(0, 130, 7, 137, 1);
    t_check((x16_gfx2l_read(0, 130) == 1 &&
            x16_gfx2l_read(7, 137) == 1) ? 1 : 0,
            "G2L_LINE_DIAG");
}

/* Rows of $F0 tile from the origin: pixels 0-3 fg (11), 4-7 bg (01). */
void test_g2l_pattern(void) {
    t_vpoke(0, ROW2(140), 0x00);
    t_vpoke(0, ROW2(140) + 1, 0x00);
    x16_gfx2l_pattern_set(pat_half, (1 << 2) | 3);
    x16_gfx2l_pattern_rect(0, 140, 8, 1);
    t_check((t_vpeek(0, ROW2(140)) == 0xFF &&
            t_vpeek(0, ROW2(140) + 1) == 0x55) ? 1 : 0,
            "G2L_PATTERN");
}

/* Copy, then XOR with itself: the second pass must erase the first. */
void test_g2l_blit(void) {
    t_vpoke(0, ROW2(150) + 2, 0x00);
    t_vpoke(0, ROW2(150) + 3, 0x00);
    x16_gfx2l_blit(8, 150, 2, 1, img2, 0);
    t_check((t_vpeek(0, ROW2(150) + 2) == 0xE4 &&
            t_vpeek(0, ROW2(150) + 3) == 0x1B) ? 1 : 0,
            "G2L_BLIT");
    x16_gfx2l_blit(8, 150, 2, 1, img2, 3);
    t_check((t_vpeek(0, ROW2(150) + 2) == 0x00 &&
            t_vpeek(0, ROW2(150) + 3) == 0x00) ? 1 : 0,
            "G2L_BLIT_XOR");
}

/* fb' = (fb AND mask) OR data, one column, one row. */
void test_g2l_blitm(void) {
    t_vpoke(0, ROW2(160), 0xFF);
    x16_gfx2l_blitm(0, 160, 1, 1, imgm2);
    t_check((t_vpeek(0, ROW2(160)) == 0x7F) ? 1 : 0, "G2L_BLITM");
}

/* 19,200 bytes via the FX cache write; check both ends. */
void test_g2l_clear(void) {
    t_vpoke(0, 0, 0x00);
    t_vpoke(0, 19199U, 0x00);
    x16_gfx2l_clear(2);
    t_check((t_vpeek(0, 0) == 0xAA &&
            t_vpeek(0, 19199U) == 0xAA) ? 1 : 0,
            "G2L_CLEAR");
}

/* ------------------------------------------------------------------ */
/* The VERA_2 SDRAM engines: only where the hardware answers.          */
/* ------------------------------------------------------------------ */

void test_g8h(void) {
    unsigned char has = x16_gfx8h_has();
    t_check((has <= 1) ? 1 : 0, "G8H_HAS_SANE");
    if (!has) {
        t_skip("G8H_ROUNDTRIP");
        t_skip("G8H_CLEAR");
        t_skip("G8H_COPY");
        return;
    }
    x16_gfx8h_init();
    x16_gfx8h_clear(0x11);
    t_check((x16_gfx8h_read(0, 0) == 0x11 &&
            x16_gfx8h_read(639, 479) == 0x11) ? 1 : 0,
            "G8H_CLEAR");
    x16_gfx8h_pset(3, 2, 0xAB);
    t_check((x16_gfx8h_read(3, 2) == 0xAB &&
            x16_gfx8h_read(640, 2) == 0xFFFF) ? 1 : 0,
            "G8H_ROUNDTRIP");
    x16_gfx8h_pset(1, 0, 0x5A);
    x16_gfx8h_copy(1, 5, 1);            /* offset (1,0) -> (5,0) */
    t_check((x16_gfx8h_read(5, 0) == 0x5A) ? 1 : 0, "G8H_COPY");
    x16_gfx8h_off();
}

/* ------------------------------------------------------------------ */
/* The FX affine sampler, and the raw FX register knobs                */
/* ------------------------------------------------------------------ */

#define TESTVRAM        0x4000U         /* bank 0: clear of the text map */

/* An 8x8 tile whose texel (x,y) holds y*16+x, behind a 2x2 map of tile
** 0: row 0 reads back 0,1,2..., column 0 reads back 0,16,32... The two
** rays are deliberately asymmetric -- a shim that swapped dx with dy
** would hand each ray the other one's walk. Tile data at $10000, map at
** $10800 (both 2 KB aligned, bank 1, clear of everything above).
*/
void test_affine(void) {
    unsigned char x, y, i, e, ok;

    i = 0;
    for (y = 0; y < 8; ++y) {           /* tile data, 2 KB aligned */
        for (x = 0; x < 8; ++x) {
            t_vpoke(1, i, (unsigned char)(y * 16 + x));
            ++i;
        }
    }
    for (i = 0; i < 4; ++i) {           /* a 2x2 map, all tile 0 */
        t_vpoke(1, 0x0800U + i, 0);
    }

    x16_fx_affine_on(0x10000UL, 0x10800UL, 0, 1);

    /* One texel per read along +x: the tile's row 0. */
    x16_fx_affine_ray(0, 0, 512, 0);
    x16_vera_addr0(X16_INC_1, 0x04000UL);
    x16_fx_affine_span(8);
    ok = 1;
    for (i = 0; i < 8; ++i) {
        if (t_vpeek(0, TESTVRAM + i) != i) ok = 0;
    }
    t_check(ok, "AFFINE_SPAN_ROW");

    /* The same walk along +y: column 0, so 0,16,32... */
    x16_fx_affine_ray(0, 0, 0, 512);
    x16_vera_addr0(X16_INC_1, 0x04010UL);
    x16_fx_affine_span(8);
    ok = 1;
    e = 0;
    for (i = 0; i < 8; ++i) {
        if (t_vpeek(0, TESTVRAM + 16 + i) != e) ok = 0;
        e += 16;
    }
    t_check(ok, "AFFINE_SPAN_COL");

    /* Affine mode deliberately stays on between rays; switching it off
    ** must hand port 1 -- and ordinary addressing -- back to everyone.
    */
    x16_fx_off();
    t_vpoke(0, TESTVRAM + 32, 0x77);
    t_check((t_vpeek(0, TESTVRAM + 32) == 0x77) ? 1 : 0,
            "AFFINE_OFF_CLEAN");
}

/* Set, OR in, read back, AND out: the ctrl knobs end to end. Bit 2 is
** the 4-bit flag -- harmless while nothing is being drawn.
*/
void test_fxu_ctrl(void) {
    unsigned char on, off;

    x16_fxu_set_ctrl(0);
    x16_fxu_ctrl_on(4);
    on = x16_fxu_get_ctrl();
    x16_fxu_ctrl_off(4);
    off = x16_fxu_get_ctrl();
    x16_fxu_off();

    t_check((on == 4 && off == 0) ? 1 : 0, "FXU_CTRL");
}

/* The poly-fill readback must return -- its value is only defined
** mid-polygon, but reading it idle may not hang or derail DCSEL.
*/
void test_fxu_poly(void) {
    unsigned int pf = x16_fxu_get_poly_fill();
    x16_fxu_off();
    t_vpoke(0, TESTVRAM + 33, 0x66);
    t_check((t_vpeek(0, TESTVRAM + 33) == 0x66 &&
            (pf | 1) != 0) ? 1 : 0,     /* consume pf; always true */
            "FXU_POLY");
}

void test_g4h(void) {
    unsigned char has = x16_gfx4h_has();
    t_check((has <= 1) ? 1 : 0, "G4H_HAS_SANE");
    if (!has) {
        t_skip("G4H_ROUNDTRIP");
        t_skip("G4H_CLEAR");
        return;
    }
    x16_gfx4h_init();
    x16_gfx4h_clear(0x3);
    t_check((x16_gfx4h_read(0, 0) == 3 &&
            x16_gfx4h_read(639, 479) == 3) ? 1 : 0,
            "G4H_CLEAR");
    x16_gfx4h_pset(3, 2, 0xA);
    t_check((x16_gfx4h_read(3, 2) == 0xA &&
            x16_gfx4h_read(2, 2) == 3 &&
            x16_gfx4h_read(640, 2) == 0xFF) ? 1 : 0,
            "G4H_ROUNDTRIP");
    x16_gfx4h_off();
}

int main(void) {
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

    t_done();
    return 0;
}
