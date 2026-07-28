; =====================================================================
; x16clib :: util/int32.s -- 32-bit integer arithmetic
; =====================================================================
; A port of the upstream assembly library's util/int32.asm -- the
; DOUBLE.TXT surface, in assembly. cc65 has a native 32-bit long, so
; add, subtract, negate, multiply and the shifts duplicate a C
; operator; they are here because the project ships FULL parity with
; the upstream surface, and because the composites C lacks -- an
; unsigned divide that hands back the remainder in the same call, and
; a decimal renderer without printf's footprint -- are built on them.
;
; Upstream the values live in named four-byte registers (i32_a, i32_b,
; i32_r) that the caller writes directly. The C entry points keep that
; model internal: longs arrive by value in cc65's EAX (A/X/sreg), the
; shims stage them into the same buffers, and the result comes back as
; a C return value.
;
; Signed and unsigned share the same add, subtract, multiply and shift:
; two's complement makes them identical. Only comparison, division and
; decimal output need to know -- and upstream carries no signed divide
; or signed renderer at 32 bits, so neither does this port.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popeax
        .importzp       sreg

        .export         _x16_i32_from_u16
        .export         _x16_i32_from_s16
        .export         _x16_i32_to_s16
        .export         _x16_i32_add
        .export         _x16_i32_sub
        .export         _x16_i32_neg
        .export         _x16_i32_abs
        .export         _x16_i32_shl
        .export         _x16_i32_shr
        .export         _x16_i32_asr
        .export         _x16_i32_cmpu
        .export         _x16_i32_cmps
        .export         _x16_i32_mul
        .export         _x16_i32_divmod
        .export         _x16_i32_to_dec

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

; A long argument or return travels in cc65's EAX: A = byte 0 (low),
; X = byte 1, sreg = byte 2, sreg+1 = byte 3.
.macro  eax_to buf
        sta     buf
        stx     buf+1
        lda     sreg
        sta     buf+2
        lda     sreg+1
        sta     buf+3
.endmacro

.macro  eax_from buf
        lda     buf+2
        sta     sreg
        lda     buf+3
        sta     sreg+1
        lda     buf
        ldx     buf+1
.endmacro

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; long __fastcall__ x16_i32_from_u16 (unsigned int v)  -- zero-extend
; long __fastcall__ x16_i32_from_s16 (int v)           -- sign-extend
; int  __fastcall__ x16_i32_to_s16 (long v)   -- the top two bytes lost
; ---------------------------------------------------------------------
_x16_i32_from_u16:
        jsr     i32_from_u16            ; v already arrives as A = low, X = high
        eax_from i32_a
        rts

_x16_i32_from_s16:
        jsr     i32_from_s16
        eax_from i32_a
        rts

_x16_i32_to_s16:
        eax_to  i32_a
        jmp     i32_to_s16              ; answers A = low, X = high

; ---------------------------------------------------------------------
; long __fastcall__ x16_i32_add (long a, long b)
; long __fastcall__ x16_i32_sub (long a, long b)
; long __fastcall__ x16_i32_mul (long a, long b)  -- modulo 2^32
; ---------------------------------------------------------------------
_x16_i32_add:
        eax_to  i32_b                   ; b (rightmost arg, in EAX)
        jsr     popeax                  ; a
        eax_to  i32_a
        jsr     i32_add
        eax_from i32_a
        rts

_x16_i32_sub:
        eax_to  i32_b
        jsr     popeax
        eax_to  i32_a
        jsr     i32_sub
        eax_from i32_a
        rts

_x16_i32_mul:
        eax_to  i32_b
        jsr     popeax
        eax_to  i32_a
        jsr     i32_mul
        eax_from i32_a
        rts

; ---------------------------------------------------------------------
; long __fastcall__ x16_i32_neg (long a)
; long __fastcall__ x16_i32_abs (long a)
; long __fastcall__ x16_i32_shl (long a)          -- a << 1
; unsigned long __fastcall__ x16_i32_shr (unsigned long a) -- logical >> 1
; long __fastcall__ x16_i32_asr (long a)          -- arithmetic >> 1
; ---------------------------------------------------------------------
_x16_i32_neg:
        eax_to  i32_a
        jsr     i32_neg
        eax_from i32_a
        rts

_x16_i32_abs:
        eax_to  i32_a
        jsr     i32_abs
        eax_from i32_a
        rts

_x16_i32_shl:
        eax_to  i32_a
        jsr     i32_shl
        eax_from i32_a
        rts

_x16_i32_shr:
        eax_to  i32_a
        jsr     i32_shr
        eax_from i32_a
        rts

_x16_i32_asr:
        eax_to  i32_a
        jsr     i32_asr
        eax_from i32_a
        rts

; ---------------------------------------------------------------------
; signed char __fastcall__ x16_i32_cmpu (unsigned long a, unsigned long b)
; signed char __fastcall__ x16_i32_cmps (long a, long b)
;   -1 if a < b, 0 if equal, 1 if a > b
; ---------------------------------------------------------------------
_x16_i32_cmpu:
        eax_to  i32_b
        jsr     popeax
        eax_to  i32_a
        jsr     i32_cmpu
        sign_extend
        rts

_x16_i32_cmps:
        eax_to  i32_b
        jsr     popeax
        eax_to  i32_a
        jsr     i32_cmps
        sign_extend
        rts

; ---------------------------------------------------------------------
; unsigned long __fastcall__ x16_i32_divmod (unsigned long a,
;                                            unsigned long b,
;                                            unsigned long *rem)
;
;   Quotient returned, remainder through *rem. On b == 0 the upstream
;   routine changes nothing: the C entry returns a and leaves *rem
;   untouched.
; ---------------------------------------------------------------------
_x16_i32_divmod:
        sta     X16_T6                  ; rem (rightmost arg, in A/X)
        stx     X16_T7
        jsr     popeax                  ; b
        eax_to  i32_b
        jsr     popeax                  ; a
        eax_to  i32_a
        jsr     i32_divmod
        bcs     @divzero                ; b was zero: *rem stays untouched
        ldy     #3
@store:
        lda     i32_r,y
        sta     (X16_T6),y
        dey
        bpl     @store
@divzero:
        eax_from i32_a
        rts

; ---------------------------------------------------------------------
; char * __fastcall__ x16_i32_to_dec (unsigned long v)
;
;   ASCII decimal in a module buffer the next call overwrites,
;   NUL-terminated; the internal routine already answers A = buffer
;   low, X = high -- exactly cc65's pointer return.
; ---------------------------------------------------------------------
_x16_i32_to_dec:
        eax_to  i32_a
        jmp     i32_to_dec

; =====================================================================
; Internal routines (the upstream x16_library body, verbatim)
; =====================================================================

; ---------------------------------------------------------------------
; i32_from_u16 -- in: A = low, X = high.   i32_a = A/X, zero-extended
; i32_from_s16 -- in: A = low, X = high.   i32_a = A/X, sign-extended
; i32_to_s16   -- out: A = low, X = high   (the top two bytes are lost)
; ---------------------------------------------------------------------
i32_from_u16:
        sta     i32_a
        stx     i32_a+1
        stz     i32_a+2
        stz     i32_a+3
        rts

i32_from_s16:
        sta     i32_a
        stx     i32_a+1
        txa
        and     #$80
        beq     @positive
        lda     #$FF                    ; negative: fill the top with ones
        sta     i32_a+2
        sta     i32_a+3
        rts
@positive:
        stz     i32_a+2
        stz     i32_a+3
        rts

i32_to_s16:
        ldx     i32_a+1
        lda     i32_a
        rts

; ---------------------------------------------------------------------
; i32_add -- i32_a += i32_b
; i32_sub -- i32_a -= i32_b
; ---------------------------------------------------------------------
i32_add:
        clc
        lda     i32_a
        adc     i32_b
        sta     i32_a
        lda     i32_a+1
        adc     i32_b+1
        sta     i32_a+1
        lda     i32_a+2
        adc     i32_b+2
        sta     i32_a+2
        lda     i32_a+3
        adc     i32_b+3
        sta     i32_a+3
        rts

i32_sub:
        sec
        lda     i32_a
        sbc     i32_b
        sta     i32_a
        lda     i32_a+1
        sbc     i32_b+1
        sta     i32_a+1
        lda     i32_a+2
        sbc     i32_b+2
        sta     i32_a+2
        lda     i32_a+3
        sbc     i32_b+3
        sta     i32_a+3
        rts

; ---------------------------------------------------------------------
; i32_neg -- i32_a = -i32_a
; i32_abs -- i32_a = |i32_a|
; ---------------------------------------------------------------------
i32_neg:
        sec
        lda     #0
        sbc     i32_a
        sta     i32_a
        lda     #0
        sbc     i32_a+1
        sta     i32_a+1
        lda     #0
        sbc     i32_a+2
        sta     i32_a+2
        lda     #0
        sbc     i32_a+3
        sta     i32_a+3
        rts

i32_abs:
        lda     i32_a+3
        bmi     i32_neg
        rts

; ---------------------------------------------------------------------
; i32_shl -- i32_a <<= 1
; i32_shr -- i32_a >>= 1, logical (zero fill)
; i32_asr -- i32_a >>= 1, arithmetic (sign fill)
; Carry holds the bit shifted out.
; ---------------------------------------------------------------------
i32_shl:
        asl     i32_a
        rol     i32_a+1
        rol     i32_a+2
        rol     i32_a+3
        rts

i32_shr:
        lsr     i32_a+3
        ror     i32_a+2
        ror     i32_a+1
        ror     i32_a
        rts

i32_asr:
        lda     i32_a+3
        asl                             ; sign bit into carry
        ror     i32_a+3                 ; ...and back in at the top
        ror     i32_a+2
        ror     i32_a+1
        ror     i32_a
        rts

; ---------------------------------------------------------------------
; i32_cmpu -- unsigned compare i32_a with i32_b
; i32_cmps -- signed compare
;   out: A = $FF if a < b, 0 if equal, 1 if a > b
;        Z set when equal.  Neither operand is modified.
; ---------------------------------------------------------------------
i32_cmpu:
        lda     i32_a+3
        cmp     i32_b+3
        bne     @differ
        lda     i32_a+2
        cmp     i32_b+2
        bne     @differ
        lda     i32_a+1
        cmp     i32_b+1
        bne     @differ
        lda     i32_a
        cmp     i32_b
        bne     @differ
        lda     #0                      ; equal
        rts
@differ:
        bcs     @greater
        lda     #$FF
        rts
@greater:
        lda     #1
        rts

i32_cmps:
        ; Same-signed operands compare like unsigned values. Different signs
        ; short-circuit: the negative one is the smaller, whatever the bits.
        lda     i32_a+3
        eor     i32_b+3
        bpl     i32_cmpu                ; signs agree
        lda     i32_a+3
        bmi     @a_negative
        lda     #1                      ; a >= 0, b < 0
        rts
@a_negative:
        lda     #$FF
        rts

; ---------------------------------------------------------------------
; i32_mul -- i32_a = i32_a * i32_b, modulo 2^32
;
; Shift-and-add. Signed and unsigned agree on the low 32 bits, so this
; serves both; only the discarded overflow differs.
; ---------------------------------------------------------------------
i32_mul:
        lda     i32_a                   ; tmp = a, then rebuild a as the product
        sta     i32_tmp
        lda     i32_a+1
        sta     i32_tmp+1
        lda     i32_a+2
        sta     i32_tmp+2
        lda     i32_a+3
        sta     i32_tmp+3
        stz     i32_a
        stz     i32_a+1
        stz     i32_a+2
        stz     i32_a+3

        lda     #32
        sta     i32_cnt
@loop:
        lsr     i32_b+3                 ; next bit of the multiplier
        ror     i32_b+2
        ror     i32_b+1
        ror     i32_b
        bcc     @no_add

        clc                             ; a += tmp
        lda     i32_a
        adc     i32_tmp
        sta     i32_a
        lda     i32_a+1
        adc     i32_tmp+1
        sta     i32_a+1
        lda     i32_a+2
        adc     i32_tmp+2
        sta     i32_a+2
        lda     i32_a+3
        adc     i32_tmp+3
        sta     i32_a+3
@no_add:
        asl     i32_tmp                 ; tmp <<= 1
        rol     i32_tmp+1
        rol     i32_tmp+2
        rol     i32_tmp+3

        dec     i32_cnt
        bne     @loop
        rts

; ---------------------------------------------------------------------
; i32_divmod -- unsigned:  i32_a = i32_a / i32_b,  i32_r = i32_a % i32_b
;   out: carry set if i32_b was zero, in which case nothing is changed
;
; Restoring division: shift the dividend left through the remainder one
; bit at a time, subtracting the divisor whenever it fits.
; ---------------------------------------------------------------------
i32_divmod:
        lda     i32_b                   ; divide by zero?
        ora     i32_b+1
        ora     i32_b+2
        ora     i32_b+3
        bne     @go
        sec
        rts
@go:
        stz     i32_r
        stz     i32_r+1
        stz     i32_r+2
        stz     i32_r+3

        lda     #32
        sta     i32_cnt
@loop:
        asl     i32_a                   ; shift dividend out of the top of a...
        rol     i32_a+1
        rol     i32_a+2
        rol     i32_a+3
        rol     i32_r                   ; ...and into the bottom of r
        rol     i32_r+1
        rol     i32_r+2
        rol     i32_r+3

        sec                             ; trial subtraction r - b
        lda     i32_r
        sbc     i32_b
        sta     i32_tmp
        lda     i32_r+1
        sbc     i32_b+1
        sta     i32_tmp+1
        lda     i32_r+2
        sbc     i32_b+2
        sta     i32_tmp+2
        lda     i32_r+3
        sbc     i32_b+3
        sta     i32_tmp+3
        bcc     @restore                ; it did not fit: leave r alone

        lda     i32_tmp                 ; it fit: keep the difference
        sta     i32_r
        lda     i32_tmp+1
        sta     i32_r+1
        lda     i32_tmp+2
        sta     i32_r+2
        lda     i32_tmp+3
        sta     i32_r+3
        inc     i32_a                   ; and set the quotient bit
@restore:
        dec     i32_cnt
        bne     @loop
        clc
        rts

; ---------------------------------------------------------------------
; i32_to_dec -- unsigned i32_a to decimal, no leading zeros
;   out: A = buffer low, X = buffer high, Y = length
;        NUL-terminated, so screen_puts can print it directly.
;   Consumes i32_a and i32_b.
;
; Repeated division by ten, digits emitted least significant first and
; then reversed in place. The digits are ASCII $30-$39, written as hex
; constants -- ca65's -t cx16 target remaps character literals.
; ---------------------------------------------------------------------
i32_to_dec:
        ldy     #0
        sty     i32_digits
@divide:
        lda     #10                     ; i32_b = 10 (upstream: +i32_const)
        sta     i32_b
        stz     i32_b+1
        stz     i32_b+2
        stz     i32_b+3
        jsr     i32_divmod
        lda     i32_r                   ; remainder is the next digit
        clc
        adc     #$30                    ; '0' (ASCII)
        ldy     i32_digits
        sta     i32_buf,y
        inc     i32_digits

        lda     i32_a                   ; quotient zero yet?
        ora     i32_a+1
        ora     i32_a+2
        ora     i32_a+3
        bne     @divide

        ; Reverse the digits in place.
        ldx     #0
        ldy     i32_digits
        dey
@reverse:
        stx     i32_lo
        sty     i32_hi
        cpx     i32_hi
        bcs     @done                   ; pointers met or crossed
        lda     i32_buf,x
        pha
        lda     i32_buf,y
        sta     i32_buf,x
        pla
        sta     i32_buf,y
        inx
        dey
        bra     @reverse
@done:
        ldy     i32_digits
        lda     #0
        sta     i32_buf,y               ; terminate; Y is the length
        lda     #<i32_buf
        ldx     #>i32_buf
        rts

; ---------------------------------------------------------------------
; Module registers and scratch. Every byte is written before it is
; read, so BSS needs no initialiser.
; ---------------------------------------------------------------------
        .segment        "BSS"

i32_a:      .res 4
i32_b:      .res 4
i32_r:      .res 4

i32_tmp:    .res 4
i32_cnt:    .res 1

i32_buf:    .res 12                     ; "4294967295" plus a terminator
i32_digits: .res 1
i32_lo:     .res 1
i32_hi:     .res 1
