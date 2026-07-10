; =====================================================================
; x16clib :: util/fixed.s -- 16x16 multiply and 8.8 fixed point
; =====================================================================
; C has no fixed-point type, and cc65's software multiply is a general
; routine. These are the two operations a sprite-mover actually needs.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   INTEGER bytes fill A, then X, then __rc2, __rc3, ... left to right.
;   A POINTER takes a whole __rc pair (__rc2/__rc3, then __rc4/__rc5) and
;   consumes no A/X -- only zero page can be indirected through.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_umul16
        .globl  x16_mul88

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; unsigned long __fastcall__ x16_umul16(unsigned int a, unsigned int b)
;
; A 32-bit return goes back in A (LSB), X, sreg, sreg+1 (MSB).
; ---------------------------------------------------------------------
; a lo -> A, a hi -> X, b lo -> __rc2, b hi -> __rc3.
; A 32-bit result goes back the way it would arrive: A, X, __rc2, __rc3.
; cc65 put the upper half in its sreg.
x16_umul16:
        sta     X16_P0                  ; a lo
        stx     X16_P1                  ; a hi
        lda     __rc2
        sta     X16_P2                  ; b lo
        lda     __rc3
        sta     X16_P3                  ; b hi

        jsr     umul16

        lda     X16_P6
        sta     __rc2                   ; product bits 16-23
        lda     X16_P7
        sta     __rc3                   ; product bits 24-31
        lda     X16_P4                  ; bits 0-7
        ldx     X16_P5                  ; bits 8-15
        rts

; ---------------------------------------------------------------------
; int __fastcall__ x16_mul88(int a, int b)
; ---------------------------------------------------------------------
x16_mul88:
        sta     X16_P0                  ; a lo
        stx     X16_P1                  ; a hi
        lda     __rc2
        sta     X16_P2                  ; b lo
        lda     __rc3
        sta     X16_P3                  ; b hi

        jsr     mul88

        lda     X16_P0                  ; a 16-bit result is A (low), X (high)
        ldx     X16_P1
        rts

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; umul16 -- unsigned 16 x 16 -> 32
;   in:  X16_P0/P1 = a, X16_P2/P3 = b
;   out: X16_P4..P7 = product, low byte first
; ---------------------------------------------------------------------
umul16:
        lda     X16_P0
        sta     fx_mcand
        lda     X16_P1
        sta     fx_mcand+1
        lda     X16_P2
        sta     fx_mplier
        lda     X16_P3
        sta     fx_mplier+1

        stz     fx_prod+2
        stz     fx_prod+3
        ldx     #16
.Lumul16_shift:
        lsr     fx_mplier+1
        ror     fx_mplier               ; low bit of the multiplier into carry
        bcc     .Lumul16_noadd
        lda     fx_prod+2
        clc
        adc     fx_mcand
        sta     fx_prod+2
        lda     fx_prod+3
        adc     fx_mcand+1              ; A = new high byte, carry = overflow
        bra     .Lumul16_rotate
.Lumul16_noadd:
        lda     fx_prod+3               ; carry is already clear
.Lumul16_rotate:
        ror     a                       ; carry rolls down through the product
        sta     fx_prod+3
        ror     fx_prod+2
        ror     fx_prod+1
        ror     fx_prod
        dex
        bne     .Lumul16_shift

        lda     fx_prod
        sta     X16_P4
        lda     fx_prod+1
        sta     X16_P5
        lda     fx_prod+2
        sta     X16_P6
        lda     fx_prod+3
        sta     X16_P7
        rts

; ---------------------------------------------------------------------
; mul88 -- signed 8.8 fixed-point multiply:  r = (a * b) >> 8
;   in:  X16_P0/P1 = a, X16_P2/P3 = b   (both signed 8.8)
;   out: X16_P0/P1 = r                  (signed 8.8)
;
; Lets sprites move at fractional speeds: hold the position in 8.8, add
; an 8.8 velocity each frame, and take the high byte as the pixel.
;
;   384 ($0180 = 1.5) * 512 ($0200 = 2.0) = 768 ($0300 = 3.0)
; ---------------------------------------------------------------------
mul88:
        stz     fx_sign

        lda     X16_P1                  ; sign of a
        bpl     .Lmul88_a_positive
        inc     fx_sign
        jsr     negate_a
.Lmul88_a_positive:
        lda     X16_P3                  ; sign of b
        bpl     .Lmul88_b_positive
        inc     fx_sign
        jsr     negate_b
.Lmul88_b_positive:

        jsr     umul16                  ; P4..P7 = |a| * |b|

        lda     X16_P5                  ; >> 8 : take bytes 1 and 2
        sta     X16_P0
        lda     X16_P6
        sta     X16_P1

        lda     fx_sign
        lsr     a                       ; odd number of negatives -> negate
        bcc     .Lmul88_done
        jsr     negate_a
.Lmul88_done:
        rts

negate_a:
        sec
        lda     #0
        sbc     X16_P0
        sta     X16_P0
        lda     #0
        sbc     X16_P1
        sta     X16_P1
        rts

negate_b:
        sec
        lda     #0
        sbc     X16_P2
        sta     X16_P2
        lda     #0
        sbc     X16_P3
        sta     X16_P3
        rts

; ---------------------------------------------------------------------
; Module scratch. Every byte is written before it is read, so BSS needs
; no initialiser: umul16 seeds the top half of fx_prod with stz and the
; bottom half fills from sixteen rotations.
; ---------------------------------------------------------------------
        .section .bss,"aw",@nobits

fx_prod:        .zero  4               ; 32-bit product
fx_mcand:       .zero  2               ; multiplicand
fx_mplier:      .zero  2               ; multiplier (consumed)
fx_sign:        .zero  1
