; =====================================================================
; x16clib :: util/math.s -- game math: PRNG, sine tables, atan2, lerp
; =====================================================================
; Angles are bytes: a full circle is 256, so 64 = 90 degrees and
; wrap-around is free. With the X16's y axis pointing DOWN the screen,
; angle 0 points east (+x) and 64 points south (+y) -- atan2 and the sine
; tables agree on that, so
;       x += (cos8(a) * speed) >> 7 ; y += (sin8(a) * speed) >> 7
; moves along the heading atan2 returned.
;
; ---------------------------------------------------------------------
; THE TABLES ARE PRECOMPUTED, NOT GENERATED.
;
; ACME evaluates floating point at assembly time, so the original wrote
;       !for i, 0, 255 { !byte int(sin(i*pi/128)*127.0 + 128.5) - 128 }
; ca65 has no floating point in expressions at all -- there is no
; translation. The bytes below are the output of exactly those two
; expressions, and MATH_TABLES in the test suite pins the values that
; would move if anyone regenerated them wrong.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   INTEGER bytes fill A, then X, then __rc2, __rc3, ... left to right.
;   A POINTER takes a whole __rc pair (__rc2/__rc3, then __rc4/__rc5) and
;   consumes no A/X -- only zero page can be indirected through.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_rnd_seed
        .globl  x16_rnd8
        .globl  x16_rnd16
        .globl  x16_sin8
        .globl  x16_cos8
        .globl  x16_sin8u
        .globl  x16_cos8u
        .globl  x16_atan2
        .globl  x16_lerp8

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

; ---------------------------------------------------------------------
; void __fastcall__ x16_rnd_seed(unsigned int seed)
; unsigned char x16_rnd8(void)
; unsigned int  x16_rnd16(void)
;
; The seed already arrives as A = low, X = high: no shim.
; ---------------------------------------------------------------------
x16_rnd_seed:
rnd_seed:
        sta     rnd_state
        stx     rnd_state+1
        ora     rnd_state+1
        bne     .Lrnd_seed_done
        inc     rnd_state               ; zero stays zero forever
.Lrnd_seed_done:
        rts

x16_rnd8:
        jmp     rnd16                   ; a char return is A alone; X ignored

x16_rnd16:
        jmp     rnd16

; ---------------------------------------------------------------------
; signed char __fastcall__ x16_sin8(unsigned char angle)   -- -127..127
; signed char __fastcall__ x16_cos8(unsigned char angle)
; unsigned char __fastcall__ x16_sin8u(unsigned char angle) -- 1..255
; unsigned char __fastcall__ x16_cos8u(unsigned char angle)
;
; The angle already arrives in A: no shim, only the return convention.
; ---------------------------------------------------------------------
; A signed char comes back in A alone -- llvm-mos widens it at the call
; site, so the cc65 build's sign_extend (which set X) is unnecessary.
x16_sin8:
        jmp     sin8

x16_cos8:
        jmp     cos8

x16_sin8u:
        jmp     sin8u

x16_cos8u:
        jmp     cos8u

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_atan2(signed char dx, signed char dy)
;   The angle of the vector: 0 = east (+x), 64 = down-screen (+y).
;
; popa leaves X alone, so dy goes to X before dx is popped into A.
; ---------------------------------------------------------------------
; A = dx, X = dy -- exactly atan2's contract, so no marshalling at all.
x16_atan2:
        jmp     atan2

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_lerp8(unsigned char a, unsigned char b,
;                                      unsigned char t)
;   t = 0 gives exactly a, t = 255 exactly b.
; ---------------------------------------------------------------------
; A = a, X = b, __rc2 = t.
x16_lerp8:
        sta     X16_P0                  ; a
        stx     X16_P1                  ; b
        lda     __rc2                   ; A = t
        jmp     lerp8

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; rnd8 / rnd16 -- John Metcalf's 16-bit xorshift (shifts 7, 9, 8):
; period 65535 and a handful of cycles, cheap enough per frame per object.
;   out: A = low, X = high
; ---------------------------------------------------------------------
rnd8:                                   ; same routine; read A, ignore X
rnd16:
        lda     rnd_state+1
        lsr     a
        lda     rnd_state
        ror     a
        eor     rnd_state+1
        sta     rnd_state+1             ; x ^= x >> 9
        ror     a
        eor     rnd_state
        sta     rnd_state               ; x ^= x << 7
        eor     rnd_state+1
        sta     rnd_state+1             ; x ^= x << 8
        lda     rnd_state
        ldx     rnd_state+1
        rts

; ---------------------------------------------------------------------
; sin8 / cos8   -- in: A = angle 0-255.  out: A = -127..127 signed
; sin8u / cos8u -- in: A = angle 0-255.  out: A = 1..255 unsigned
; Preserve X; clobber Y.
; ---------------------------------------------------------------------
sin8:
        tay
        lda     sintab,y
        rts

cos8:
        clc
        adc     #64                     ; cos(a) = sin(a + 90 degrees)
        tay
        lda     sintab,y
        rts

sin8u:
        tay
        lda     sintab,y
        clc
        adc     #128
        rts

cos8u:
        clc
        adc     #64
        tay
        lda     sintab,y
        clc
        adc     #128
        rts

; ---------------------------------------------------------------------
; atan2 -- the angle of a vector
;   in:  A = dx, X = dy  (signed bytes)
;   out: A = angle 0-255 (0 = +x/east, 64 = +y/down-screen)
;
; Octant reduction plus a 33-entry arctangent table; the only work is one
; 8-bit divide. atan2(0,0) answers 0.
; ---------------------------------------------------------------------
atan2:
        stz     at_negx
        tay                             ; |dx|, remembering the sign
        bpl     .Latan2_dx_pos
        inc     at_negx
        eor     #$FF
        clc
        adc     #1
.Latan2_dx_pos:
        sta     at_ax
        txa                             ; |dy|
        stz     at_negy
        bpl     .Latan2_dy_pos
        inc     at_negy
        eor     #$FF
        clc
        adc     #1
.Latan2_dy_pos:
        sta     at_ay

        ; base angle 0..64 within the positive quadrant
        cmp     at_ax
        beq     .Latan2_diag
        bcc     .Latan2_shallow
        lda     at_ax                   ; steep: base = 64 - atan(ax/ay)
        ldx     at_ay
        jsr     ratio32
        tay
        sec
        lda     #64
        sbc     atantab,y
        bra     .Latan2_quad
.Latan2_diag:
        ora     at_ax
        bne     .Latan2_is45
        lda     #0                      ; atan2(0,0): call it east
        rts
.Latan2_is45:
        lda     #32                     ; exactly 45 degrees
        bra     .Latan2_quad
.Latan2_shallow:
        lda     at_ay                   ; shallow: base = atan(ay/ax)
        ldx     at_ax
        jsr     ratio32
        tay
        lda     atantab,y

.Latan2_quad:
        ; fold the base angle into the right quadrant
        ldy     at_negx
        beq     .Latan2_dx_ok
        eor     #$FF                    ; dx < 0: angle = 128 - base
        clc
        adc     #129
.Latan2_dx_ok:
        ldy     at_negy
        beq     .Latan2_done
        eor     #$FF                    ; dy < 0: angle = -angle
        clc
        adc     #1
.Latan2_done:
        rts

; A = (A * 32) / X, for A <= X and X nonzero. Result 0..32.
ratio32:
        stx     at_den
        sta     at_num+1                ; num = A * 256...
        stz     at_num
        ldx     #3
.Lratio32_shift:
        lsr     at_num+1                ; ...then >> 3 = A * 32
        ror     at_num
        dex
        bne     .Lratio32_shift
        lda     #0                      ; 16-bit / 8-bit restoring divide
        ldx     #16
.Lratio32_div:
        asl     at_num
        rol     at_num+1
        rol     a
        cmp     at_den
        bcc     .Lratio32_no
        sbc     at_den
        inc     at_num
.Lratio32_no:
        dex
        bne     .Lratio32_div
        lda     at_num                  ; the quotient
        rts

; ---------------------------------------------------------------------
; lerp8 -- linear interpolation between two unsigned bytes
;   in:  X16_P0 = a, X16_P1 = b, A = t (0 = a ... 255 = b)
;   out: A = the interpolated value; t=0 is exactly a, t=255 exactly b
;
; Computes a +/- (|b-a| * (t+1)) / 256 -- at most one off from the ideal
; /255 midway, exact at both ends.
; ---------------------------------------------------------------------
lerp8:
        sta     lp_t
        lda     X16_P1
        cmp     X16_P0
        bcc     .Llerp8_down
        sbc     X16_P0                  ; carry set: a clean subtract
        jsr     scale_t
        clc
        adc     X16_P0
        rts
.Llerp8_down:
        lda     X16_P0                  ; b < a: interpolate downwards
        sec
        sbc     X16_P1
        jsr     scale_t
        sta     lp_d
        sec
        lda     X16_P0
        sbc     lp_d
        rts

; A = (A * (lp_t + 1)) >> 8
scale_t:
        sta     lp_d
        lda     lp_t
        cmp     #$FF
        beq     .Lscale_t_whole                  ; t+1 = 256: the answer is d itself
        inc     a                       ; n = t+1, fits a byte
        sta     lp_n
        ; 8x8 multiply keeping only the high byte: per multiplier bit
        ; (LSB first), optionally add d, then rotate the result right.
        lda     #0
        ldx     #8
.Lscale_t_mul:
        lsr     lp_n
        bcc     .Lscale_t_skip
        clc
        adc     lp_d
.Lscale_t_skip:
        ror     a
        dex
        bne     .Lscale_t_mul
        rts
.Lscale_t_whole:
        lda     lp_d
        rts

; ---------------------------------------------------------------------
; Tables. See the header: ca65 cannot compute these, so they are the
; literal output of ACME's build-time expressions.
;
;   sintab[i]  = int(sin(i * pi/128) * 127.0 + 128.5) - 128
;   atantab[i] = int(arctan(i/32.0) * 128.0/pi + 0.5)
; ---------------------------------------------------------------------
        .section .rodata,"a",@progbits

sintab:
        .byte   $00, $03, $06, $09, $0C, $10, $13, $16
        .byte   $19, $1C, $1F, $22, $25, $28, $2B, $2E
        .byte   $31, $33, $36, $39, $3C, $3F, $41, $44
        .byte   $47, $49, $4C, $4E, $51, $53, $55, $58
        .byte   $5A, $5C, $5E, $60, $62, $64, $66, $68
        .byte   $6A, $6B, $6D, $6F, $70, $71, $73, $74
        .byte   $75, $76, $78, $79, $7A, $7A, $7B, $7C
        .byte   $7D, $7D, $7E, $7E, $7E, $7F, $7F, $7F
        .byte   $7F, $7F, $7F, $7F, $7E, $7E, $7E, $7D
        .byte   $7D, $7C, $7B, $7A, $7A, $79, $78, $76
        .byte   $75, $74, $73, $71, $70, $6F, $6D, $6B
        .byte   $6A, $68, $66, $64, $62, $60, $5E, $5C
        .byte   $5A, $58, $55, $53, $51, $4E, $4C, $49
        .byte   $47, $44, $41, $3F, $3C, $39, $36, $33
        .byte   $31, $2E, $2B, $28, $25, $22, $1F, $1C
        .byte   $19, $16, $13, $10, $0C, $09, $06, $03
        .byte   $00, $FD, $FA, $F7, $F4, $F0, $ED, $EA
        .byte   $E7, $E4, $E1, $DE, $DB, $D8, $D5, $D2
        .byte   $CF, $CD, $CA, $C7, $C4, $C1, $BF, $BC
        .byte   $B9, $B7, $B4, $B2, $AF, $AD, $AB, $A8
        .byte   $A6, $A4, $A2, $A0, $9E, $9C, $9A, $98
        .byte   $96, $95, $93, $91, $90, $8F, $8D, $8C
        .byte   $8B, $8A, $88, $87, $86, $86, $85, $84
        .byte   $83, $83, $82, $82, $82, $81, $81, $81
        .byte   $81, $81, $81, $81, $82, $82, $82, $83
        .byte   $83, $84, $85, $86, $86, $87, $88, $8A
        .byte   $8B, $8C, $8D, $8F, $90, $91, $93, $95
        .byte   $96, $98, $9A, $9C, $9E, $A0, $A2, $A4
        .byte   $A6, $A8, $AB, $AD, $AF, $B2, $B4, $B7
        .byte   $B9, $BC, $BF, $C1, $C4, $C7, $CA, $CD
        .byte   $CF, $D2, $D5, $D8, $DB, $DE, $E1, $E4
        .byte   $E7, $EA, $ED, $F0, $F4, $F7, $FA, $FD

atantab:
        .byte   $00, $01, $03, $04, $05, $06, $08, $09, $0A, $0B, $0C
        .byte   $0D, $0F, $10, $11, $12, $13, $14, $15, $16, $17, $18
        .byte   $19, $19, $1A, $1B, $1C, $1D, $1D, $1E, $1F, $1F, $20

; ---------------------------------------------------------------------
; The PRNG seed has a nonzero default, so it lives in DATA (loaded from
; the PRG) rather than BSS (zeroed by crt0). A zero state is xorshift's
; one fixed point.
; ---------------------------------------------------------------------
        .section .data,"aw",@progbits

rnd_state: .word $2A56

        .section .bss,"aw",@nobits

at_ax:   .zero  1
at_ay:   .zero  1
at_negx: .zero  1
at_negy: .zero  1
at_num:  .zero  2
at_den:  .zero  1

lp_t:    .zero  1
lp_n:    .zero  1
lp_d:    .zero  1
