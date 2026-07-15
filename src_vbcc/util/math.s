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

        include        "macros.inc"
        include        "x16zp.inc"

; The lerp8 shim reads a,b out of vbcc's argument registers r0,r1.
        zpage   r0
        zpage   r1

        global	_x16_rnd_seed
        global	_x16_rnd8
        global	_x16_rnd16
        global	_x16_sin8
        global	_x16_cos8
        global	_x16_sin8u
        global	_x16_cos8u
        global	_x16_atan2
        global	_x16_lerp8

        section text

; A holds a signed byte; give X its sign extension so the value survives
; promotion to int.
sign_extend	macro
        ldx     #0
        cmp     #$80
        bcc +
        ldx     #$FF
+
endm

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void x16_rnd_seed(__reg("a/x") unsigned int seed)
; unsigned char x16_rnd8(void)
; unsigned int  x16_rnd16(void)
;
; The header pins seed to the a/x pair, so it arrives A = low, X = high:
; no shim. rnd8/rnd16 take nothing.
; ---------------------------------------------------------------------
_x16_rnd_seed:
rnd_seed:
        sta     rnd_state
        stx     rnd_state+1
        ora     rnd_state+1
        bne     .done
        inc     rnd_state               ; zero stays zero forever
.done:
        rts

_x16_rnd8:
        jmp     rnd16                   ; char return in A; X (high) ignored

_x16_rnd16:
        jmp     rnd16                   ; int return in A/X

; ---------------------------------------------------------------------
; signed char   x16_sin8(__reg("a") unsigned char angle)   -- -127..127
; signed char   x16_cos8(__reg("a") unsigned char angle)
; unsigned char x16_sin8u(__reg("a") unsigned char angle)  -- 1..255
; unsigned char x16_cos8u(__reg("a") unsigned char angle)
;
; angle arrives in A; each returns its char in A. vbcc handles the
; int-promotion sign extension itself, so the cc65 shim vanishes.
; ---------------------------------------------------------------------
_x16_sin8:
        jmp     sin8

_x16_cos8:
        jmp     cos8

_x16_sin8u:
        jmp     sin8u

_x16_cos8u:
        jmp     cos8u

; ---------------------------------------------------------------------
; unsigned char x16_atan2(__reg("a") signed char dx,
;                         __reg("r0") signed char dy)
;   The angle of the vector: 0 = east (+x), 64 = down-screen (+y).
;
; atan2 wants dx in A and dy in X. vbcc will not pass an argument in X
; (it is reserved for the code generator), so the header routes dy through
; r0 and the shim moves it into X.
; ---------------------------------------------------------------------
_x16_atan2:
        ldx     r0
        jmp     atan2

; ---------------------------------------------------------------------
; unsigned char x16_lerp8(__reg("r0") unsigned char a,
;                         __reg("r1") unsigned char b,
;                         __reg("a")  unsigned char t)
;   t = 0 gives exactly a, t = 255 exactly b.
;
; lerp8 reads a from X16_P0 and b from X16_P1 with t in A. The header
; delivers a,b in r0,r1 and t in A; the shim just moves the two bytes
; into the P block. (r0/r1 = vbcc's arg registers, distinct from P0/P1.)
; ---------------------------------------------------------------------
_x16_lerp8:
        pha                             ; save t
        lda     r0
        sta     X16_P0                  ; a
        lda     r1
        sta     X16_P1                  ; b
        pla                             ; t back into A
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
        bpl     .dx_pos
        inc     at_negx
        eor     #$FF
        clc
        adc     #1
.dx_pos:
        sta     at_ax
        txa                             ; |dy|
        stz     at_negy
        bpl     .dy_pos
        inc     at_negy
        eor     #$FF
        clc
        adc     #1
.dy_pos:
        sta     at_ay

        ; base angle 0..64 within the positive quadrant
        cmp     at_ax
        beq     .diag
        bcc     .shallow
        lda     at_ax                   ; steep: base = 64 - atan(ax/ay)
        ldx     at_ay
        jsr     ratio32
        tay
        sec
        lda     #64
        sbc     atantab,y
        bra     .quad
.diag:
        ora     at_ax
        bne     .is45
        lda     #0                      ; atan2(0,0): call it east
        rts
.is45:
        lda     #32                     ; exactly 45 degrees
        bra     .quad
.shallow:
        lda     at_ay                   ; shallow: base = atan(ay/ax)
        ldx     at_ax
        jsr     ratio32
        tay
        lda     atantab,y

.quad:
        ; fold the base angle into the right quadrant
        ldy     at_negx
        beq     .dx_ok
        eor     #$FF                    ; dx < 0: angle = 128 - base
        clc
        adc     #129
.dx_ok:
        ldy     at_negy
        beq     .done
        eor     #$FF                    ; dy < 0: angle = -angle
        clc
        adc     #1
.done:
        rts

; A = (A * 32) / X, for A <= X and X nonzero. Result 0..32.
ratio32:
        stx     at_den
        sta     at_num+1                ; num = A * 256...
        stz     at_num
        ldx     #3
.shift:
        lsr     at_num+1                ; ...then >> 3 = A * 32
        ror     at_num
        dex
        bne     .shift
        lda     #0                      ; 16-bit / 8-bit restoring divide
        ldx     #16
.div:
        asl     at_num
        rol     at_num+1
        rol     a
        cmp     at_den
        bcc     .no
        sbc     at_den
        inc     at_num
.no:
        dex
        bne     .div
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
        bcc     .down
        sbc     X16_P0                  ; carry set: a clean subtract
        jsr     scale_t
        clc
        adc     X16_P0
        rts
.down:
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
        beq     .whole                  ; t+1 = 256: the answer is d itself
        inc     a                       ; n = t+1, fits a byte
        sta     lp_n
        ; 8x8 multiply keeping only the high byte: per multiplier bit
        ; (LSB first), optionally add d, then rotate the result right.
        lda     #0
        ldx     #8
.mul:
        lsr     lp_n
        bcc     .skip
        clc
        adc     lp_d
.skip:
        ror     a
        dex
        bne     .mul
        rts
.whole:
        lda     lp_d
        rts

; ---------------------------------------------------------------------
; Tables. See the header: ca65 cannot compute these, so they are the
; literal output of ACME's build-time expressions.
;
;   sintab[i]  = int(sin(i * pi/128) * 127.0 + 128.5) - 128
;   atantab[i] = int(arctan(i/32.0) * 128.0/pi + 0.5)
; ---------------------------------------------------------------------
        section rodata

sintab:
        byte   $00, $03, $06, $09, $0C, $10, $13, $16
        byte   $19, $1C, $1F, $22, $25, $28, $2B, $2E
        byte   $31, $33, $36, $39, $3C, $3F, $41, $44
        byte   $47, $49, $4C, $4E, $51, $53, $55, $58
        byte   $5A, $5C, $5E, $60, $62, $64, $66, $68
        byte   $6A, $6B, $6D, $6F, $70, $71, $73, $74
        byte   $75, $76, $78, $79, $7A, $7A, $7B, $7C
        byte   $7D, $7D, $7E, $7E, $7E, $7F, $7F, $7F
        byte   $7F, $7F, $7F, $7F, $7E, $7E, $7E, $7D
        byte   $7D, $7C, $7B, $7A, $7A, $79, $78, $76
        byte   $75, $74, $73, $71, $70, $6F, $6D, $6B
        byte   $6A, $68, $66, $64, $62, $60, $5E, $5C
        byte   $5A, $58, $55, $53, $51, $4E, $4C, $49
        byte   $47, $44, $41, $3F, $3C, $39, $36, $33
        byte   $31, $2E, $2B, $28, $25, $22, $1F, $1C
        byte   $19, $16, $13, $10, $0C, $09, $06, $03
        byte   $00, $FD, $FA, $F7, $F4, $F0, $ED, $EA
        byte   $E7, $E4, $E1, $DE, $DB, $D8, $D5, $D2
        byte   $CF, $CD, $CA, $C7, $C4, $C1, $BF, $BC
        byte   $B9, $B7, $B4, $B2, $AF, $AD, $AB, $A8
        byte   $A6, $A4, $A2, $A0, $9E, $9C, $9A, $98
        byte   $96, $95, $93, $91, $90, $8F, $8D, $8C
        byte   $8B, $8A, $88, $87, $86, $86, $85, $84
        byte   $83, $83, $82, $82, $82, $81, $81, $81
        byte   $81, $81, $81, $81, $82, $82, $82, $83
        byte   $83, $84, $85, $86, $86, $87, $88, $8A
        byte   $8B, $8C, $8D, $8F, $90, $91, $93, $95
        byte   $96, $98, $9A, $9C, $9E, $A0, $A2, $A4
        byte   $A6, $A8, $AB, $AD, $AF, $B2, $B4, $B7
        byte   $B9, $BC, $BF, $C1, $C4, $C7, $CA, $CD
        byte   $CF, $D2, $D5, $D8, $DB, $DE, $E1, $E4
        byte   $E7, $EA, $ED, $F0, $F4, $F7, $FA, $FD

atantab:
        byte   $00, $01, $03, $04, $05, $06, $08, $09, $0A, $0B, $0C
        byte   $0D, $0F, $10, $11, $12, $13, $14, $15, $16, $17, $18
        byte   $19, $19, $1A, $1B, $1C, $1D, $1D, $1E, $1F, $1F, $20

; ---------------------------------------------------------------------
; The PRNG seed has a nonzero default, so it lives in DATA (loaded from
; the PRG) rather than BSS (zeroed by crt0). A zero state is xorshift's
; one fixed point.
; ---------------------------------------------------------------------
        section data

rnd_state: word $2A56

        section bss

at_ax:   reserve 1
at_ay:   reserve 1
at_negx: reserve 1
at_negy: reserve 1
at_num:  reserve 2
at_den:  reserve 1

lp_t:    reserve 1
lp_n:    reserve 1
lp_d:    reserve 1
