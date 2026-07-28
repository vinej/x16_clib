// =====================================================================
// x16clib :: x16/bitmap2l.c -- 320x240x4 bitmap drawing (2bpp)
// =====================================================================
// The framebuffer is 2bpp at VRAM $00000: 4 pixels per byte packed
// MSB-first, rows of 80 bytes, 19,200 bytes in all. A pixel byte is at
// y*80 + (x>>2).
//
// x16_gfx2l_pset/read clip. The span/rect/line/blit primitives do NOT:
// they assume their arguments are on screen.
//
// HOW THIS PORT IS SPLIT (the same rule as x16/bitmap2h.c, which this
// module mirrors span for span). The whole framebuffer fits 16 bits of
// address, so the byte address is plain C; the hot read-modify-write,
// column and blit loops are the same hand-written 6502 as
// src_ca65/gfx/bitmap2l.s, operating on a module operand block.
// =====================================================================

#include <x16/bitmap2l.h>
#include <x16/vera.h>
#include <x16/verafx.h>
#include <x16/palette.h>

// The operand block (the ca65 build's X16_P0..P7 and g2l_* variables).
__mem volatile unsigned int x16__g2l_x;
__mem volatile unsigned int x16__g2l_y;
__mem volatile unsigned int x16__g2l_a;  // the byte address (fits 16 bits)
__mem volatile char x16__g2l_c;          // colour 0-3
__mem volatile char x16__g2l_cb;         // that colour in all four pixels
__mem volatile char x16__g2l_off;        // pset/read's clip verdict
__mem volatile char x16__g2l_phase;      // x & 3
__mem volatile char x16__g2l_msk;        // RMW mask (pixels to KEEP)
__mem volatile char x16__g2l_ink;        // RMW ink (already masked)
__mem volatile char x16__g2l_t;

// Pattern state: 8 rows x 2 bytes, expanded once by pattern_set.
__mem volatile char x16__g2l_pat[16];
__mem volatile char x16__g2l_pfg;
__mem volatile char x16__g2l_pbg;
__mem volatile char x16__g2l_pb0;
__mem volatile char x16__g2l_pb1;

// Blit operands. The source is indirected -- (ptr),y -- so it is pinned
// in the shared $78 pointer slot (see x16/zpsafe.h); no module's slot is
// live across a call into another, and the blits call nothing.
__address(0x78) const char* volatile x16__g2l_src;
__mem volatile char x16__g2l_n;          // rows / columns counter

// The tables the ca65 module carries.
const char x16__g2l_colbyte[4] = { 0x00, 0x55, 0xAA, 0xFF };  // colour x4
const char x16__g2l_pix[4]     = { 0xC0, 0x30, 0x0C, 0x03 };  // pixel p
const char x16__g2l_keep[4]    = { 0x3F, 0xCF, 0xF3, 0xFC };  // all but p
const char x16__g2l_from[4]    = { 0xFF, 0x3F, 0x0F, 0x03 };  // pixels p..3
const char x16__g2l_upto[4]    = { 0xC0, 0xF0, 0xFC, 0xFF };  // pixels 0..q

const char x16__g2l_defpal[8] = {        // white, light gray, dark gray, black
    0xFF, 0x0F, 0xAA, 0x0A, 0x55, 0x05, 0x00, 0x00
};

// ---------------------------------------------------------------------
// Internal: g2l_a = y*80 + (x>>2). 19,199 max: plain 16-bit C.
// ---------------------------------------------------------------------
void x16__gfx2l_addr(void) {
    x16__g2l_a = x16__g2l_y * 80 + (x16__g2l_x >> 2);
}

// ---------------------------------------------------------------------
// Internal: point a data port at g2l_a. `incr` is a VERA increment
// INDEX, pre-shifted here. The bitmap sits below bank 1, so ADDR_H's
// bank bit is always 0.
// ---------------------------------------------------------------------
void x16__gfx2l_aim0(__mem unsigned char incr) {
    asm {
        lda incr
        asl
        asl
        asl
        asl
        sta x16__g2l_t
        lda #1 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/
        lda x16__g2l_a
        sta $9f20 /*VERA_ADDR_L*/
        lda x16__g2l_a+1
        sta $9f21 /*VERA_ADDR_M*/
        lda x16__g2l_t
        sta $9f22 /*VERA_ADDR_H*/
    }
}

void x16__gfx2l_aim1(__mem unsigned char incr) {
    asm {
        lda incr
        asl
        asl
        asl
        asl
        sta x16__g2l_t
        lda #1 /*VERA_CTRL_ADDRSEL*/
        tsb $9f25 /*VERA_CTRL*/
        lda x16__g2l_a
        sta $9f20 /*VERA_ADDR_L*/
        lda x16__g2l_a+1
        sta $9f21 /*VERA_ADDR_M*/
        lda x16__g2l_t
        sta $9f22 /*VERA_ADDR_H*/
    }
}

// ---------------------------------------------------------------------
// Internal: is (g2l_x, g2l_y) on screen? Sets g2l_off.
// ---------------------------------------------------------------------
void x16__gfx2l_onscreen(void) {
    x16__g2l_off = 0;
    if (x16__g2l_x >= X16_GFX2L_WIDTH) {
        x16__g2l_off = 1;
    }
    if (x16__g2l_y >= X16_GFX2L_HEIGHT) {
        x16__g2l_off = 1;
    }
}

// ---------------------------------------------------------------------
// Internal: read-modify-write the byte at g2l_a through g2l_msk, laying
// in g2l_ink. INC_0 keeps the port still, so one aim serves both halves.
// ---------------------------------------------------------------------
void x16__gfx2l_rmw(void) {
    x16__gfx2l_aim0(X16_INC_0);
    asm {
        lda x16__g2l_msk
        eor #$ff
        and $9f23 /*VERA_DATA0*/
        sta x16__g2l_t
        lda x16__g2l_ink
        and x16__g2l_msk
        ora x16__g2l_t
        sta $9f23 /*VERA_DATA0*/
    }
}

// ---------------------------------------------------------------------
// Public: program the mode on bare VERA registers.
// ---------------------------------------------------------------------
void x16_gfx2l_init(void) {
    unsigned char i;

    asm {
        lda $9f25 /*VERA_CTRL*/         // DCSEL = 0, keep ADDRSEL
        and #1 /*VERA_CTRL_ADDRSEL*/
        sta $9f25 /*VERA_CTRL*/
        lda #$40                        // 64 = two output pixels per input,
        sta $9f2a /*VERA_DC_HSCALE*/    // so 320x240 fills the 640x480 display
        sta $9f2b /*VERA_DC_VSCALE*/
        stz $9f2c /*VERA_DC_BORDER*/

        lda #5 /*BITMAP|BPP_2*/
        sta $9f2d /*VERA_L0_CONFIG*/
        stz $9f2f /*VERA_L0_TILEBASE*/  // base $00000, 320 wide
        stz $9f30 /*VERA_L0_HSCROLL_L*/
        stz $9f31 /*VERA_L0_HSCROLL_H*/
        stz $9f32 /*VERA_L0_VSCROLL_L*/
        stz $9f33 /*VERA_L0_VSCROLL_H*/
    }

    for (i = 0; i < 4; i++) {
        x16_pal_set(i, (unsigned int)x16__g2l_defpal[i * 2] |
                       ((unsigned int)x16__g2l_defpal[i * 2 + 1] << 8));
    }

    asm {
        lda #$20 /*VIDEO_LAYER1_EN*/    // layer 1 off, layer 0 on
        trb $9f29 /*VERA_DC_VIDEO*/
        lda #$10 /*VIDEO_LAYER0_EN*/
        tsb $9f29 /*VERA_DC_VIDEO*/
    }
}

// ---------------------------------------------------------------------
// Public: fill the whole framebuffer through the FX 32-bit cache.
// 19,200 bytes fits one 16-bit count.
// ---------------------------------------------------------------------
void x16_gfx2l_clear(unsigned char color) {
    x16_fx_fill(x16__g2l_colbyte[color & 3], 19200, 0);
}

// ---------------------------------------------------------------------
// Public: point data port 0 at the byte holding (x,y); returns x & 3.
// ---------------------------------------------------------------------
unsigned char x16_gfx2l_setptr(unsigned char inc, unsigned int x,
                              unsigned int y) {
    x16__g2l_x = x;
    x16__g2l_y = y;
    x16__gfx2l_addr();
    x16__gfx2l_aim0(inc);
    return (unsigned char)(x & 3);
}

// ---------------------------------------------------------------------
// Internal: set pixel (g2l_x, g2l_y) to g2l_c, clipped.
// ---------------------------------------------------------------------
void x16__gfx2l_pset_i(void) {
    x16__gfx2l_onscreen();
    if (x16__g2l_off) {
        return;
    }
    x16__gfx2l_addr();
    x16__g2l_ink = x16__g2l_colbyte[x16__g2l_c & 3];
    x16__g2l_msk = x16__g2l_pix[(unsigned char)(x16__g2l_x & 3)];
    x16__gfx2l_rmw();
}

// ---------------------------------------------------------------------
// Public: one pixel, clipped.
// ---------------------------------------------------------------------
void x16_gfx2l_pset(unsigned int x, unsigned int y, unsigned char color) {
    x16__g2l_x = x;
    x16__g2l_y = y;
    x16__g2l_c = color;
    x16__gfx2l_pset_i();
}

// ---------------------------------------------------------------------
// Public: one pixel back, or $FF off screen.
// ---------------------------------------------------------------------
unsigned char x16_gfx2l_read(unsigned int x, unsigned int y) {
    x16__g2l_x = x;
    x16__g2l_y = y;
    x16__gfx2l_onscreen();
    if (x16__g2l_off) {
        return 0xFF;
    }
    x16__gfx2l_addr();
    x16__gfx2l_aim0(X16_INC_0);
    x16__g2l_phase = (unsigned char)(x & 3);
    asm {
        ldx x16__g2l_phase
        lda $9f23 /*VERA_DATA0*/
    g2lrd_shift:
        cpx #3                          // pixel 3 is already in bits 1:0
        beq g2lrd_done
        lsr
        lsr
        inx
        bra g2lrd_shift
    g2lrd_done:
        and #3
        sta x16__g2l_c
    }
    return x16__g2l_c;
}

// ---------------------------------------------------------------------
// Internal: horizontal span from the operand block, `len` pixels.
// Head and tail partials are read-modify-write; the whole bytes in
// between are one vera_fill.
// ---------------------------------------------------------------------
void x16__gfx2l_hline_i(unsigned int len) {
    unsigned char p, q, head;
    unsigned int m;

    if (len == 0) {
        return;
    }
    x16__gfx2l_addr();
    p = (unsigned char)(x16__g2l_x & 3);

    // A head byte exists when the span starts mid-byte, or is so short
    // it begins and ends inside one byte.
    if (p != 0 || len < 4) {
        q = 3;                          // last pixel of the head byte
        if (len < 4 && p + len - 1 < 4) {
            q = (unsigned char)(p + len - 1);
        }
        head = q - p + 1;
        x16__g2l_ink = x16__g2l_cb;
        x16__g2l_msk = x16__g2l_from[p] & x16__g2l_upto[q];
        x16__gfx2l_rmw();
        len -= head;
        x16__g2l_a++;
    }

    m = len >> 2;                       // whole bytes
    if (m != 0) {
        x16__gfx2l_aim0(X16_INC_1);
        x16_vera_fill(x16__g2l_cb, m);
        x16__g2l_a += m;
    }

    if ((len & 3) != 0) {               // tail: pixels 0..(len&3)-1
        x16__g2l_ink = x16__g2l_cb;
        x16__g2l_msk = x16__g2l_upto[(unsigned char)(len & 3) - 1];
        x16__gfx2l_rmw();
    }
}

// ---------------------------------------------------------------------
// Public: horizontal span.
// ---------------------------------------------------------------------
void x16_gfx2l_hline(unsigned int x, unsigned int y, unsigned int len,
                    unsigned char color) {
    x16__g2l_x = x;
    x16__g2l_y = y;
    x16__g2l_cb = x16__g2l_colbyte[color & 3];
    x16__gfx2l_hline_i(len);
}

// ---------------------------------------------------------------------
// Public: vertical span. One column of read-modify-writes, both ports
// stepping a whole row per access -- no calls, so one asm block.
// ---------------------------------------------------------------------
void x16_gfx2l_vline(unsigned int x, unsigned int y, unsigned int len,
                    unsigned char color) {
    if (len == 0) {
        return;
    }
    x16__g2l_x = x;
    x16__g2l_y = y;
    x16__g2l_cb = x16__g2l_colbyte[color & 3];
    x16__gfx2l_addr();
    x16__gfx2l_aim1(X16_INC_80);
    x16__gfx2l_aim0(X16_INC_80);

    // ink and keep are loop-invariant: this column's pixel never moves.
    x16__g2l_ink = x16__g2l_cb & x16__g2l_pix[(unsigned char)(x & 3)];
    x16__g2l_msk = x16__g2l_keep[(unsigned char)(x & 3)];

    x16__g2l_x = len;                   // borrowed as the 16-bit counter
    asm {
        ldx x16__g2l_x                  // vera_fill's page-count idiom
        ldy x16__g2l_x+1
        txa
        beq g2lv_full                   // low byte 0 -> exactly hi*256
        iny
    g2lv_full:
    g2lv_loop:
        lda $9f24 /*VERA_DATA1*/
        and x16__g2l_msk
        ora x16__g2l_ink
        sta $9f23 /*VERA_DATA0*/
        dex
        bne g2lv_loop
        dey
        bne g2lv_loop
    }
}

// ---------------------------------------------------------------------
// Public: filled rectangle and outline.
// ---------------------------------------------------------------------
void x16_gfx2l_rect(unsigned int x, unsigned int y, unsigned int w,
                   unsigned int h, unsigned char color) {
    unsigned int i;

    x16__g2l_cb = x16__g2l_colbyte[color & 3];
    for (i = 0; i < h; i++) {
        x16__g2l_x = x;
        x16__g2l_y = y + i;
        x16__gfx2l_hline_i(w);
    }
}

void x16_gfx2l_frame(unsigned int x, unsigned int y, unsigned int w,
                    unsigned int h, unsigned char color) {
    x16_gfx2l_hline(x, y, w, color);
    x16_gfx2l_hline(x, y + h - 1, w, color);
    x16_gfx2l_vline(x, y, h, color);
    x16_gfx2l_vline(x + w - 1, y, h, color);
}

// ---------------------------------------------------------------------
// Public: Bresenham, any direction; plots through the clipped pset.
// ---------------------------------------------------------------------
void x16_gfx2l_line(unsigned int x0, unsigned int y0, unsigned int x1,
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

    x16__g2l_c = color;
    for (;;) {
        x16__g2l_x = (unsigned int)lx0;
        x16__g2l_y = (unsigned int)ly0;
        x16__gfx2l_pset_i();

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
// Public: expand an 8x8 1bpp pattern into the 2bpp row cache.
// Patterns tile from the screen origin, so each row is two bytes and
// which one a framebuffer byte uses is the parity of its address.
// ---------------------------------------------------------------------
void x16_gfx2l_pattern_set(const unsigned char *pattern,
                          unsigned char colors) {
    unsigned char prow, half, bit, acc, bits;

    x16__g2l_pfg = x16__g2l_colbyte[colors & 3];
    x16__g2l_pbg = x16__g2l_colbyte[(colors >> 2) & 3];

    for (prow = 0; prow < 8; prow++) {
        bits = pattern[prow];
        for (half = 0; half < 2; half++) {
            acc = 0;
            for (bit = 0; bit < 4; bit++) {
                // MSB first: a set bit is foreground.
                if (bits & 0x80) {
                    acc |= x16__g2l_pfg & x16__g2l_pix[bit];
                } else {
                    acc |= x16__g2l_pbg & x16__g2l_pix[bit];
                }
                bits <<= 1;
            }
            x16__g2l_pat[prow * 2 + half] = acc;
        }
    }
}

// ---------------------------------------------------------------------
// Internal: one pattern row at (g2l_x, g2l_y), `len` pixels wide.
// ---------------------------------------------------------------------
void x16__gfx2l_prow(unsigned int len) {
    unsigned char p, q, head, swap;
    unsigned int m;

    if (len == 0) {
        return;
    }
    x16__gfx2l_addr();

    // The row's two pattern bytes, in address-parity order.
    swap = (unsigned char)(x16__g2l_y & 7) * 2;
    if ((x16__g2l_a & 1) != 0) {
        x16__g2l_pb0 = x16__g2l_pat[swap + 1];
        x16__g2l_pb1 = x16__g2l_pat[swap];
    } else {
        x16__g2l_pb0 = x16__g2l_pat[swap];
        x16__g2l_pb1 = x16__g2l_pat[swap + 1];
    }

    p = (unsigned char)(x16__g2l_x & 3);
    if (p != 0 || len < 4) {
        q = 3;
        if (len < 4 && p + len - 1 < 4) {
            q = (unsigned char)(p + len - 1);
        }
        head = q - p + 1;
        x16__g2l_ink = x16__g2l_pb0;
        x16__g2l_msk = x16__g2l_from[p] & x16__g2l_upto[q];
        x16__gfx2l_rmw();
        len -= head;
        x16__g2l_a++;
        swap = x16__g2l_pb0;            // the next byte flips parity
        x16__g2l_pb0 = x16__g2l_pb1;
        x16__g2l_pb1 = swap;
    }

    m = len >> 2;
    if (m != 0) {
        x16__gfx2l_aim0(X16_INC_1);
        x16__g2l_x = m;                 // borrowed as the counter
        asm {
            ldx x16__g2l_x
            ldy x16__g2l_x+1
            txa
            beq g2lp_full
            iny
        g2lp_full:
        g2lp_loop:
            lda x16__g2l_pb0
            sta $9f23 /*VERA_DATA0*/
            lda x16__g2l_pb0            // swap the parity pair
            ldx x16__g2l_pb1
            sta x16__g2l_pb1
            stx x16__g2l_pb0
            ldx x16__g2l_x
            dex
            stx x16__g2l_x
            bne g2lp_loop
            dey
            bne g2lp_loop
        }
        x16__g2l_a += m;
    }

    if ((len & 3) != 0) {
        x16__g2l_ink = x16__g2l_pb0;
        x16__g2l_msk = x16__g2l_upto[(unsigned char)(len & 3) - 1];
        x16__gfx2l_rmw();
    }
}

void x16_gfx2l_pattern_rect(unsigned int x, unsigned int y, unsigned int w,
                           unsigned int h) {
    unsigned int i;

    for (i = 0; i < h; i++) {
        x16__g2l_x = x;
        x16__g2l_y = y + i;
        x16__gfx2l_prow(w);
    }
}

// ---------------------------------------------------------------------
// Public: byte-aligned image copy with a raster op.
//
// The op is dispatched in C so each inner loop is its own asm block
// (see x16/bitmap2h.c for why).
// ---------------------------------------------------------------------
void x16_gfx2l_blit(unsigned int x, unsigned int y, unsigned char wbytes,
                   unsigned char h, const unsigned char *src,
                   unsigned char op) {
    unsigned char brow;

    x16__g2l_x = x;
    x16__g2l_y = y;
    x16__g2l_src = src;
    x16__g2l_n = wbytes;
    x16__gfx2l_addr();

    for (brow = 0; brow < h; brow++) {
        x16__gfx2l_aim1(X16_INC_1);      // ops read port 1...
        x16__gfx2l_aim0(X16_INC_1);      // ...everything writes port 0
        if (op == 1) {
            asm {
                ldy #0
            g2lb_or:
                lda $9f24 /*VERA_DATA1*/
                ora (x16__g2l_src),y
                sta $9f23 /*VERA_DATA0*/
                iny
                cpy x16__g2l_n
                bne g2lb_or
            }
        } else if (op == 2) {
            asm {
                ldy #0
            g2lb_and:
                lda $9f24 /*VERA_DATA1*/
                and (x16__g2l_src),y
                sta $9f23 /*VERA_DATA0*/
                iny
                cpy x16__g2l_n
                bne g2lb_and
            }
        } else if (op == 3) {
            asm {
                ldy #0
            g2lb_xor:
                lda $9f24 /*VERA_DATA1*/
                eor (x16__g2l_src),y
                sta $9f23 /*VERA_DATA0*/
                iny
                cpy x16__g2l_n
                bne g2lb_xor
            }
        } else {
            asm {
                ldy #0
            g2lb_copy:
                lda (x16__g2l_src),y
                sta $9f23 /*VERA_DATA0*/
                iny
                cpy x16__g2l_n
                bne g2lb_copy
            }
        }
        x16__g2l_src = x16__g2l_src + wbytes;
        x16__g2l_a += X16_GFX2L_STRIDE;          // down one row
    }
}

// ---------------------------------------------------------------------
// Public: masked blit of pre-shifted column-major data. For each of
// `cols` byte columns, `h` (mask, data) pairs walk down the rows.
// ---------------------------------------------------------------------
void x16_gfx2l_blitm(unsigned int x, unsigned int y, unsigned char h,
                    unsigned char cols, const unsigned char *src) {
    unsigned char col;

    x16__g2l_x = x;
    x16__g2l_y = y;
    x16__g2l_src = src;
    x16__g2l_n = h;
    x16__gfx2l_addr();

    for (col = 0; col < cols; col++) {
        x16__gfx2l_aim1(X16_INC_80);
        x16__gfx2l_aim0(X16_INC_80);
        asm {
            ldy #0
            ldx x16__g2l_n
        g2lm_row:
            lda $9f24 /*VERA_DATA1*/
            and (x16__g2l_src),y        // mask byte
            iny
            ora (x16__g2l_src),y        // data byte
            iny
            sta $9f23 /*VERA_DATA0*/
            dex
            bne g2lm_row
        }
        x16__g2l_src = x16__g2l_src + h + h;     // one column of pairs
        x16__g2l_a++;                            // next byte column
    }
}
