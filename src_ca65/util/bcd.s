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

        .import         popax
        .importzp       sreg

        .export         _x16_bcd_add8
        .export         _x16_bcd_add16
        .export         _x16_bcd_add32
        .export         _x16_bcd_sub8
        .export         _x16_bcd_sub16
        .export         _x16_bcd_sub32
        .export         _x16_bcd_addto
        .export         _x16_bcd_subfrom

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_bcd_add8 (unsigned char *a,
;                                          unsigned char b)
; unsigned char __fastcall__ x16_bcd_sub8 (unsigned char *a,
;                                          unsigned char b)
;   *a += b (resp. -=) in packed BCD. Add returns 1 if the sum
;   overflowed the byte; sub returns 1 on borrow.
; ---------------------------------------------------------------------
_x16_bcd_add8:
        jsr     bcd_arg8
        jsr     bcd_add8
        lda     bcd_a                   ; lda/sta leave the carry alone
        sta     (X16_T0)
        lda     #0
        adc     #0                      ; A = the carry
        ldx     #0
        rts

_x16_bcd_sub8:
        jsr     bcd_arg8
        jsr     bcd_sub8
        lda     bcd_a
        sta     (X16_T0)
        lda     #0
        adc     #0
        eor     #1                      ; 1 = borrow (carry came back clear)
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_bcd_add16 (unsigned int *a,
;                                           unsigned int b)
; unsigned char __fastcall__ x16_bcd_sub16 (unsigned int *a,
;                                           unsigned int b)
; ---------------------------------------------------------------------
_x16_bcd_add16:
        jsr     bcd_arg16
        jsr     bcd_add16
        jsr     bcd_store16
        lda     #0
        adc     #0
        ldx     #0
        rts

_x16_bcd_sub16:
        jsr     bcd_arg16
        jsr     bcd_sub16
        jsr     bcd_store16
        lda     #0
        adc     #0
        eor     #1
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_bcd_add32 (unsigned long *a,
;                                           unsigned long b)
; unsigned char __fastcall__ x16_bcd_sub32 (unsigned long *a,
;                                           unsigned long b)
; ---------------------------------------------------------------------
_x16_bcd_add32:
        jsr     bcd_arg32
        jsr     bcd_add32
        lda     #0                      ; capture the carry before the
        adc     #0                      ; copy-back loop's cpy eats it
        pha
        jsr     bcd_store32
        pla
        ldx     #0
        rts

_x16_bcd_sub32:
        jsr     bcd_arg32
        jsr     bcd_sub32
        lda     #0
        adc     #0
        eor     #1
        pha
        jsr     bcd_store32
        pla
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_bcd_addto   (unsigned char *value,
;                                             unsigned long b)
; unsigned char __fastcall__ x16_bcd_subfrom (unsigned char *value,
;                                             unsigned long b)
;   The upstream in-place forms: value points at a 4-byte packed-BCD
;   buffer, low byte first, and b is added to (subtracted from) it
;   without staging through bcd_a. Same returns as add32/sub32.
; ---------------------------------------------------------------------
_x16_bcd_addto:
        jsr     bcd_arg_b32
        jsr     popax                   ; A = value low, X = value high
        jsr     bcd_addto
        lda     #0
        adc     #0
        ldx     #0
        rts

_x16_bcd_subfrom:
        jsr     bcd_arg_b32
        jsr     popax
        jsr     bcd_subfrom
        lda     #0
        adc     #0
        eor     #1
        ldx     #0
        rts

; --- argument plumbing ------------------------------------------------

; b (in A) -> bcd_b; pop a -> X16_T0/T1; *a -> bcd_a
bcd_arg8:
        sta     bcd_b
        jsr     popax
        sta     X16_T0
        stx     X16_T1
        lda     (X16_T0)
        sta     bcd_a
        rts

bcd_arg16:
        sta     bcd_b
        stx     bcd_b+1
        jsr     popax
        sta     X16_T0
        stx     X16_T1
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

; b (long, sreg:X:A) -> bcd_b
bcd_arg_b32:
        sta     bcd_b
        stx     bcd_b+1
        ldy     sreg
        sty     bcd_b+2
        ldy     sreg+1
        sty     bcd_b+3
        rts

bcd_arg32:
        jsr     bcd_arg_b32
        jsr     popax
        sta     X16_T0
        stx     X16_T1
        ldy     #3
@copy:  lda     (X16_T0),y
        sta     bcd_a,y
        dey
        bpl     @copy
        rts

bcd_store32:
        ldy     #3
@copy:  lda     bcd_a,y
        sta     (X16_T0),y
        dey
        bpl     @copy
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
@loop:
        lda     bcd_a,x
        adc     bcd_b,x                 ; carry threads through the loop untouched:
        sta     bcd_a,x                 ; inx and dey leave it alone, cpx would not
        inx
        dey
        bne     @loop
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
@loop:
        lda     bcd_a,x
        sbc     bcd_b,x
        sta     bcd_a,x
        inx
        dey
        bne     @loop
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

        .segment        "BSS"

bcd_a:  .res 4
bcd_b:  .res 4
