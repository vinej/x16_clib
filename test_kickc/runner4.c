/* =====================================================================
 * x16clib :: test_kickc/runner4.c -- the 640x480@2bpp half
 * =====================================================================
 * The fourth PRG of the suite: x16/bitmap2.c. Split from the others for
 * the same zero-page reason runner2.c documents, and because
 * x16_gfx2_init() reprograms the display and palette entries 0-3 --
 * nothing else wants that done to it mid-run.
 *
 * A 2bpp framebuffer byte sits at y*160 + (x>>2), 4 pixels per byte
 * MSB-first. The screen is 76,800 bytes, so it crosses the 64K VRAM
 * boundary: addresses from row 410 up need bank 1, which is why the
 * probes below carry a bank argument.
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

/* Row y starts at y*160. Rows 0-409 are bank 0. */
#define ROW(y)  ((unsigned int)(y) * 160)

const unsigned char pat_half[8] = {
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0
};
const unsigned char img[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
const unsigned char mcol[8] = {         /* (mask,data) x 4 rows */
    0x0F, 0x50, 0x0F, 0x50, 0x0F, 0x50, 0x0F, 0x50
};

void test_g2_init(void) {
    x16_gfx2_init();
    t_check((*((char *)0x9f2d) == 0x05 &&        /* L0_CONFIG: bitmap|2bpp */
            *((char *)0x9f2f) == 0x01) ? 1 : 0,
            /* L0_TILEBASE: $00000/640 */
            "G2_INIT");
}

void test_g2_pset(void) {
    t_vpoke(0, ROW(10) + 1, 0x00);
    x16_gfx2_pset(5, 10, 2);                    /* byte 1, pixel 1 */
    t_check((t_vpeek(0, ROW(10) + 1) == 0x20) ? 1 : 0,
            "G2_PSET");
}

/* Unclipped, (640,0) would land at byte 160 and (0,480) at 76,800. */
void test_g2_clip(void) {
    t_vpoke(0, 160, 0x11);
    t_vpoke(1, 0x2C00, 0x22);                   /* 76800 = $12C00 */
    x16_gfx2_pset(640, 0, 3);
    x16_gfx2_pset(0, 480, 3);
    t_check((t_vpeek(0, 160) == 0x11 &&
            t_vpeek(1, 0x2C00) == 0x22) ? 1 : 0,
            "G2_CLIP");
}

void test_g2_read(void) {
    t_vpoke(0, ROW(12), 0x1B);                  /* pixels 0,1,2,3 */
    t_check((x16_gfx2_read(0, 12) == 0 &&
            x16_gfx2_read(1, 12) == 1 &&
            x16_gfx2_read(2, 12) == 2 &&
            x16_gfx2_read(3, 12) == 3 &&
            x16_gfx2_read(640, 12) == 0xFF) ? 1 : 0,
            "G2_READ");
}

/* x=5 len=13: head = byte 1 pixels 1-3, middle bytes 2-3, tail = byte 4
 * pixels 0-1. The bytes either side must survive.
 */
void test_g2_hline(void) {
    unsigned char i;
    for (i = 0; i < 6; i++) {
        t_vpoke(0, ROW(20) + i, 0x00);
    }
    x16_gfx2_hline(5, 20, 13, 3);
    t_check((t_vpeek(0, ROW(20) + 0) == 0x00 &&
            t_vpeek(0, ROW(20) + 1) == 0x3F &&
            t_vpeek(0, ROW(20) + 2) == 0xFF &&
            t_vpeek(0, ROW(20) + 3) == 0xFF &&
            t_vpeek(0, ROW(20) + 4) == 0xF0 &&
            t_vpeek(0, ROW(20) + 5) == 0x00) ? 1 : 0,
            "G2_HLINE");
}

/* A span that begins and ends inside one byte. */
void test_g2_hline_short(void) {
    t_vpoke(0, ROW(21), 0x00);
    t_vpoke(0, ROW(21) + 1, 0x00);
    x16_gfx2_hline(1, 21, 2, 2);
    t_check((t_vpeek(0, ROW(21)) == 0x28 &&      /* pixels 1,2 only */
            t_vpeek(0, ROW(21) + 1) == 0x00) ? 1 : 0,
            "G2_HLINE_SHORT");
}

/* Colour 0 ink onto $FF: proves the column really is read-modify-write. */
void test_g2_vline(void) {
    unsigned char i;
    for (i = 30; i <= 34; i++) {
        t_vpoke(0, ROW(i) + 1, 0xFF);
    }
    x16_gfx2_vline(6, 30, 4, 0);                /* byte 1, pixel 2 */
    t_check((t_vpeek(0, ROW(30) + 1) == 0xF3 &&
            t_vpeek(0, ROW(33) + 1) == 0xF3 &&
            t_vpeek(0, ROW(34) + 1) == 0xFF) ? 1 : 0,
            /* one past the end */
            "G2_VLINE");
}

void test_g2_rect(void) {
    unsigned char i;
    for (i = 0; i < 4; i++) {
        t_vpoke(0, ROW(40) + i, 0x00);
        t_vpoke(0, ROW(41) + i, 0x00);
        t_vpoke(0, ROW(42) + i, 0x00);
    }
    x16_gfx2_rect(4, 40, 8, 2, 1);
    t_check((t_vpeek(0, ROW(40) + 0) == 0x00 &&
            t_vpeek(0, ROW(40) + 1) == 0x55 &&
            t_vpeek(0, ROW(40) + 2) == 0x55 &&
            t_vpeek(0, ROW(40) + 3) == 0x00 &&
            t_vpeek(0, ROW(41) + 1) == 0x55 &&
            t_vpeek(0, ROW(42) + 1) == 0x00) ? 1 : 0,
            "G2_RECT");
}

void test_g2_frame(void) {
    unsigned char i;
    for (i = 0; i < 4; i++) {
        t_vpoke(0, ROW(50) + i, 0x00);
        t_vpoke(0, ROW(51) + i, 0x00);
        t_vpoke(0, ROW(52) + i, 0x00);
    }
    x16_gfx2_frame(0, 50, 16, 3, 3);
    t_check((t_vpeek(0, ROW(50) + 0) == 0xFF &&  /* top edge */
            t_vpeek(0, ROW(50) + 3) == 0xFF &&
            t_vpeek(0, ROW(51) + 0) == 0xC0 &&  /* left edge only */
            t_vpeek(0, ROW(51) + 1) == 0x00 &&  /* hollow */
            t_vpeek(0, ROW(51) + 3) == 0x03 &&  /* right edge only */
            t_vpeek(0, ROW(52) + 2) == 0xFF) ? 1 : 0,
            /* bottom edge */
            "G2_FRAME");
}

/* The 45-degree diagonal (0,60)-(7,67): pixel (i, 60+i) for every i. */
void test_g2_line(void) {
    unsigned char i;
    for (i = 60; i <= 67; i++) {
        t_vpoke(0, ROW(i), 0x00);
        t_vpoke(0, ROW(i) + 1, 0x00);
    }
    x16_gfx2_line(0, 60, 7, 67, 3);
    t_check((t_vpeek(0, ROW(60)) == 0xC0 &&
            t_vpeek(0, ROW(63)) == 0x03 &&
            t_vpeek(0, ROW(64) + 1) == 0xC0 &&
            t_vpeek(0, ROW(67) + 1) == 0x03) ? 1 : 0,
            "G2_LINE");
}

/* Pattern $F0 (left half ink) with fg 3: even bytes $FF, odd bytes $00.
 * Patterns anchor to the screen, so the x=2 fill gets a phase-2 head and
 * the even byte again in its tail.
 */
void test_g2_pattern(void) {
    unsigned char i;
    for (i = 0; i < 5; i++) {
        t_vpoke(0, ROW(70) + i, 0x55);
    }
    for (i = 0; i < 4; i++) {
        t_vpoke(0, ROW(74) + i, 0x00);
    }
    x16_gfx2_pattern_set(pat_half, 0x03);       /* bg 0, fg 3 */
    x16_gfx2_pattern_rect(0, 70, 16, 1);
    x16_gfx2_pattern_rect(2, 74, 8, 1);
    t_check((t_vpeek(0, ROW(70) + 0) == 0xFF &&
            t_vpeek(0, ROW(70) + 1) == 0x00 &&
            t_vpeek(0, ROW(70) + 2) == 0xFF &&
            t_vpeek(0, ROW(70) + 3) == 0x00 &&
            t_vpeek(0, ROW(70) + 4) == 0x55 &&  /* untouched */
            t_vpeek(0, ROW(74) + 0) == 0x0F &&  /* phase-2 head */
            t_vpeek(0, ROW(74) + 1) == 0x00 &&
            t_vpeek(0, ROW(74) + 2) == 0xF0) ? 1 : 0,
            /* tail */
            "G2_PATTERN");
}

/* Lay the image down, then XOR the same image over it: back to zero. */
void test_g2_blit(void) {
    t_vpoke(0, ROW(80) + 2, 0x00);
    t_vpoke(0, ROW(80) + 3, 0x00);
    t_vpoke(0, ROW(81) + 2, 0x00);
    t_vpoke(0, ROW(81) + 3, 0x00);
    x16_gfx2_blit(8, 80, 2, 2, img, 0);         /* copy */
    if (t_vpeek(0, ROW(80) + 2) != 0xDE || t_vpeek(0, ROW(80) + 3) != 0xAD ||
        t_vpeek(0, ROW(81) + 2) != 0xBE || t_vpeek(0, ROW(81) + 3) != 0xEF) {
        t_check(0, "G2_BLIT");
        return;
    }
    x16_gfx2_blit(8, 80, 2, 2, img, 3);         /* xor */
    t_check((t_vpeek(0, ROW(80) + 2) == 0x00 &&
            t_vpeek(0, ROW(81) + 3) == 0x00) ? 1 : 0,
            "G2_BLIT");
}

/* Onto $FF: keep pixels 2-3 (mask $0F), ink pixels 0-1 with colour 1
 * (data $50) -> every touched byte reads $5F.
 */
void test_g2_blitm(void) {
    unsigned char i;
    for (i = 90; i <= 94; i++) {
        t_vpoke(0, ROW(i) + 3, 0xFF);
    }
    x16_gfx2_blitm(12, 90, 4, 1, mcol);
    t_check((t_vpeek(0, ROW(90) + 3) == 0x5F &&
            t_vpeek(0, ROW(93) + 3) == 0x5F &&
            t_vpeek(0, ROW(94) + 3) == 0xFF) ? 1 : 0,
            /* one past the end */
            "G2_BLITM");
}

/* Exactly the 76,800 framebuffer bytes and not one more. The last byte
 * is $12BFF and the sentinel $12C00 -- both in bank 1.
 */
void test_g2_clear(void) {
    t_vpoke(1, 0x2C00, 0x77);
    x16_gfx2_clear(2);
    t_check((t_vpeek(0, 0) == 0xAA &&
            t_vpeek(0, 38400) == 0xAA &&        /* the second fill half */
            t_vpeek(1, 0x2BFF) == 0xAA &&       /* the very last byte */
            t_vpeek(1, 0x2C00) == 0x77) ? 1 : 0,
            /* ...and the sentinel */
            "G2_CLEAR");
}

int main(void) {
    t_init();

    test_g2_init();
    test_g2_pset();
    test_g2_clip();
    test_g2_read();
    test_g2_hline();
    test_g2_hline_short();
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

    t_done();
    return 0;
}
