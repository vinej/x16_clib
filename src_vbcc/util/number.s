; =====================================================================
; x16clib :: util/number.s -- number formatting and parsing
; =====================================================================
; Decimal, hex and binary rendering without printf's footprint, plus a
; decimal parser. Results land in a shared module buffer that the NEXT
; CALL OVERWRITES -- the C entries return a pointer into it, so copy
; the string out if you need to keep it.
;
; The buffer holds ASCII: digits $30-$39, 'A'-'F' as $41-$46, '-' as
; $2D. They are written here as hex constants, not character literals:
; ca65's -t cx16 target remaps 'A'-'Z' literals to PETSCII $C1+, which
; would silently corrupt the hex digits (the upstream ACME source
; assembled them as plain ASCII).
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers: an 8-bit value rides the accumulator, a
; 16-bit one r0/r1; dec_to_u16 takes s in r0/r1, len in r2 and the
; value pointer in r4/r5. Returns: char in a; a POINTER in a/x --
; exactly where the internal routines leave the buffer address.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r4
        zpage	r5

        global	_x16_u8_to_dec
        global	_x16_u16_to_dec
        global	_x16_s8_to_dec
        global	_x16_s16_to_dec
        global	_x16_u8_to_hex
        global	_x16_u16_to_hex
        global	_x16_u8_to_bin
        global	_x16_u16_to_bin
        global	_x16_dec_to_u16

        section text

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; char *x16_u8_to_dec  (__reg("a") unsigned char v)
; char *x16_u16_to_dec (__reg("r0/r1") unsigned int v)
; char *x16_s8_to_dec  (__reg("a") signed char v)
; char *x16_s16_to_dec (__reg("r0/r1") int v)
; char *x16_u8_to_hex  (__reg("a") unsigned char v)
; char *x16_u16_to_hex (__reg("r0/r1") unsigned int v)
; char *x16_u8_to_bin  (__reg("a") unsigned char v)
; char *x16_u16_to_bin (__reg("r0/r1") unsigned int v)
;
;   All return the module buffer, NUL-terminated: no leading zeros for
;   decimal (signed values get a leading '-'), fixed width for hex and
;   binary. The buffer is module-owned and overwritten per call.
;
;   The 8-bit values arrive in A, the 16-bit ones in r0/r1, and every
;   internal routine already returns A = buffer low, X = buffer high --
;   exactly vbcc's pointer return.
; ---------------------------------------------------------------------
_x16_u8_to_dec:
        jmp     u8_to_dec

_x16_u16_to_dec:
        lda     r0
        sta     X16_P0
        lda     r1
        sta     X16_P1
        jmp     u16_to_dec

_x16_s8_to_dec:
        jmp     s8_to_dec

_x16_s16_to_dec:
        lda     r0
        sta     X16_P0
        lda     r1
        sta     X16_P1
        jmp     s16_to_dec

_x16_u8_to_hex:
        jmp     u8_to_hex

_x16_u16_to_hex:
        lda     r0
        sta     X16_P0
        lda     r1
        sta     X16_P1
        jmp     u16_to_hex

_x16_u8_to_bin:
        jmp     u8_to_bin

_x16_u16_to_bin:
        lda     r0
        sta     X16_P0
        lda     r1
        sta     X16_P1
        jmp     u16_to_bin

; ---------------------------------------------------------------------
; unsigned char x16_dec_to_u16 (__reg("r0/r1") const char *s,
;                               __reg("r2") unsigned char len,
;                               __reg("r4/r5") unsigned int *value)
;   Parses len decimal digits from s into *value. Returns 1 on success,
;   0 if a non-digit was found or the value overflowed 16 bits (*value
;   is untouched on failure). value parks in T6/T7: dec_to_u16 only
;   touches P0-P5/T0-T2.
; ---------------------------------------------------------------------
_x16_dec_to_u16:
        lda     r0
        sta     X16_P0                  ; s
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; len
        lda     r4
        sta     X16_T6                  ; value
        lda     r5
        sta     X16_T7
        jsr     dec_to_u16
        bcs     .bad
        lda     X16_P4
        sta     (X16_T6)
        ldy     #1
        lda     X16_P5
        sta     (X16_T6),y
        lda     #1
        ldx     #0
        rts
.bad:
        lda     #0
        ldx     #0
        rts

; =====================================================================
; Internal routines (the upstream x16_library body, verbatim)
; =====================================================================

; ---------------------------------------------------------------------
; u16_to_dec -- unsigned 16-bit to decimal, no leading zeros
;   in:  X16_P0/P1 = value
;   out: A = buffer address low, X = high, Y = length
;        The buffer is NUL-terminated as well, for screen_puts.
;
; Repeated subtraction against a table of powers of ten. Small and
; obvious; a 16-bit divide would not be faster at this size.
; ---------------------------------------------------------------------
; Consumes X16_P0/P1.
u16_to_dec:
        stz     X16_T2                  ; have we emitted a significant digit yet?
        stz     X16_T4                  ; output length

        ldx     #0                      ; index into the power-of-ten table
.digit:
        lda     #$30                    ; '0' (ASCII)
        sta     X16_T3                  ; digit accumulator
.subtract:
        sec
        lda     X16_P0
        sbc     number_pow10_lo,x
        sta     X16_T0                  ; tentative low byte
        lda     X16_P1
        sbc     number_pow10_hi,x
        bcc     .next_digit             ; would go negative: this digit is done
        sta     X16_P1
        lda     X16_T0
        sta     X16_P0
        inc     X16_T3
        bra     .subtract

.next_digit:
        lda     X16_T3
        cmp     #$30                    ; '0'
        bne     .emit                   ; a non-zero digit always prints
        lda     X16_T2
        bne     .emit                   ; already past the leading zeros
        cpx     #4
        beq     .emit                   ; the units digit always prints
        bra     .skip
.emit:
        inc     X16_T2
        ldy     X16_T4
        lda     X16_T3
        sta     num_buf,y
        iny
        sty     X16_T4
.skip:
        inx
        cpx     #5
        bne     .digit

        ldy     X16_T4
        lda     #0
        sta     num_buf,y               ; NUL terminator; Y is now the length

        lda     #<num_buf
        ldx     #>num_buf
        rts

; ---------------------------------------------------------------------
; u16_to_hex -- unsigned 16-bit to four hex digits
;   in:  X16_P0/P1 = value
;   out: A = buffer low, X = buffer high, Y = 4
; ---------------------------------------------------------------------
u16_to_hex:
        lda     X16_P1
        jsr     number_hi_digit
        sta     num_buf
        lda     X16_P1
        jsr     number_lo_digit
        sta     num_buf+1
        lda     X16_P0
        jsr     number_hi_digit
        sta     num_buf+2
        lda     X16_P0
        jsr     number_lo_digit
        sta     num_buf+3
        stz     num_buf+4

        lda     #<num_buf
        ldx     #>num_buf
        ldy     #4
        rts

number_hi_digit:
        lsr
        lsr
        lsr
        lsr
number_lo_digit:
        and     #$0F
        cmp     #10
        bcs     number_letter
        clc
        adc     #$30                    ; '0'
        rts
number_letter:
        clc
        adc     #($41 - 10)             ; 'A' - 10 (ASCII)
        rts

; ---------------------------------------------------------------------
; dec_to_u16 -- parse decimal digits
;   in:  X16_P0/P1 = string address, X16_P2 = length
;   out: X16_P4/P5 = value, carry set if a non-digit or overflow was found
; ---------------------------------------------------------------------
dec_to_u16:
        stz     X16_P4
        stz     X16_P5
        ldy     #0
.loop:
        cpy     X16_P2
        beq     .ok
        lda     (X16_P0),y
        sec
        sbc     #$30                    ; '0'
        cmp     #10
        bcs     .bad
        sta     X16_T0                  ; the new digit

        ; value = value * 10 + digit
        lda     X16_P4
        sta     X16_T1
        lda     X16_P5
        sta     X16_T2                  ; T2:T1 = value
        asl     X16_P4
        rol     X16_P5                  ; value * 2
        bcs     .bad
        asl     X16_P4
        rol     X16_P5                  ; value * 4
        bcs     .bad
        clc
        lda     X16_P4
        adc     X16_T1
        sta     X16_P4
        lda     X16_P5
        adc     X16_T2
        bcs     .bad
        sta     X16_P5                  ; value * 5
        asl     X16_P4
        rol     X16_P5                  ; value * 10
        bcs     .bad
        clc
        lda     X16_P4
        adc     X16_T0
        sta     X16_P4
        lda     X16_P5
        adc     #0
        bcs     .bad
        sta     X16_P5

        iny
        bra     .loop
.ok:
        clc
        rts
.bad:
        sec
        rts

; ---------------------------------------------------------------------
; u8_to_dec -- unsigned 8-bit to decimal (no leading zeros)
;   in:  A = value      out: A = buf low, X = buf high, Y = length
; ---------------------------------------------------------------------
u8_to_dec:
        sta     X16_P0
        stz     X16_P1
        jmp     u16_to_dec

; ---------------------------------------------------------------------
; u8_to_hex -- unsigned 8-bit to two hex digits
;   in:  A = value      out: A = buf low, X = buf high, Y = 2
; ---------------------------------------------------------------------
u8_to_hex:
        pha
        jsr     number_hi_digit
        sta     num_buf
        pla
        jsr     number_lo_digit
        sta     num_buf+1
        stz     num_buf+2
        lda     #<num_buf
        ldx     #>num_buf
        ldy     #2
        rts

; ---------------------------------------------------------------------
; u8_to_bin -- unsigned 8-bit to eight binary digits, MSB first
;   in:  A = value      out: A = buf low, X = buf high, Y = 8
; ---------------------------------------------------------------------
u8_to_bin:
        sta     X16_T0
        ldy     #0
.loop:
        asl     X16_T0                  ; MSB -> carry
        lda     #$30                    ; '0'
        adc     #0
        sta     num_buf,y
        iny
        cpy     #8
        bne     .loop
        lda     #0
        sta     num_buf,y
        lda     #<num_buf
        ldx     #>num_buf
        ldy     #8
        rts

; ---------------------------------------------------------------------
; u16_to_bin -- unsigned 16-bit to sixteen binary digits, MSB first
;   in:  X16_P0/P1 = value (consumed)   out: A/X = buf, Y = 16
; ---------------------------------------------------------------------
u16_to_bin:
        ldy     #0
.loop:
        asl     X16_P0
        rol     X16_P1                  ; MSB -> carry
        lda     #$30                    ; '0'
        adc     #0
        sta     num_buf,y
        iny
        cpy     #16
        bne     .loop
        lda     #0
        sta     num_buf,y
        lda     #<num_buf
        ldx     #>num_buf
        ldy     #16
        rts

; ---------------------------------------------------------------------
; s16_to_dec -- signed 16-bit to decimal, with a leading '-'
;   in:  X16_P0/P1 = value (consumed)   out: A/X = buf, Y = length
; ---------------------------------------------------------------------
s16_to_dec:
        lda     X16_P1
        bpl     .pos
        sec                             ; value = -value
        lda     #0
        sbc     X16_P0
        sta     X16_P0
        lda     #0
        sbc     X16_P1
        sta     X16_P1
        jsr     u16_to_dec              ; format the magnitude
        sty     X16_T5                  ; length of the digits
        ldx     X16_T5
.shift:
        lda     num_buf,x               ; make room for the sign: shift right by one
        sta     num_buf+1,x
        dex
        bpl     .shift
        lda     #$2D                    ; '-'
        sta     num_buf
        lda     #<num_buf
        ldx     #>num_buf
        ldy     X16_T5
        iny
        rts
.pos:
        jmp     u16_to_dec

; ---------------------------------------------------------------------
; s8_to_dec -- signed 8-bit to decimal, with a leading '-'
;   in:  A = value      out: A/X = buf, Y = length
; ---------------------------------------------------------------------
s8_to_dec:
        sta     X16_P0
        stz     X16_P1
        bit     X16_P0
        bpl     .go
        lda     #$FF                    ; sign-extend into the high byte
        sta     X16_P1
.go:
        jmp     s16_to_dec

        section rodata

number_pow10_lo:
        byte   <10000, <1000, <100, <10, <1
number_pow10_hi:
        byte   >10000, >1000, >100, >10, >1

        section bss

num_buf: reserve 17                ; enough for 16 binary digits plus a terminator
