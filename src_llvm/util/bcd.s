; =====================================================================
; x16clib :: util/bcd.s -- packed-BCD (decimal-mode) add and subtract
; =====================================================================
; Decimal arithmetic through the 65C02's BCD mode, so 8-bit, 16-bit and
; 32-bit packed-BCD values add and subtract the way you read them:
;
;       $0987 + $1111 = $2098          (not the binary $1A98)
;
; Each byte holds two decimal digits, low byte first. The point is to
; skip the costly binary->decimal conversion a game score or clock would
; otherwise need every frame: keep the count in BCD and print its hex
; form, which already reads as decimal.
;
; The upstream x16_library keeps the operands in module registers bcd_a
; and bcd_b. The C entries stage through them: the destination is passed
; by pointer, updated in place, and the routine's carry comes back as
; the return value -- 1 on overflow past the width for the adds, 1 on
; borrow (the result wrapped below zero) for the subtracts.
;
; INTERRUPTS: these run in decimal mode across the operation. The
; KERNAL's IRQ handler is decimal-safe (it saves and restores the flags
; and does no decimal-sensitive ADC/SBC), so ordinary use is fine. A
; CUSTOM interrupt handler that does its own ADC/SBC must `cld` first,
; or bracket the call in sei/cli -- otherwise it would run those adds
; in decimal by mistake.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine (see gfx/bitmap4l.s):
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X, then whatever __rc bytes the pointers
;   left free -- so the b of (ptr, long b) rides A, X, __rc4, __rc5,
;   low byte first. Returns: char in A.

        .globl  x16_bcd_add8
        .globl  x16_bcd_add16
        .globl  x16_bcd_add32
        .globl  x16_bcd_sub8
        .globl  x16_bcd_sub16
        .globl  x16_bcd_sub32
        .globl  x16_bcd_addto
        .globl  x16_bcd_subfrom

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_bcd_add8 (unsigned char *a, unsigned char b)
; unsigned char x16_bcd_sub8 (unsigned char *a, unsigned char b)
;   *a += b (resp. -=) in packed BCD. Add returns 1 if the sum
;   overflowed the byte; sub returns 1 on borrow.
; a -> __rc2/__rc3 (the pointer), b -> A (the integer byte).
; ---------------------------------------------------------------------
x16_bcd_add8:
        jsr     bcd_arg8
        jsr     bcd_add8
        lda     bcd_a                   ; lda/sta leave the carry alone
        sta     (X16_T0)
        lda     #0
        adc     #0                      ; A = the carry
        rts

x16_bcd_sub8:
        jsr     bcd_arg8
        jsr     bcd_sub8
        lda     bcd_a
        sta     (X16_T0)
        lda     #0
        adc     #0
        eor     #1                      ; 1 = borrow (carry came back clear)
        rts

; ---------------------------------------------------------------------
; unsigned char x16_bcd_add16 (unsigned int *a, unsigned int b)
; unsigned char x16_bcd_sub16 (unsigned int *a, unsigned int b)
; a -> __rc2/__rc3, b -> A/X.
; ---------------------------------------------------------------------
x16_bcd_add16:
        jsr     bcd_arg16
        jsr     bcd_add16
        jsr     bcd_store16
        lda     #0
        adc     #0
        rts

x16_bcd_sub16:
        jsr     bcd_arg16
        jsr     bcd_sub16
        jsr     bcd_store16
        lda     #0
        adc     #0
        eor     #1
        rts

; ---------------------------------------------------------------------
; unsigned char x16_bcd_add32 (unsigned long *a, unsigned long b)
; unsigned char x16_bcd_sub32 (unsigned long *a, unsigned long b)
; a -> __rc2/__rc3, b -> A, X, __rc4, __rc5 (low byte first).
; ---------------------------------------------------------------------
x16_bcd_add32:
        jsr     bcd_arg32
        jsr     bcd_add32
        lda     #0                      ; capture the carry before the
        adc     #0                      ; copy-back loop's cpy eats it
        pha
        jsr     bcd_store32
        pla
        rts

x16_bcd_sub32:
        jsr     bcd_arg32
        jsr     bcd_sub32
        lda     #0
        adc     #0
        eor     #1
        pha
        jsr     bcd_store32
        pla
        rts

; ---------------------------------------------------------------------
; unsigned char x16_bcd_addto   (unsigned char *value, unsigned long b)
; unsigned char x16_bcd_subfrom (unsigned char *value, unsigned long b)
;   The upstream in-place forms: value points at a 4-byte packed-BCD
;   buffer, low byte first, and b is added to (subtracted from) it
;   without staging through bcd_a. Same returns as add32/sub32.
; value -> __rc2/__rc3, b -> A, X, __rc4, __rc5 (low byte first).
; ---------------------------------------------------------------------
x16_bcd_addto:
        jsr     bcd_arg_b32
        lda     __rc2                   ; A = value low, X = value high
        ldx     __rc3
        jsr     bcd_addto
        lda     #0
        adc     #0
        rts

x16_bcd_subfrom:
        jsr     bcd_arg_b32
        lda     __rc2
        ldx     __rc3
        jsr     bcd_subfrom
        lda     #0
        adc     #0
        eor     #1
        rts

; --- argument plumbing ------------------------------------------------

; b (in A) -> bcd_b; a (__rc2/__rc3) -> X16_T0/T1; *a -> bcd_a
bcd_arg8:
        sta     bcd_b
        lda     __rc2
        sta     X16_T0
        lda     __rc3
        sta     X16_T1
        lda     (X16_T0)
        sta     bcd_a
        rts

bcd_arg16:
        sta     bcd_b
        stx     bcd_b+1
        lda     __rc2
        sta     X16_T0
        lda     __rc3
        sta     X16_T1
        lda     (X16_T0)
        sta     bcd_a
        ldy     #1
        lda     (X16_T0),y
        sta     bcd_a+1
        rts

bcd_store16:
        lda     bcd_a                   ; ldy/lda/sta leave the carry alone
        sta     (X16_T0)
        ldy     #1
        lda     bcd_a+1
        sta     (X16_T0),y
        rts

; b (long: A, X, __rc4, __rc5) -> bcd_b
bcd_arg_b32:
        sta     bcd_b
        stx     bcd_b+1
        ldy     __rc4
        sty     bcd_b+2
        ldy     __rc5
        sty     bcd_b+3
        rts

bcd_arg32:
        jsr     bcd_arg_b32
        lda     __rc2
        sta     X16_T0
        lda     __rc3
        sta     X16_T1
        ldy     #3
.Lbcd_arg32_copy:  lda     (X16_T0),y
        sta     bcd_a,y
        dey
        bpl     .Lbcd_arg32_copy
        rts

bcd_store32:
        ldy     #3
.Lbcd_store32_copy:  lda     bcd_a,y
        sta     (X16_T0),y
        dey
        bpl     .Lbcd_store32_copy
        rts

; =====================================================================
; Internal routines (the upstream x16_library body, verbatim)
; =====================================================================

; ---------------------------------------------------------------------
; bcd_add8 / bcd_add16 / bcd_add32 -- bcd_a += bcd_b at that width.
;   out: carry set if the sum overflowed the width
; ---------------------------------------------------------------------
bcd_add8:
        sed
        clc
        lda     bcd_a
        adc     bcd_b
        sta     bcd_a
        cld
        rts

bcd_add16:
        sed
        clc
        lda     bcd_a
        adc     bcd_b
        sta     bcd_a
        lda     bcd_a+1
        adc     bcd_b+1
        sta     bcd_a+1
        cld
        rts

bcd_add32:
        sed
        clc
        ldx     #0
        ldy     #4
.Lbcd_add32_loop:
        lda     bcd_a,x
        adc     bcd_b,x                 ; carry threads through the loop untouched:
        sta     bcd_a,x                 ; inx and dey leave it alone, cpx would not
        inx
        dey
        bne     .Lbcd_add32_loop
        cld
        rts

; ---------------------------------------------------------------------
; bcd_sub8 / bcd_sub16 / bcd_sub32 -- bcd_a -= bcd_b at that width.
;   out: carry clear if the result went below zero (borrow)
; ---------------------------------------------------------------------
bcd_sub8:
        sed
        sec
        lda     bcd_a
        sbc     bcd_b
        sta     bcd_a
        cld
        rts

bcd_sub16:
        sed
        sec
        lda     bcd_a
        sbc     bcd_b
        sta     bcd_a
        lda     bcd_a+1
        sbc     bcd_b+1
        sta     bcd_a+1
        cld
        rts

bcd_sub32:
        sed
        sec
        ldx     #0
        ldy     #4
.Lbcd_sub32_loop:
        lda     bcd_a,x
        sbc     bcd_b,x
        sta     bcd_a,x
        inx
        dey
        bne     .Lbcd_sub32_loop
        cld
        rts

; ---------------------------------------------------------------------
; bcd_addto -- add bcd_b (32-bit) to a 4-byte BCD value in place.
;   in:  A = value low, X = value high (pointer to 4 bytes, low first)
;   out: carry set on overflow. Saves copying the value through bcd_a.
; ---------------------------------------------------------------------
bcd_addto:
        sta     X16_T0
        stx     X16_T1
        sed
        clc
        ldy     #0
        lda     (X16_T0),y
        adc     bcd_b
        sta     (X16_T0),y
        iny
        lda     (X16_T0),y
        adc     bcd_b+1
        sta     (X16_T0),y
        iny
        lda     (X16_T0),y
        adc     bcd_b+2
        sta     (X16_T0),y
        iny
        lda     (X16_T0),y
        adc     bcd_b+3
        sta     (X16_T0),y
        cld
        rts

; ---------------------------------------------------------------------
; bcd_subfrom -- subtract bcd_b (32-bit) from a 4-byte BCD value in place.
;   in:  A = value low, X = value high (pointer to 4 bytes, low first)
;   out: carry clear on borrow.
; ---------------------------------------------------------------------
bcd_subfrom:
        sta     X16_T0
        stx     X16_T1
        sed
        sec
        ldy     #0
        lda     (X16_T0),y
        sbc     bcd_b
        sta     (X16_T0),y
        iny
        lda     (X16_T0),y
        sbc     bcd_b+1
        sta     (X16_T0),y
        iny
        lda     (X16_T0),y
        sbc     bcd_b+2
        sta     (X16_T0),y
        iny
        lda     (X16_T0),y
        sbc     bcd_b+3
        sta     (X16_T0),y
        cld
        rts

        .section .bss,"aw",@nobits

bcd_a:  .zero  4
bcd_b:  .zero  4
