// =====================================================================
// x16clib :: x16/bitmap8h.c -- VERA_2 640x480x256 SDRAM bitmap drawing
// =====================================================================
// Requires the MiSTer VERA_2 bitmap layer behind $9F60-$9F6F; the
// framebuffer is its 20-bit SDRAM byte space, NOT VERA VRAM. Feature-
// detect with x16_gfx8h_has() -- on stock hardware (and the emulator)
// every routine here writes into open bus.
//
// The framebuffer is 8bpp, one byte per pixel, rows of 640 bytes:
// offset = y*640 + x, 307,200 bytes in all.
//
// The VERA_2 layer is plain memory-mapped registers -- no data-port
// pairing tricks -- so this port is mostly C. The fill loops are asm;
// the blit raster ops go per-pixel through read/pset (the layer has
// one DATA register, and re-aiming per byte costs what a pixel walk
// costs).
// =====================================================================

#include <x16/bitmap8h.h>
#include <x16/vera.h>

// The operand block.
volatile unsigned int x16__g8h_x;
volatile unsigned int x16__g8h_y;
volatile char x16__g8h_a0;         // the 20-bit SDRAM byte address
volatile char x16__g8h_a1;
volatile char x16__g8h_a2;
volatile char x16__g8h_c;
volatile char x16__g8h_off;
volatile char x16__g8h_t;
volatile unsigned int x16__g8h_n;

// Pattern state: raw rows plus the two colours (the 4l way).
volatile char x16__g8h_pat[8];
volatile char x16__g8h_pfg;
volatile char x16__g8h_pbg;


// ---------------------------------------------------------------------
// Public: 1 if the VERA_2 bitmap layer answers, 0 otherwise.
// ---------------------------------------------------------------------
unsigned char x16_gfx8h_has(void) {
    if (*((char *)0x9F61) == 0xB5) {
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------
// Internal: a2:a1:a0 = y*640 + x. y*640 >> 8 is exactly y*2.5, so the
// middle/high bytes are (y<<1)+(y>>1) and the low byte is (y&1)<<7 --
// all of it fits 16-bit C.
// ---------------------------------------------------------------------
void x16__gfx8h_addr(void) {
    unsigned int mdhi;
    unsigned int lo;

    mdhi = (x16__g8h_y << 1) + (x16__g8h_y >> 1);
    lo = x16__g8h_x;
    if ((unsigned char)(x16__g8h_y & 1) != 0) {
        lo += 128;
    }
    mdhi += lo >> 8;
    x16__g8h_a0 = (unsigned char)lo;
    x16__g8h_a1 = (unsigned char)mdhi;
    x16__g8h_a2 = (unsigned char)(mdhi >> 8);
}

// ---------------------------------------------------------------------
// Internal: point the VERA_2 DATA register at a2:a1:a0 with a
// VERA2_INC_* stride index (note: index 0 is stride +1, 1 is stride 0).
// ---------------------------------------------------------------------
void x16__gfx8h_aim(unsigned char incr) {
    __asm {
        lda x16__g8h_a0
        sta 0x9f62                      // VERA2_ADDR_L
        lda x16__g8h_a1
        sta 0x9f63                      // VERA2_ADDR_M
        lda incr
        asl
        asl
        asl
        asl
        sta x16__g8h_t
        lda x16__g8h_a2
        and #0x0f
        ora x16__g8h_t
        sta 0x9f64                      // VERA2_ADDR_H
    }
}

void x16__gfx8h_onscreen(void) {
    x16__g8h_off = 0;
    if (x16__g8h_x >= X16_GFX8H_WIDTH) {
        x16__g8h_off = 1;
    }
    if (x16__g8h_y >= X16_GFX8H_HEIGHT) {
        x16__g8h_off = 1;
    }
}

// ---------------------------------------------------------------------
// Public: palette. The VERA_2 layer has its own 256-entry palette.
// ---------------------------------------------------------------------
void x16_gfx8h_pal_set(unsigned char index, unsigned char lo,
                      unsigned char hi) {
    *((char *)0x9F66) = index;
    *((char *)0x9F67) = lo;
    *((char *)0x9F68) = hi;
}

void x16_gfx8h_pal_load(const unsigned char *src, unsigned char first,
                       unsigned char count) {
    unsigned char i;

    for (i = 0; i < count; i++) {
        x16_gfx8h_pal_set((unsigned char)(first + i),
                          src[(unsigned char)(i * 2)],
                          src[(unsigned char)(i * 2 + 1)]);
    }
}

void x16_gfx8h_pal_gray(void) {
    unsigned char i, v;

    i = 0;
    do {
        v = i >> 4;
        x16_gfx8h_pal_set(i, (unsigned char)((v << 4) | v), v);
        i++;
    } while (i != 0);
}

// ---------------------------------------------------------------------
// Public: mode control.
// ---------------------------------------------------------------------
void x16_gfx8h_init(void) {
    x16_gfx8h_pal_gray();
    *((char *)0x9F60) = 0x03;   // ENABLE | MODE_8BPP
}

void x16_gfx8h_off(void) {
    *((char *)0x9F60) = 0x00;
}

void x16_gfx8h_passthru_on(void) {
    *((char *)0x9F60) =
        *((char *)0x9F60) | 0x08;
}

void x16_gfx8h_passthru_off(void) {
    *((char *)0x9F60) =
        *((char *)0x9F60) & 0xF7;
}

// ---------------------------------------------------------------------
// Public: aim the DATA register at pixel (x,y).
// ---------------------------------------------------------------------
void x16_gfx8h_setptr(unsigned char inc, unsigned int x, unsigned int y) {
    x16__g8h_x = x;
    x16__g8h_y = y;
    x16__gfx8h_addr();
    x16__gfx8h_aim(inc);
}

// ---------------------------------------------------------------------
// Internal: write g8h_n bytes of g8h_c through DATA (aim first).
// ---------------------------------------------------------------------
void x16__gfx8h_stream(void) {
    __asm {
        ldx x16__g8h_n              // the fill count idiom
        ldy x16__g8h_n+1
        txa
        beq g8hs_full
        iny
    g8hs_full:
        lda x16__g8h_c
    g8hs_loop:
        sta 0x9f65                      // VERA2_DATA
        dex
        bne g8hs_loop
        dey
        bne g8hs_loop
    }
}

// ---------------------------------------------------------------------
// Public: fill the whole framebuffer (307,200 bytes = 1200 pages).
// ---------------------------------------------------------------------
void x16_gfx8h_clear(unsigned char color) {
    x16__g8h_a0 = 0;
    x16__g8h_a1 = 0;
    x16__g8h_a2 = 0;
    x16__gfx8h_aim(X16_INC2_1);
    x16__g8h_c = color;
    x16__g8h_n = 38400;             // 8 x 38,400 = 307,200
    x16__gfx8h_stream();
    x16__gfx8h_stream();
    x16__gfx8h_stream();
    x16__gfx8h_stream();
    x16__gfx8h_stream();
    x16__gfx8h_stream();
    x16__gfx8h_stream();
    x16__gfx8h_stream();
}

// ---------------------------------------------------------------------
// Internal + public: clipped pixel access.
// ---------------------------------------------------------------------
void x16__gfx8h_pset_i(void) {
    x16__gfx8h_onscreen();
    if (x16__g8h_off) {
        return;
    }
    x16__gfx8h_addr();
    x16__gfx8h_aim(X16_INC2_1);
    *((char *)0x9F65) = (unsigned char)x16__g8h_c;
}

void x16_gfx8h_pset(unsigned int x, unsigned int y, unsigned char color) {
    x16__g8h_x = x;
    x16__g8h_y = y;
    x16__g8h_c = color;
    x16__gfx8h_pset_i();
}

unsigned int x16_gfx8h_read(unsigned int x, unsigned int y) {
    x16__g8h_x = x;
    x16__g8h_y = y;
    x16__gfx8h_onscreen();
    if (x16__g8h_off) {
        return 0xFFFF;
    }
    x16__gfx8h_addr();
    x16__gfx8h_aim(X16_INC2_0);
    return (unsigned int)*((char *)0x9F65);
}

// ---------------------------------------------------------------------
// Public: spans and rectangles. No clipping.
// ---------------------------------------------------------------------
void x16_gfx8h_hline(unsigned int x, unsigned int y, unsigned int len,
                    unsigned char color) {
    if (len == 0) {
        return;
    }
    x16__g8h_x = x;
    x16__g8h_y = y;
    x16__gfx8h_addr();
    x16__gfx8h_aim(X16_INC2_1);
    x16__g8h_c = color;
    x16__g8h_n = len;
    x16__gfx8h_stream();
}

void x16_gfx8h_vline(unsigned int x, unsigned int y, unsigned int len,
                    unsigned char color) {
    if (len == 0) {
        return;
    }
    x16__g8h_x = x;
    x16__g8h_y = y;
    x16__gfx8h_addr();
    x16__gfx8h_aim(X16_INC2_640);
    x16__g8h_c = color;
    x16__g8h_n = len;
    x16__gfx8h_stream();
}

void x16_gfx8h_rect(unsigned int x, unsigned int y, unsigned int w,
                   unsigned int h, unsigned char color) {
    unsigned int i;

    for (i = 0; i < h; i++) {
        x16_gfx8h_hline(x, y + i, w, color);
    }
}

void x16_gfx8h_frame(unsigned int x, unsigned int y, unsigned int w,
                    unsigned int h, unsigned char color) {
    x16_gfx8h_hline(x, y, w, color);
    x16_gfx8h_hline(x, y + h - 1, w, color);
    x16_gfx8h_vline(x, y, h, color);
    x16_gfx8h_vline(x + w - 1, y, h, color);
}

// ---------------------------------------------------------------------
// Public: Bresenham, any direction; plots through the clipped pset.
// ---------------------------------------------------------------------
void x16_gfx8h_line(unsigned int x0, unsigned int y0, unsigned int x1,
                   unsigned int y1, unsigned char color) {
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

    dx = lx1 - lx0;
    if (dx < 0) {
        dx = lx0 - lx1;
        sx = -1;
    } else {
        sx = 1;
    }
    dy = ly1 - ly0;
    if (dy < 0) {
        sy = -1;
    } else {
        dy = ly0 - ly1;
        sy = 1;
    }
    err = dx + dy;

    x16__g8h_c = color;
    for (;;) {
        x16__g8h_x = (unsigned int)lx0;
        x16__g8h_y = (unsigned int)ly0;
        x16__gfx8h_pset_i();

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
// Public: screen-anchored 8x8 pattern (raw rows; rotate at draw time).
// ---------------------------------------------------------------------
void x16_gfx8h_pattern_set(const unsigned char *pattern, unsigned char bg,
                          unsigned char fg) {
    unsigned char i;

    for (i = 0; i < 8; i++) {
        x16__g8h_pat[i] = pattern[i];
    }
    x16__g8h_pbg = bg;
    x16__g8h_pfg = fg;
}

void x16_gfx8h_pattern_rect(unsigned int x, unsigned int y, unsigned int w,
                           unsigned int h) {
    unsigned char rot, cur, r;
    unsigned int i, j;

    if (w == 0 || h == 0) {
        return;
    }
    rot = (unsigned char)(x & 7);
    for (i = 0; i < h; i++) {
        cur = (unsigned char)x16__g8h_pat[(unsigned char)((y + i) & 7)];
        for (r = 0; r < rot; r++) {
            cur = (unsigned char)((cur << 1) | (cur >> 7));
        }
        for (j = 0; j < w; j++) {
            if (cur & 0x80) {
                x16__g8h_c = x16__g8h_pfg;
            } else {
                x16__g8h_c = x16__g8h_pbg;
            }
            x16__g8h_x = x + j;
            x16__g8h_y = y + i;
            x16__gfx8h_pset_i();
            cur = (unsigned char)((cur << 1) | (cur >> 7));
        }
    }
}

// ---------------------------------------------------------------------
// Public: rows of bytes from RAM. op 0 streams; the RMW ops go per
// pixel through read (one DATA register, so a paired-port loop does
// not exist here).
// ---------------------------------------------------------------------
void x16_gfx8h_blit(unsigned int x, unsigned int y, unsigned char w,
                   unsigned char h, const unsigned char *src,
                   unsigned char op) {
    unsigned char brow, i, ink, fb;

    if (w == 0) {
        return;
    }
    op = op & 3;

    for (brow = 0; brow < h; brow++) {
        if (op == 0) {
            x16__g8h_x = x;
            x16__g8h_y = y + brow;
            x16__gfx8h_addr();
            x16__gfx8h_aim(X16_INC2_1);
            x16__g8h_t = w;
            __asm {
                ldy #0
            g8hb_copy:
                lda (src),y
                sta 0x9f65              // VERA2_DATA
                iny
                cpy x16__g8h_t
                bne g8hb_copy
            }
        } else {
            for (i = 0; i < w; i++) {
                ink = (unsigned char)src[i];
                fb = (unsigned char)x16_gfx8h_read(x + i, y + brow);
                if (op == 1) {
                    ink = ink | fb;
                } else if (op == 2) {
                    ink = ink & fb;
                } else {
                    ink = ink ^ fb;
                }
                x16__g8h_x = x + i;
                x16__g8h_y = y + brow;
                x16__g8h_c = ink;
                x16__gfx8h_pset_i();
            }
        }
        src = src + w;
    }
}

// ---------------------------------------------------------------------
// Public: masked blit -- colour 0 is transparent.
// ---------------------------------------------------------------------
void x16_gfx8h_blitm(unsigned int x, unsigned int y, unsigned char w,
                    unsigned char h, const unsigned char *src) {
    unsigned char brow, i, ink;

    if (w == 0) {
        return;
    }

    for (brow = 0; brow < h; brow++) {
        for (i = 0; i < w; i++) {
            ink = (unsigned char)src[i];
            if (ink != 0) {
                x16__g8h_x = x + i;
                x16__g8h_y = y + brow;
                x16__g8h_c = ink;
                x16__gfx8h_pset_i();
            }
        }
        src = src + w;
    }
}

// ---------------------------------------------------------------------
// Public: hardware SDRAM-to-SDRAM copy, then wait.
// ---------------------------------------------------------------------
void x16_gfx8h_copy_wait(void) {
    __asm {
    g8hcw_wait:
        lda 0x9f6f                      // VERA2_BLIT_CTRL
        and #1
        bne g8hcw_wait
    }
}

void x16_gfx8h_copy(unsigned long src, unsigned long dst,
                   unsigned long len) {
    __asm {
        lda len
        sta 0x9f6c                      // VERA2_BLIT_LEN_L
        lda len+1
        sta 0x9f6d                      // VERA2_BLIT_LEN_M
        lda len+2
        sta 0x9f6e                      // VERA2_BLIT_LEN_H
        lda src
        sta 0x9f62                      // VERA2_ADDR_L
        lda src+1
        sta 0x9f63                      // VERA2_ADDR_M
        lda src+2
        and #0x0f                    // source pointer, stride +1
        sta 0x9f64                      // VERA2_ADDR_H
        lda dst
        sta 0x9f69                      // VERA2_BLIT_DST_L
        lda dst+1
        sta 0x9f6a                      // VERA2_BLIT_DST_M
        lda dst+2
        and #0x0f
        sta 0x9f6b                      // VERA2_BLIT_DST_H
        lda #1
        sta 0x9f6f                      // VERA2_BLIT_CTRL
    }
    x16_gfx8h_copy_wait();
}
