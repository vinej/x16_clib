// =====================================================================
// x16clib :: x16/fixed.c -- 16x16 multiply and 8.8 fixed point
// =====================================================================
// The shift-add multiply is the same hand-written 6502 as
// src_ca65/util/fixed.s, kept in one copy that both entry points call.
// The sign handling that mul88 did in place on the cc65 operand block
// happens while loading the multiplier block instead -- same
// instruction shapes, different plumbing.
// =====================================================================

#include <x16/fixed.h>

// Module scratch. Every byte is written before it is read, so no
// initialisers are needed: the core seeds the top half of the product
// with stz and the bottom half fills from sixteen rotations.
volatile char x16__fx_mcand[2];
volatile char x16__fx_mplier[2];        // consumed by the core
volatile char x16__fx_prod[4];
volatile char x16__fx_sign;

// ---------------------------------------------------------------------
// The core: x16__fx_prod = x16__fx_mcand * x16__fx_mplier, unsigned.
// Internal -- do not call. Per multiplier bit (LSB first), optionally
// add the multiplicand to the top half, then rotate the whole product
// right; the adc carry rolls down through the rotate.
// ---------------------------------------------------------------------
void x16__fx_umul16(void) {
    __asm {
        ldx #0
        stx x16__fx_prod+2
        stx x16__fx_prod+3
        ldx #16
    um_shift:
        lsr x16__fx_mplier+1
        ror x16__fx_mplier      /* low bit of the multiplier into carry */
        bcc um_noadd
        lda x16__fx_prod+2
        clc
        adc x16__fx_mcand
        sta x16__fx_prod+2
        lda x16__fx_prod+3
        adc x16__fx_mcand+1     /* A = new high byte, carry = overflow */
        jmp um_rotate
    um_noadd:
        lda x16__fx_prod+3      /* carry is already clear */
    um_rotate:
        ror                     /* carry rolls down through the product */
        sta x16__fx_prod+3
        ror x16__fx_prod+2
        ror x16__fx_prod+1
        ror x16__fx_prod
        dex
        bne um_shift
    }
}

unsigned long x16_umul16(unsigned int a, unsigned int b) {
    __asm {
        lda a
        sta x16__fx_mcand
        lda a+1
        sta x16__fx_mcand+1
        lda b
        sta x16__fx_mplier
        lda b+1
        sta x16__fx_mplier+1
    }
    x16__fx_umul16();
    return __asm {
        lda x16__fx_prod
        sta accu
        lda x16__fx_prod+1
        sta accu + 1
        lda x16__fx_prod+2
        sta accu + 2
        lda x16__fx_prod+3
        sta accu + 3
    };
}

// ---------------------------------------------------------------------
// Signed 8.8 fixed-point multiply: r = (a * b) >> 8.
//
//   384 ($0180 = 1.5) * 512 ($0200 = 2.0) = 768 ($0300 = 3.0)
// ---------------------------------------------------------------------
int x16_mul88(int a, int b) {
    __asm {
        lda #0
        sta x16__fx_sign

        lda a+1                 /* sign of a */
        bpl m88_a_pos
        inc x16__fx_sign
        sec                     /* load |a| */
        lda #0
        sbc a
        sta x16__fx_mcand
        lda #0
        sbc a+1
        sta x16__fx_mcand+1
        jmp m88_b
    m88_a_pos:
        lda a
        sta x16__fx_mcand
        lda a+1
        sta x16__fx_mcand+1
    m88_b:
        lda b+1                 /* sign of b */
        bpl m88_b_pos
        inc x16__fx_sign
        sec                     /* load |b| */
        lda #0
        sbc b
        sta x16__fx_mplier
        lda #0
        sbc b+1
        sta x16__fx_mplier+1
        jmp m88_go
    m88_b_pos:
        lda b
        sta x16__fx_mplier
        lda b+1
        sta x16__fx_mplier+1
    m88_go:
    }
    x16__fx_umul16();           // prod = |a| * |b|
    return __asm {
        lda x16__fx_prod+1      /* >> 8 : take bytes 1 and 2 */
        sta accu
        lda x16__fx_prod+2
        sta accu + 1

        lda x16__fx_sign
        lsr                     /* odd number of negatives -> negate */
        bcc m88_done
        sec
        lda #0
        sbc accu
        sta accu
        lda #0
        sbc accu + 1
        sta accu + 1
    m88_done:
    };
}
