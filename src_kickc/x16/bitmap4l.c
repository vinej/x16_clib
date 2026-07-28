// =====================================================================
// x16clib :: x16/bitmap4l.c -- 320x240x16 bitmap drawing (4bpp)
// =====================================================================
// The framebuffer is 4bpp at VRAM $00000: 2 pixels per byte packed
// MSB-first, rows of 160 bytes, 38,400 bytes in all. A pixel byte is
// at y*160 + (x>>1); the left pixel is the high nibble.
//
// x16_gfx4l_pset/read clip. The span/rect/line/blit primitives do NOT:
// they assume their arguments are on screen.
//
// HOW THIS PORT IS SPLIT. The upstream bitmap4l module routes every
// span, rect, blit and glyph through its clipped pset -- there is no
// byte-streaming fast path to preserve -- so this port is C mirroring
// that control flow, with the nibble read-modify-write and the port
// aim as the only asm.
// =====================================================================

#include <x16/bitmap4l.h>
#include <x16/vera.h>
#include <x16/palette.h>

// The operand block (the ca65 build's X16_P0..P7 and g4l_* variables).
__mem volatile unsigned int x16__g4l_x;
__mem volatile unsigned char x16__g4l_y;
__mem volatile unsigned int x16__g4l_a;  // the byte address (fits 16 bits)
__mem volatile char x16__g4l_c;          // colour 0-15
__mem volatile char x16__g4l_off;        // pset/read's clip verdict
__mem volatile char x16__g4l_t;

// Pattern state: the RAW 8x8 rows plus the two colours; the rotation
// happens per row at draw time (the upstream module's way).
__mem volatile char x16__g4l_pat[8];
__mem volatile char x16__g4l_pfg;
__mem volatile char x16__g4l_pbg;

// Blit source, pinned in the shared $78 pointer slot (see x16/zpsafe.h).
__address(0x78) const char* volatile x16__g4l_src;

// Glyph cache for char/text.
__mem volatile char x16__g4l_glyph[8];

const char x16__g4l_colbyte[16] = {      // a colour in both nibbles
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

const char x16__g4l_defpal[32] = {       // the upstream default palette
    0xFF, 0x0F, 0xAA, 0x0A, 0x55, 0x05, 0x00, 0x00,
    0xF0, 0x00, 0x0F, 0x00, 0xF8, 0x08, 0x88, 0x00,
    0x8F, 0x00, 0x0F, 0x0F, 0xF0, 0x0F, 0xFF, 0x00,
    0x0F, 0x0F, 0xF0, 0x00, 0x99, 0x09, 0x66, 0x06
};

// ---------------------------------------------------------------------
// Internal: point data port 0 at byte y*160 + (x>>1), increment INDEX
// pre-shifted. 38,399 max: the bank bit is always 0.
// ---------------------------------------------------------------------
void x16__gfx4l_aim0(__mem unsigned char incr) {
    x16__g4l_a = (unsigned int)x16__g4l_y * 160 + (x16__g4l_x >> 1);
    asm {
        lda incr
        asl
        asl
        asl
        asl
        sta x16__g4l_t
        lda #1 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/
        lda x16__g4l_a
        sta $9f20 /*VERA_ADDR_L*/
        lda x16__g4l_a+1
        sta $9f21 /*VERA_ADDR_M*/
        lda x16__g4l_t
        sta $9f22 /*VERA_ADDR_H*/
    }
}

// ---------------------------------------------------------------------
// Internal: is (g4l_x, g4l_y) on screen? Sets g4l_off. y is a byte, so
// only y >= 240 can be off.
// ---------------------------------------------------------------------
void x16__gfx4l_onscreen(void) {
    x16__g4l_off = 0;
    if (x16__g4l_x >= X16_GFX4L_WIDTH) {
        x16__g4l_off = 1;
    }
    if (x16__g4l_y >= X16_GFX4L_HEIGHT) {
        x16__g4l_off = 1;
    }
}

// ---------------------------------------------------------------------
// Internal: set pixel (g4l_x, g4l_y) to g4l_c, clipped. One INC_0 aim
// serves the read and the write halves of the nibble RMW.
// ---------------------------------------------------------------------
void x16__gfx4l_pset_i(void) {
    x16__gfx4l_onscreen();
    if (x16__g4l_off) {
        return;
    }
    x16__gfx4l_aim0(X16_INC_0);
    x16__g4l_c = x16__g4l_c & 0x0F;
    if ((unsigned char)(x16__g4l_x & 1) != 0) {
        asm {
            lda $9f23 /*VERA_DATA0*/
            and #$f0
            ora x16__g4l_c
            sta $9f23 /*VERA_DATA0*/
        }
    } else {
        asm {
            lda x16__g4l_c
            asl
            asl
            asl
            asl
            sta x16__g4l_t
            lda $9f23 /*VERA_DATA0*/
            and #$0f
            ora x16__g4l_t
            sta $9f23 /*VERA_DATA0*/
        }
    }
}

// ---------------------------------------------------------------------
// Public: program the mode on bare VERA registers.
// ---------------------------------------------------------------------
void x16_gfx4l_init(void) {
    unsigned char i;

    asm {
        lda $9f25 /*VERA_CTRL*/         // DCSEL = 0, keep ADDRSEL
        and #1 /*VERA_CTRL_ADDRSEL*/
        sta $9f25 /*VERA_CTRL*/
        lda #$40                        // 64 = two output pixels per input,
        sta $9f2a /*VERA_DC_HSCALE*/    // so 320x240 fills the 640x480 display
        sta $9f2b /*VERA_DC_VSCALE*/
        stz $9f2c /*VERA_DC_BORDER*/

        lda #6 /*BITMAP|BPP_4*/
        sta $9f2d /*VERA_L0_CONFIG*/
        lda #0                          // base $00000, 320 pixels wide
        sta $9f2f /*VERA_L0_TILEBASE*/
        stz $9f30 /*VERA_L0_HSCROLL_L*/
        stz $9f31 /*VERA_L0_HSCROLL_H*/
        stz $9f32 /*VERA_L0_VSCROLL_L*/
        stz $9f33 /*VERA_L0_VSCROLL_H*/
    }

    for (i = 0; i < 16; i++) {
        x16_pal_set(i, (unsigned int)x16__g4l_defpal[i * 2] |
                       ((unsigned int)x16__g4l_defpal[i * 2 + 1] << 8));
    }

    asm {
        lda #$20 /*VIDEO_LAYER1_EN*/    // layer 1 off, layer 0 on
        trb $9f29 /*VERA_DC_VIDEO*/
        lda #$10 /*VIDEO_LAYER0_EN*/
        tsb $9f29 /*VERA_DC_VIDEO*/
    }
}

// ---------------------------------------------------------------------
// Public: fill the whole framebuffer with one colour. 38,400 bytes
// fits one 16-bit vera_fill count.
// ---------------------------------------------------------------------
void x16_gfx4l_clear(unsigned char color) {
    x16__g4l_x = 0;
    x16__g4l_y = 0;
    x16__gfx4l_aim0(X16_INC_1);
    x16_vera_fill(x16__g4l_colbyte[color & 0x0F], 38400);
}

// ---------------------------------------------------------------------
// Public: point data port 0 at the byte holding (x,y).
// ---------------------------------------------------------------------
void x16_gfx4l_setptr(unsigned char inc, unsigned int x, unsigned char y) {
    x16__g4l_x = x;
    x16__g4l_y = y;
    x16__gfx4l_aim0(inc);
}

// ---------------------------------------------------------------------
// Public: one pixel, clipped.
// ---------------------------------------------------------------------
void x16_gfx4l_pset(unsigned int x, unsigned char y, unsigned char color) {
    x16__g4l_x = x;
    x16__g4l_y = y;
    x16__g4l_c = color;
    x16__gfx4l_pset_i();
}

// ---------------------------------------------------------------------
// Public: one pixel back, or $FF off screen.
// ---------------------------------------------------------------------
unsigned char x16_gfx4l_read(unsigned int x, unsigned char y) {
    unsigned char b;

    x16__g4l_x = x;
    x16__g4l_y = y;
    x16__gfx4l_onscreen();
    if (x16__g4l_off) {
        return 0xFF;
    }
    x16__gfx4l_aim0(X16_INC_0);
    asm {
        lda $9f23 /*VERA_DATA0*/
        sta x16__g4l_t
    }
    b = (unsigned char)x16__g4l_t;
    if ((unsigned char)(x & 1) != 0) {
        return b & 0x0F;
    }
    return b >> 4;
}

// ---------------------------------------------------------------------
// Public: spans. The upstream module walks these through its clipped
// pset -- mirrored here, quirks and all.
// ---------------------------------------------------------------------
void x16_gfx4l_hline(unsigned int x, unsigned char y, unsigned int len,
                    unsigned char color) {
    unsigned int i;

    x16__g4l_c = color;
    for (i = 0; i < len; i++) {
        x16__g4l_x = x + i;
        x16__g4l_y = y;
        x16__gfx4l_pset_i();
    }
}

void x16_gfx4l_vline(unsigned int x, unsigned char y, unsigned char len,
                    unsigned char color) {
    unsigned char i;

    x16__g4l_c = color;
    for (i = 0; i < len; i++) {
        x16__g4l_x = x;
        x16__g4l_y = (unsigned char)(y + i);
        x16__gfx4l_pset_i();
    }
}

void x16_gfx4l_rect(unsigned int x, unsigned char y, unsigned int w,
                   unsigned char h, unsigned char color) {
    unsigned char i;

    for (i = 0; i < h; i++) {
        x16_gfx4l_hline(x, (unsigned char)(y + i), w, color);
    }
}

void x16_gfx4l_frame(unsigned int x, unsigned char y, unsigned int w,
                    unsigned char h, unsigned char color) {
    x16_gfx4l_hline(x, y, w, color);
    x16_gfx4l_hline(x, (unsigned char)(y + h - 1), w, color);
    x16_gfx4l_vline(x, y, h, color);
    x16_gfx4l_vline(x + w - 1, y, h, color);
}

// ---------------------------------------------------------------------
// Public: Bresenham, any direction; plots through the clipped pset.
// ---------------------------------------------------------------------
void x16_gfx4l_line(unsigned int x0, unsigned char y0, unsigned int x1,
                   unsigned char y1, unsigned char color) {
    int lx0;
    int ly0;
    int lx1;
    int ly1;
    int dx;
    int dy;
    int sx;
    int sy;
    int err;
    int e2;

    lx0 = (int)x0;
    ly0 = (int)y0;
    lx1 = (int)x1;
    ly1 = (int)y1;

    // Subtractions rather than `d = 0 - d`: see x16/bitmap2h.c -- KickC
    // constant-folds a literal call and drops the negation otherwise.
    dx = lx1 - lx0;
    if (dx < 0) {
        dx = lx0 - lx1;                 // dx = |dx|
        sx = -1;
    } else {
        sx = 1;
    }
    dy = ly1 - ly0;
    if (dy < 0) {
        sy = -1;
    } else {
        dy = ly0 - ly1;                 // dy = -|dy|
        sy = 1;
    }
    err = dx + dy;

    x16__g4l_c = color;
    for (;;) {
        x16__g4l_x = (unsigned int)lx0;
        x16__g4l_y = (unsigned char)ly0;
        x16__gfx4l_pset_i();

        if (lx0 == lx1 && ly0 == ly1) {
            break;
        }
        e2 = err << 1;
        if (e2 >= dy) {
            err = err + dy;
            lx0 = lx0 + sx;
        }
        if (e2 <= dx) {
            err = err + dx;
            ly0 = ly0 + sy;
        }
    }
}

// ---------------------------------------------------------------------
// Public: one 8x8 glyph from the VERA charset. code is a SCREEN code.
// Set bits plot in `color`; clear bits are left transparent.
// ---------------------------------------------------------------------
void x16_gfx4l_char(unsigned int x, unsigned char y, unsigned char color,
                   unsigned char code) {
    unsigned char grow, gcol, bits;

    // The charset lives at VRAM $1F000: fetch the glyph on port 1 so
    // the drawing port is undisturbed.
    x16_vera_addr1(X16_INC_1,
                   0x1F000UL + ((unsigned int)code << 3));
    for (grow = 0; grow < 8; grow++) {
        asm {
            lda $9f24 /*VERA_DATA1*/
            sta x16__g4l_t
        }
        x16__g4l_glyph[grow] = x16__g4l_t;
    }

    x16__g4l_c = color;
    for (grow = 0; grow < 8; grow++) {
        bits = (unsigned char)x16__g4l_glyph[grow];
        if (bits == 0) {
            continue;
        }
        // y+row past 255 would wrap: the ca65 module skips those.
        if ((unsigned int)y + grow > 255) {
            continue;
        }
        for (gcol = 0; gcol < 8; gcol++) {
            if (bits & 0x80) {
                x16__g4l_x = x + gcol;
                x16__g4l_y = (unsigned char)(y + grow);
                x16__gfx4l_pset_i();
            }
            bits <<= 1;
        }
    }
}

// ---------------------------------------------------------------------
// Public: a NUL-terminated PETSCII string, 8 pixels per glyph.
// ---------------------------------------------------------------------
void x16_gfx4l_text(unsigned int x, unsigned char y, unsigned char color,
                   const char *s) {
    unsigned char ch;

    while (*s) {
        ch = (unsigned char)*s;
        // PETSCII letters to screen codes, the upstream module's rule.
        if (ch & 0x40) {
            ch = ch & 0x1F;
        }
        x16_gfx4l_char(x, y, color, ch);
        x = x + 8;
        s++;
    }
}

// ---------------------------------------------------------------------
// Public: cache an 8x8 1bpp pattern; rotation happens per drawn row.
// ---------------------------------------------------------------------
void x16_gfx4l_pattern_set(const unsigned char *pattern, unsigned char bg,
                          unsigned char fg) {
    unsigned char i;

    for (i = 0; i < 8; i++) {
        x16__g4l_pat[i] = pattern[i];
    }
    x16__g4l_pbg = bg & 0x0F;
    x16__g4l_pfg = fg & 0x0F;
}

// ---------------------------------------------------------------------
// Public: fill a rectangle with the cached pattern, anchored to the
// screen origin: the row byte is pat[y&7] rotated left by x&7.
// ---------------------------------------------------------------------
void x16_gfx4l_pattern_rect(unsigned int x, unsigned char y, unsigned int w,
                           unsigned char h) {
    unsigned char rot, cur, i;
    unsigned int j;

    if (w == 0 || h == 0) {
        return;
    }
    rot = (unsigned char)(x & 7);
    for (i = 0; i < h; i++) {
        cur = (unsigned char)x16__g4l_pat[(unsigned char)((y + i) & 7)];
        if (rot != 0) {
            unsigned char r;
            for (r = 0; r < rot; r++) {
                cur = (unsigned char)((cur << 1) | (cur >> 7));
            }
        }
        for (j = 0; j < w; j++) {
            if (cur & 0x80) {
                x16__g4l_c = x16__g4l_pfg;
            } else {
                x16__g4l_c = x16__g4l_pbg;
            }
            x16__g4l_x = x + j;
            x16__g4l_y = (unsigned char)(y + i);
            x16__gfx4l_pset_i();
            cur = (unsigned char)((cur << 1) | (cur >> 7));
        }
    }
}

// ---------------------------------------------------------------------
// Public: rows of pixels from RAM with a raster op. w is in PIXELS
// (1-255); the source row is (w+1)/2 bytes, high nibble first.
// ---------------------------------------------------------------------
void x16_gfx4l_blit(unsigned int x, unsigned char y, unsigned char w,
                   unsigned char h, const unsigned char *src,
                   unsigned char op) {
    unsigned char brow, i, ink, fb, rowbytes;

    if (w == 0) {
        return;
    }
    op = op & 3;
    rowbytes = (unsigned char)((w + 1) >> 1);
    x16__g4l_src = src;

    for (brow = 0; brow < h; brow++) {
        for (i = 0; i < w; i++) {
            unsigned char bidx = i >> 1;
            ink = (unsigned char)x16__g4l_src[bidx];
            if ((i & 1) != 0) {
                ink = ink & 0x0F;
            } else {
                ink = ink >> 4;
            }
            if (op != 0) {
                fb = x16_gfx4l_read(x + i, (unsigned char)(y + brow));
                if (op == 1) {
                    ink = ink | fb;
                } else if (op == 2) {
                    ink = ink & fb;
                } else {
                    ink = ink ^ fb;
                }
            }
            x16__g4l_x = x + i;
            x16__g4l_y = (unsigned char)(y + brow);
            x16__g4l_c = ink;
            x16__gfx4l_pset_i();
        }
        x16__g4l_src = x16__g4l_src + rowbytes;
    }
}

// ---------------------------------------------------------------------
// Public: masked blit -- colour 0 is transparent. Same layout as blit.
// ---------------------------------------------------------------------
void x16_gfx4l_blitm(unsigned int x, unsigned char y, unsigned char w,
                    unsigned char h, const unsigned char *src) {
    unsigned char brow, i, ink, rowbytes;

    if (w == 0) {
        return;
    }
    rowbytes = (unsigned char)((w + 1) >> 1);
    x16__g4l_src = src;

    for (brow = 0; brow < h; brow++) {
        for (i = 0; i < w; i++) {
            unsigned char bidx = i >> 1;
            ink = (unsigned char)x16__g4l_src[bidx];
            if ((i & 1) != 0) {
                ink = ink & 0x0F;
            } else {
                ink = ink >> 4;
            }
            if (ink != 0) {
                x16__g4l_x = x + i;
                x16__g4l_y = (unsigned char)(y + brow);
                x16__g4l_c = ink;
                x16__gfx4l_pset_i();
            }
        }
        x16__g4l_src = x16__g4l_src + rowbytes;
    }
}
