; =====================================================================
; x16clib :: util/int16.s -- 16-bit integer arithmetic
; =====================================================================
; A port of the upstream assembly library's util/int16.asm. C has a
; native 16-bit int, so several of these routines duplicate a C
; operator exactly; they are here because the project ships FULL parity
; with the upstream surface, and because the composites C lacks --
; division that hands back the remainder in the same call, the integer
; square root, and the decimal renderer -- share this module's register
; buffers and internal helpers.
;
; Upstream the values live in named two-byte registers (i16_a, i16_b,
; i16_r) that the caller writes directly. The C entry points keep that
; model internal: arguments arrive by value, the shims stage them into
; the same buffers, and the result comes back as a C return value.
;
; Add, subtract, negate, multiply and the left shift are shared between
; signed and unsigned: two's complement makes them identical. Only
; comparison, division, the right shift and decimal output need to know
; which you meant, and those come in pairs.
;
; For the full 32-bit product of two 16-bit values use x16_umul16() in
; util/fixed.s; i16_mul keeps only the low 16 bits.
;
; Upstream i16_to_dec leans on util/number's u16_to_dec. That module is
; not in this tree yet, so the same upstream converter body (powers-of-
; ten subtraction, ASCII digits, module buffer) is carried privately
; here; the output is byte-identical, and the symbols are file-local so
; the two modules can coexist once number lands.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popax

        .export         _x16_i16_from_u8
        .export         _x16_i16_from_s8
        .export         _x16_i16_add
        .export         _x16_i16_sub
        .export         _x16_i16_neg
        .export         _x16_i16_abs
        .export         _x16_i16_shl
        .export         _x16_i16_shr
        .export         _x16_i16_asr
        .export         _x16_i16_cmpu
        .export         _x16_i16_cmps
        .export         _x16_i16_mul
        .export         _x16_i16_divmod
        .export         _x16_i16_divmod_s
        .export         _x16_i16_sqrt
        .export         _x16_i16_to_dec
        .export         _x16_i16_to_dec_s

        .segment        "CODE"

; A holds a signed byte; give X its sign extension so the value survives
; promotion to int.
.macro  sign_extend
        ldx     #0
        cmp     #$80
        bcc     :+
        ldx     #$FF
:
.endmacro

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; int __fastcall__ x16_i16_from_u8 (unsigned char v)  -- zero-extend
; int __fastcall__ x16_i16_from_s8 (signed char v)    -- sign-extend
;
; A C cast does the same; these exist for parity with the upstream
; register model, and they exercise the same code path it shipped.
; ---------------------------------------------------------------------
_x16_i16_from_u8:
        jsr     i16_from_u8
        lda     i16_a
        ldx     i16_a+1
        rts

_x16_i16_from_s8:
        jsr     i16_from_s8
        lda     i16_a
        ldx     i16_a+1
        rts

; ---------------------------------------------------------------------
; int __fastcall__ x16_i16_add (int a, int b)
; int __fastcall__ x16_i16_sub (int a, int b)
; int __fastcall__ x16_i16_mul (int a, int b)   -- low 16 bits
; ---------------------------------------------------------------------
_x16_i16_add:
        sta     i16_b                   ; b (rightmost arg, in A/X)
        stx     i16_b+1
        jsr     popax                   ; a
        sta     i16_a
        stx     i16_a+1
        jsr     i16_add
        lda     i16_a
        ldx     i16_a+1
        rts

_x16_i16_sub:
        sta     i16_b
        stx     i16_b+1
        jsr     popax
        sta     i16_a
        stx     i16_a+1
        jsr     i16_sub
        lda     i16_a
        ldx     i16_a+1
        rts

_x16_i16_mul:
        sta     i16_b
        stx     i16_b+1
        jsr     popax
        sta     i16_a
        stx     i16_a+1
        jsr     i16_mul
        lda     i16_a
        ldx     i16_a+1
        rts

; ---------------------------------------------------------------------
; int __fastcall__ x16_i16_neg (int a)
; int __fastcall__ x16_i16_abs (int a)
; int __fastcall__ x16_i16_shl (int a)          -- a << 1
; unsigned int __fastcall__ x16_i16_shr (unsigned int a)  -- logical >> 1
; int __fastcall__ x16_i16_asr (int a)          -- arithmetic >> 1
; ---------------------------------------------------------------------
_x16_i16_neg:
        sta     i16_a
        stx     i16_a+1
        jsr     i16_neg
        lda     i16_a
        ldx     i16_a+1
        rts

_x16_i16_abs:
        sta     i16_a
        stx     i16_a+1
        jsr     i16_abs
        lda     i16_a
        ldx     i16_a+1
        rts

_x16_i16_shl:
        sta     i16_a
        stx     i16_a+1
        jsr     i16_shl
        lda     i16_a
        ldx     i16_a+1
        rts

_x16_i16_shr:
        sta     i16_a
        stx     i16_a+1
        jsr     i16_shr
        lda     i16_a
        ldx     i16_a+1
        rts

_x16_i16_asr:
        sta     i16_a
        stx     i16_a+1
        jsr     i16_asr
        lda     i16_a
        ldx     i16_a+1
        rts

; ---------------------------------------------------------------------
; signed char __fastcall__ x16_i16_cmpu (unsigned int a, unsigned int b)
; signed char __fastcall__ x16_i16_cmps (int a, int b)
;   -1 if a < b, 0 if equal, 1 if a > b
; ---------------------------------------------------------------------
_x16_i16_cmpu:
        sta     i16_b
        stx     i16_b+1
        jsr     popax
        sta     i16_a
        stx     i16_a+1
        jsr     i16_cmpu
        sign_extend
        rts

_x16_i16_cmps:
        sta     i16_b
        stx     i16_b+1
        jsr     popax
        sta     i16_a
        stx     i16_a+1
        jsr     i16_cmps
        sign_extend
        rts

; ---------------------------------------------------------------------
; unsigned int __fastcall__ x16_i16_divmod (unsigned int a,
;                                           unsigned int b,
;                                           unsigned int *rem)
; int __fastcall__ x16_i16_divmod_s (int a, int b, int *rem)
;
;   Quotient returned, remainder through *rem. On b == 0 the upstream
;   routine changes nothing: the C entry returns a and leaves *rem
;   untouched.
; ---------------------------------------------------------------------
_x16_i16_divmod:
        sta     X16_T6                  ; rem (rightmost arg, in A/X)
        stx     X16_T7
        jsr     popax                   ; b
        sta     i16_b
        stx     i16_b+1
        jsr     popax                   ; a
        sta     i16_a
        stx     i16_a+1
        jsr     i16_divmod
        bcs     @divzero                ; b was zero: *rem stays untouched
        lda     i16_r
        sta     (X16_T6)
        ldy     #1
        lda     i16_r+1
        sta     (X16_T6),y
@divzero:
        lda     i16_a
        ldx     i16_a+1
        rts

_x16_i16_divmod_s:
        sta     X16_T6
        stx     X16_T7
        jsr     popax
        sta     i16_b
        stx     i16_b+1
        jsr     popax
        sta     i16_a
        stx     i16_a+1
        jsr     i16_divmod_s
        bcs     @divzero
        lda     i16_r
        sta     (X16_T6)
        ldy     #1
        lda     i16_r+1
        sta     (X16_T6),y
@divzero:
        lda     i16_a
        ldx     i16_a+1
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_i16_sqrt (unsigned int v)
;   floor(sqrt(v)), 0..255.
; ---------------------------------------------------------------------
_x16_i16_sqrt:
        sta     i16_a
        stx     i16_a+1
        jsr     i16_sqrt
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; char * __fastcall__ x16_i16_to_dec   (unsigned int v)
; char * __fastcall__ x16_i16_to_dec_s (int v)
;
;   ASCII decimal in a module buffer the next call overwrites,
;   NUL-terminated; the internal routines already answer A = buffer
;   low, X = high -- exactly cc65's pointer return.
; ---------------------------------------------------------------------
_x16_i16_to_dec:
        sta     i16_a
        stx     i16_a+1
        jmp     i16_to_dec

_x16_i16_to_dec_s:
        sta     i16_a
        stx     i16_a+1
        jmp     i16_to_dec_s

; =====================================================================
; Internal routines (the upstream x16_library body, verbatim)
; =====================================================================

; ---------------------------------------------------------------------
; i16_from_u8 -- in: A.  i16_a = A, zero-extended
; i16_from_s8 -- in: A.  i16_a = A, sign-extended
; ---------------------------------------------------------------------
i16_from_u8:
        sta     i16_a
        stz     i16_a+1
        rts

i16_from_s8:
        sta     i16_a
        and     #$80
        beq     @positive
        lda     #$FF
        sta     i16_a+1
        rts
@positive:
        stz     i16_a+1
        rts

; ---------------------------------------------------------------------
; i16_add -- i16_a += i16_b
; i16_sub -- i16_a -= i16_b
; ---------------------------------------------------------------------
i16_add:
        clc
        lda     i16_a
        adc     i16_b
        sta     i16_a
        lda     i16_a+1
        adc     i16_b+1
        sta     i16_a+1
        rts

i16_sub:
        sec
        lda     i16_a
        sbc     i16_b
        sta     i16_a
        lda     i16_a+1
        sbc     i16_b+1
        sta     i16_a+1
        rts

; ---------------------------------------------------------------------
; i16_neg -- i16_a = -i16_a
; i16_abs -- i16_a = |i16_a|
; ---------------------------------------------------------------------
i16_neg:
        sec
        lda     #0
        sbc     i16_a
        sta     i16_a
        lda     #0
        sbc     i16_a+1
        sta     i16_a+1
        rts

i16_abs:
        lda     i16_a+1
        bmi     i16_neg
        rts

; ---------------------------------------------------------------------
; i16_shl -- i16_a <<= 1
; i16_shr -- i16_a >>= 1, logical (zero fill)
; i16_asr -- i16_a >>= 1, arithmetic (sign fill)
; Carry holds the bit shifted out.
; ---------------------------------------------------------------------
i16_shl:
        asl     i16_a
        rol     i16_a+1
        rts

i16_shr:
        lsr     i16_a+1
        ror     i16_a
        rts

i16_asr:
        lda     i16_a+1
        asl                             ; sign bit into carry
        ror     i16_a+1                 ; ...and back in at the top
        ror     i16_a
        rts

; ---------------------------------------------------------------------
; i16_cmpu -- unsigned compare i16_a with i16_b
; i16_cmps -- signed compare
;   out: A = $FF if a < b, 0 if equal, 1 if a > b.  Z set when equal.
;        Neither operand is modified.
; ---------------------------------------------------------------------
i16_cmpu:
        lda     i16_a+1
        cmp     i16_b+1
        bne     @differ
        lda     i16_a
        cmp     i16_b
        bne     @differ
        lda     #0
        rts
@differ:
        bcs     @greater
        lda     #$FF
        rts
@greater:
        lda     #1
        rts

i16_cmps:
        ; Same-signed operands compare like unsigned ones. Different signs
        ; short-circuit: the negative one is smaller, whatever the bits say.
        lda     i16_a+1
        eor     i16_b+1
        bpl     i16_cmpu                ; signs agree
        lda     i16_a+1
        bmi     @a_negative
        lda     #1                      ; a >= 0, b < 0
        rts
@a_negative:
        lda     #$FF
        rts

; ---------------------------------------------------------------------
; i16_mul -- i16_a = i16_a * i16_b, modulo 2^16
;
; Shift-and-add. Signed and unsigned agree on the low 16 bits, so this
; serves both; only the discarded overflow differs.
; ---------------------------------------------------------------------
i16_mul:
        lda     i16_a                   ; tmp = a, then rebuild a as the product
        sta     i16_tmp
        lda     i16_a+1
        sta     i16_tmp+1
        stz     i16_a
        stz     i16_a+1

        lda     #16
        sta     i16_cnt
@loop:
        lsr     i16_b+1                 ; next bit of the multiplier
        ror     i16_b
        bcc     @no_add

        clc                             ; a += tmp
        lda     i16_a
        adc     i16_tmp
        sta     i16_a
        lda     i16_a+1
        adc     i16_tmp+1
        sta     i16_a+1
@no_add:
        asl     i16_tmp                 ; tmp <<= 1
        rol     i16_tmp+1

        dec     i16_cnt
        bne     @loop
        rts

; ---------------------------------------------------------------------
; i16_divmod -- unsigned:  i16_a = i16_a / i16_b,  i16_r = i16_a % i16_b
;   out: carry set if i16_b was zero, in which case nothing is changed
;
; Restoring division: shift the dividend left through the remainder one
; bit at a time, subtracting the divisor whenever it fits.
; ---------------------------------------------------------------------
i16_divmod:
        lda     i16_b
        ora     i16_b+1
        bne     @go
        sec                             ; divide by zero
        rts
@go:
        stz     i16_r
        stz     i16_r+1

        lda     #16
        sta     i16_cnt
@loop:
        asl     i16_a                   ; dividend out of the top of a...
        rol     i16_a+1
        rol     i16_r                   ; ...and into the bottom of r
        rol     i16_r+1

        sec                             ; trial subtraction r - b
        lda     i16_r
        sbc     i16_b
        sta     i16_tmp
        lda     i16_r+1
        sbc     i16_b+1
        sta     i16_tmp+1
        bcc     @next                   ; did not fit: leave r alone

        lda     i16_tmp                 ; it fit: keep the difference
        sta     i16_r
        lda     i16_tmp+1
        sta     i16_r+1
        inc     i16_a                   ; and set the quotient bit
@next:
        dec     i16_cnt
        bne     @loop
        clc
        rts

; ---------------------------------------------------------------------
; i16_divmod_s -- signed divide, truncating toward zero
;   i16_a = i16_a / i16_b,  i16_r = i16_a % i16_b
;   out: carry set if i16_b was zero
;
; The quotient's sign is the exclusive-or of the operands' signs; the
; remainder takes the sign of the DIVIDEND, which is what C and Forth's
; SM/REM both do. -7 / 2 is -3 remainder -1, not -4 remainder 1.
; ---------------------------------------------------------------------
; Note: i16_divmod_s leaves i16_b holding |i16_b|.
; -32768 has no positive counterpart, so |a| overflows for that one value.
i16_divmod_s:
        lda     i16_b
        ora     i16_b+1
        bne     @go
        sec                             ; divide by zero
        rts
@go:
        ; Capture both signs BEFORE taking absolute values, or they are gone.
        lda     i16_a+1
        sta     i16_rsign               ; remainder follows the dividend
        eor     i16_b+1
        sta     i16_qsign               ; quotient follows sign(a) xor sign(b)

        jsr     i16_abs                 ; |a|

        lda     i16_b+1                 ; |b|
        bpl     @b_positive
        sec
        lda     #0
        sbc     i16_b
        sta     i16_b
        lda     #0
        sbc     i16_b+1
        sta     i16_b+1
@b_positive:

        jsr     i16_divmod              ; unsigned |a| / |b|; b is nonzero

        lda     i16_rsign
        bpl     @quotient
        sec                             ; negate the remainder
        lda     #0
        sbc     i16_r
        sta     i16_r
        lda     #0
        sbc     i16_r+1
        sta     i16_r+1
@quotient:
        lda     i16_qsign
        bpl     @done
        jsr     i16_neg
@done:
        clc
        rts

; ---------------------------------------------------------------------
; i16_sqrt -- floor(sqrt(i16_a)), the ISQRT of FLOAT.TXT
;   out: A = the root (0..255).  Consumes i16_a.
;
; Digit-by-digit binary square root: two bits of the operand enter the
; remainder each round, and the trial subtrahend is 4*root+1.
; ---------------------------------------------------------------------
i16_sqrt:
        stz     i16_root
        stz     i16_rem
        stz     i16_rem+1

        ldx     #8
@iter:
        asl     i16_a                   ; two bits of a into the remainder
        rol     i16_a+1
        rol     i16_rem
        rol     i16_rem+1
        asl     i16_a
        rol     i16_a+1
        rol     i16_rem
        rol     i16_rem+1

        lda     i16_root                ; trial = (root << 2) | 1
        sta     i16_tmp
        stz     i16_tmp+1
        asl     i16_tmp
        rol     i16_tmp+1
        asl     i16_tmp
        rol     i16_tmp+1
        lda     i16_tmp
        ora     #1
        sta     i16_tmp

        asl     i16_root                ; root <<= 1, bit 0 clear

        lda     i16_rem                 ; rem >= trial ?
        cmp     i16_tmp
        lda     i16_rem+1
        sbc     i16_tmp+1
        bcc     @next

        sec                             ; rem -= trial
        lda     i16_rem
        sbc     i16_tmp
        sta     i16_rem
        lda     i16_rem+1
        sbc     i16_tmp+1
        sta     i16_rem+1
        inc     i16_root                ; set the new root bit
@next:
        dex
        bne     @iter

        lda     i16_root
        rts

; ---------------------------------------------------------------------
; i16_to_dec   -- unsigned i16_a to decimal
; i16_to_dec_s -- signed i16_a to decimal, with a leading '-'
;   out: A = buffer low, X = buffer high, Y = length; NUL-terminated.
;   Both consume i16_a.
; ---------------------------------------------------------------------
i16_to_dec:
        lda     i16_a
        sta     X16_P0
        lda     i16_a+1
        sta     X16_P1
        jmp     i16_u16_to_dec

i16_to_dec_s:
        stz     i16_sign
        lda     i16_a+1
        bpl     @positive

        inc     i16_sign                ; negative: print the magnitude
        sec
        lda     #0
        sbc     i16_a
        sta     X16_P0
        lda     #0
        sbc     i16_a+1
        sta     X16_P1
        bra     @convert
@positive:
        lda     i16_a
        sta     X16_P0
        lda     i16_a+1
        sta     X16_P1
@convert:
        jsr     i16_u16_to_dec          ; digits land in i16_nbuf

        ldx     #0
        lda     i16_sign
        beq     @copy
        lda     #$2D                    ; '-' (ASCII)
        sta     i16_buf
        ldx     #1
@copy:
        ldy     #0
@loop:
        lda     i16_nbuf,y              ; the terminator is copied too
        sta     i16_buf,x
        beq     @done
        inx
        iny
        bra     @loop
@done:
        txa
        tay                             ; Y = length, not counting the terminator
        lda     #<i16_buf
        ldx     #>i16_buf
        rts

; ---------------------------------------------------------------------
; i16_u16_to_dec -- unsigned 16-bit to decimal, no leading zeros
;   in:  X16_P0/P1 = value (consumed)
;   out: A = buffer low, X = high, Y = length; NUL-terminated.
;
; This is util/number's u16_to_dec, carried privately (see the file
; header): repeated subtraction against a table of powers of ten.
; The digits are ASCII $30-$39, written as hex constants -- ca65's
; -t cx16 target remaps character literals to PETSCII.
; ---------------------------------------------------------------------
i16_u16_to_dec:
        stz     X16_T2                  ; have we emitted a significant digit yet?
        stz     X16_T4                  ; output length

        ldx     #0                      ; index into the power-of-ten table
@digit:
        lda     #$30                    ; '0' (ASCII)
        sta     X16_T3                  ; digit accumulator
@subtract:
        sec
        lda     X16_P0
        sbc     i16_pow10_lo,x
        sta     X16_T0                  ; tentative low byte
        lda     X16_P1
        sbc     i16_pow10_hi,x
        bcc     @next_digit             ; would go negative: this digit is done
        sta     X16_P1
        lda     X16_T0
        sta     X16_P0
        inc     X16_T3
        bra     @subtract

@next_digit:
        lda     X16_T3
        cmp     #$30                    ; '0'
        bne     @emit                   ; a non-zero digit always prints
        lda     X16_T2
        bne     @emit                   ; already past the leading zeros
        cpx     #4
        beq     @emit                   ; the units digit always prints
        bra     @skip
@emit:
        inc     X16_T2
        ldy     X16_T4
        lda     X16_T3
        sta     i16_nbuf,y
        iny
        sty     X16_T4
@skip:
        inx
        cpx     #5
        bne     @digit

        ldy     X16_T4
        lda     #0
        sta     i16_nbuf,y              ; NUL terminator; Y is now the length

        lda     #<i16_nbuf
        ldx     #>i16_nbuf
        rts

        .segment        "RODATA"

i16_pow10_lo:   .byte   <10000, <1000, <100, <10, <1
i16_pow10_hi:   .byte   >10000, >1000, >100, >10, >1

; ---------------------------------------------------------------------
; Module registers and scratch. Every byte is written before it is
; read, so BSS needs no initialiser.
; ---------------------------------------------------------------------
        .segment        "BSS"

i16_a:     .res 2
i16_b:     .res 2
i16_r:     .res 2

i16_tmp:   .res 2
i16_rem:   .res 2
i16_cnt:   .res 1
i16_root:  .res 1
i16_sign:  .res 1
i16_qsign: .res 1
i16_rsign: .res 1
i16_buf:   .res 8                       ; "-32768" plus a terminator
i16_nbuf:  .res 8                       ; the private converter's digits
