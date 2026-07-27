// =====================================================================
// x16clib :: x16/bitmap4h.c -- VERA_2 640x480x16 SDRAM bitmap drawing
// =====================================================================
// Requires the MiSTer VERA_2 bitmap layer behind $9F60-$9F6F; the
// framebuffer is its 20-bit SDRAM byte space, NOT VERA VRAM. Feature-
// detect with x16_gfx4h_has() -- on stock hardware (and the emulator)
// every routine here writes into open bus.
//
// The framebuffer is 4bpp, two pixels per byte, rows of 320 bytes:
// offset = y*320 + (x>>1), 153,600 bytes in all. The left pixel is the
// high nibble.
//
// The VERA_2 layer is plain memory-mapped registers, so this port is
// mostly C (see x16/bitmap8h.c); the nibble read-modify-write rides
// the stride-0 aim.
// =====================================================================

#include <x16/bitmap4h.h>
#include <x16/vera.h>

// The operand block.
volatile unsigned int x16__g4h_x;
volatile unsigned int x16__g4h_y;
volatile char x16__g4h_a0;         // the 20-bit SDRAM byte address
volatile char x16__g4h_a1;
volatile char x16__g4h_a2;
volatile char x16__g4h_c;
volatile char x16__g4h_off;
volatile char x16__g4h_t;
volatile unsigned int x16__g4h_n;

// Pattern state: raw rows plus the two colours.
volatile char x16__g4h_pat[8];
volatile char x16__g4h_pfg;
volatile char x16__g4h_pbg;


const char x16__g4h_colbyte[16] = {      // a colour in both nibbles
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

// ---------------------------------------------------------------------
// Public: 1 if the VERA_2 bitmap layer answers, 0 otherwise.
// ---------------------------------------------------------------------
unsigned char x16_gfx4h_has(void) {
    if (*((char *)0x9F61) == 0xB5) {
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------
// Internal: a2:a1:a0 = y*320 + (x>>1). y*320 = (y*5)*64, and y*5 fits
// 16-bit C, so the bytes fall out of one shift.
// ---------------------------------------------------------------------
void x16__gfx4h_addr(void) {
    unsigned int t;
    unsigned int mdhi;
    unsigned int lo;

    t = (x16__g4h_y << 2) + x16__g4h_y;         // y*5
    mdhi = t >> 2;                              // (y*320) >> 8
    lo = (unsigned int)((unsigned char)((t & 3) << 6));
    lo += x16__g4h_x >> 1;
    mdhi += lo >> 8;
    x16__g4h_a0 = (unsigned char)lo;
    x16__g4h_a1 = (unsigned char)mdhi;
    x16__g4h_a2 = (unsigned char)(mdhi >> 8);
}

// ---------------------------------------------------------------------
// Internal: point the VERA_2 DATA register at a2:a1:a0 with a
// VERA2_INC_* stride index (note: index 0 is stride +1, 1 is stride 0).
// ---------------------------------------------------------------------
void x16__gfx4h_aim(unsigned char incr) {
    __asm {
        lda x16__g4h_a0
        sta 0x9f62                      // VERA2_ADDR_L
        lda x16__g4h_a1
        sta 0x9f63                      // VERA2_ADDR_M
        lda incr
        asl
        asl
        asl
        asl
        sta x16__g4h_t
        lda x16__g4h_a2
        and #0x0f
        ora x16__g4h_t
        sta 0x9f64                      // VERA2_ADDR_H
    }
}

void x16__gfx4h_onscreen(void) {
    x16__g4h_off = 0;
    if (x16__g4h_x >= X16_GFX4H_WIDTH) {
        x16__g4h_off = 1;
    }
    if (x16__g4h_y >= X16_GFX4H_HEIGHT) {
        x16__g4h_off = 1;
    }
}

// ---------------------------------------------------------------------
// Public: palette. The VERA_2 layer has its own palette registers.
// ---------------------------------------------------------------------
void x16_gfx4h_pal_set(unsigned char index, unsigned char lo,
                      unsigned char hi) {
    *((char *)0x9F66) = index;
    *((char *)0x9F67) = lo;
    *((char *)0x9F68) = hi;
}

void x16_gfx4h_pal_load(const unsigned char *src, unsigned char first,
                       unsigned char count) {
    unsigned char i;

    for (i = 0; i < count; i++) {
        x16_gfx4h_pal_set((unsigned char)(first + i),
                          src[(unsigned char)(i * 2)],
                          src[(unsigned char)(i * 2 + 1)]);
    }
}

void x16_gfx4h_pal_gray(void) {
    unsigned char i, v;

    for (i = 0; i < 16; i++) {
        v = (unsigned char)((i << 4) | i);
        x16_gfx4h_pal_set(i, v, i);
    }
}

// ---------------------------------------------------------------------
// Public: mode control.
// ---------------------------------------------------------------------
void x16_gfx4h_init(void) {
    x16_gfx4h_pal_gray();
    *((char *)0x9F60) = 0x05;   // ENABLE | MODE_4BPP
}

void x16_gfx4h_off(void) {
    *((char *)0x9F60) = 0x00;
}

void x16_gfx4h_passthru_on(void) {
    *((char *)0x9F60) =
        *((char *)0x9F60) | 0x08;
}

void x16_gfx4h_passthru_off(void) {
    *((char *)0x9F60) =
        *((char *)0x9F60) & 0xF7;
}

// ---------------------------------------------------------------------
// Public: aim the DATA register at the byte holding pixel (x,y).
// ---------------------------------------------------------------------
void x16_gfx4h_setptr(unsigned char inc, unsigned int x, unsigned int y) {
    x16__g4h_x = x;
    x16__g4h_y = y;
    x16__gfx4h_addr();
    x16__gfx4h_aim(inc);
}

// ---------------------------------------------------------------------
// Internal: write g4h_n bytes of g4h_c through DATA (aim first).
// ---------------------------------------------------------------------
void x16__gfx4h_stream(void) {
    __asm {
        ldx x16__g4h_n              // the fill count idiom
        ldy x16__g4h_n+1
        txa
        beq g4hs_full
        iny
    g4hs_full:
        lda x16__g4h_c
    g4hs_loop:
        sta 0x9f65                      // VERA2_DATA
        dex
        bne g4hs_loop
        dey
        bne g4hs_loop
    }
}

// ---------------------------------------------------------------------
// Public: fill the whole framebuffer (153,600 bytes).
// ---------------------------------------------------------------------
void x16_gfx4h_clear(unsigned char color) {
    x16__g4h_a0 = 0;
    x16__g4h_a1 = 0;
    x16__g4h_a2 = 0;
    x16__gfx4h_aim(X16_INC2_1);
    x16__g4h_c = x16__g4h_colbyte[color & 0x0F];
    x16__g4h_n = 38400;             // 4 x 38,400 = 153,600
    x16__gfx4h_stream();
    x16__gfx4h_stream();
    x16__gfx4h_stream();
    x16__gfx4h_stream();
}

// ---------------------------------------------------------------------
// Internal + public: clipped pixel access, nibble read-modify-write.
// ---------------------------------------------------------------------
void x16__gfx4h_pset_i(void) {
    x16__gfx4h_onscreen();
    if (x16__g4h_off) {
        return;
    }
    x16__gfx4h_addr();
    x16__gfx4h_aim(X16_INC2_0);
    x16__g4h_c = x16__g4h_c & 0x0F;
    if ((unsigned char)(x16__g4h_x & 1) != 0) {
        __asm {
            lda 0x9f65                  // VERA2_DATA
            and #0xf0
            ora x16__g4h_c
            sta 0x9f65                  // VERA2_DATA
        }
    } else {
        __asm {
            lda x16__g4h_c
            asl
            asl
            asl
            asl
            sta x16__g4h_t
            lda 0x9f65                  // VERA2_DATA
            and #0x0f
            ora x16__g4h_t
            sta 0x9f65                  // VERA2_DATA
        }
    }
}

void x16_gfx4h_pset(unsigned int x, unsigned int y, unsigned char color) {
    x16__g4h_x = x;
    x16__g4h_y = y;
    x16__g4h_c = color;
    x16__gfx4h_pset_i();
}

unsigned char x16_gfx4h_read(unsigned int x, unsigned int y) {
    unsigned char b;

    x16__g4h_x = x;
    x16__g4h_y = y;
    x16__gfx4h_onscreen();
    if (x16__g4h_off) {
        return 0xFF;
    }
    x16__gfx4h_addr();
    x16__gfx4h_aim(X16_INC2_0);
    __asm {
        lda 0x9f65                      // VERA2_DATA
        sta x16__g4h_t
    }
    b = (unsigned char)x16__g4h_t;
    if ((unsigned char)(x & 1) != 0) {
        return b & 0x0F;
    }
    return b >> 4;
}

// ---------------------------------------------------------------------
// Public: spans. The head/tail odd nibbles RMW through pset; the whole
// bytes in between are one stream (the upstream module's shape).
// ---------------------------------------------------------------------
void x16_gfx4h_hline(unsigned int x, unsigned int y, unsigned int len,
                    unsigned char color) {
    unsigned int m;

    if (len == 0) {
        return;
    }
    // Odd leading pixel.
    if ((unsigned char)(x & 1) != 0) {
        x16_gfx4h_pset(x, y, color);
        x++;
        len--;
        if (len == 0) {
            return;
        }
    }
    m = len >> 1;                   // whole two-pixel bytes
    if (m != 0) {
        x16__g4h_x = x;
        x16__g4h_y = y;
        x16__gfx4h_addr();
        x16__gfx4h_aim(X16_INC2_1);
        x16__g4h_c = x16__g4h_colbyte[color & 0x0F];
        x16__g4h_n = m;
        x16__gfx4h_stream();
    }
    // Odd trailing pixel.
    if ((unsigned char)(len & 1) != 0) {
        x16_gfx4h_pset(x + len - 1, y, color);
    }
}

void x16_gfx4h_vline(unsigned int x, unsigned int y, unsigned int len,
                    unsigned char color) {
    unsigned int i;

    x16__g4h_c = color;
    for (i = 0; i < len; i++) {
        x16__g4h_x = x;
        x16__g4h_y = y + i;
        x16__gfx4h_pset_i();
    }
}

void x16_gfx4h_rect(unsigned int x, unsigned int y, unsigned int w,
                   unsigned int h, unsigned char color) {
    unsigned int i;

    for (i = 0; i < h; i++) {
        x16_gfx4h_hline(x, y + i, w, color);
    }
}

void x16_gfx4h_frame(unsigned int x, unsigned int y, unsigned int w,
                    unsigned int h, unsigned char color) {
    x16_gfx4h_hline(x, y, w, color);
    x16_gfx4h_hline(x, y + h - 1, w, color);
    x16_gfx4h_vline(x, y, h, color);
    x16_gfx4h_vline(x + w - 1, y, h, color);
}

// ---------------------------------------------------------------------
// Public: Bresenham, any direction; plots through the clipped pset.
// ---------------------------------------------------------------------
void x16_gfx4h_line(unsigned int x0, unsigned int y0, unsigned int x1,
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

    x16__g4h_c = color;
    for (;;) {
        x16__g4h_x = (unsigned int)lx0;
        x16__g4h_y = (unsigned int)ly0;
        x16__gfx4h_pset_i();

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
void x16_gfx4h_pattern_set(const unsigned char *pattern, unsigned char bg,
                          unsigned char fg) {
    unsigned char i;

    for (i = 0; i < 8; i++) {
        x16__g4h_pat[i] = pattern[i];
    }
    x16__g4h_pbg = bg & 0x0F;
    x16__g4h_pfg = fg & 0x0F;
}

void x16_gfx4h_pattern_rect(unsigned int x, unsigned int y, unsigned int w,
                           unsigned int h) {
    unsigned char rot, cur, r;
    unsigned int i, j;

    if (w == 0 || h == 0) {
        return;
    }
    rot = (unsigned char)(x & 7);
    for (i = 0; i < h; i++) {
        cur = (unsigned char)x16__g4h_pat[(unsigned char)((y + i) & 7)];
        for (r = 0; r < rot; r++) {
            cur = (unsigned char)((cur << 1) | (cur >> 7));
        }
        for (j = 0; j < w; j++) {
            if (cur & 0x80) {
                x16__g4h_c = x16__g4h_pfg;
            } else {
                x16__g4h_c = x16__g4h_pbg;
            }
            x16__g4h_x = x + j;
            x16__g4h_y = y + i;
            x16__gfx4h_pset_i();
            cur = (unsigned char)((cur << 1) | (cur >> 7));
        }
    }
}

// ---------------------------------------------------------------------
// Public: rows of pixels from RAM, packed two per byte (high nibble
// first); the row is (w+1)/2 bytes. Per-pixel through read/pset.
// ---------------------------------------------------------------------
void x16_gfx4h_blit(unsigned int x, unsigned int y, unsigned char w,
                   unsigned char h, const unsigned char *src,
                   unsigned char op) {
    unsigned char brow, i, ink, fb, rowbytes;

    if (w == 0) {
        return;
    }
    op = op & 3;
    rowbytes = (unsigned char)((w + 1) >> 1);

    for (brow = 0; brow < h; brow++) {
        for (i = 0; i < w; i++) {
            unsigned char bidx = i >> 1;
            ink = (unsigned char)src[bidx];
            if ((i & 1) != 0) {
                ink = ink & 0x0F;
            } else {
                ink = ink >> 4;
            }
            if (op != 0) {
                fb = x16_gfx4h_read(x + i, y + brow);
                if (op == 1) {
                    ink = ink | fb;
                } else if (op == 2) {
                    ink = ink & fb;
                } else {
                    ink = ink ^ fb;
                }
            }
            x16__g4h_x = x + i;
            x16__g4h_y = y + brow;
            x16__g4h_c = ink;
            x16__gfx4h_pset_i();
        }
        src = src + rowbytes;
    }
}

// ---------------------------------------------------------------------
// Public: masked blit -- colour 0 is transparent.
// ---------------------------------------------------------------------
void x16_gfx4h_blitm(unsigned int x, unsigned int y, unsigned char w,
                    unsigned char h, const unsigned char *src) {
    unsigned char brow, i, ink, rowbytes;

    if (w == 0) {
        return;
    }
    rowbytes = (unsigned char)((w + 1) >> 1);

    for (brow = 0; brow < h; brow++) {
        for (i = 0; i < w; i++) {
            unsigned char bidx = i >> 1;
            ink = (unsigned char)src[bidx];
            if ((i & 1) != 0) {
                ink = ink & 0x0F;
            } else {
                ink = ink >> 4;
            }
            if (ink != 0) {
                x16__g4h_x = x + i;
                x16__g4h_y = y + brow;
                x16__g4h_c = ink;
                x16__gfx4h_pset_i();
            }
        }
        src = src + rowbytes;
    }
}

// ---------------------------------------------------------------------
// Public: hardware SDRAM-to-SDRAM copy, then wait.
// ---------------------------------------------------------------------
void x16_gfx4h_copy_wait(void) {
    __asm {
    g4hcw_wait:
        lda 0x9f6f                      // VERA2_BLIT_CTRL
        and #1
        bne g4hcw_wait
    }
}

void x16_gfx4h_copy(unsigned long src, unsigned long dst,
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
    x16_gfx4h_copy_wait();
}
