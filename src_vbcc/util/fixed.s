; =====================================================================
; x16clib :: util/fixed.s -- 16x16 multiply and 8.8 fixed point
; =====================================================================
; C has no fixed-point type, and cc65's software multiply is a general
; routine. These are the two operations a sprite-mover actually needs.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers: the first int rides in r0/r1, the second in
; r2/r3. A 32-bit (long) return goes back in btmp0..btmp0+3, low first.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3
        zpage	btmp0

        global	_x16_umul16
        global	_x16_mul88

        section text

; ---------------------------------------------------------------------
; unsigned long x16_umul16(__reg("r0/r1") unsigned int a,
;                          __reg("r2/r3") unsigned int b)
;
; umul16 reads a from P0/P1, b from P2/P3 and writes the 32-bit product to
; P4..P7. The long return leaves in btmp0..btmp0+3, low byte first.
; ---------------------------------------------------------------------
_x16_umul16:
        lda     r0
        sta     X16_P0
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2
        lda     r3
        sta     X16_P3

        jsr     umul16

        lda     X16_P4
        sta     btmp0
        lda     X16_P5
        sta     btmp0+1
        lda     X16_P6
        sta     btmp0+2
        lda     X16_P7
        sta     btmp0+3
        rts

; ---------------------------------------------------------------------
; int x16_mul88(__reg("r0/r1") int a, __reg("r2/r3") int b)
;   returns int in a/x.
; ---------------------------------------------------------------------
_x16_mul88:
        lda     r0
        sta     X16_P0
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2
        lda     r3
        sta     X16_P3

        jsr     mul88

        lda     X16_P0
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
.shift:
        lsr     fx_mplier+1
        ror     fx_mplier               ; low bit of the multiplier into carry
        bcc     .noadd
        lda     fx_prod+2
        clc
        adc     fx_mcand
        sta     fx_prod+2
        lda     fx_prod+3
        adc     fx_mcand+1              ; A = new high byte, carry = overflow
        bra     .rotate
.noadd:
        lda     fx_prod+3               ; carry is already clear
.rotate:
        ror     a                       ; carry rolls down through the product
        sta     fx_prod+3
        ror     fx_prod+2
        ror     fx_prod+1
        ror     fx_prod
        dex
        bne     .shift

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
        bpl     .a_positive
        inc     fx_sign
        jsr     negate_a
.a_positive:
        lda     X16_P3                  ; sign of b
        bpl     .b_positive
        inc     fx_sign
        jsr     negate_b
.b_positive:

        jsr     umul16                  ; P4..P7 = |a| * |b|

        lda     X16_P5                  ; >> 8 : take bytes 1 and 2
        sta     X16_P0
        lda     X16_P6
        sta     X16_P1

        lda     fx_sign
        lsr     a                       ; odd number of negatives -> negate
        bcc     .done
        jsr     negate_a
.done:
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
        section bss

fx_prod:        reserve    4               ; 32-bit product
fx_mcand:       reserve    2               ; multiplicand
fx_mplier:      reserve    2               ; multiplier (consumed)
fx_sign:        reserve    1
