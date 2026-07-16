// =====================================================================
// x16clib :: x16/bitmap2.c -- 640x480x4 bitmap drawing (2bpp)
// =====================================================================
// The framebuffer is 2bpp at VRAM $00000: 4 pixels per byte packed
// MSB-first, rows of 160 bytes. A pixel byte is at y*160 + (x>>2).
//
// x16_gfx2_pset/read clip. The span/rect/line/blit primitives do NOT:
// they assume their arguments are on screen.
//
// HOW THIS PORT IS SPLIT (the same rule as x16/bitmap.c). The hot paths
// -- the y*160+(x>>2) address, the masked read-modify-write, the column
// and blit loops -- are the same hand-written 6502 as
// src_ca65/gfx/bitmap2.s, operating on a module operand block. The
// ORCHESTRATION around them is C, mirroring the ca65 control flow
// statement for statement: inline asm cannot jsr another function, and
// a 2bpp span is three phases (partial head byte, whole-byte middle
// through vera_fill, partial tail byte) that must call out to do their
// work.
// =====================================================================

#include <x16/bitmap2.h>
#include <x16/vera.h>
#include <x16/verafx.h>
#include <x16/palette.h>

// The operand block (the ca65 build's X16_P0..P7 and g2_* variables).
volatile unsigned int x16__g2_x;
volatile unsigned int x16__g2_y;
volatile char x16__g2_c;          // colour 0-3
volatile char x16__g2_cb;         // that colour in all four pixels
volatile char x16__g2_off;        // pset/read's clip verdict
volatile char x16__g2_phase;      // x & 3
volatile char x16__g2_msk;        // RMW mask (pixels to KEEP)
volatile char x16__g2_ink;        // RMW ink (already masked)

volatile char x16__g2_a0;         // the 17-bit framebuffer address
volatile char x16__g2_a1;
volatile char x16__g2_a2;
volatile char x16__g2_t;

// Pattern state: 8 rows x 2 bytes, expanded once by pattern_set.
volatile char x16__g2_pat[16];
volatile char x16__g2_pfg;
volatile char x16__g2_pbg;
volatile char x16__g2_pb0;
volatile char x16__g2_pb1;

// Blit operands. No pinned source pointer here: Oscar64 keeps pointer
// PARAMETERS in zero page, so the blits below indirect `src` directly
// -- the KickC build's __address() slot has nothing to do.
volatile char x16__g2_n;          // rows / columns counter

// The tables the ca65 module carries. Indexed from asm, so they are
// plain module arrays rather than kickasm data.
const char x16__g2_colbyte[4] = { 0x00, 0x55, 0xAA, 0xFF };  // colour x4
const char x16__g2_pix[4]     = { 0xC0, 0x30, 0x0C, 0x03 };  // pixel p
const char x16__g2_keep[4]    = { 0x3F, 0xCF, 0xF3, 0xFC };  // all but p
const char x16__g2_from[4]    = { 0xFF, 0x3F, 0x0F, 0x03 };  // pixels p..3
const char x16__g2_upto[4]    = { 0xC0, 0xF0, 0xFC, 0xFF };  // pixels 0..q

const char x16__g2_defpal[8] = {        // white, light gray, dark gray, black
    0xFF, 0x0F, 0xAA, 0x0A, 0x55, 0x05, 0x00, 0x00
};

// ---------------------------------------------------------------------
// Internal: g2_a = y*160 + (x>>2), the 17-bit byte address.
// y*160 = t + (t<<2) where t = y<<5, so no multiply is needed.
// ---------------------------------------------------------------------
void x16__gfx2_addr(void) {
    __asm {
        lda x16__g2_y                   // t = y << 5
        sta x16__g2_a0
        lda x16__g2_y+1
        sta x16__g2_a1
        asl x16__g2_a0
        rol x16__g2_a1
        asl x16__g2_a0
        rol x16__g2_a1
        asl x16__g2_a0
        rol x16__g2_a1
        asl x16__g2_a0
        rol x16__g2_a1
        asl x16__g2_a0
        rol x16__g2_a1

        lda x16__g2_a0                  // a2:t:x16__g2_t = t << 2
        sta x16__g2_t
        lda x16__g2_a1
        sta x16__g2_msk                 // borrowed: t<<2 middle byte
        lda #0
        sta x16__g2_a2
        asl x16__g2_t
        rol x16__g2_msk
        rol x16__g2_a2
        asl x16__g2_t
        rol x16__g2_msk
        rol x16__g2_a2

        clc                             // y*160 = t + (t << 2)
        lda x16__g2_a0
        adc x16__g2_t
        sta x16__g2_a0
        lda x16__g2_a1
        adc x16__g2_msk
        sta x16__g2_a1
        lda #0
        adc x16__g2_a2
        sta x16__g2_a2

        lda x16__g2_x+1                 // + x >> 2
        sta x16__g2_t
        lda x16__g2_x
        lsr x16__g2_t
        ror
        lsr x16__g2_t
        ror
        clc
        adc x16__g2_a0
        sta x16__g2_a0
        lda x16__g2_t
        adc x16__g2_a1
        sta x16__g2_a1
        lda #0
        adc x16__g2_a2
        sta x16__g2_a2
    }
}

// ---------------------------------------------------------------------
// Internal: point a data port at g2_a. `port1` selects the read side.
// `incr` is a VERA increment INDEX, pre-shifted here.
// ---------------------------------------------------------------------
void x16__gfx2_aim0(unsigned char incr) {
    __asm {
        lda incr
        asl
        asl
        asl
        asl
        sta x16__g2_t
        lda 0x9f25                      // VERA_CTRL
        and #0xfe                       // ADDRSEL = 0, DCSEL kept
        sta 0x9f25
        lda x16__g2_a0
        sta 0x9f20                      // VERA_ADDR_L
        lda x16__g2_a1
        sta 0x9f21                      // VERA_ADDR_M
        lda x16__g2_a2
        and #1                          // VERA_ADDR_H_BANK
        ora x16__g2_t
        sta 0x9f22                      // VERA_ADDR_H
    }
}

void x16__gfx2_aim1(unsigned char incr) {
    __asm {
        lda incr
        asl
        asl
        asl
        asl
        sta x16__g2_t
        lda 0x9f25                      // VERA_CTRL
        ora #0x01                       // ADDRSEL = 1, DCSEL kept
        sta 0x9f25
        lda x16__g2_a0
        sta 0x9f20                      // VERA_ADDR_L
        lda x16__g2_a1
        sta 0x9f21                      // VERA_ADDR_M
        lda x16__g2_a2
        and #1                          // VERA_ADDR_H_BANK
        ora x16__g2_t
        sta 0x9f22                      // VERA_ADDR_H
    }
}

// ---------------------------------------------------------------------
// Internal: is (g2_x, g2_y) on screen? Sets g2_off.
//
// C, not asm: a label that ends a procedure has nothing to attach to
// and KickC drops it, which leaves any branch to it dangling. Two
// unsigned compares are not a hot path worth fighting that over.
// ---------------------------------------------------------------------
void x16__gfx2_onscreen(void) {
    x16__g2_off = 0;
    if (x16__g2_x >= X16_GFX2_WIDTH) {
        x16__g2_off = 1;
    }
    if (x16__g2_y >= X16_GFX2_HEIGHT) {
        x16__g2_off = 1;
    }
}

// ---------------------------------------------------------------------
// Internal: g2_a += 1, the 17-bit carry chain.
// ---------------------------------------------------------------------
void x16__gfx2_ainc(void) {
    x16__g2_a0++;
    if (x16__g2_a0 == 0) {
        x16__g2_a1++;
        if (x16__g2_a1 == 0) {
            x16__g2_a2++;
        }
    }
}

// ---------------------------------------------------------------------
// Internal: g2_a += n (n is a byte count, not a pixel count).
// ---------------------------------------------------------------------
void x16__gfx2_aadd(unsigned int n) {
    unsigned int lo;

    lo = (unsigned int)x16__g2_a0 + n;
    x16__g2_a0 = (unsigned char)lo;
    lo = (unsigned int)x16__g2_a1 + (lo >> 8);
    x16__g2_a1 = (unsigned char)lo;
    x16__g2_a2 = (unsigned char)(x16__g2_a2 + (unsigned char)(lo >> 8));
}

// ---------------------------------------------------------------------
// Internal: read-modify-write the byte at g2_a through g2_msk, laying
// in g2_ink. INC_0 keeps the port still, so one aim serves both halves.
// ---------------------------------------------------------------------
void x16__gfx2_rmw(void) {
    x16__gfx2_aim0(X16_INC_0);
    __asm {
        lda x16__g2_msk
        eor #0xff
        and 0x9f23                      // VERA_DATA0
        sta x16__g2_t
        lda x16__g2_ink
        and x16__g2_msk
        ora x16__g2_t
        sta 0x9f23                      // VERA_DATA0
    }
}

// ---------------------------------------------------------------------
// Public: program the mode on bare VERA registers.
// ---------------------------------------------------------------------
void x16_gfx2_init(void) {
    unsigned char i;

    __asm {
        lda 0x9f25         // DCSEL = 0, keep ADDRSEL (VERA_CTRL)
        and #1                          // VERA_CTRL_ADDRSEL
        sta 0x9f25                      // VERA_CTRL
        lda #0x80                        // 1:1 scale -> full 640x480
        sta 0x9f2a                      // VERA_DC_HSCALE
        sta 0x9f2b                      // VERA_DC_VSCALE
        lda #0
        sta 0x9f2c                      // VERA_DC_BORDER

        lda #5                          // BITMAP|BPP_2
        sta 0x9f2d                      // VERA_L0_CONFIG
        lda #1                          // base 0x00000, 640 wide
        sta 0x9f2f                      // VERA_L0_TILEBASE
        lda #0
        sta 0x9f30                      // VERA_L0_HSCROLL_L
        sta 0x9f31                      // VERA_L0_HSCROLL_H
        sta 0x9f32                      // VERA_L0_VSCROLL_L
        sta 0x9f33                      // VERA_L0_VSCROLL_H
    }

    // Palette 0-3. pal_set takes a 16-bit colour, and the default table
    // is byte pairs (low, high) exactly as the other ports store them.
    for (i = 0; i < 4; i++) {
        x16_pal_set(i, (unsigned int)x16__g2_defpal[i * 2] |
                       ((unsigned int)x16__g2_defpal[i * 2 + 1] << 8));
    }

    __asm {
        lda 0x9f29  /*VERA_DC_VIDEO*/
        and #0xdf
        sta 0x9f29
        lda 0x9f29  /*VERA_DC_VIDEO*/
        ora #0x10
        sta 0x9f29
    }
}

// ---------------------------------------------------------------------
// Public: fill the whole framebuffer through the FX 32-bit cache.
// 76,800 bytes does not fit fx_fill's 16-bit count, so two halves.
// ---------------------------------------------------------------------
void x16_gfx2_clear(unsigned char color) {
    unsigned char cb = x16__g2_colbyte[color & 3];

    x16_fx_fill(cb, 38400, 0);
    x16_fx_fill(cb, 38400, 38400);
}

// ---------------------------------------------------------------------
// Public: point data port 0 at the byte holding (x,y); returns x & 3.
// ---------------------------------------------------------------------
unsigned char x16_gfx2_setptr(unsigned char inc, unsigned int x,
                              unsigned int y) {
    x16__g2_x = x;
    x16__g2_y = y;
    x16__gfx2_addr();
    x16__gfx2_aim0(inc);
    return (unsigned char)(x & 3);
}

// ---------------------------------------------------------------------
// Internal: set pixel (g2_x, g2_y) to g2_c, clipped.
// ---------------------------------------------------------------------
void x16__gfx2_pset_i(void) {
    x16__gfx2_onscreen();
    if (x16__g2_off) {
        return;
    }
    x16__gfx2_addr();
    x16__g2_ink = x16__g2_colbyte[x16__g2_c & 3];
    x16__g2_msk = x16__g2_pix[(unsigned char)(x16__g2_x & 3)];
    x16__gfx2_rmw();
}

// ---------------------------------------------------------------------
// Public: one pixel, clipped.
// ---------------------------------------------------------------------
void x16_gfx2_pset(unsigned int x, unsigned int y, unsigned char color) {
    x16__g2_x = x;
    x16__g2_y = y;
    x16__g2_c = color;
    x16__gfx2_pset_i();
}

// ---------------------------------------------------------------------
// Public: one pixel back, or $FF off screen.
// ---------------------------------------------------------------------
unsigned char x16_gfx2_read(unsigned int x, unsigned int y) {
    x16__g2_x = x;
    x16__g2_y = y;
    x16__gfx2_onscreen();
    if (x16__g2_off) {
        return 0xFF;
    }
    x16__gfx2_addr();
    x16__gfx2_aim0(X16_INC_0);
    x16__g2_phase = (unsigned char)(x & 3);
    __asm {
        ldx x16__g2_phase
        lda 0x9f23                      // VERA_DATA0
    g2rd_shift:
        cpx #3                          // pixel 3 is already in bits 1:0
        beq g2rd_done
        lsr
        lsr
        inx
        jmp g2rd_shift
    g2rd_done:
        and #3
        sta x16__g2_c
    }
    return x16__g2_c;
}

// ---------------------------------------------------------------------
// Internal: horizontal span from the operand block, `len` pixels.
// Head and tail partials are read-modify-write; the whole bytes in
// between are one vera_fill.
// ---------------------------------------------------------------------
void x16__gfx2_hline_i(unsigned int len) {
    unsigned char p, q, head;
    unsigned int m;

    if (len == 0) {
        return;
    }
    x16__gfx2_addr();
    p = (unsigned char)(x16__g2_x & 3);

    // A head byte exists when the span starts mid-byte, or is so short
    // it begins and ends inside one byte.
    if (p != 0 || len < 4) {
        q = 3;                          // last pixel of the head byte
        if (len < 4 && p + len - 1 < 4) {
            q = (unsigned char)(p + len - 1);
        }
        head = q - p + 1;
        x16__g2_ink = x16__g2_cb;
        x16__g2_msk = x16__g2_from[p] & x16__g2_upto[q];
        x16__gfx2_rmw();
        len -= head;
        x16__gfx2_ainc();
    }

    m = len >> 2;                       // whole bytes
    if (m != 0) {
        x16__gfx2_aim0(X16_INC_1);
        x16_vera_fill(x16__g2_cb, m);
        x16__gfx2_aadd(m);
    }

    if ((len & 3) != 0) {               // tail: pixels 0..(len&3)-1
        x16__g2_ink = x16__g2_cb;
        x16__g2_msk = x16__g2_upto[(unsigned char)(len & 3) - 1];
        x16__gfx2_rmw();
    }
}

// ---------------------------------------------------------------------
// Public: horizontal span.
// ---------------------------------------------------------------------
void x16_gfx2_hline(unsigned int x, unsigned int y, unsigned int len,
                    unsigned char color) {
    x16__g2_x = x;
    x16__g2_y = y;
    x16__g2_cb = x16__g2_colbyte[color & 3];
    x16__gfx2_hline_i(len);
}

// ---------------------------------------------------------------------
// Public: vertical span. One column of read-modify-writes, both ports
// stepping a whole row per access -- no calls, so one asm block.
// ---------------------------------------------------------------------
void x16_gfx2_vline(unsigned int x, unsigned int y, unsigned int len,
                    unsigned char color) {
    if (len == 0) {
        return;
    }
    x16__g2_x = x;
    x16__g2_y = y;
    x16__g2_cb = x16__g2_colbyte[color & 3];
    x16__gfx2_addr();
    x16__gfx2_aim1(X16_INC_160);
    x16__gfx2_aim0(X16_INC_160);

    // ink and keep are loop-invariant: this column's pixel never moves.
    x16__g2_ink = x16__g2_cb & x16__g2_pix[(unsigned char)(x & 3)];
    x16__g2_msk = x16__g2_keep[(unsigned char)(x & 3)];

    x16__g2_x = len;                    // borrowed as the 16-bit counter
    __asm {
        ldx x16__g2_x                   // vera_fill's page-count idiom
        ldy x16__g2_x+1
        txa
        beq g2v_full                    // low byte 0 -> exactly hi*256
        iny
    g2v_full:
    g2v_loop:
        lda 0x9f24                      // VERA_DATA1
        and x16__g2_msk
        ora x16__g2_ink
        sta 0x9f23                      // VERA_DATA0
        dex
        bne g2v_loop
        dey
        bne g2v_loop
    }
}

// ---------------------------------------------------------------------
// Public: filled rectangle and outline.
// ---------------------------------------------------------------------
void x16_gfx2_rect(unsigned int x, unsigned int y, unsigned int w,
                   unsigned int h, unsigned char color) {
    unsigned int i;

    x16__g2_cb = x16__g2_colbyte[color & 3];
    for (i = 0; i < h; i++) {
        x16__g2_x = x;
        x16__g2_y = y + i;
        x16__gfx2_hline_i(w);
    }
}

void x16_gfx2_frame(unsigned int x, unsigned int y, unsigned int w,
                    unsigned int h, unsigned char color) {
    x16_gfx2_hline(x, y, w, color);
    x16_gfx2_hline(x, y + h - 1, w, color);
    x16_gfx2_vline(x, y, h, color);
    x16_gfx2_vline(x + w - 1, y, h, color);
}

// ---------------------------------------------------------------------
// Public: Bresenham, any direction. C, like x16/bitmap.c's gfx_line --
// it plots through the clipped pset, so the line clips for free.
// ---------------------------------------------------------------------
void x16_gfx2_line(unsigned int x0, unsigned int y0, unsigned int x1,
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

    // Written as subtractions rather than `d = 0 - d`: KickC constant-
    // folds a call with literal endpoints, and folding `0 - d` drops the
    // negation -- err then starts at dx+|dy| instead of dx-|dy| and the
    // walk never reaches its end point. Same value, folded correctly.
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

    x16__g2_c = color;
    for (;;) {
        x16__g2_x = (unsigned int)lx0;
        x16__g2_y = (unsigned int)ly0;
        x16__gfx2_pset_i();

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
void x16_gfx2_pattern_set(const unsigned char *pattern,
                          unsigned char colors) {
    unsigned char row, half, bit, acc, bits;

    x16__g2_pfg = x16__g2_colbyte[colors & 3];
    x16__g2_pbg = x16__g2_colbyte[(colors >> 2) & 3];

    for (row = 0; row < 8; row++) {
        bits = pattern[row];
        for (half = 0; half < 2; half++) {
            acc = 0;
            for (bit = 0; bit < 4; bit++) {
                // MSB first: a set bit is foreground.
                if (bits & 0x80) {
                    acc |= x16__g2_pfg & x16__g2_pix[bit];
                } else {
                    acc |= x16__g2_pbg & x16__g2_pix[bit];
                }
                bits <<= 1;
            }
            x16__g2_pat[row * 2 + half] = acc;
        }
    }
}

// ---------------------------------------------------------------------
// Internal: one pattern row at (g2_x, g2_y), `len` pixels wide.
// ---------------------------------------------------------------------
void x16__gfx2_prow(unsigned int len) {
    unsigned char p, q, head, swap;
    unsigned int m;

    if (len == 0) {
        return;
    }
    x16__gfx2_addr();

    // The row's two pattern bytes, in address-parity order.
    swap = (unsigned char)(x16__g2_y & 7) * 2;
    if ((x16__g2_a0 & 1) != 0) {
        x16__g2_pb0 = x16__g2_pat[swap + 1];
        x16__g2_pb1 = x16__g2_pat[swap];
    } else {
        x16__g2_pb0 = x16__g2_pat[swap];
        x16__g2_pb1 = x16__g2_pat[swap + 1];
    }

    p = (unsigned char)(x16__g2_x & 3);
    if (p != 0 || len < 4) {
        q = 3;
        if (len < 4 && p + len - 1 < 4) {
            q = (unsigned char)(p + len - 1);
        }
        head = q - p + 1;
        x16__g2_ink = x16__g2_pb0;
        x16__g2_msk = x16__g2_from[p] & x16__g2_upto[q];
        x16__gfx2_rmw();
        len -= head;
        x16__gfx2_ainc();
        swap = x16__g2_pb0;             // the next byte flips parity
        x16__g2_pb0 = x16__g2_pb1;
        x16__g2_pb1 = swap;
    }

    m = len >> 2;
    if (m != 0) {
        x16__gfx2_aim0(X16_INC_1);
        x16__g2_x = m;                  // borrowed as the counter
        __asm {
            ldx x16__g2_x
            ldy x16__g2_x+1
            txa
            beq g2p_full
            iny
        g2p_full:
        g2p_loop:
            lda x16__g2_pb0
            sta 0x9f23                  // VERA_DATA0
            lda x16__g2_pb0             // swap the parity pair
            ldx x16__g2_pb1
            sta x16__g2_pb1
            stx x16__g2_pb0
            ldx x16__g2_x
            dex
            stx x16__g2_x
            bne g2p_loop
            dey
            bne g2p_loop
        }
        x16__gfx2_aadd(m);
    }

    if ((len & 3) != 0) {
        x16__g2_ink = x16__g2_pb0;
        x16__g2_msk = x16__g2_upto[(unsigned char)(len & 3) - 1];
        x16__gfx2_rmw();
    }
}

void x16_gfx2_pattern_rect(unsigned int x, unsigned int y, unsigned int w,
                           unsigned int h) {
    unsigned int i;

    for (i = 0; i < h; i++) {
        x16__g2_x = x;
        x16__g2_y = y + i;
        x16__gfx2_prow(w);
    }
}

// ---------------------------------------------------------------------
// Public: byte-aligned image copy with a raster op.
//
// The op is dispatched in C so each inner loop is its own asm block:
// a block that ended in a shared `done` label would end the procedure
// on a label, which KickC drops.
// ---------------------------------------------------------------------
void x16_gfx2_blit(unsigned int x, unsigned int y, unsigned char wbytes,
                   unsigned char h, const unsigned char *src,
                   unsigned char op) {
    unsigned char row;

    x16__g2_x = x;
    x16__g2_y = y;
    x16__g2_n = wbytes;
    x16__gfx2_addr();

    for (row = 0; row < h; row++) {
        x16__gfx2_aim1(X16_INC_1);      // ops read port 1...
        x16__gfx2_aim0(X16_INC_1);      // ...everything writes port 0
        if (op == 1) {
            __asm {
                ldy #0
            g2b_or:
                lda 0x9f24              // VERA_DATA1
                ora (src),y
                sta 0x9f23              // VERA_DATA0
                iny
                cpy x16__g2_n
                bne g2b_or
            }
        } else if (op == 2) {
            __asm {
                ldy #0
            g2b_and:
                lda 0x9f24              // VERA_DATA1
                and (src),y
                sta 0x9f23              // VERA_DATA0
                iny
                cpy x16__g2_n
                bne g2b_and
            }
        } else if (op == 3) {
            __asm {
                ldy #0
            g2b_xor:
                lda 0x9f24              // VERA_DATA1
                eor (src),y
                sta 0x9f23              // VERA_DATA0
                iny
                cpy x16__g2_n
                bne g2b_xor
            }
        } else {
            __asm {
                ldy #0
            g2b_copy:
                lda (src),y
                sta 0x9f23              // VERA_DATA0
                iny
                cpy x16__g2_n
                bne g2b_copy
            }
        }
        src = src + wbytes;
        x16__gfx2_aadd(X16_GFX2_STRIDE);    // down one row
    }
}

// ---------------------------------------------------------------------
// Public: masked blit of pre-shifted column-major data. For each of
// `cols` byte columns, `h` (mask, data) pairs walk down the rows.
// ---------------------------------------------------------------------
void x16_gfx2_blitm(unsigned int x, unsigned int y, unsigned char h,
                    unsigned char cols, const unsigned char *src) {
    unsigned char col;

    x16__g2_x = x;
    x16__g2_y = y;
    x16__g2_n = h;
    x16__gfx2_addr();

    for (col = 0; col < cols; col++) {
        x16__gfx2_aim1(X16_INC_160);
        x16__gfx2_aim0(X16_INC_160);
        __asm {
            ldy #0
            ldx x16__g2_n
        g2m_row:
            lda 0x9f24                  // VERA_DATA1
            and (src),y         // mask byte
            iny
            ora (src),y         // data byte
            iny
            sta 0x9f23                  // VERA_DATA0
            dex
            bne g2m_row
        }
        src = src + h + h;                      // one column of pairs
        x16__gfx2_ainc();                       // next byte column
    }
}
