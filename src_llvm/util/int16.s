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

; (import dropped: popax)

        .globl  x16_i16_from_u8
        .globl  x16_i16_from_s8
        .globl  x16_i16_add
        .globl  x16_i16_sub
        .globl  x16_i16_neg
        .globl  x16_i16_abs
        .globl  x16_i16_shl
        .globl  x16_i16_shr
        .globl  x16_i16_asr
        .globl  x16_i16_cmpu
        .globl  x16_i16_cmps
        .globl  x16_i16_mul
        .globl  x16_i16_divmod
        .globl  x16_i16_divmod_s
        .globl  x16_i16_sqrt
        .globl  x16_i16_to_dec
        .globl  x16_i16_to_dec_s

        .section .text,"ax",@progbits

; A holds a signed byte; give X its sign extension so the value survives
; promotion to int.
.macro  sign_extend
        ldx     #0
        cmp     #$80
        bcc 1f
        ldx     #$FF
1:
.endm

; =====================================================================
; C entry points
; =====================================================================
; llvm-mos argument placement: an int takes A/X, the next takes
; __rc2/__rc3, a pointer takes the next aligned __rc pair. An int comes
; back in A/X, a char in A, a pointer in __rc2/__rc3. The internal
; routines read and write the module's own i16_a / i16_b / i16_r
; buffers, so each shim is just a staging move.
; ---------------------------------------------------------------------

; ---------------------------------------------------------------------
; int x16_i16_from_u8 (unsigned char v)  -- zero-extend
; int x16_i16_from_s8 (signed char v)    -- sign-extend
;
; A C cast does the same; these exist for parity with the upstream
; register model, and they exercise the same code path it shipped.
; ---------------------------------------------------------------------
x16_i16_from_u8:
        jsr     i16_from_u8
        lda     i16_a
        ldx     i16_a+1
        rts

x16_i16_from_s8:
        jsr     i16_from_s8
        lda     i16_a
        ldx     i16_a+1
        rts

; ---------------------------------------------------------------------
; int x16_i16_add (int a, int b)
; int x16_i16_sub (int a, int b)
; int x16_i16_mul (int a, int b)   -- low 16 bits
; ---------------------------------------------------------------------
x16_i16_add:
        jsr     i16_stage_ab
        jsr     i16_add
        lda     i16_a
        ldx     i16_a+1
        rts

x16_i16_sub:
        jsr     i16_stage_ab
        jsr     i16_sub
        lda     i16_a
        ldx     i16_a+1
        rts

x16_i16_mul:
        jsr     i16_stage_ab
        jsr     i16_mul
        lda     i16_a
        ldx     i16_a+1
        rts

; a -> i16_a, b -> i16_b. Shared by every two-argument entry.
i16_stage_ab:
        sta     i16_a
        stx     i16_a+1
        lda     __rc2
        sta     i16_b
        lda     __rc3
        sta     i16_b+1
        rts

; ---------------------------------------------------------------------
; int x16_i16_neg (int a)
; int x16_i16_abs (int a)
; int x16_i16_shl (int a)
; unsigned int x16_i16_shr (unsigned int a)
; int x16_i16_asr (int a)
; ---------------------------------------------------------------------
x16_i16_neg:
        jsr     i16_stage_a
        jsr     i16_neg
        lda     i16_a
        ldx     i16_a+1
        rts

x16_i16_abs:
        jsr     i16_stage_a
        jsr     i16_abs
        lda     i16_a
        ldx     i16_a+1
        rts

x16_i16_shl:
        jsr     i16_stage_a
        jsr     i16_shl
        lda     i16_a
        ldx     i16_a+1
        rts

x16_i16_shr:
        jsr     i16_stage_a
        jsr     i16_shr
        lda     i16_a
        ldx     i16_a+1
        rts

x16_i16_asr:
        jsr     i16_stage_a
        jsr     i16_asr
        lda     i16_a
        ldx     i16_a+1
        rts

i16_stage_a:
        sta     i16_a
        stx     i16_a+1
        rts

; ---------------------------------------------------------------------
; signed char x16_i16_cmpu (unsigned int a, unsigned int b)
; signed char x16_i16_cmps (int a, int b)
;   -1, 0 or 1. The internal routines answer in A already.
; ---------------------------------------------------------------------
x16_i16_cmpu:
        jsr     i16_stage_ab
        jmp     i16_cmpu

x16_i16_cmps:
        jsr     i16_stage_ab
        jmp     i16_cmps

; ---------------------------------------------------------------------
; unsigned int x16_i16_divmod   (unsigned int a, unsigned int b,
;                                unsigned int *rem)
; int          x16_i16_divmod_s (int a, int b, int *rem)
;   Quotient returned; the remainder is stored through the pointer.
;   b == 0 returns a and leaves *rem untouched.
;
; a is in A/X, b in __rc2/__rc3, and the POINTER takes the next aligned
; pair, __rc4/__rc5. It is copied to X16_PTR0 first: __rc4 is not a
; zero-page pointer this module may indirect through directly.
; ---------------------------------------------------------------------
x16_i16_divmod:
        jsr     i16_stage_abp
        jsr     i16_divmod
        jmp     i16_store_rem

x16_i16_divmod_s:
        jsr     i16_stage_abp
        jsr     i16_divmod_s
        jmp     i16_store_rem

; Carry set means b was zero: the quotient is a and *rem is left alone,
; exactly as the cc65 build does it.

i16_stage_abp:
        jsr     i16_stage_ab
        lda     __rc4
        sta     X16_PTR0
        lda     __rc5
        sta     X16_PTR0+1
        rts

i16_store_rem:
        bcs     .Li16_quotient          ; b == 0: *rem untouched
        ldy     #0
        lda     i16_r
        sta     (X16_PTR0),y
        iny
        lda     i16_r+1
        sta     (X16_PTR0),y
.Li16_quotient:
        lda     i16_a
        ldx     i16_a+1
        rts

; ---------------------------------------------------------------------
; unsigned char x16_i16_sqrt (unsigned int v)  -- floor of the root
; ---------------------------------------------------------------------
x16_i16_sqrt:
        jsr     i16_stage_a
        jmp     i16_sqrt

; ---------------------------------------------------------------------
; char *x16_i16_to_dec   (unsigned int v)
; char *x16_i16_to_dec_s (int v)
;   Both render into the module's own buffer, which the NEXT call
;   overwrites. llvm-mos returns a pointer in __rc2/__rc3.
; ---------------------------------------------------------------------
x16_i16_to_dec:
        jsr     i16_stage_a
        jsr     i16_to_dec
        jmp     i16_ret_ptr

x16_i16_to_dec_s:
        jsr     i16_stage_a
        jsr     i16_to_dec_s
        jmp     i16_ret_ptr

; The internal converters leave the buffer address in A/X.
i16_ret_ptr:
        sta     __rc2
        stx     __rc3
        rts

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
        beq     .Li16_from_s8_positive
        lda     #$FF
        sta     i16_a+1
        rts
.Li16_from_s8_positive:
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
        bne     .Li16_cmpu_differ
        lda     i16_a
        cmp     i16_b
        bne     .Li16_cmpu_differ
        lda     #0
        rts
.Li16_cmpu_differ:
        bcs     .Li16_cmpu_greater
        lda     #$FF
        rts
.Li16_cmpu_greater:
        lda     #1
        rts

i16_cmps:
        ; Same-signed operands compare like unsigned ones. Different signs
        ; short-circuit: the negative one is smaller, whatever the bits say.
        lda     i16_a+1
        eor     i16_b+1
        bpl     i16_cmpu                ; signs agree
        lda     i16_a+1
        bmi     .Li16_cmps_a_negative
        lda     #1                      ; a >= 0, b < 0
        rts
.Li16_cmps_a_negative:
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
.Li16_mul_loop:
        lsr     i16_b+1                 ; next bit of the multiplier
        ror     i16_b
        bcc     .Li16_mul_no_add

        clc                             ; a += tmp
        lda     i16_a
        adc     i16_tmp
        sta     i16_a
        lda     i16_a+1
        adc     i16_tmp+1
        sta     i16_a+1
.Li16_mul_no_add:
        asl     i16_tmp                 ; tmp <<= 1
        rol     i16_tmp+1

        dec     i16_cnt
        bne     .Li16_mul_loop
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
        bne     .Li16_divmod_go
        sec                             ; divide by zero
        rts
.Li16_divmod_go:
        stz     i16_r
        stz     i16_r+1

        lda     #16
        sta     i16_cnt
.Li16_divmod_loop:
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
        bcc     .Li16_divmod_next                   ; did not fit: leave r alone

        lda     i16_tmp                 ; it fit: keep the difference
        sta     i16_r
        lda     i16_tmp+1
        sta     i16_r+1
        inc     i16_a                   ; and set the quotient bit
.Li16_divmod_next:
        dec     i16_cnt
        bne     .Li16_divmod_loop
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
        bne     .Li16_divmod_s_go
        sec                             ; divide by zero
        rts
.Li16_divmod_s_go:
        ; Capture both signs BEFORE taking absolute values, or they are gone.
        lda     i16_a+1
        sta     i16_rsign               ; remainder follows the dividend
        eor     i16_b+1
        sta     i16_qsign               ; quotient follows sign(a) xor sign(b)

        jsr     i16_abs                 ; |a|

        lda     i16_b+1                 ; |b|
        bpl     .Li16_divmod_s_b_positive
        sec
        lda     #0
        sbc     i16_b
        sta     i16_b
        lda     #0
        sbc     i16_b+1
        sta     i16_b+1
.Li16_divmod_s_b_positive:

        jsr     i16_divmod              ; unsigned |a| / |b|; b is nonzero

        lda     i16_rsign
        bpl     .Li16_divmod_s_quotient
        sec                             ; negate the remainder
        lda     #0
        sbc     i16_r
        sta     i16_r
        lda     #0
        sbc     i16_r+1
        sta     i16_r+1
.Li16_divmod_s_quotient:
        lda     i16_qsign
        bpl     .Li16_divmod_s_done
        jsr     i16_neg
.Li16_divmod_s_done:
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
.Li16_sqrt_iter:
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
        bcc     .Li16_sqrt_next

        sec                             ; rem -= trial
        lda     i16_rem
        sbc     i16_tmp
        sta     i16_rem
        lda     i16_rem+1
        sbc     i16_tmp+1
        sta     i16_rem+1
        inc     i16_root                ; set the new root bit
.Li16_sqrt_next:
        dex
        bne     .Li16_sqrt_iter

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
        bpl     .Li16_to_dec_s_positive

        inc     i16_sign                ; negative: print the magnitude
        sec
        lda     #0
        sbc     i16_a
        sta     X16_P0
        lda     #0
        sbc     i16_a+1
        sta     X16_P1
        bra     .Li16_to_dec_s_convert
.Li16_to_dec_s_positive:
        lda     i16_a
        sta     X16_P0
        lda     i16_a+1
        sta     X16_P1
.Li16_to_dec_s_convert:
        jsr     i16_u16_to_dec          ; digits land in i16_nbuf

        ldx     #0
        lda     i16_sign
        beq     .Li16_to_dec_s_copy
        lda     #$2D                    ; '-' (ASCII)
        sta     i16_buf
        ldx     #1
.Li16_to_dec_s_copy:
        ldy     #0
.Li16_to_dec_s_loop:
        lda     i16_nbuf,y              ; the terminator is copied too
        sta     i16_buf,x
        beq     .Li16_to_dec_s_done
        inx
        iny
        bra     .Li16_to_dec_s_loop
.Li16_to_dec_s_done:
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
.Li16_u16_to_dec_digit:
        lda     #$30                    ; '0' (ASCII)
        sta     X16_T3                  ; digit accumulator
.Li16_u16_to_dec_subtract:
        sec
        lda     X16_P0
        sbc     i16_pow10_lo,x
        sta     X16_T0                  ; tentative low byte
        lda     X16_P1
        sbc     i16_pow10_hi,x
        bcc     .Li16_u16_to_dec_next_digit             ; would go negative: this digit is done
        sta     X16_P1
        lda     X16_T0
        sta     X16_P0
        inc     X16_T3
        bra     .Li16_u16_to_dec_subtract

.Li16_u16_to_dec_next_digit:
        lda     X16_T3
        cmp     #$30                    ; '0'
        bne     .Li16_u16_to_dec_emit                   ; a non-zero digit always prints
        lda     X16_T2
        bne     .Li16_u16_to_dec_emit                   ; already past the leading zeros
        cpx     #4
        beq     .Li16_u16_to_dec_emit                   ; the units digit always prints
        bra     .Li16_u16_to_dec_skip
.Li16_u16_to_dec_emit:
        inc     X16_T2
        ldy     X16_T4
        lda     X16_T3
        sta     i16_nbuf,y
        iny
        sty     X16_T4
.Li16_u16_to_dec_skip:
        inx
        cpx     #5
        bne     .Li16_u16_to_dec_digit

        ldy     X16_T4
        lda     #0
        sta     i16_nbuf,y              ; NUL terminator; Y is now the length

        lda     #<i16_nbuf
        ldx     #>i16_nbuf
        rts

        .section .rodata,"a",@progbits

i16_pow10_lo:   .byte   <10000, <1000, <100, <10, <1
i16_pow10_hi:   .byte   >10000, >1000, >100, >10, >1

; ---------------------------------------------------------------------
; Module registers and scratch. Every byte is written before it is
; read, so BSS needs no initialiser.
; ---------------------------------------------------------------------
        .section .bss,"aw",@nobits

i16_a:     .zero  2
i16_b:     .zero  2
i16_r:     .zero  2

i16_tmp:   .zero  2
i16_rem:   .zero  2
i16_cnt:   .zero  1
i16_root:  .zero  1
i16_sign:  .zero  1
i16_qsign: .zero  1
i16_rsign: .zero  1
i16_buf:   .zero  8                       ; "-32768" plus a terminator
i16_nbuf:  .zero  8                       ; the private converter's digits
