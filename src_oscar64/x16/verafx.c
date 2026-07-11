// =====================================================================
// x16clib :: x16/verafx.c -- VERA FX: hardware multiply, fast fills,
//                            hardware lines and filled triangles
// =====================================================================
// Requires VERA firmware v0.3.1+ (emulator R44+). Probe with
// x16_vera_has_fx() first; on older VERA these write to registers that
// do not exist and quietly do the wrong thing.
//
// The FX registers are $9F29-$9F2C banked behind DCSEL 2..6; every
// dcsel here preserves ADDRSEL (lda CTRL / and #1 / ora #n<<1). Every
// routine leaves FX disabled and DCSEL at 0 -- a lingering Addr1 Mode
// would silently change ordinary VRAM addressing for everyone.
//
// The same hand-written 6502 as src_ca65/gfx/verafx.s, with the shared
// helpers (udiv24, pix_addr) duplicated into the line and triangle
// bodies: inline asm cannot jsr across functions. The two hardware
// facts the FX reference does not tell you -- ADDRx writes prefetch
// through a live line mode, and the increment write does not clear the
// position's carry -- are honoured in the same load-bearing order.
// =====================================================================

#include <x16/verafx.h>

// The triangle's vertices are copied straight through the pointer
// parameters (renamed pa/pb/pc: `a` reads as the accumulator to an
// inline assembler).

// tri_x0..tri_y2 as one block, three bytes per vertex (x lo, x hi, y):
// the vertex copy is a straight three-byte loop per point.
volatile char x16__tri[9];
volatile char x16__tri_c;

volatile char x16__fxl_dx[2];
volatile char x16__fxl_dy[2];
volatile char x16__fxl_major[2];
volatile char x16__fxl_minor[2];
volatile char x16__fxl_v[2];
volatile char x16__fxl_sx;
volatile char x16__fxl_sy;
volatile char x16__fxl_h1;
volatile char x16__fxl_h0;

volatile char x16__fxt_n1;
volatile char x16__fxt_n2;
volatile char x16__fxt_swap;
volatile char x16__fxt_xl;        // encoded increments, two slots
volatile char x16__fxt_xh;
volatile char x16__fxt_yl;
volatile char x16__fxt_yh;
volatile char x16__fxt_px[2];     // starting x of each edge
volatile char x16__fxt_py[2];
volatile char x16__fxt_a_l;       // the long edge, parked
volatile char x16__fxt_a_h;
volatile char x16__fxt_a_sgn;
volatile char x16__fxt_a_mag[3];
volatile char x16__fxt_rows;
volatile char x16__fxt_fl;
volatile char x16__fxt_fh;
volatile char x16__fxt_len[2];

volatile char x16__fxs_dxl;
volatile char x16__fxs_dxh;
volatile char x16__fxs_dy;
volatile char x16__fxs_sgn;
volatile char x16__fxs_32;
volatile char x16__fxs_mag[3];
volatile char x16__fxs_el;
volatile char x16__fxs_eh;

volatile char x16__fxd_num[3];
volatile char x16__fxd_den[2];
volatile char x16__fxd_rem[2];

volatile char x16__fxa[3];

// Disable FX and return DCSEL to 0. Safe whether or not FX was ever on.
void x16_fx_off(void) {
    __asm {
        lda 0x9f25         /* vera_dcsel 2 (VERA_CTRL) */
        and #0x01
        ora #0x04                        /* 2 << 1 */
        sta 0x9f25
        lda #0
        sta 0x9f29 /* cache/transparency/Addr1 mode off (VERA_FX_CTRL) */
        sta 0x9f2c /* multiplier off (VERA_FX_MULT) */
        lda 0x9f25                       /* vera_dcsel 0 */
        and #0x01
        sta 0x9f25
    }
}

// ---------------------------------------------------------------------
// Signed 16 x 16 -> 32, in hardware. The operands go into the halves of
// the 32-bit cache; the result is not readable from a register, so
// triggering the multiply writes four bytes to VRAM_FX_SCRATCH and they
// are read back. Only ADDR0/DATA0 is used: VERA pre-fetches whenever an
// address pointer changes, so touching the same VRAM through the other
// port would risk a stale latch.
// ---------------------------------------------------------------------
// `ma`/`mb`, not `a`/`b`: `a` reads as the accumulator to an inline
// assembler (KickC crashed outright on `lda a`; the rename stays).
long x16_fx_mult(int ma, int mb) {
    __asm {
        lda 0x9f25                       /* vera_dcsel 2 */
        and #0x01
        ora #0x04
        sta 0x9f25
        lda #0
        sta 0x9f29 /* Addr1 Mode 0 (VERA_FX_CTRL) */
        lda #0x10                       /* VERA_FX_MULT_ENABLE */
        sta 0x9f2c                      /* VERA_FX_MULT */

        lda 0x9f25                       /* vera_dcsel 6 */
        and #0x01
        ora #0x0c                        /* 6 << 1 */
        sta 0x9f25
        /* A READ of 0x9F29 clears the accumulator. `bit`, not `lda`: */
        /* the value is unused, and `bit` performs the same bus read */
        /* without looking like a dead load to any optimizer (KickC */
        /* 0.8.6 crashed eliminating the lda form). */
        bit 0x9f29                      /* VERA_FX_ACCUM_RESET */
        lda ma
        sta 0x9f29                      /* VERA_FX_CACHE_L */
        lda ma+1
        sta 0x9f2a   /* cache 15:0  = a (VERA_FX_CACHE_M) */
        lda mb
        sta 0x9f2b                      /* VERA_FX_CACHE_H */
        lda mb+1
        sta 0x9f2c   /* cache 31:16 = b (VERA_FX_CACHE_U) */

        lda 0x9f25                       /* vera_dcsel 2 */
        and #0x01
        ora #0x04
        sta 0x9f25
        lda #0x40                       /* VERA_FX_CACHE_WRITE */
        sta 0x9f29      /* multiplier on: writes the product (VERA_FX_CTRL) */

        /* Trigger: any store to DATA0 emits the 32-bit result at */
        /* VRAM_FX_SCRATCH (0x1F800). The stored value is ignored. */
        lda 0x9f25
        and #0xfe
        sta 0x9f25
        lda #0
        sta 0x9f20 /* <0xF800 (VERA_ADDR_L) */
        lda #0xf8
        sta 0x9f21                      /* VERA_ADDR_M */
        lda #0x01                        /* bank 1, INC_0 */
        sta 0x9f22                      /* VERA_ADDR_H */
        lda #0
        sta 0x9f23 /* VERA_DATA0 */

    }
    x16_fx_off();
    /* Read it back, now advancing one byte at a time. fx_off touches
       only the FX registers, never ADDR0/DATA0, so the readback can
       wait until after it. */
    return __asm {
        lda 0x9f25
        and #0xfe
        sta 0x9f25
        lda #0
        sta 0x9f20
        lda #0xf8
        sta 0x9f21
        lda #0x11                        /* bank 1, INC_1 */
        sta 0x9f22
        lda 0x9f23
        sta accu
        lda 0x9f23
        sta accu + 1
        lda 0x9f23
        sta accu + 2
        lda 0x9f23
        sta accu + 3
    };
}

// ---------------------------------------------------------------------
// Fill VRAM through the 32-bit write cache (~4x a byte loop). With
// Cache Write Enable set, one store to DATA0 writes all four cache
// bytes; stepping the port by 4 covers the region a quad at a time. The
// remaining 1-3 bytes are written normally with FX back off.
// ---------------------------------------------------------------------
void x16_fx_fill(unsigned char value, unsigned int count,
                 unsigned long addr) {
    __asm {
        lda 0x9f25                       /* vera_dcsel 2 */
        and #0x01
        ora #0x04
        sta 0x9f25
        lda #0
        sta 0x9f2c /* multiplier off: write the cache (VERA_FX_MULT) */
        lda #0x40                       /* VERA_FX_CACHE_WRITE */
        sta 0x9f29                      /* VERA_FX_CTRL */

        lda 0x9f25                       /* vera_dcsel 6 */
        and #0x01
        ora #0x0c
        sta 0x9f25
        lda value
        sta 0x9f29                      /* VERA_FX_CACHE_L */
        sta 0x9f2a                      /* VERA_FX_CACHE_M */
        sta 0x9f2b                      /* VERA_FX_CACHE_H */
        sta 0x9f2c                      /* VERA_FX_CACHE_U */
        lda 0x9f25                       /* vera_dcsel 0 */
        and #0x01
        sta 0x9f25

        /* Point port 0 at the destination, stepping 4 bytes per write. */
        lda 0x9f25
        and #0xfe
        sta 0x9f25
        lda addr
        sta 0x9f20                      /* VERA_ADDR_L */
        lda addr+1
        sta 0x9f21                      /* VERA_ADDR_M */
        lda addr+2
        and #0x01                       /* VERA_ADDR_H_BANK */
        ora #0x30                        /* VERA_INC_4 << 4 */
        sta 0x9f22                      /* VERA_ADDR_H */

        /* quads = count >> 2, remainder = count & 3 */
        lda count
        and #0x03
        sta x16__fxt_fl                 /* borrow a scratch byte */
        lda count+1
        sta x16__fxt_len+1
        lda count
        sta x16__fxt_len
        lsr x16__fxt_len+1
        ror x16__fxt_len
        lsr x16__fxt_len+1
        ror x16__fxt_len

        lda x16__fxt_len
        ora x16__fxt_len+1
        beq ffq_tail                    /* fewer than four bytes */

        ldx x16__fxt_len
        ldy x16__fxt_len+1
        lda #0                          /* the value every write pushes */
        cpx #0
        beq ffq_loop
        iny
    ffq_loop:
        sta 0x9f23        /* writes the four cache bytes (VERA_DATA0) */
        dex
        bne ffq_loop
        dey
        bne ffq_loop

    ffq_tail:
        /* FX off first: the leftovers must be written singly. */
        lda 0x9f25                       /* vera_dcsel 2 */
        and #0x01
        ora #0x04
        sta 0x9f25
        lda #0
        sta 0x9f29 /* VERA_FX_CTRL */
        lda 0x9f25                       /* vera_dcsel 0 */
        and #0x01
        sta 0x9f25

        lda x16__fxt_fl
        beq ffq_done

        /* Port 0 sits just past the quads: keep its bank and DECR bits, */
        /* switch the increment back to 1. */
        lda 0x9f22                      /* VERA_ADDR_H */
        and #0x0f
        ora #0x10                        /* VERA_INC_1 << 4 */
        sta 0x9f22

        ldx x16__fxt_fl
        lda value
    ffq_rest:
        sta 0x9f23                      /* VERA_DATA0 */
        dex
        bne ffq_rest
    ffq_done:
    }
}

void x16_fx_clear(unsigned int count, unsigned long addr) {
    x16_fx_fill(0, count, addr);
}

// ---------------------------------------------------------------------
// VRAM to VRAM through the 32-bit cache (~4x a byte loop). Each DATA1
// read latches a byte into the cache; after four, one DATA0 write
// (mask 0) flushes all four to the 4-BYTE ALIGNED destination. The
// source needs no alignment; the 0-3 leftovers copy singly with FX off.
// ---------------------------------------------------------------------
void x16_fx_copy(unsigned long src, unsigned long dst,
                 unsigned int count) {
    __asm {
        lda 0x9f25                       /* vera_dcsel 2 */
        and #0x01
        ora #0x04
        sta 0x9f25
        lda #0
        sta 0x9f29 /* mode 0 while the ports are aimed (VERA_FX_CTRL) */
        sta 0x9f2c /* multiplier off, cache index to 0 (VERA_FX_MULT) */

        lda 0x9f25
        ora #0x01
        sta 0x9f25
        lda src
        sta 0x9f20                      /* VERA_ADDR_L */
        lda src+1
        sta 0x9f21                      /* VERA_ADDR_M */
        lda src+2
        and #0x01
        ora #0x10                        /* INC_1 */
        sta 0x9f22                      /* VERA_ADDR_H */
        lda 0x9f25
        and #0xfe
        sta 0x9f25
        lda dst
        sta 0x9f20
        lda dst+1
        sta 0x9f21
        lda dst+2
        and #0x01
        ora #0x30                        /* INC_4 */
        sta 0x9f22

        lda 0x9f25                       /* vera_dcsel 2 */
        and #0x01
        ora #0x04
        sta 0x9f25
        lda #0x60                        /* CACHE_FILL | CACHE_WRITE */
        sta 0x9f29                      /* VERA_FX_CTRL */

        /* quads = count >> 2, remainder = count & 3 */
        lda count
        and #0x03
        sta x16__fxt_fl
        lda count+1
        sta x16__fxt_len+1
        lda count
        sta x16__fxt_len
        lsr x16__fxt_len+1
        ror x16__fxt_len
        lsr x16__fxt_len+1
        ror x16__fxt_len

        lda x16__fxt_len
        ora x16__fxt_len+1
        beq fcp_tail

        ldx x16__fxt_len
        ldy x16__fxt_len+1
        lda #0                          /* the cache-flush write value */
        cpx #0
        beq fcp_quad
        iny
    fcp_quad:
        /* four reads fill the cache; `bit`, not `lda`: an optimizer */
        /* that collapses the "redundant" loads to one (KickC's did) */
        /* leaves three cache bytes unlatched. Any DATA1 read works. */
        bit 0x9f24                      /* VERA_DATA1 */
        bit 0x9f24
        bit 0x9f24
        bit 0x9f24
        sta 0x9f23        /* ...one write flushes it (mask 0) (VERA_DATA0) */
        dex
        bne fcp_quad
        dey
        bne fcp_quad

    fcp_tail:
        lda 0x9f25                       /* vera_dcsel 2 */
        and #0x01
        ora #0x04
        sta 0x9f25
        lda #0
        sta 0x9f29 /* leftovers are plain byte copies (VERA_FX_CTRL) */
        lda 0x9f25                       /* vera_dcsel 0 */
        and #0x01
        sta 0x9f25

        lda x16__fxt_fl
        beq fcp_done
        lda 0x9f22       /* port 0 sits just past the quads: (VERA_ADDR_H) */
        and #0x0f                        /* step it by 1 for the tail */
        ora #0x10
        sta 0x9f22
        ldx x16__fxt_fl
    fcp_rest:
        lda 0x9f24                      /* VERA_DATA1 */
        sta 0x9f23                      /* VERA_DATA0 */
        dex
        bne fcp_rest
    fcp_done:
        lda 0x9f25  /* leave ADDRSEL 0 */
        and #0xfe
        sta 0x9f25
    }
}

void x16_fx_transp_on(void) {
    __asm {
        lda 0x9f25                       /* vera_dcsel 2 */
        and #0x01
        ora #0x04
        sta 0x9f25
        lda 0x9f29                      /* VERA_FX_CTRL */
        ora #0x80                       /* VERA_FX_TRANSPARENT */
        sta 0x9f29
        lda 0x9f25                       /* vera_dcsel 0 */
        and #0x01
        sta 0x9f25
    }
}

void x16_fx_transp_off(void) {
    __asm {
        lda 0x9f25
        and #0x01
        ora #0x04
        sta 0x9f25
        lda 0x9f29                      /* VERA_FX_CTRL */
        and #0x7f                        /* clear VERA_FX_TRANSPARENT */
        sta 0x9f29
        lda 0x9f25
        and #0x01
        sta 0x9f25
    }
}

// ---------------------------------------------------------------------
// Hardware line (Addr1 Mode 1). VERA tracks the Bresenham error
// internally: ADDR1 steps one pixel along the major axis on every DATA1
// write, and a 9.9 accumulator carries it along the minor axis when the
// slope fraction overflows. The CPU's whole job is one sta per pixel.
// Assumes the 320x240@8bpp framebuffer. Does NOT clip.
// ---------------------------------------------------------------------
void x16_fx_line(unsigned int x0, unsigned char y0,
                 unsigned int x1, unsigned char y1,
                 unsigned char color) {
    /* |dx| and the x direction */
    __asm {
        lda #0
        sta x16__fxl_sx
        sec
        lda x1
        sbc x0
        sta x16__fxl_dx
        lda x1+1
        sbc x0+1
        sta x16__fxl_dx+1
        bpl fxl_dx_done
        inc x16__fxl_sx                 /* x runs right to left */
        sec
        lda #0
        sbc x16__fxl_dx
        sta x16__fxl_dx
        lda #0
        sbc x16__fxl_dx+1
        sta x16__fxl_dx+1
    fxl_dx_done:

        /* |dy| and the y direction, 16-bit (239-0 overflows a byte) */
        lda #0
        sta x16__fxl_sy
        sec
        lda y1
        sbc y0
        sta x16__fxl_dy
        lda #0
        sbc #0
        sta x16__fxl_dy+1
        bpl fxl_dy_done
        inc x16__fxl_sy
        sec
        lda #0
        sbc x16__fxl_dy
        sta x16__fxl_dy
        lda #0
        sbc x16__fxl_dy+1
        sta x16__fxl_dy+1
    fxl_dy_done:

        /* pick the octant: ADDR1 steps the major axis every pixel, */
        /* ADDR0's increment is borrowed for the minor sometimes-step */
        lda x16__fxl_dy+1
        cmp x16__fxl_dx+1
        bne fxl_which
        lda x16__fxl_dy
        cmp x16__fxl_dx
    fxl_which:
        bcc fxl_x_major

        lda x16__fxl_dy                 /* Y-major */
        sta x16__fxl_major
        lda x16__fxl_dy+1
        sta x16__fxl_major+1
        lda x16__fxl_dx
        sta x16__fxl_minor
        lda x16__fxl_dx+1
        sta x16__fxl_minor+1
        lda #0xe0                        /* VERA_INC_320 << 4 */
        ldx x16__fxl_sy
        beq fxl_ym1
        ora #0x08                       /* VERA_ADDR_H_DECR */
    fxl_ym1:
        sta x16__fxl_h1                 /* ADDR1: a row per step */
        lda #0x10                        /* VERA_INC_1 << 4 */
        ldx x16__fxl_sx
        beq fxl_ym0
        ora #0x08
    fxl_ym0:
        sta x16__fxl_h0                 /* ADDR0: a pixel, sometimes */
        jmp fxl_slope

    fxl_x_major:
        lda x16__fxl_dx
        sta x16__fxl_major
        lda x16__fxl_dx+1
        sta x16__fxl_major+1
        lda x16__fxl_dy
        sta x16__fxl_minor
        lda x16__fxl_dy+1
        sta x16__fxl_minor+1
        lda #0x10
        ldx x16__fxl_sx
        beq fxl_xm1
        ora #0x08
    fxl_xm1:
        sta x16__fxl_h1
        lda #0xe0
        ldx x16__fxl_sy
        beq fxl_xm0
        ora #0x08
    fxl_xm0:
        sta x16__fxl_h0

    fxl_slope:
        /* slope = minor/major in 1/512ths; a point has no slope */
        lda #0
        sta x16__fxl_v
        sta x16__fxl_v+1
        lda x16__fxl_major
        ora x16__fxl_major+1
        beq fxl_program
        lda #0
        sta x16__fxd_num /* dividend = minor * 512 */
        lda x16__fxl_minor
        asl
        sta x16__fxd_num+1
        lda x16__fxl_minor+1
        rol
        sta x16__fxd_num+2
        lda x16__fxl_major
        sta x16__fxd_den
        lda x16__fxl_major+1
        sta x16__fxd_den+1
        jsr fxl_udiv24
        lda x16__fxd_num
        sta x16__fxl_v
        lda x16__fxd_num+1
        sta x16__fxl_v+1

    fxl_program:
        jsr fxl_pix_addr                /* fxa = address of (x0, y0) */

        /* An axis-aligned line (minor delta 0) is just a run along */
        /* port 1's increment -- no FX needed. */
        lda x16__fxl_minor
        ora x16__fxl_minor+1
        beq fxl_plain

        /* ORDER IS LOAD-BEARING: all addresses while the mode is still */
        /* off, then the mode, and the slope very last. */
        lda 0x9f25                       /* vera_dcsel 2 */
        and #0x01
        ora #0x04
        sta 0x9f25
        lda #0
        sta 0x9f29      /* Addr1 mode 0 while addressing (VERA_FX_CTRL) */
        jsr fxl_set_addr1
        lda 0x9f25
        and #0xfe
        sta 0x9f25
        lda x16__fxl_h0
        sta 0x9f22                      /* VERA_ADDR_H */
        lda 0x9f25                       /* vera_dcsel 2 */
        and #0x01
        ora #0x04
        sta 0x9f25
        lda #0x01                       /* VERA_FX_ADDR1_LINE */
        sta 0x9f29                      /* VERA_FX_CTRL */
        lda 0x9f25                       /* vera_dcsel 3 */
        and #0x01
        ora #0x06
        sta 0x9f25
        lda x16__fxl_v
        sta 0x9f29                      /* VERA_FX_X_INCR_L */
        lda x16__fxl_v+1
        sta 0x9f2a  /* seeds the fraction to 0.5... (VERA_FX_X_INCR_H) */
        lda 0x9f25                       /* vera_dcsel 4 */
        and #0x01
        ora #0x08
        sta 0x9f25
        lda #0       /* ...but NOT the carry bit: a leftover carry */
        sta 0x9f29   /* would eat the line's first (VERA_FX_X_POS_L) */
        sta 0x9f2a   /* minor step (VERA_FX_X_POS_H) */
        jmp fxl_count                   /* line's first minor-step */

    fxl_plain:
        jsr fxl_set_addr1

    fxl_count:
        /* draw major+1 pixels */
        clc
        lda x16__fxl_major
        adc #1
        tax
        lda x16__fxl_major+1
        adc #0
        tay
        txa
        beq fxl_full
        iny
    fxl_full:
        lda color
    fxl_draw:
        sta 0x9f24                      /* VERA_DATA1 */
        dex
        bne fxl_draw
        dey
        bne fxl_draw
        jmp fxl_end

        /* point port 1 at the start pixel, major-axis increment */
    fxl_set_addr1:
        lda 0x9f25
        ora #0x01
        sta 0x9f25
        lda x16__fxa
        sta 0x9f20                      /* VERA_ADDR_L */
        lda x16__fxa+1
        sta 0x9f21                      /* VERA_ADDR_M */
        lda x16__fxa+2
        and #0x01
        ora x16__fxl_h1
        sta 0x9f22                      /* VERA_ADDR_H */
        rts

        /* fxa = x0 + y0*320 (17-bit); y*320 = y*64 + y*256 */
    fxl_pix_addr:
        lda #0
        sta x16__fxt_fh                 /* borrow a scratch byte */
        lda y0
        asl
        rol x16__fxt_fh
        asl
        rol x16__fxt_fh
        asl
        rol x16__fxt_fh
        asl
        rol x16__fxt_fh
        asl
        rol x16__fxt_fh
        asl
        rol x16__fxt_fh
        sta x16__fxa
        clc
        lda y0
        adc x16__fxt_fh
        sta x16__fxa+1
        lda #0
        adc #0
        sta x16__fxa+2
        clc
        lda x16__fxa
        adc x0
        sta x16__fxa
        lda x16__fxa+1
        adc x0+1
        sta x16__fxa+1
        lda x16__fxa+2
        adc #0
        sta x16__fxa+2
        rts

        /* fxd_num(24) / fxd_den(16) -> quotient in fxd_num */
    fxl_udiv24:
        ldx #0
        stx x16__fxd_rem
        stx x16__fxd_rem+1
        ldx #24
    fxl_dv:
        asl x16__fxd_num
        rol x16__fxd_num+1
        rol x16__fxd_num+2
        rol x16__fxd_rem
        rol x16__fxd_rem+1
        sec
        lda x16__fxd_rem
        sbc x16__fxd_den
        tay
        lda x16__fxd_rem+1
        sbc x16__fxd_den+1
        bcc fxl_dv_no
        sta x16__fxd_rem+1
        sty x16__fxd_rem
        inc x16__fxd_num
    fxl_dv_no:
        dex
        bne fxl_dv
        rts

    fxl_end:
    }
    x16_fx_off();
}

// ---------------------------------------------------------------------
// Filled triangle via the polygon helper (Addr1 Mode 2). VERA walks two
// edges at once, each advanced by its own signed slope twice per row
// (hence: HALF the per-row increment is programmed). Reading DATA1
// latches a row and points ADDR1 at its left edge; the CPU fills the
// span and a DATA0 read advances to the next row.
//
// Vertices in any order; rasterisation is half-open (the bottom row is
// not drawn), so triangles sharing an edge do not double-paint it.
// Assumes the 320x240@8bpp framebuffer. Does NOT clip.
// ---------------------------------------------------------------------
void x16_fx_triangle(const x16_point *pa, const x16_point *pb,
                     const x16_point *pc, unsigned char color) {
    x16__tri_c = color;
    __asm {
        ldy #2
    fxt_ca:
        lda (pa),y
        sta x16__tri,y
        dey
        bpl fxt_ca
    }
    __asm {
        ldy #2
    fxt_cb:
        lda (pb),y
        sta x16__tri+3,y
        dey
        bpl fxt_cb
    }
    __asm {
        ldy #2
    fxt_cc:
        lda (pc),y
        sta x16__tri+6,y
        dey
        bpl fxt_cc
    }
    // The vertex block: x0=+0/+1 y0=+2, x1=+3/+4 y1=+5, x2=+6/+7 y2=+8.
    /* sort the vertices by y (three compare-swaps) */
    __asm {
        lda x16__tri+5                  /* y1 < y0 ? */
        cmp x16__tri+2
        bcs fxt_s1
        jsr fxt_swap01
    fxt_s1:
        lda x16__tri+8                  /* y2 < y1 ? */
        cmp x16__tri+5
        bcs fxt_s2
        jsr fxt_swap12
    fxt_s2:
        lda x16__tri+5
        cmp x16__tri+2
        bcs fxt_s3
        jsr fxt_swap01
    fxt_s3:
        sec                             /* row counts of the two parts */
        lda x16__tri+5
        sbc x16__tri+2
        sta x16__fxt_n1
        sec
        lda x16__tri+8
        sbc x16__tri+5
        sta x16__fxt_n2
        lda x16__fxt_n1
        ora x16__fxt_n2
        bne fxt_go
        jmp fxt_end                     /* a single row: nothing (half-open) */
    fxt_go:
        /* slope of the long edge v0 -> v2 (always needed) */
        lda x16__fxt_n1
        clc
        adc x16__fxt_n2
        sta x16__fxs_dy
        sec
        lda x16__tri+6
        sbc x16__tri
        sta x16__fxs_dxl
        lda x16__tri+7
        sbc x16__tri+1
        sta x16__fxs_dxh
        jsr fxt_slope
        /* park it as edge A */
        lda x16__fxs_el
        sta x16__fxt_a_l
        lda x16__fxs_eh
        sta x16__fxt_a_h
        lda x16__fxs_sgn
        sta x16__fxt_a_sgn
        lda x16__fxs_mag
        sta x16__fxt_a_mag
        lda x16__fxs_mag+1
        sta x16__fxt_a_mag+1
        lda x16__fxs_mag+2
        sta x16__fxt_a_mag+2

        lda x16__fxt_n1
        bne fxt_two_parts
        jmp fxt_flat_top                /* out of branch range from here */
    fxt_two_parts:

        /* slope of the top short edge v0 -> v1 */
        lda x16__fxt_n1
        sta x16__fxs_dy
        sec
        lda x16__tri+3
        sbc x16__tri
        sta x16__fxs_dxl
        lda x16__tri+4
        sbc x16__tri+1
        sta x16__fxs_dxh
        jsr fxt_slope                   /* edge B, still in fxs_* */

        jsr fxt_b_lt_a                  /* carry set: B is the left edge */
        bcs fxt_b_left
        lda x16__fxt_a_l                /* A (long) left in the X slot, */
        sta x16__fxt_xl                 /* B right in the Y/X2 slot */
        lda x16__fxt_a_h
        sta x16__fxt_xh
        lda x16__fxs_el
        sta x16__fxt_yl
        lda x16__fxs_eh
        sta x16__fxt_yh
        lda #1
        sta x16__fxt_swap               /* part 2 replaces the Y/X2 slot */
        jmp fxt_pos
    fxt_b_left:
        lda x16__fxs_el                 /* B left, A (long) right */
        sta x16__fxt_xl
        lda x16__fxs_eh
        sta x16__fxt_xh
        lda x16__fxt_a_l
        sta x16__fxt_yl
        lda x16__fxt_a_h
        sta x16__fxt_yh
        lda #0
        sta x16__fxt_swap               /* part 2 replaces the X slot */
    fxt_pos:
        lda x16__tri                    /* both edges start at the apex */
        sta x16__fxt_px
        sta x16__fxt_py
        lda x16__tri+1
        sta x16__fxt_px+1
        sta x16__fxt_py+1
        jsr fxt_poly_setup
        lda x16__fxt_n1
        jsr fxt_poly_rows

        lda x16__fxt_n2
        bne fxt_have_part2
        jmp fxt_end                     /* flat bottom: one part was all */
    fxt_have_part2:

        /* part 2: the finished short edge becomes v1 -> v2 */
        lda x16__fxt_n2
        sta x16__fxs_dy
        sec
        lda x16__tri+6
        sbc x16__tri+3
        sta x16__fxs_dxl
        lda x16__tri+7
        sbc x16__tri+4
        sta x16__fxs_dxh
        jsr fxt_slope
        lda 0x9f25                       /* vera_dcsel 3 */
        and #0x01
        ora #0x06
        sta 0x9f25
        lda x16__fxt_swap
        beq fxt_repl_x
        lda x16__fxs_el
        sta 0x9f2b                      /* VERA_FX_Y_INCR_L */
        lda x16__fxs_eh
        sta 0x9f2c  /* resets that edge's subpixel (VERA_FX_Y_INCR_H) */
        lda 0x9f25                       /* vera_dcsel 4 */
        and #0x01
        ora #0x08
        sta 0x9f25
        lda x16__tri+3
        sta 0x9f2b                      /* VERA_FX_Y_POS_L */
        lda x16__tri+4
        and #0x07
        sta 0x9f2c                      /* VERA_FX_Y_POS_H */
        jmp fxt_part2
    fxt_repl_x:
        lda x16__fxs_el
        sta 0x9f29                      /* VERA_FX_X_INCR_L */
        lda x16__fxs_eh
        sta 0x9f2a                      /* VERA_FX_X_INCR_H */
        lda 0x9f25                       /* vera_dcsel 4 */
        and #0x01
        ora #0x08
        sta 0x9f25
        lda x16__tri+3
        sta 0x9f29                      /* VERA_FX_X_POS_L */
        lda x16__tri+4
        and #0x07
        sta 0x9f2a                      /* VERA_FX_X_POS_H */
    fxt_part2:
        lda 0x9f25                       /* vera_dcsel 5: fill-length window */
        and #0x01
        ora #0x0a
        sta 0x9f25
        lda x16__fxt_n2
        jsr fxt_poly_rows
        jmp fxt_end

    fxt_flat_top:
        /* v0 and v1 share the top row; the second edge is v1 -> v2 */
        lda x16__fxt_n2
        sta x16__fxs_dy
        sec
        lda x16__tri+6
        sbc x16__tri+3
        sta x16__fxs_dxl
        lda x16__tri+7
        sbc x16__tri+4
        sta x16__fxs_dxh
        jsr fxt_slope                   /* edge B = v1 -> v2 */

        lda x16__tri+1                  /* the leftmost vertex owns X */
        cmp x16__tri+4
        bne fxt_ft_pick
        lda x16__tri
        cmp x16__tri+3
    fxt_ft_pick:
        bcc fxt_ft_v0_left
        lda x16__fxs_el                 /* v1 left: B in X at x1, A in Y */
        sta x16__fxt_xl
        lda x16__fxs_eh
        sta x16__fxt_xh
        lda x16__fxt_a_l
        sta x16__fxt_yl
        lda x16__fxt_a_h
        sta x16__fxt_yh
        lda x16__tri+3
        sta x16__fxt_px
        lda x16__tri+4
        sta x16__fxt_px+1
        lda x16__tri
        sta x16__fxt_py
        lda x16__tri+1
        sta x16__fxt_py+1
        jmp fxt_ft_run
    fxt_ft_v0_left:
        lda x16__fxt_a_l                /* v0 left: A in X at x0, B in Y */
        sta x16__fxt_xl
        lda x16__fxt_a_h
        sta x16__fxt_xh
        lda x16__fxs_el
        sta x16__fxt_yl
        lda x16__fxs_eh
        sta x16__fxt_yh
        lda x16__tri
        sta x16__fxt_px
        lda x16__tri+1
        sta x16__fxt_px+1
        lda x16__tri+3
        sta x16__fxt_py
        lda x16__tri+4
        sta x16__fxt_py+1
    fxt_ft_run:
        jsr fxt_poly_setup
        lda x16__fxt_n2
        jsr fxt_poly_rows
        jmp fxt_end

        /* program the polygon helper: mode, both slopes, both */
        /* positions, ADDR0 at the top row (+320/row), ADDR1 stepping */
        /* +1, DCSEL left at 5. */
    fxt_poly_setup:
        lda 0x9f25                       /* vera_dcsel 2 */
        and #0x01
        ora #0x04
        sta 0x9f25
        lda #0x02                       /* VERA_FX_ADDR1_POLY */
        sta 0x9f29                      /* VERA_FX_CTRL */
        lda 0x9f25                       /* vera_dcsel 3 */
        and #0x01
        ora #0x06
        sta 0x9f25
        lda x16__fxt_xl
        sta 0x9f29                      /* VERA_FX_X_INCR_L */
        lda x16__fxt_xh
        sta 0x9f2a  /* seeds the subpixel to 0.5 (VERA_FX_X_INCR_H) */
        lda x16__fxt_yl
        sta 0x9f2b                      /* VERA_FX_Y_INCR_L */
        lda x16__fxt_yh
        sta 0x9f2c                      /* VERA_FX_Y_INCR_H */
        lda 0x9f25                       /* vera_dcsel 4 */
        and #0x01
        ora #0x08
        sta 0x9f25
        lda x16__fxt_px
        sta 0x9f29                      /* VERA_FX_X_POS_L */
        lda x16__fxt_px+1
        and #0x07
        sta 0x9f2a                      /* VERA_FX_X_POS_H */
        lda x16__fxt_py
        sta 0x9f2b                      /* VERA_FX_Y_POS_L */
        lda x16__fxt_py+1
        and #0x07
        sta 0x9f2c                      /* VERA_FX_Y_POS_H */

        /* ADDR0 = row base of the top row (y0 * 320), stepping +320 */
        lda #0
        sta x16__fxt_fh
        lda x16__tri+2                  /* y0 << 6 */
        asl
        rol x16__fxt_fh
        asl
        rol x16__fxt_fh
        asl
        rol x16__fxt_fh
        asl
        rol x16__fxt_fh
        asl
        rol x16__fxt_fh
        asl
        rol x16__fxt_fh
        sta x16__fxa
        clc
        lda x16__tri+2
        adc x16__fxt_fh
        sta x16__fxa+1
        lda #0
        adc #0
        sta x16__fxa+2
        lda 0x9f25
        and #0xfe
        sta 0x9f25
        lda x16__fxa
        sta 0x9f20                      /* VERA_ADDR_L */
        lda x16__fxa+1
        sta 0x9f21                      /* VERA_ADDR_M */
        lda x16__fxa+2
        and #0x01
        ora #0xe0                        /* VERA_INC_320 << 4 */
        sta 0x9f22                      /* VERA_ADDR_H */
        lda 0x9f25  /* we step +1 */
        ora #0x01
        sta 0x9f25
        lda #0x10
        sta 0x9f22
        lda 0x9f25                       /* vera_dcsel 5 */
        and #0x01
        ora #0x0a
        sta 0x9f25
        rts

        /* draw A rows; DCSEL must be 5 (poly_setup leaves it there) */
    fxt_poly_rows:
        sta x16__fxt_rows
    fxt_prow:
        lda x16__fxt_rows
        beq fxt_pdone
        lda 0x9f24        /* latch: half-step, point ADDR1 (VERA_DATA1) */
        lda 0x9f2b                      /* VERA_FX_POLY_FILL_L */
        sta x16__fxt_fl
        bmi fxt_plong
        lsr                             /* short row: length is bits 4:1 */
        and #0x0f
        sta x16__fxt_len
        lda #0
        sta x16__fxt_len+1
        jmp fxt_pdraw
    fxt_plong:
        lda 0x9f2c                      /* VERA_FX_POLY_FILL_H */
        sta x16__fxt_fh
        and #0xc0
        cmp #0xc0
        beq fxt_pskip                   /* negative width: no row */
        lda x16__fxt_fl
        lsr
        and #0x0f
        sta x16__fxt_len
        lda #0
        sta x16__fxt_len+1
        lda x16__fxt_fh
        lsr                             /* H bits 7:1 are length bits 9:3 */
        asl                             /* ...so shift them up by 3 */
        rol x16__fxt_len+1
        asl
        rol x16__fxt_len+1
        asl
        rol x16__fxt_len+1
        ora x16__fxt_len
        sta x16__fxt_len
    fxt_pdraw:
        ldx x16__fxt_len
        ldy x16__fxt_len+1
        txa
        ora x16__fxt_len+1
        beq fxt_pskip                   /* zero-width row */
        txa
        beq fxt_pfull
        iny
    fxt_pfull:
        lda x16__tri_c
    fxt_ploop:
        sta 0x9f24                      /* VERA_DATA1 */
        dex
        bne fxt_ploop
        dey
        bne fxt_ploop
    fxt_pskip:
        lda 0x9f23        /* second half-step, next row (VERA_DATA0) */
        dec x16__fxt_rows
        jmp fxt_prow
    fxt_pdone:
        rts

        /* signed (dx * 256) / dy -> the 15-bit (+32x) register format */
        /* in fxs_el/eh, magnitude kept for the left/right comparison. */
        /* *256, not *512: the poly filler wants HALF the per-row step. */
    fxt_slope:
        lda #0
        sta x16__fxs_sgn
        lda x16__fxs_dxh
        bpl fxt_sl_abs
        inc x16__fxs_sgn
        sec
        lda #0
        sbc x16__fxs_dxl
        sta x16__fxs_dxl
        lda #0
        sbc x16__fxs_dxh
        sta x16__fxs_dxh
    fxt_sl_abs:
        lda #0
        sta x16__fxd_num /* dividend = |dx| * 256 */
        lda x16__fxs_dxl
        sta x16__fxd_num+1
        lda x16__fxs_dxh
        sta x16__fxd_num+2
        lda x16__fxs_dy
        sta x16__fxd_den
        lda #0
        sta x16__fxd_den+1
        jsr fxt_udiv24

        lda x16__fxd_num                /* keep the magnitude */
        sta x16__fxs_mag
        lda x16__fxd_num+1
        sta x16__fxs_mag+1
        lda x16__fxd_num+2
        sta x16__fxs_mag+2

        lda #0
        sta x16__fxs_32 /* encode: 14 bits direct, or /32 */
        lda x16__fxd_num+2
        bne fxt_sl_big
        lda x16__fxd_num+1
        cmp #0x40
        bcc fxt_sl_small
    fxt_sl_big:
        ldx #5
    fxt_sl_shift:
        lsr x16__fxd_num+2
        ror x16__fxd_num+1
        ror x16__fxd_num
        dex
        bne fxt_sl_shift
        inc x16__fxs_32
    fxt_sl_small:
        lda x16__fxd_num
        sta x16__fxs_el
        lda x16__fxd_num+1
        sta x16__fxs_eh
        lda x16__fxs_sgn
        beq fxt_sl_pos
        sec                             /* two's complement in 15 bits */
        lda #0
        sbc x16__fxs_el
        sta x16__fxs_el
        lda #0
        sbc x16__fxs_eh
        and #0x7f
        sta x16__fxs_eh
    fxt_sl_pos:
        lda x16__fxs_32
        beq fxt_sl_done
        lda x16__fxs_eh
        ora #0x80                        /* the 32x flag rides on bit 15 */
        sta x16__fxs_eh
    fxt_sl_done:
        rts

        /* carry set if edge B (fxs_*) is a smaller signed slope than A */
    fxt_b_lt_a:
        lda x16__fxs_sgn
        cmp x16__fxt_a_sgn
        beq fxt_cmp_same
        lda x16__fxs_sgn                /* different signs: negative less */
        bne fxt_cmp_yes
        clc
        rts
    fxt_cmp_same:
        lda x16__fxt_a_sgn
        bne fxt_cmp_neg
        lda x16__fxs_mag+2              /* both positive: B < A iff |B|<|A| */
        cmp x16__fxt_a_mag+2
        bne fxt_cmp_p
        lda x16__fxs_mag+1
        cmp x16__fxt_a_mag+1
        bne fxt_cmp_p
        lda x16__fxs_mag
        cmp x16__fxt_a_mag
    fxt_cmp_p:
        bcc fxt_cmp_yes
        clc
        rts
    fxt_cmp_neg:
        lda x16__fxt_a_mag+2            /* both negative: B < A iff |A|<|B| */
        cmp x16__fxs_mag+2
        bne fxt_cmp_n
        lda x16__fxt_a_mag+1
        cmp x16__fxs_mag+1
        bne fxt_cmp_n
        lda x16__fxt_a_mag
        cmp x16__fxs_mag
    fxt_cmp_n:
        bcc fxt_cmp_yes
        clc
        rts
    fxt_cmp_yes:
        sec
        rts

    fxt_udiv24:
        ldx #0
        stx x16__fxd_rem
        stx x16__fxd_rem+1
        ldx #24
    fxt_dv:
        asl x16__fxd_num
        rol x16__fxd_num+1
        rol x16__fxd_num+2
        rol x16__fxd_rem
        rol x16__fxd_rem+1
        sec
        lda x16__fxd_rem
        sbc x16__fxd_den
        tay
        lda x16__fxd_rem+1
        sbc x16__fxd_den+1
        bcc fxt_dv_no
        sta x16__fxd_rem+1
        sty x16__fxd_rem
        inc x16__fxd_num
    fxt_dv_no:
        dex
        bne fxt_dv
        rts

    fxt_swap01:
        lda x16__tri
        ldx x16__tri+3
        stx x16__tri
        sta x16__tri+3
        lda x16__tri+1
        ldx x16__tri+4
        stx x16__tri+1
        sta x16__tri+4
        lda x16__tri+2
        ldx x16__tri+5
        stx x16__tri+2
        sta x16__tri+5
        rts

    fxt_swap12:
        lda x16__tri+3
        ldx x16__tri+6
        stx x16__tri+3
        sta x16__tri+6
        lda x16__tri+4
        ldx x16__tri+7
        stx x16__tri+4
        sta x16__tri+7
        lda x16__tri+5
        ldx x16__tri+8
        stx x16__tri+5
        sta x16__tri+8
        rts

    fxt_end:
    }
    x16_fx_off();
}
