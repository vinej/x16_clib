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

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers: the destination pointer rides r0/r1 and a
; byte/word b rides r2 (r2/r3); a LONG b is passed in btmp0..btmp0+3,
; low byte first. Returns: char in a.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3
        zpage	btmp0

        global	_x16_bcd_add8
        global	_x16_bcd_add16
        global	_x16_bcd_add32
        global	_x16_bcd_sub8
        global	_x16_bcd_sub16
        global	_x16_bcd_sub32
        global	_x16_bcd_addto
        global	_x16_bcd_subfrom

        section text

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_bcd_add8 (__reg("r0/r1") unsigned char *a,
;                             __reg("r2") unsigned char b)
; unsigned char x16_bcd_sub8 (same)
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
; unsigned char x16_bcd_add16 (__reg("r0/r1") unsigned int *a,
;                              __reg("r2/r3") unsigned int b)
; unsigned char x16_bcd_sub16 (same)
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
; unsigned char x16_bcd_add32 (__reg("r0/r1") unsigned long *a,
;                              unsigned long b)
; unsigned char x16_bcd_sub32 (same)
;   b is a plain long: vbcc delivers it in btmp0..btmp0+3, low first.
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
; unsigned char x16_bcd_addto   (__reg("r0/r1") unsigned char *value,
;                                unsigned long b)
; unsigned char x16_bcd_subfrom (same)
;   The upstream in-place forms: value points at a 4-byte packed-BCD
;   buffer, low byte first, and b is added to (subtracted from) it
;   without staging through bcd_a. Same returns as add32/sub32; b rides
;   btmp0..btmp0+3 like add32's.
; ---------------------------------------------------------------------
_x16_bcd_addto:
        jsr     bcd_arg_b32
        lda     r0                      ; A = value low, X = value high
        ldx     r1
        jsr     bcd_addto
        lda     #0
        adc     #0
        ldx     #0
        rts

_x16_bcd_subfrom:
        jsr     bcd_arg_b32
        lda     r0
        ldx     r1
        jsr     bcd_subfrom
        lda     #0
        adc     #0
        eor     #1
        ldx     #0
        rts

; --- argument plumbing ------------------------------------------------

; b (r2) -> bcd_b; a (r0/r1) -> X16_T0/T1; *a -> bcd_a
bcd_arg8:
        lda     r2
        sta     bcd_b
        lda     r0
        sta     X16_T0
        lda     r1
        sta     X16_T1
        lda     (X16_T0)
        sta     bcd_a
        rts

bcd_arg16:
        lda     r2
        sta     bcd_b
        lda     r3
        sta     bcd_b+1
        lda     r0
        sta     X16_T0
        lda     r1
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

; b (long, btmp0..btmp0+3) -> bcd_b
bcd_arg_b32:
        ldy     #3
.copy:  lda     btmp0,y
        sta     bcd_b,y
        dey
        bpl     .copy
        rts

bcd_arg32:
        jsr     bcd_arg_b32
        lda     r0
        sta     X16_T0
        lda     r1
        sta     X16_T1
        ldy     #3
.copy:  lda     (X16_T0),y
        sta     bcd_a,y
        dey
        bpl     .copy
        rts

bcd_store32:
        ldy     #3
.copy:  lda     bcd_a,y
        sta     (X16_T0),y
        dey
        bpl     .copy
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
.loop:
        lda     bcd_a,x
        adc     bcd_b,x                 ; carry threads through the loop untouched:
        sta     bcd_a,x                 ; inx and dey leave it alone, cpx would not
        inx
        dey
        bne     .loop
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
.loop:
        lda     bcd_a,x
        sbc     bcd_b,x
        sta     bcd_a,x
        inx
        dey
        bne     .loop
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

        section bss

bcd_a:  reserve 4
bcd_b:  reserve 4
