; =====================================================================
; x16clib :: util/double.s -- 64-bit software floating point (binary64)
; =====================================================================
; A port of the upstream assembly library's util/double.asm.
;
; util/float.s binds the ROM's 5-byte float (about 9 significant
; digits). Fine for graphics, thin for a calculator. This is a
; from-scratch IEEE-754 double: 8 bytes, ~15-16 significant digits, the
; full 10^+/-308 range -- the ROM has nothing to lean on, so it is all
; software.
;
; The shape mirrors float.s: a floating accumulator d_ac (like FAC)
; and operations that take a pointer to a memory operand and act on
; d_ac. A double in memory is 8 bytes, little-endian IEEE-754 binary64
; -- byte 0 is the low mantissa byte, byte 7 carries the sign and the
; top of the exponent -- so values interoperate with anyone else's
; doubles. Subnormals are flushed to zero; overflow makes an infinity;
; round-to-nearest-even throughout.
;
; The decimal I/O is ASCII: digits $30-$39, '-' $2D, '+' $2B, '.' $2E,
; the exponent marker 'E' $45 (and $65 'e' accepted on input), and the
; specials spelled "INF" / "NAN" in ASCII capitals. They are written as
; hex constants because ca65's -t cx16 target remaps character literals
; to PETSCII; a C caller comparing against literals needs an ASCII
; charmap (or explicit bytes) on its side too.
;
; d_to_str's result lands in a module buffer that the NEXT CALL
; OVERWRITES -- the C entry returns a pointer into it, so copy the
; text out if you need to keep it.
;
; --- internal unpacked form ------------------------------------------
;   dac_c  class: 0 zero, 1 normal, 2 infinity, 3 NaN
;   dac_s  sign in bit 7
;   dac_e  exponent, signed 16-bit
;   dac_m  64-bit significand, little-endian, normalised so bit 63 = 1
;          value = (-1)^sign * dac_m * 2^dac_e
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; (import dropped: popax)
; (import dropped: sreg)

        .globl  x16_d_load
        .globl  x16_d_store
        .globl  x16_d_neg
        .globl  x16_d_abs
        .globl  x16_d_from_s16
        .globl  x16_d_from_s32
        .globl  x16_d_to_s32
        .globl  x16_d_cmp
        .globl  x16_d_add
        .globl  x16_d_sub
        .globl  x16_d_mul
        .globl  x16_d_div
        .globl  x16_d_sqrt
        .globl  x16_d_exp
        .globl  x16_d_ln
        .globl  x16_d_pow
        .globl  x16_d_sin
        .globl  x16_d_cos
        .globl  x16_d_tan
        .globl  x16_d_atan
        .globl  x16_d_sinh
        .globl  x16_d_cosh
        .globl  x16_d_tanh
        .globl  x16_d_from_str
        .globl  x16_d_to_str

        .section .text,"ax",@progbits

; llvm-mos hands a pointer over in the aligned pair __rc2/__rc3. The
; internal routines want A = low, Y = high.
.macro  ptr_ax_to_ay
        lda     mos8(__rc2)
        ldy     mos8(__rc3)
.endm

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
; void __fastcall__ x16_d_load (const x16_double m)   -- d_ac = *m
; void __fastcall__ x16_d_store (x16_double m)        -- *m = d_ac
; ---------------------------------------------------------------------
x16_d_load:
        ptr_ax_to_ay
        jmp     d_load

x16_d_store:
        ptr_ax_to_ay
        jmp     d_store

; ---------------------------------------------------------------------
; void x16_d_neg (void)   void x16_d_abs (void)
; ---------------------------------------------------------------------
x16_d_neg:
        jmp     d_neg

x16_d_abs:
        jmp     d_abs

; ---------------------------------------------------------------------
; void __fastcall__ x16_d_from_s16 (int v)
; void __fastcall__ x16_d_from_s32 (long v)
; long x16_d_to_s32 (void)    -- truncates toward zero; NaN and inf
;                                answer 0, out-of-range values clamp
;                                (the assembly-level carry is not
;                                visible from C)
; ---------------------------------------------------------------------
x16_d_from_s16:
        jmp     d_from_s16              ; v already arrives as A = low, X = high

x16_d_from_s32:
        sta     mos8(X16_P0)            ; v: A/X/__rc2/__rc3
        stx     mos8(X16_P1)
        lda     mos8(__rc2)
        sta     mos8(X16_P2)
        lda     mos8(__rc3)
        sta     mos8(X16_P3)
        jmp     d_from_s32

x16_d_to_s32:
        jsr     d_to_s32
        lda     mos8(X16_P2)
        sta     mos8(__rc2)
        lda     mos8(X16_P3)
        sta     mos8(__rc3)
        lda     mos8(X16_P0)
        ldx     mos8(X16_P1)
        rts

; ---------------------------------------------------------------------
; signed char __fastcall__ x16_d_cmp (const x16_double m)
;   -1 if d_ac < *m, 0 if equal, 1 if d_ac > *m; NaN on either side is
;   unordered and answers 1.
; ---------------------------------------------------------------------
x16_d_cmp:
        ptr_ax_to_ay
        jmp     d_cmp

; ---------------------------------------------------------------------
; void __fastcall__ x16_d_add (const x16_double m)   -- d_ac += *m
; void __fastcall__ x16_d_sub (const x16_double m)   -- d_ac -= *m
; void __fastcall__ x16_d_mul (const x16_double m)   -- d_ac *= *m
; void __fastcall__ x16_d_div (const x16_double m)   -- d_ac /= *m
; void __fastcall__ x16_d_pow (const x16_double m)   -- d_ac ^= *m
; ---------------------------------------------------------------------
x16_d_add:
        ptr_ax_to_ay
        jmp     d_add

x16_d_sub:
        ptr_ax_to_ay
        jmp     d_sub

x16_d_mul:
        ptr_ax_to_ay
        jmp     d_mul

x16_d_div:
        ptr_ax_to_ay
        jmp     d_div

x16_d_pow:
        ptr_ax_to_ay
        jmp     d_pow

; ---------------------------------------------------------------------
; The transcendentals. Each replaces d_ac.
; ---------------------------------------------------------------------
x16_d_sqrt:
        jmp     d_sqrt

x16_d_exp:
        jmp     d_exp

x16_d_ln:
        jmp     d_ln

x16_d_sin:
        jmp     d_sin

x16_d_cos:
        jmp     d_cos

x16_d_tan:
        jmp     d_tan

x16_d_atan:
        jmp     d_atan

x16_d_sinh:
        jmp     d_sinh

x16_d_cosh:
        jmp     d_cosh

x16_d_tanh:
        jmp     d_tanh

; ---------------------------------------------------------------------
; void __fastcall__ x16_d_from_str (const char *s, unsigned char len)
; char * x16_d_to_str (void)
;
; d_from_str takes an explicit length -- no terminator scan, so a slice
; of a longer string works. d_to_str answers the module buffer, which
; the internal routine returns as A = low, X = high; llvm-mos wants a
; pointer return in __rc2/__rc3.
; ---------------------------------------------------------------------
x16_d_from_str:
        tax                             ; X = length (A held it)
        lda     mos8(__rc2)             ; A = address low
        ldy     mos8(__rc3)             ; Y = address high
        jmp     d_from_str

x16_d_to_str:
        jsr     d_to_str                ; answers A = low, X = high
        sta     mos8(__rc2)
        stx     mos8(__rc3)
        rts

; =====================================================================
; Internal routines (the upstream x16_library body, verbatim)
; =====================================================================

D_SIZE = 8

D_ZERO = 0
D_NORM = 1
D_INF  = 2
D_NAN  = 3

d_ptr    = X16_TPTR0       ; operand pointer (shared scratch)
dstr_ptr = X16_TPTR1       ; d_from_str's string pointer (survives the
        ; inner d_* calls, which only touch TPTR0)

; ---------------------------------------------------------------------
; d_load  -- in: A = low, Y = high of an 8-byte double.  d_ac = mem
; d_store -- in: A = low, Y = high.  mem = d_ac
; ---------------------------------------------------------------------
d_load:
        sta     d_ptr
        sty     d_ptr+1
        ldy     #D_SIZE-1
double_dld_l:
        lda     (d_ptr),y
        sta     d_ac,y
        dey
        bpl     double_dld_l
        rts

d_store:
        sta     d_ptr
        sty     d_ptr+1
        ldy     #D_SIZE-1
double_dst_l:
        lda     d_ac,y
        sta     (d_ptr),y
        dey
        bpl     double_dst_l
        rts

; ---------------------------------------------------------------------
; d_neg -- d_ac = -d_ac      d_abs -- d_ac = |d_ac|
; ---------------------------------------------------------------------
d_neg:
        lda     d_ac+7
        eor     #$80
        sta     d_ac+7
        rts

d_abs:
        lda     d_ac+7
        and     #$7F
        sta     d_ac+7
        rts

; ---------------------------------------------------------------------
; d_from_s16 -- in: A = low, X = high (signed 16-bit).  d_ac = value
; ---------------------------------------------------------------------
d_from_s16:
        sta     mos8(X16_P0)
        stx     mos8(X16_P1)
        txa                             ; sign-extend into P2/P3
        and     #$80
        beq     double_dfs16_pos
        lda     #$FF
double_dfs16_pos:
        sta     mos8(X16_P2)
        sta     mos8(X16_P3)
        ; fall through

; ---------------------------------------------------------------------
; d_from_s32 -- in: X16_P0..P3 = signed 32-bit, little-endian.
;               d_ac = value (exact; 32 bits fit the 53-bit mantissa)
; ---------------------------------------------------------------------
d_from_s32:
        lda     mos8(X16_P3)            ; remember the sign, then take |value|
        and     #$80
        sta     dac_s
        beq     double_dfr_mag
        sec                             ; negate P0..P3
        lda     #0
        sbc     mos8(X16_P0)
        sta     mos8(X16_P0)
        lda     #0
        sbc     mos8(X16_P1)
        sta     mos8(X16_P1)
        lda     #0
        sbc     mos8(X16_P2)
        sta     mos8(X16_P2)
        lda     #0
        sbc     mos8(X16_P3)
        sta     mos8(X16_P3)
double_dfr_mag:
        lda     mos8(X16_P0)
        ora     mos8(X16_P1)
        ora     mos8(X16_P2)
        ora     mos8(X16_P3)
        bne     double_dfr_nz
        jmp     double_d_zero_signed    ; +/- 0
double_dfr_nz:
        ; magnitude into the top 32 bits of dac_m (= value * 2^32), then
        ; normalise up to bit 63.
        stz     dac_m
        stz     dac_m+1
        stz     dac_m+2
        stz     dac_m+3
        lda     mos8(X16_P0)
        sta     dac_m+4
        lda     mos8(X16_P1)
        sta     dac_m+5
        lda     mos8(X16_P2)
        sta     dac_m+6
        lda     mos8(X16_P3)
        sta     dac_m+7
        lda     #<(-32)
        sta     dac_e
        lda     #>(-32)
        sta     dac_e+1
        lda     #D_NORM
        sta     dac_c
        stz     d_sticky                ; an integer converts exactly
        jsr     double_d_norm
        jmp     double_d_pack

; ---------------------------------------------------------------------
; d_to_s32 -- out: X16_P0..P3 = (s32) d_ac, truncated toward zero.
;   carry set on overflow (|d_ac| too big; result clamped) or NaN.
;
; value = dac_m * 2^dac_e with dac_m normalised (bit 63 = 1). The
; integer part is dac_m >> (-dac_e). For an s32-range value dac_e lies
; in -63..-33; dac_e >= -32 overflows, dac_e <= -64 truncates to 0.
; ---------------------------------------------------------------------
d_to_s32:
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack         ; d_ac -> dac_*
        lda     dac_c
        cmp     #D_NORM
        beq     double_dto_norm
        stz     mos8(X16_P0)            ; zero -> 0 ; inf/nan -> 0 + carry
        stz     mos8(X16_P1)
        stz     mos8(X16_P2)
        stz     mos8(X16_P3)
        cmp     #D_ZERO
        beq     double_dto_okz
        sec
        rts
double_dto_okz:
        clc
        rts
double_dto_norm:
        ; shift = -dac_e
        sec
        lda     #0
        sbc     dac_e
        sta     d_cnt
        lda     #0
        sbc     dac_e+1
        sta     d_cnt+1
        ; shift < 33  -> overflow  (signed compare d_cnt vs 33)
        lda     d_cnt
        cmp     #33
        lda     d_cnt+1
        sbc     #0
        bvc     double_dto_v1
        eor     #$80
double_dto_v1:
        bmi     double_dto_over
        ; shift > 63  -> 0  (signed compare d_cnt vs 64)
        lda     d_cnt
        cmp     #64
        lda     d_cnt+1
        sbc     #0
        bvc     double_dto_v2
        eor     #$80
double_dto_v2:
        bpl     double_dto_tiny
        ; 33..63: shift dac_m right d_cnt into the work area
        ldy     #7
double_dto_cp:
        lda     dac_m,y
        sta     d_work,y
        dey
        bpl     double_dto_cp
        ldx     d_cnt
double_dto_sr:
        lsr     d_work+7
        ror     d_work+6
        ror     d_work+5
        ror     d_work+4
        ror     d_work+3
        ror     d_work+2
        ror     d_work+1
        ror     d_work
        dex
        bne     double_dto_sr
        lda     d_work
        sta     mos8(X16_P0)            ; low 32 bits are the integer
        lda     d_work+1
        sta     mos8(X16_P1)
        lda     d_work+2
        sta     mos8(X16_P2)
        lda     d_work+3
        sta     mos8(X16_P3)
        lda     dac_s
        bpl     double_dto_pos
        sec                             ; apply the sign
        lda     #0
        sbc     mos8(X16_P0)
        sta     mos8(X16_P0)
        lda     #0
        sbc     mos8(X16_P1)
        sta     mos8(X16_P1)
        lda     #0
        sbc     mos8(X16_P2)
        sta     mos8(X16_P2)
        lda     #0
        sbc     mos8(X16_P3)
        sta     mos8(X16_P3)
double_dto_pos:
        clc
        rts
double_dto_tiny:
        stz     mos8(X16_P0)
        stz     mos8(X16_P1)
        stz     mos8(X16_P2)
        stz     mos8(X16_P3)
        clc
        rts
double_dto_over:
        lda     dac_s
        bmi     double_dto_oneg
        lda     #$FF
        sta     mos8(X16_P0)            ; +2147483647
        sta     mos8(X16_P1)
        sta     mos8(X16_P2)
        lda     #$7F
        sta     mos8(X16_P3)
        sec
        rts
double_dto_oneg:
        stz     mos8(X16_P0)            ; -2147483648
        stz     mos8(X16_P1)
        stz     mos8(X16_P2)
        lda     #$80
        sta     mos8(X16_P3)
        sec
        rts

; ---------------------------------------------------------------------
; d_cmp -- in: A = low, Y = high of an operand.
;   out: A = $FF if d_ac < mem, 0 if equal, 1 if d_ac > mem.  Z if equal.
;        A NaN on either side is unordered and answers 1.
; ---------------------------------------------------------------------
d_cmp:
        sta     d_ptr
        sty     d_ptr+1
        jsr     double_d_unpack         ; operand -> dac_*
        jsr     double_d_ac_to_bf       ; ...move it to dbf_*
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack         ; d_ac -> dac_*

        lda     dac_c
        cmp     #D_NAN
        beq     double_dcm_gt
        lda     dbf_c
        cmp     #D_NAN
        beq     double_dcm_gt
        lda     dac_c                   ; both zero -> equal (any sign)
        ora     dbf_c
        bne     double_dcm_nz
        lda     #0
        rts
double_dcm_nz:
        lda     dac_s                   ; different signs decide
        eor     dbf_s
        bpl     double_dcm_same
        lda     dac_s
        bmi     double_dcm_lt           ; a<0, b>=0
        bra     double_dcm_gt           ; a>=0, b<0
double_dcm_same:
        jsr     double_d_mag_cmp        ; A = -1/0/1 by |a| vs |b|
        tax
        lda     dac_s
        bpl     double_dcm_done         ; positive: order as computed
        txa                             ; negative: reverse
        eor     #$FF
        clc
        adc     #1
        tax
double_dcm_done:
        txa
        rts
double_dcm_lt:
        lda     #$FF
        rts
double_dcm_gt:
        lda     #1
        rts

; |a| (dac_*) vs |b| (dbf_*): A = $FF/0/1
double_d_mag_cmp:
        lda     dac_c                   ; zero < normal < inf
        cmp     dbf_c
        bne     double_dmg_class
        cmp     #D_NORM
        beq     double_dmg_num
        lda     #0
        rts
double_dmg_class:
        bcs     double_dmg_gt
        lda     #$FF
        rts
double_dmg_gt:
        lda     #1
        rts
double_dmg_num:
        lda     dac_e+1                 ; exponent (signed 16-bit)
        cmp     dbf_e+1
        bne     double_dmg_ehi
        lda     dac_e
        cmp     dbf_e
        bne     double_dmg_elo
        ldy     #7                      ; equal exponent: mantissa, high first
double_dmg_ml:
        lda     dac_m,y
        cmp     dbf_m,y
        bne     double_dmg_md
        dey
        bpl     double_dmg_ml
        lda     #0
        rts
double_dmg_md:
        bcs     double_dmg_gt
        lda     #$FF
        rts
double_dmg_elo:
        bcs     double_dmg_gt
        lda     #$FF
        rts
double_dmg_ehi:
        lda     dac_e+1
        sec
        sbc     dbf_e+1
        bvc     double_dmg_ev
        eor     #$80
double_dmg_ev:
        bmi     double_dmg_lt
        lda     #1
        rts
double_dmg_lt:
        lda     #$FF
        rts

; ---------------------------------------------------------------------
; d_add -- d_ac += mem(A/Y)        d_sub -- d_ac -= mem(A/Y)
;
; Classic align/add-or-subtract/normalise. Bits shifted out during the
; alignment become a sticky bit that feeds double_d_pack's rounding. Near-total
; cancellation keeps only a sticky (not full guard bits), so a subtraction
; that annihilates most of the significand can round up to 1 ulp loose --
; refined in a later pass; ordinary sums are correctly rounded.
; ---------------------------------------------------------------------
d_sub:
        sta     d_ptr
        sty     d_ptr+1
        jsr     double_d_unpack         ; operand -> dac_*
        lda     dac_s
        eor     #$80                    ; subtract = add the negation
        sta     dac_s
        jmp     double_d_add_common
d_add:
        sta     d_ptr
        sty     d_ptr+1
        jsr     double_d_unpack         ; operand -> dac_*
double_d_add_common:
        jsr     double_d_ac_to_bf       ; operand -> dbf_*
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack         ; d_ac -> dac_*
        stz     d_sticky

        lda     dac_c                   ; NaN on either side -> NaN
        cmp     #D_NAN
        beq     double_dad_nan
        lda     dbf_c
        cmp     #D_NAN
        beq     double_dad_nan
        lda     dac_c
        cmp     #D_INF
        bne     double_dad_acfin
        lda     dbf_c                   ; dac is inf
        cmp     #D_INF
        bne     double_dad_packac       ; inf + finite -> dac
        lda     dac_s
        eor     dbf_s
        bmi     double_dad_nan          ; inf + (-inf) -> NaN
        bpl     double_dad_packac       ; inf + inf -> dac
double_dad_acfin:
        lda     dbf_c
        cmp     #D_INF
        beq     double_dad_retbf        ; finite + inf -> dbf
        lda     dac_c
        beq     double_dad_retbf        ; 0 + x -> x
        lda     dbf_c
        bne     double_dad_align
        jmp     double_d_pack           ; x + 0 -> x
double_dad_nan:
        lda     #D_NAN
        sta     dac_c
        jmp     double_d_pack
double_dad_packac:
        jmp     double_d_pack
double_dad_retbf:
        jsr     double_d_bf_to_ac
        jmp     double_d_pack

double_dad_align:
        ; make dac the larger-or-equal exponent (swap if needed)
        lda     dac_e+1
        cmp     dbf_e+1
        bne     double_dad_ehi
        lda     dac_e
        cmp     dbf_e
        bcs     double_dad_noswap
        bra     double_dad_swap
double_dad_ehi:
        lda     dac_e+1
        sec
        sbc     dbf_e+1
        bvc     double_dad_ev
        eor     #$80
double_dad_ev:
        bpl     double_dad_noswap
double_dad_swap:
        jsr     double_d_swap_ab
double_dad_noswap:
        sec                             ; diff = dac_e - dbf_e (>= 0)
        lda     dac_e
        sbc     dbf_e
        sta     d_cnt
        lda     dac_e+1
        sbc     dbf_e+1
        sta     d_cnt+1
        lda     d_cnt+1                 ; diff >= 64 -> dbf negligible
        bne     double_dad_big
        lda     d_cnt
        cmp     #64
        bcc     double_dad_doshift
double_dad_big:
        lda     dbf_m
        ora     dbf_m+1
        ora     dbf_m+2
        ora     dbf_m+3
        ora     dbf_m+4
        ora     dbf_m+5
        ora     dbf_m+6
        ora     dbf_m+7
        beq     double_dad_zb
        lda     #1
        sta     d_sticky
double_dad_zb:
        lda     #0
        ldy     #7
double_dad_zbl:
        sta     dbf_m,y
        dey
        bpl     double_dad_zbl
        bra     double_dad_addsub
double_dad_doshift:
        ldx     d_cnt
        beq     double_dad_addsub
double_dad_shl:
        lsr     dbf_m+7
        ror     dbf_m+6
        ror     dbf_m+5
        ror     dbf_m+4
        ror     dbf_m+3
        ror     dbf_m+2
        ror     dbf_m+1
        ror     dbf_m
        bcc     double_dad_ns
        lda     #1
        sta     d_sticky
double_dad_ns:
        dex
        bne     double_dad_shl
double_dad_addsub:
        lda     dac_s
        eor     dbf_s
        bpl     double_dad_addm
        jmp     double_dad_dosub
double_dad_addm:
        ; --- same sign: add the magnitudes ---
        clc
        lda     dac_m
        adc     dbf_m
        sta     dac_m
        lda     dac_m+1
        adc     dbf_m+1
        sta     dac_m+1
        lda     dac_m+2
        adc     dbf_m+2
        sta     dac_m+2
        lda     dac_m+3
        adc     dbf_m+3
        sta     dac_m+3
        lda     dac_m+4
        adc     dbf_m+4
        sta     dac_m+4
        lda     dac_m+5
        adc     dbf_m+5
        sta     dac_m+5
        lda     dac_m+6
        adc     dbf_m+6
        sta     dac_m+6
        lda     dac_m+7
        adc     dbf_m+7
        sta     dac_m+7
        bcc     double_dad_pack
        lda     dac_m                   ; carry out of bit 63: >>1, exp++
        and     #1
        beq     double_dad_c0
        lda     #1
        sta     d_sticky
double_dad_c0:
        ror     dac_m+7
        ror     dac_m+6
        ror     dac_m+5
        ror     dac_m+4
        ror     dac_m+3
        ror     dac_m+2
        ror     dac_m+1
        ror     dac_m
        lda     #$80
        ora     dac_m+7
        sta     dac_m+7
        inc     dac_e
        bne     double_dad_pack
        inc     dac_e+1
double_dad_pack:
        jmp     double_d_pack
double_dad_dosub:
        ; --- opposite sign: larger magnitude minus smaller ---
        jsr     double_d_mant_cmp
        bne     double_dad_subgo
        jmp     double_dad_cancel
double_dad_subgo:
        bpl     double_dad_abig
        sec                             ; dbf > dac: dbf - dac, sign = dbf_s
        lda     dbf_m
        sbc     dac_m
        sta     dac_m
        lda     dbf_m+1
        sbc     dac_m+1
        sta     dac_m+1
        lda     dbf_m+2
        sbc     dac_m+2
        sta     dac_m+2
        lda     dbf_m+3
        sbc     dac_m+3
        sta     dac_m+3
        lda     dbf_m+4
        sbc     dac_m+4
        sta     dac_m+4
        lda     dbf_m+5
        sbc     dac_m+5
        sta     dac_m+5
        lda     dbf_m+6
        sbc     dac_m+6
        sta     dac_m+6
        lda     dbf_m+7
        sbc     dac_m+7
        sta     dac_m+7
        lda     dbf_s
        sta     dac_s
        bra     double_dad_subnorm
double_dad_abig:
        sec                             ; dac - dbf, sign = dac_s (unchanged)
        lda     dac_m
        sbc     dbf_m
        sta     dac_m
        lda     dac_m+1
        sbc     dbf_m+1
        sta     dac_m+1
        lda     dac_m+2
        sbc     dbf_m+2
        sta     dac_m+2
        lda     dac_m+3
        sbc     dbf_m+3
        sta     dac_m+3
        lda     dac_m+4
        sbc     dbf_m+4
        sta     dac_m+4
        lda     dac_m+5
        sbc     dbf_m+5
        sta     dac_m+5
        lda     dac_m+6
        sbc     dbf_m+6
        sta     dac_m+6
        lda     dac_m+7
        sbc     dbf_m+7
        sta     dac_m+7
double_dad_subnorm:
        jsr     double_d_norm           ; renormalise (may be a big left shift)
        jmp     double_d_pack
double_dad_cancel:
        lda     #0                      ; exact cancellation -> +0
        sta     dac_s
        lda     #D_ZERO
        sta     dac_c
        jmp     double_d_pack

; swap dac_* <-> dbf_*
double_d_swap_ab:
        ldx     dac_c
        lda     dbf_c
        sta     dac_c
        stx     dbf_c
        ldx     dac_s
        lda     dbf_s
        sta     dac_s
        stx     dbf_s
        ldx     dac_e
        lda     dbf_e
        sta     dac_e
        stx     dbf_e
        ldx     dac_e+1
        lda     dbf_e+1
        sta     dac_e+1
        stx     dbf_e+1
        ldy     #7
double_dsw_m:
        ldx     dac_m,y
        lda     dbf_m,y
        sta     dac_m,y
        txa
        sta     dbf_m,y
        dey
        bpl     double_dsw_m
        rts

; dbf_* -> dac_*
double_d_bf_to_ac:
        lda     dbf_c
        sta     dac_c
        lda     dbf_s
        sta     dac_s
        lda     dbf_e
        sta     dac_e
        lda     dbf_e+1
        sta     dac_e+1
        ldy     #7
double_dba_m:
        lda     dbf_m,y
        sta     dac_m,y
        dey
        bpl     double_dba_m
        rts

; compare dac_m vs dbf_m (64-bit unsigned): A = $FF/0/1
double_d_mant_cmp:
        ldy     #7
double_dmc_l:
        lda     dac_m,y
        cmp     dbf_m,y
        bne     double_dmc_diff
        dey
        bpl     double_dmc_l
        lda     #0
        rts
double_dmc_diff:
        bcs     double_dmc_gt
        lda     #$FF
        rts
double_dmc_gt:
        lda     #1
        rts

; ---------------------------------------------------------------------
; d_mul -- d_ac *= mem(A/Y)
; 64x64 -> 128 shift-add (umul16's shape, one size up), then take the
; top 64 bits normalised, the rest a sticky.
; ---------------------------------------------------------------------
d_mul:
        sta     d_ptr
        sty     d_ptr+1
        jsr     double_d_unpack
        jsr     double_d_ac_to_bf       ; operand -> dbf_*
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack         ; d_ac -> dac_*
        stz     d_sticky
        lda     dac_s
        eor     dbf_s
        and     #$80
        sta     d_rsign

        lda     dac_c
        cmp     #D_NAN
        beq     double_dml_nan
        lda     dbf_c
        cmp     #D_NAN
        beq     double_dml_nan
        lda     dac_c
        cmp     #D_INF
        beq     double_dml_ainf
        lda     dbf_c
        cmp     #D_INF
        beq     double_dml_binf
        lda     dac_c
        beq     double_dml_zero
        lda     dbf_c
        beq     double_dml_zero
        jmp     double_dml_mul
double_dml_ainf:
        lda     dbf_c
        beq     double_dml_nan          ; inf * 0
        bra     double_dml_inf
double_dml_binf:
        lda     dac_c
        beq     double_dml_nan          ; 0 * inf
        bra     double_dml_inf
double_dml_zero:
        lda     d_rsign
        sta     dac_s
        lda     #D_ZERO
        sta     dac_c
        jmp     double_d_pack
double_dml_nan:
        lda     #D_NAN
        sta     dac_c
        jmp     double_d_pack
double_dml_inf:
        lda     d_rsign
        sta     dac_s
        lda     #D_INF
        sta     dac_c
        jmp     double_d_pack
double_dml_mul:
        lda     #0                      ; product high half = 0
        ldx     #7
double_dml_zh:
        sta     d_prod+8,x
        dex
        bpl     double_dml_zh
        ldx     #64
double_dml_loop:
        lsr     dbf_m+7
        ror     dbf_m+6
        ror     dbf_m+5
        ror     dbf_m+4
        ror     dbf_m+3
        ror     dbf_m+2
        ror     dbf_m+1
        ror     dbf_m
        bcc     double_dml_noadd
        clc
        lda     d_prod+8
        adc     dac_m
        sta     d_prod+8
        lda     d_prod+9
        adc     dac_m+1
        sta     d_prod+9
        lda     d_prod+10
        adc     dac_m+2
        sta     d_prod+10
        lda     d_prod+11
        adc     dac_m+3
        sta     d_prod+11
        lda     d_prod+12
        adc     dac_m+4
        sta     d_prod+12
        lda     d_prod+13
        adc     dac_m+5
        sta     d_prod+13
        lda     d_prod+14
        adc     dac_m+6
        sta     d_prod+14
        lda     d_prod+15
        adc     dac_m+7
        bra     double_dml_rot
double_dml_noadd:
        lda     d_prod+15
double_dml_rot:
        ror
        sta     d_prod+15
        ror     d_prod+14
        ror     d_prod+13
        ror     d_prod+12
        ror     d_prod+11
        ror     d_prod+10
        ror     d_prod+9
        ror     d_prod+8
        ror     d_prod+7
        ror     d_prod+6
        ror     d_prod+5
        ror     d_prod+4
        ror     d_prod+3
        ror     d_prod+2
        ror     d_prod+1
        ror     d_prod
        dex
        beq     double_dml_norm
        jmp     double_dml_loop
double_dml_norm:
        ; normalise: bit127 set -> adjust 64; else shift left 1, adjust 63
        ldy     #64
        lda     d_prod+15
        bmi     double_dml_top
        asl     d_prod
        rol     d_prod+1
        rol     d_prod+2
        rol     d_prod+3
        rol     d_prod+4
        rol     d_prod+5
        rol     d_prod+6
        rol     d_prod+7
        rol     d_prod+8
        rol     d_prod+9
        rol     d_prod+10
        rol     d_prod+11
        rol     d_prod+12
        rol     d_prod+13
        rol     d_prod+14
        rol     d_prod+15
        ldy     #63
double_dml_top:
        sty     d_t0
        lda     d_prod+8
        sta     dac_m
        lda     d_prod+9
        sta     dac_m+1
        lda     d_prod+10
        sta     dac_m+2
        lda     d_prod+11
        sta     dac_m+3
        lda     d_prod+12
        sta     dac_m+4
        lda     d_prod+13
        sta     dac_m+5
        lda     d_prod+14
        sta     dac_m+6
        lda     d_prod+15
        sta     dac_m+7
        lda     d_prod
        ora     d_prod+1
        ora     d_prod+2
        ora     d_prod+3
        ora     d_prod+4
        ora     d_prod+5
        ora     d_prod+6
        ora     d_prod+7
        beq     double_dml_nost
        lda     #1
        sta     d_sticky
double_dml_nost:
        clc                             ; exp = dac_e + dbf_e + adjust
        lda     dac_e
        adc     dbf_e
        sta     dac_e
        lda     dac_e+1
        adc     dbf_e+1
        sta     dac_e+1
        clc
        lda     dac_e
        adc     d_t0
        sta     dac_e
        lda     dac_e+1
        adc     #0
        sta     dac_e+1
        lda     d_rsign
        sta     dac_s
        lda     #D_NORM
        sta     dac_c
        jmp     double_d_pack

; ---------------------------------------------------------------------
; d_div -- d_ac /= mem(A/Y)
; Q = floor((dividend_m << 63) / divisor_m), 64 restoring-division steps,
; then normalise; the remainder becomes the sticky.
; ---------------------------------------------------------------------
d_div:
        sta     d_ptr
        sty     d_ptr+1
        jsr     double_d_unpack
        jsr     double_d_ac_to_bf       ; divisor -> dbf_*
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack         ; dividend (d_ac) -> dac_*
        stz     d_sticky
        lda     dac_s
        eor     dbf_s
        and     #$80
        sta     d_rsign

        lda     dac_c
        cmp     #D_NAN
        beq     double_ddv_nan
        lda     dbf_c
        cmp     #D_NAN
        beq     double_ddv_nan
        lda     dac_c
        cmp     #D_INF
        bne     double_ddv_afin
        lda     dbf_c                   ; dividend inf
        cmp     #D_INF
        beq     double_ddv_nan          ; inf / inf
        bra     double_ddv_inf
double_ddv_afin:
        lda     dbf_c
        cmp     #D_INF
        beq     double_ddv_zero         ; finite / inf = 0
        lda     dbf_c
        bne     double_ddv_bnz
        lda     dac_c                   ; divisor 0
        beq     double_ddv_nan          ; 0 / 0
        bra     double_ddv_inf          ; x / 0 = inf
double_ddv_bnz:
        lda     dac_c
        bne     double_ddv_div
        bra     double_ddv_zero         ; 0 / finite = 0
double_ddv_nan:
        lda     #D_NAN
        sta     dac_c
        jmp     double_d_pack
double_ddv_inf:
        lda     d_rsign
        sta     dac_s
        lda     #D_INF
        sta     dac_c
        jmp     double_d_pack
double_ddv_zero:
        lda     d_rsign
        sta     dac_s
        lda     #D_ZERO
        sta     dac_c
        jmp     double_d_pack
double_ddv_div:
        ; dividend = dac_m << 63 in the 128-bit d_prod (= dac_m<<64 then >>1)
        lda     dac_m
        sta     d_prod+8
        lda     dac_m+1
        sta     d_prod+9
        lda     dac_m+2
        sta     d_prod+10
        lda     dac_m+3
        sta     d_prod+11
        lda     dac_m+4
        sta     d_prod+12
        lda     dac_m+5
        sta     d_prod+13
        lda     dac_m+6
        sta     d_prod+14
        lda     dac_m+7
        sta     d_prod+15
        lda     #0
        ldx     #7
double_ddv_zl:
        sta     d_prod,x
        dex
        bpl     double_ddv_zl
        lsr     d_prod+15
        ror     d_prod+14
        ror     d_prod+13
        ror     d_prod+12
        ror     d_prod+11
        ror     d_prod+10
        ror     d_prod+9
        ror     d_prod+8
        ror     d_prod+7
        ror     d_prod+6
        ror     d_prod+5
        ror     d_prod+4
        ror     d_prod+3
        ror     d_prod+2
        ror     d_prod+1
        ror     d_prod
        lda     d_prod+8
        sta     d_rem                   ; rem = high half
        lda     d_prod+9
        sta     d_rem+1
        lda     d_prod+10
        sta     d_rem+2
        lda     d_prod+11
        sta     d_rem+3
        lda     d_prod+12
        sta     d_rem+4
        lda     d_prod+13
        sta     d_rem+5
        lda     d_prod+14
        sta     d_rem+6
        lda     d_prod+15
        sta     d_rem+7
        ldx     #64
double_ddv_loop:
        asl     d_prod
        rol     d_prod+1
        rol     d_prod+2
        rol     d_prod+3
        rol     d_prod+4
        rol     d_prod+5
        rol     d_prod+6
        rol     d_prod+7
        rol     d_rem
        rol     d_rem+1
        rol     d_rem+2
        rol     d_rem+3
        rol     d_rem+4
        rol     d_rem+5
        rol     d_rem+6
        rol     d_rem+7
        bcs     double_ddv_sub          ; bit 64 set -> definitely subtract
        sec
        lda     d_rem
        sbc     dbf_m
        sta     d_diff
        lda     d_rem+1
        sbc     dbf_m+1
        sta     d_diff+1
        lda     d_rem+2
        sbc     dbf_m+2
        sta     d_diff+2
        lda     d_rem+3
        sbc     dbf_m+3
        sta     d_diff+3
        lda     d_rem+4
        sbc     dbf_m+4
        sta     d_diff+4
        lda     d_rem+5
        sbc     dbf_m+5
        sta     d_diff+5
        lda     d_rem+6
        sbc     dbf_m+6
        sta     d_diff+6
        lda     d_rem+7
        sbc     dbf_m+7
        sta     d_diff+7
        bcc     double_ddv_noq          ; borrow -> rem < divisor
        bra     double_ddv_setq
double_ddv_sub:
        sec
        lda     d_rem
        sbc     dbf_m
        sta     d_diff
        lda     d_rem+1
        sbc     dbf_m+1
        sta     d_diff+1
        lda     d_rem+2
        sbc     dbf_m+2
        sta     d_diff+2
        lda     d_rem+3
        sbc     dbf_m+3
        sta     d_diff+3
        lda     d_rem+4
        sbc     dbf_m+4
        sta     d_diff+4
        lda     d_rem+5
        sbc     dbf_m+5
        sta     d_diff+5
        lda     d_rem+6
        sbc     dbf_m+6
        sta     d_diff+6
        lda     d_rem+7
        sbc     dbf_m+7
        sta     d_diff+7
double_ddv_setq:
        lda     d_diff
        sta     d_rem
        lda     d_diff+1
        sta     d_rem+1
        lda     d_diff+2
        sta     d_rem+2
        lda     d_diff+3
        sta     d_rem+3
        lda     d_diff+4
        sta     d_rem+4
        lda     d_diff+5
        sta     d_rem+5
        lda     d_diff+6
        sta     d_rem+6
        lda     d_diff+7
        sta     d_rem+7
        inc     d_prod                  ; quotient bit
double_ddv_noq:
        dex
        beq     double_ddv_fin
        jmp     double_ddv_loop
double_ddv_fin:
        lda     d_rem
        ora     d_rem+1
        ora     d_rem+2
        ora     d_rem+3
        ora     d_rem+4
        ora     d_rem+5
        ora     d_rem+6
        ora     d_rem+7
        beq     double_ddv_nost
        lda     #1
        sta     d_sticky
double_ddv_nost:
        lda     d_prod
        sta     dac_m
        lda     d_prod+1
        sta     dac_m+1
        lda     d_prod+2
        sta     dac_m+2
        lda     d_prod+3
        sta     dac_m+3
        lda     d_prod+4
        sta     dac_m+4
        lda     d_prod+5
        sta     dac_m+5
        lda     d_prod+6
        sta     dac_m+6
        lda     d_prod+7
        sta     dac_m+7
        sec                             ; exp = dac_e - dbf_e - 63
        lda     dac_e
        sbc     dbf_e
        sta     dac_e
        lda     dac_e+1
        sbc     dbf_e+1
        sta     dac_e+1
        sec
        lda     dac_e
        sbc     #63
        sta     dac_e
        lda     dac_e+1
        sbc     #0
        sta     dac_e+1
        lda     d_rsign
        sta     dac_s
        lda     #D_NORM
        sta     dac_c
        jsr     double_d_norm           ; quotient may need one left shift
        jmp     double_d_pack

; ---------------------------------------------------------------------
; d_sqrt -- d_ac = sqrt(d_ac)
;
; A "magic constant" bit-hack picks a guess within ~3% (sqrt(4) and other
; powers of four come out exact), then Newton's iteration
; x' = (x + v/x)/2 refines it -- six passes reach full binary64. NaN for
; a negative operand; 0/inf/NaN pass through.
; ---------------------------------------------------------------------
d_sqrt:
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack
        lda     dac_c
        cmp     #D_NAN
        beq     double_dsq_ret
        cmp     #D_ZERO
        beq     double_dsq_ret
        lda     dac_s
        bmi     double_dsq_neg
        lda     dac_c
        cmp     #D_INF
        beq     double_dsq_ret
        ; normal positive
        lda     #<d_sqv
        ldy     #>d_sqv
        jsr     d_store                 ; save the operand
        ; guess bits = (value bits >> 1) + 0x1FF8000000000000
        lsr     d_ac+7
        ror     d_ac+6
        ror     d_ac+5
        ror     d_ac+4
        ror     d_ac+3
        ror     d_ac+2
        ror     d_ac+1
        ror     d_ac
        clc
        lda     d_ac+6
        adc     #$F8
        sta     d_ac+6
        lda     d_ac+7
        adc     #$1F
        sta     d_ac+7
        bcc     double_dsq_gok
        ; (carry only if the exponent overflowed -- clamp, will still refine)
double_dsq_gok:
        lda     #<d_sqg
        ldy     #>d_sqg
        jsr     d_store
        ldx     #6
double_dsq_it:
        stx     d_sqi
        lda     #<d_sqv
        ldy     #>d_sqv
        jsr     d_load                  ; d_ac = v
        lda     #<d_sqg
        ldy     #>d_sqg
        jsr     d_div                   ; d_ac = v / x
        lda     #<d_sqg
        ldy     #>d_sqg
        jsr     d_add                   ; + x
        lda     #<d_half
        ldy     #>d_half
        jsr     d_mul                   ; * 0.5
        lda     #<d_sqg
        ldy     #>d_sqg
        jsr     d_store                 ; x = (x + v/x)/2
        ldx     d_sqi
        dex
        bne     double_dsq_it
        lda     #<d_sqg
        ldy     #>d_sqg
        jsr     d_load
double_dsq_ret:
        rts
double_dsq_neg:
        lda     #D_NAN
        sta     dac_c
        jmp     double_d_pack

; ---------------------------------------------------------------------
; d_exp -- d_ac = e^d_ac
;
; Range-reduce x = n*ln2 + r (n = trunc(x/ln2), |r| < ln2), sum the
; Taylor series e^r = 1 + r + r^2/2! + ..., then scale by 2^n (add n to
; the binary exponent). 0->1, +inf->+inf, -inf->+0, NaN->NaN.
; ---------------------------------------------------------------------
d_exp:
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack
        lda     dac_c
        cmp     #D_NAN
        beq     double_dex_ret
        cmp     #D_ZERO
        beq     double_dex_one
        cmp     #D_INF
        bne     double_dex_norm
        lda     dac_s
        bmi     double_dex_zero         ; e^-inf = 0
        rts                             ; e^+inf = +inf
double_dex_one:
        lda     #<d_one
        ldy     #>d_one
        jmp     d_load
double_dex_zero:
        lda     #D_ZERO
        sta     dac_c
        stz     dac_s
        jmp     double_d_pack
double_dex_ret:
        rts
double_dex_norm:
        lda     #<d_tv                  ; save x
        ldy     #>d_tv
        jsr     d_store
        lda     #<d_log2e               ; v = x * log2e = x / ln2
        ldy     #>d_log2e
        jsr     d_mul
        jsr     d_to_s32                ; n = trunc(v)
        lda     mos8(X16_P0)
        sta     d_tn16
        lda     mos8(X16_P1)
        sta     d_tn16+1
        jsr     d_from_s32              ; (double) n
        lda     #<d_ln2
        ldy     #>d_ln2
        jsr     d_mul                   ; n * ln2
        lda     #<d_tt
        ldy     #>d_tt
        jsr     d_store
        lda     #<d_tv
        ldy     #>d_tv
        jsr     d_load                  ; x
        lda     #<d_tt
        ldy     #>d_tt
        jsr     d_sub                   ; r = x - n*ln2
        lda     #<d_tr
        ldy     #>d_tr
        jsr     d_store
        lda     #<d_one                 ; sum = 1
        ldy     #>d_one
        jsr     d_load
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_store
        lda     #<d_one                 ; term = 1
        ldy     #>d_one
        jsr     d_load
        lda     #<d_tterm
        ldy     #>d_tterm
        jsr     d_store
        lda     #1
        sta     d_tkc
double_dex_loop:
        lda     #<d_tterm               ; term = term * r / k
        ldy     #>d_tterm
        jsr     d_load
        lda     #<d_tr
        ldy     #>d_tr
        jsr     d_mul
        lda     #<d_tt
        ldy     #>d_tt
        jsr     d_store
        lda     d_tkc
        ldx     #0
        jsr     d_from_s16
        lda     #<d_tk
        ldy     #>d_tk
        jsr     d_store
        lda     #<d_tt
        ldy     #>d_tt
        jsr     d_load
        lda     #<d_tk
        ldy     #>d_tk
        jsr     d_div
        lda     #<d_tterm
        ldy     #>d_tterm
        jsr     d_store
        lda     #<d_tsum                ; sum += term
        ldy     #>d_tsum
        jsr     d_load
        lda     #<d_tterm
        ldy     #>d_tterm
        jsr     d_add
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_store
        inc     d_tkc
        lda     d_tkc
        cmp     #19
        beq     double_dex_scale
        jmp     double_dex_loop
double_dex_scale:
        lda     #<d_tsum                ; e^r
        ldy     #>d_tsum
        jsr     d_load
        lda     #<d_ac                  ; multiply by 2^n: exponent += n
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack
        lda     dac_c
        cmp     #D_NORM
        bne     double_dex_sdone        ; a zero cannot be scaled
        clc
        lda     dac_e
        adc     d_tn16
        sta     dac_e
        lda     dac_e+1
        adc     d_tn16+1
        sta     dac_e+1
        stz     d_sticky
        jmp     double_d_pack
double_dex_sdone:
        rts

; ---------------------------------------------------------------------
; d_ln -- d_ac = ln(d_ac)
;
; Split value = m * 2^e with m in [0.75, 1.5) (halving m once if >= 1.5),
; so ln = e*ln2 + ln(m); ln(m) = 2*(t + t^3/3 + t^5/5 + ...) with
; t = (m-1)/(m+1), |t| <= 0.2. x<=0 -> -inf / NaN; +inf -> +inf.
; ---------------------------------------------------------------------
d_ln:
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack
        lda     dac_c
        cmp     #D_NAN
        bne     double_dln_c1
        rts                             ; NaN -> NaN
double_dln_c1:
        cmp     #D_ZERO
        bne     double_dln_c2
        jmp     double_dln_ninf         ; ln(0) = -inf
double_dln_c2:
        lda     dac_s
        bpl     double_dln_c3
        jmp     double_dln_nan          ; ln(negative) = NaN
double_dln_c3:
        lda     dac_c
        cmp     #D_INF
        bne     double_dln_norm
        rts                             ; ln(+inf) = +inf
double_dln_norm:
        ; e = dac_e + 63
        clc
        lda     dac_e
        adc     #63
        sta     d_tn16
        lda     dac_e+1
        adc     #0
        sta     d_tn16+1
        ; m = value with exponent -63 (in [1,2))
        lda     #<(-63)
        sta     dac_e
        lda     #>(-63)
        sta     dac_e+1
        stz     dac_s
        lda     #D_NORM
        sta     dac_c
        stz     d_sticky
        jsr     double_d_pack
        ; if m >= 1.5: m /= 2, e++
        lda     #<d_1p5
        ldy     #>d_1p5
        jsr     d_cmp
        cmp     #$FF
        beq     double_dln_mok
        lda     #<d_half
        ldy     #>d_half
        jsr     d_mul
        inc     d_tn16
        bne     double_dln_mok
        inc     d_tn16+1
double_dln_mok:
        lda     #<d_tv                  ; save m
        ldy     #>d_tv
        jsr     d_store
        lda     #<d_one                 ; num = m - 1
        ldy     #>d_one
        jsr     d_sub
        lda     #<d_tt
        ldy     #>d_tt
        jsr     d_store
        lda     #<d_tv                  ; den = m + 1
        ldy     #>d_tv
        jsr     d_load
        lda     #<d_one
        ldy     #>d_one
        jsr     d_add
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_store
        lda     #<d_tt                  ; t = num / den
        ldy     #>d_tt
        jsr     d_load
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_div
        lda     #<d_tr
        ldy     #>d_tr
        jsr     d_store
        lda     #<d_tr                  ; t2 = t*t
        ldy     #>d_tr
        jsr     d_mul
        lda     #<d_tt
        ldy     #>d_tt
        jsr     d_store
        lda     #<d_tr                  ; sum = t
        ldy     #>d_tr
        jsr     d_load
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_store
        lda     #<d_tr                  ; term = t
        ldy     #>d_tr
        jsr     d_load
        lda     #<d_tterm
        ldy     #>d_tterm
        jsr     d_store
        lda     #3
        sta     d_tkc
double_dln_loop:
        lda     #<d_tterm               ; term *= t2
        ldy     #>d_tterm
        jsr     d_load
        lda     #<d_tt
        ldy     #>d_tt
        jsr     d_mul
        lda     #<d_tterm
        ldy     #>d_tterm
        jsr     d_store
        lda     d_tkc                   ; sum += term / k
        ldx     #0
        jsr     d_from_s16
        lda     #<d_tk
        ldy     #>d_tk
        jsr     d_store
        lda     #<d_tterm
        ldy     #>d_tterm
        jsr     d_load
        lda     #<d_tk
        ldy     #>d_tk
        jsr     d_div
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_add
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_store
        lda     d_tkc
        clc
        adc     #2
        sta     d_tkc
        cmp     #33
        bcs     double_dln_series_done
        jmp     double_dln_loop
double_dln_series_done:
        lda     #<d_tsum                ; ln(m) = 2 * sum
        ldy     #>d_tsum
        jsr     d_load
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_add
        lda     #<d_tterm
        ldy     #>d_tterm
        jsr     d_store                 ; ln(m)
        ; + e * ln2
        lda     d_tn16
        sta     mos8(X16_P0)
        lda     d_tn16+1
        sta     mos8(X16_P1)
        and     #$80
        beq     double_dln_epos
        lda     #$FF
double_dln_epos:
        sta     mos8(X16_P2)
        sta     mos8(X16_P3)
        jsr     d_from_s32              ; (double) e
        lda     #<d_ln2
        ldy     #>d_ln2
        jsr     d_mul                   ; e * ln2
        lda     #<d_tterm
        ldy     #>d_tterm
        jsr     d_add                   ; + ln(m)
        rts
double_dln_nan:
        lda     #D_NAN
        sta     dac_c
        jmp     double_d_pack
double_dln_ninf:
        lda     #D_INF
        sta     dac_c
        lda     #$80
        sta     dac_s
        jmp     double_d_pack
double_dln_ret:
        rts

; ---------------------------------------------------------------------
; d_pow -- d_ac = d_ac ^ mem(A/Y)   (base ^ exponent)
;
; x^y = exp(y * ln x). y == 0 gives 1 (even for x <= 0); otherwise a
; base <= 0 yields NaN/inf through d_ln (no integer-power special case).
; ---------------------------------------------------------------------
d_pow:
        sta     d_powyp
        sty     d_powyp+1
        lda     #<d_powx                ; save the base
        ldy     #>d_powx
        jsr     d_store
        lda     d_powyp                 ; y == 0 ?  -> 1
        ldy     d_powyp+1
        jsr     d_load
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack
        lda     dac_c
        beq     double_dpw_one
        lda     #<d_powx                ; exp(y * ln x)
        ldy     #>d_powx
        jsr     d_load
        jsr     d_ln
        lda     d_powyp
        ldy     d_powyp+1
        jsr     d_mul
        jmp     d_exp
double_dpw_one:
        lda     #<d_one
        ldy     #>d_one
        jmp     d_load

; ---------------------------------------------------------------------
; d_sin / d_cos / d_tan -- d_ac = sin/cos/tan(d_ac)
;
; Reduce x = n*(pi/2) + r with |r| <= pi/4 (a single subtraction, so a
; huge x loses precision), Taylor sin(r)/cos(r), select by n mod 4.
; NaN/inf -> NaN; sin(0)=0, cos(0)=1.
; ---------------------------------------------------------------------
d_sin:
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack
        lda     dac_c
        cmp     #D_NORM
        beq     double_dsn_go
        cmp     #D_ZERO
        beq     double_dsn_ret
        lda     #D_NAN
        sta     dac_c
        jmp     double_d_pack
double_dsn_ret:
        rts
double_dsn_go:
        jsr     double_d_trig_reduce
        lda     d_scq
        beq     double_dsn_q0
        cmp     #1
        beq     double_dsn_q1
        cmp     #2
        beq     double_dsn_q2
        jsr     double_d_cosr           ; q3: -cos(r)
        jmp     d_neg
double_dsn_q0:
        jmp     double_d_sinr
double_dsn_q1:
        jmp     double_d_cosr
double_dsn_q2:
        jsr     double_d_sinr           ; q2: -sin(r)
        jmp     d_neg

d_cos:
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack
        lda     dac_c
        cmp     #D_NORM
        beq     double_dcs_go
        cmp     #D_ZERO
        bne     double_dcs_nan
        lda     #<d_one
        ldy     #>d_one
        jmp     d_load
double_dcs_nan:
        lda     #D_NAN
        sta     dac_c
        jmp     double_d_pack
double_dcs_go:
        jsr     double_d_trig_reduce
        lda     d_scq
        beq     double_dcs_q0
        cmp     #1
        beq     double_dcs_q1
        cmp     #2
        beq     double_dcs_q2
        jmp     double_d_sinr           ; q3: sin(r)
double_dcs_q0:
        jmp     double_d_cosr
double_dcs_q1:
        jsr     double_d_sinr           ; q1: -sin(r)
        jmp     d_neg
double_dcs_q2:
        jsr     double_d_cosr           ; q2: -cos(r)
        jmp     d_neg

d_tan:
        lda     #<d_tanx
        ldy     #>d_tanx
        jsr     d_store
        jsr     d_sin
        lda     #<d_tans
        ldy     #>d_tans
        jsr     d_store
        lda     #<d_tanx
        ldy     #>d_tanx
        jsr     d_load
        jsr     d_cos
        lda     #<d_tanc
        ldy     #>d_tanc
        jsr     d_store
        lda     #<d_tans
        ldy     #>d_tans
        jsr     d_load
        lda     #<d_tanc
        ldy     #>d_tanc
        jmp     d_div

; ---------------------------------------------------------------------
; d_atan -- d_ac = atan(d_ac)
;
; Fold to x in [0, tan(pi/12)] via atan(-x)=-atan(x), atan(x)=pi/2-atan(1/x)
; for x>1, and atan(x)=pi/6+atan((x*sqrt3-1)/(x+sqrt3)) for x>tan(pi/12);
; then the fast series x - x^3/3 + x^5/5 - ...  +-inf -> +-pi/2.
; ---------------------------------------------------------------------
d_atan:
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack
        lda     dac_c
        cmp     #D_NORM
        beq     double_dat_go
        cmp     #D_ZERO
        beq     double_dat_ret
        cmp     #D_INF
        beq     double_dat_inf
        lda     #D_NAN
        sta     dac_c
        jmp     double_d_pack
double_dat_ret:
        rts
double_dat_inf:
        lda     dac_s
        php
        lda     #<d_pihalf
        ldy     #>d_pihalf
        jsr     d_load
        plp
        bpl     double_dat_ret
        jmp     d_neg
double_dat_go:
        stz     d_atflags
        lda     dac_s
        bpl     double_dat_pos
        lda     #1
        sta     d_atflags               ; negx
        jsr     d_abs
double_dat_pos:
        lda     #<d_one                 ; x > 1 ?  x = 1/x
        ldy     #>d_one
        jsr     d_cmp
        cmp     #1
        bne     double_dat_norecip
        lda     #<d_atx
        ldy     #>d_atx
        jsr     d_store
        lda     #<d_one
        ldy     #>d_one
        jsr     d_load
        lda     #<d_atx
        ldy     #>d_atx
        jsr     d_div
        lda     d_atflags
        ora     #2                      ; recip
        sta     d_atflags
double_dat_norecip:
        lda     #<d_tan15               ; x > tan(pi/12) ?
        ldy     #>d_tan15
        jsr     d_cmp
        cmp     #1
        bne     double_dat_nosixth
        lda     #<d_atx                 ; x = (x*sqrt3 - 1)/(x + sqrt3)
        ldy     #>d_atx
        jsr     d_store
        lda     #<d_sqrt3
        ldy     #>d_sqrt3
        jsr     d_mul
        lda     #<d_one
        ldy     #>d_one
        jsr     d_sub
        lda     #<d_atn
        ldy     #>d_atn
        jsr     d_store
        lda     #<d_atx
        ldy     #>d_atx
        jsr     d_load
        lda     #<d_sqrt3
        ldy     #>d_sqrt3
        jsr     d_add
        lda     #<d_atd
        ldy     #>d_atd
        jsr     d_store
        lda     #<d_atn
        ldy     #>d_atn
        jsr     d_load
        lda     #<d_atd
        ldy     #>d_atd
        jsr     d_div
        lda     d_atflags
        ora     #4                      ; sixth
        sta     d_atflags
double_dat_nosixth:
        ; atan(r) = r - r^3/3 + r^5/5 - ...  Carry the power p = r^(2k+1) in
        ; d_atn (only ever *= -r^2, so its sign alternates); each term divides
        ; a COPY of p by (2k+1) -- p itself must not be divided.
        lda     #<d_tr
        ldy     #>d_tr
        jsr     d_store                 ; r = reduced x
        jsr     double_d_trig_nr2       ; d_tt = -r^2
        lda     #<d_tr
        ldy     #>d_tr
        jsr     d_load
        lda     #<d_atn
        ldy     #>d_atn
        jsr     d_store                 ; p = r
        lda     #<d_tr
        ldy     #>d_tr
        jsr     d_load
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_store                 ; sum = r
        lda     #1
        sta     d_tkc
double_dat_loop:
        lda     #<d_atn                 ; p *= -r^2
        ldy     #>d_atn
        jsr     d_load
        lda     #<d_tt
        ldy     #>d_tt
        jsr     d_mul
        lda     #<d_atn
        ldy     #>d_atn
        jsr     d_store
        lda     d_tkc                   ; divisor = 2k+1
        asl
        clc
        adc     #1
        ldx     #0
        jsr     d_from_s16
        lda     #<d_tk
        ldy     #>d_tk
        jsr     d_store
        lda     #<d_atn                 ; term = p / (2k+1)
        ldy     #>d_atn
        jsr     d_load
        lda     #<d_tk
        ldy     #>d_tk
        jsr     d_div
        lda     #<d_tsum                ; sum += term
        ldy     #>d_tsum
        jsr     d_add
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_store
        inc     d_tkc
        lda     d_tkc
        cmp     #16
        beq     double_dat_reassemble
        jmp     double_dat_loop
double_dat_reassemble:
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_load                  ; atan(r)
        lda     d_atflags
        and     #4
        beq     double_dat_no6
        lda     #<d_pi6
        ldy     #>d_pi6
        jsr     d_add                   ; + pi/6
double_dat_no6:
        lda     d_atflags
        and     #2
        beq     double_dat_norec2
        jsr     d_neg                   ; pi/2 - result
        lda     #<d_pihalf
        ldy     #>d_pihalf
        jsr     d_add
double_dat_norec2:
        lda     d_atflags
        and     #1
        beq     double_dat_fin
        jmp     d_neg
double_dat_fin:
        rts

; ---------------------------------------------------------------------
; d_sinh / d_cosh / d_tanh -- d_ac = sinh/cosh/tanh(d_ac), via exp
;   sinh = (e^x - e^-x)/2, cosh = (e^x + e^-x)/2, tanh = sinh/cosh.
; tanh saturates to +-1 for |x| >= 20 (where e^x would overflow) and
; propagates NaN. (sinh of a tiny x cancels e^x - e^-x -- ~ulp there.)
; ---------------------------------------------------------------------
d_sinh:
        jsr     double_d_hyp_exps       ; d_hypa = e^x, d_hypb = e^-x
        lda     #<d_hypa
        ldy     #>d_hypa
        jsr     d_load
        lda     #<d_hypb
        ldy     #>d_hypb
        jsr     d_sub                   ; e^x - e^-x
        lda     #<d_half
        ldy     #>d_half
        jmp     d_mul                   ; / 2

d_cosh:
        jsr     double_d_hyp_exps
        lda     #<d_hypa
        ldy     #>d_hypa
        jsr     d_load
        lda     #<d_hypb
        ldy     #>d_hypb
        jsr     d_add                   ; e^x + e^-x
        lda     #<d_half
        ldy     #>d_half
        jmp     d_mul

d_tanh:
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack
        lda     dac_c
        cmp     #D_NAN
        bne     double_dth_go
        rts                             ; NaN -> NaN
double_dth_go:
        lda     #<d_hypx                ; save x (for the sign, and to restore)
        ldy     #>d_hypx
        jsr     d_store
        jsr     d_abs
        lda     #<d_hyp20
        ldy     #>d_hyp20
        jsr     d_cmp                   ; |x| < 20 ?
        cmp     #$FF
        beq     double_dth_small
        lda     #<d_one                 ; |x| >= 20: tanh = sign(x)
        ldy     #>d_one
        jsr     d_load
        lda     d_hypx+7
        bpl     double_dth_ret
        jmp     d_neg
double_dth_ret:
        rts
double_dth_small:
        lda     #<d_hypx
        ldy     #>d_hypx
        jsr     d_load                  ; x
        jsr     double_d_hyp_exps
        lda     #<d_hypa                ; num = e^x - e^-x
        ldy     #>d_hypa
        jsr     d_load
        lda     #<d_hypb
        ldy     #>d_hypb
        jsr     d_sub
        lda     #<d_hypn
        ldy     #>d_hypn
        jsr     d_store
        lda     #<d_hypa                ; den = e^x + e^-x
        ldy     #>d_hypa
        jsr     d_load
        lda     #<d_hypb
        ldy     #>d_hypb
        jsr     d_add
        lda     #<d_hypd
        ldy     #>d_hypd
        jsr     d_store
        lda     #<d_hypn
        ldy     #>d_hypn
        jsr     d_load
        lda     #<d_hypd
        ldy     #>d_hypd
        jmp     d_div                   ; num / den

; d_hypa = e^(d_ac), d_hypb = e^(-d_ac)
double_d_hyp_exps:
        lda     #<d_hypx
        ldy     #>d_hypx
        jsr     d_store                 ; save x
        lda     #<d_hypx
        ldy     #>d_hypx
        jsr     d_load
        jsr     d_exp
        lda     #<d_hypa
        ldy     #>d_hypa
        jsr     d_store                 ; e^x
        lda     #<d_hypx
        ldy     #>d_hypx
        jsr     d_load
        jsr     d_neg
        jsr     d_exp
        lda     #<d_hypb
        ldy     #>d_hypb
        jmp     d_store                 ; e^-x

; x (d_ac) -> d_tr = r in [-pi/4, pi/4], d_scq = n mod 4
double_d_trig_reduce:
        lda     #<d_tv
        ldy     #>d_tv
        jsr     d_store                 ; save x
        lda     #<d_pihalf
        ldy     #>d_pihalf
        jsr     d_div                   ; x / (pi/2)
        lda     d_ac+7                  ; round to nearest: += copysign(0.5)
        bmi     double_dtr_neg
        lda     #<d_half
        ldy     #>d_half
        jsr     d_add
        bra     double_dtr_trunc
double_dtr_neg:
        lda     #<d_half
        ldy     #>d_half
        jsr     d_sub
double_dtr_trunc:
        jsr     d_to_s32                ; n
        lda     mos8(X16_P0)
        and     #3
        sta     d_scq
        jsr     d_from_s32              ; (double) n
        lda     #<d_pihalf
        ldy     #>d_pihalf
        jsr     d_mul                   ; n * (pi/2)
        lda     #<d_tt
        ldy     #>d_tt
        jsr     d_store
        lda     #<d_tv
        ldy     #>d_tv
        jsr     d_load                  ; x
        lda     #<d_tt
        ldy     #>d_tt
        jsr     d_sub                   ; r = x - n*(pi/2)
        lda     #<d_tr
        ldy     #>d_tr
        jmp     d_store

; sin(d_tr) via Taylor: sum = r, term *= -r^2/((2k)(2k+1)), sum += term
double_d_sinr:
        jsr     double_d_trig_nr2       ; d_tt = -r^2
        lda     #<d_tr
        ldy     #>d_tr
        jsr     d_load
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_store                 ; sum = r
        lda     #<d_tr
        ldy     #>d_tr
        jsr     d_load
        lda     #<d_tterm
        ldy     #>d_tterm
        jsr     d_store                 ; term = r
        lda     #1
        sta     d_tkc
double_dsr_loop:
        jsr     double_d_trig_termstep  ; term *= -r^2
        lda     d_tkc                   ; / (2k)
        asl
        ldx     #0
        jsr     double_d_trig_divk
        lda     d_tkc                   ; / (2k+1)
        asl
        clc
        adc     #1
        ldx     #0
        jsr     double_d_trig_divk
        jsr     double_d_trig_addsum    ; sum += term
        inc     d_tkc
        lda     d_tkc
        cmp     #10
        beq     double_dsr_done
        jmp     double_dsr_loop
double_dsr_done:
        lda     #<d_tsum
        ldy     #>d_tsum
        jmp     d_load

; cos(d_tr) via Taylor: sum = 1, term *= -r^2/((2k-1)(2k)), sum += term
double_d_cosr:
        jsr     double_d_trig_nr2       ; d_tt = -r^2
        lda     #<d_one
        ldy     #>d_one
        jsr     d_load
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_store                 ; sum = 1
        lda     #<d_one
        ldy     #>d_one
        jsr     d_load
        lda     #<d_tterm
        ldy     #>d_tterm
        jsr     d_store                 ; term = 1
        lda     #1
        sta     d_tkc
double_dcr_loop:
        jsr     double_d_trig_termstep  ; term *= -r^2
        lda     d_tkc                   ; / (2k-1)
        asl
        sec
        sbc     #1
        ldx     #0
        jsr     double_d_trig_divk
        lda     d_tkc                   ; / (2k)
        asl
        ldx     #0
        jsr     double_d_trig_divk
        jsr     double_d_trig_addsum
        inc     d_tkc
        lda     d_tkc
        cmp     #10
        beq     double_dcr_done
        jmp     double_dcr_loop
double_dcr_done:
        lda     #<d_tsum
        ldy     #>d_tsum
        jmp     d_load

; term = term * (-r^2)   [-r^2 is in d_tt]
double_d_trig_termstep:
        lda     #<d_tterm
        ldy     #>d_tterm
        jsr     d_load
        lda     #<d_tt
        ldy     #>d_tt
        jsr     d_mul
        lda     #<d_tterm
        ldy     #>d_tterm
        jmp     d_store

; term = term / (A:X as a small integer)
double_d_trig_divk:
        jsr     d_from_s16
        lda     #<d_tk
        ldy     #>d_tk
        jsr     d_store
        lda     #<d_tterm
        ldy     #>d_tterm
        jsr     d_load
        lda     #<d_tk
        ldy     #>d_tk
        jsr     d_div
        lda     #<d_tterm
        ldy     #>d_tterm
        jmp     d_store

; sum += term
double_d_trig_addsum:
        lda     #<d_tsum
        ldy     #>d_tsum
        jsr     d_load
        lda     #<d_tterm
        ldy     #>d_tterm
        jsr     d_add
        lda     #<d_tsum
        ldy     #>d_tsum
        jmp     d_store

; d_tt = -(d_tr * d_tr)
double_d_trig_nr2:
        lda     #<d_tr
        ldy     #>d_tr
        jsr     d_load
        lda     #<d_tr
        ldy     #>d_tr
        jsr     d_mul
        jsr     d_neg
        lda     #<d_tt
        ldy     #>d_tt
        jmp     d_store

; ---------------------------------------------------------------------
; d_from_str -- parse a decimal string into d_ac
;   in: A = low, Y = high of the string, X = length
;
; Accepts  [+/-] digits [ . digits ] [ (E|e) [+/-] digits ].  Digits are
; accumulated as a double (d_ac = d_ac*10 + digit), then scaled by
; 10^(exponent - fraction_digits) with repeated *10 / /10. Each step
; rounds, so a long mantissa can land a unit-in-the-last-place off -- fine
; for a calculator; a correctly-rounded parser is a later refinement.
; ---------------------------------------------------------------------
d_from_str:
        sta     dstr_ptr
        sty     dstr_ptr+1
        stx     dstr_len
        stz     dstr_i
        stz     dstr_neg
        stz     dstr_frac
        stz     dstr_exp
        stz     dstr_exp+1
        jsr     double_d_zero           ; accumulator = 0
        lda     #<dstr_acc
        ldy     #>dstr_acc
        jsr     d_store

        jsr     double_dstr_peek        ; optional sign
        bcs     double_dstr_int         ; empty: fall through the phases to scaling
        cmp     #$2D
        bne     double_dstr_ckplus
        inc     dstr_neg
        jsr     double_dstr_next
        bra     double_dstr_int
double_dstr_ckplus:
        cmp     #$2B
        bne     double_dstr_int
        jsr     double_dstr_next
double_dstr_int:
        jsr     double_dstr_peek        ; integer digits
        bcs     double_dstr_dot
        cmp     #$30
        bcc     double_dstr_dot
        cmp     #$3A
        bcs     double_dstr_dot
        sec
        sbc     #$30
        jsr     double_dstr_muladd
        jsr     double_dstr_next
        bra     double_dstr_int
double_dstr_dot:
        jsr     double_dstr_peek
        bcs     double_dstr_exp0
        cmp     #$2E
        bne     double_dstr_exp0
        jsr     double_dstr_next
double_dstr_frc:
        jsr     double_dstr_peek        ; fraction digits
        bcs     double_dstr_exp0
        cmp     #$30
        bcc     double_dstr_exp0
        cmp     #$3A
        bcs     double_dstr_exp0
        sec
        sbc     #$30
        jsr     double_dstr_muladd
        inc     dstr_frac
        jsr     double_dstr_next
        bra     double_dstr_frc
double_dstr_exp0:
        jsr     double_dstr_peek
        bcc     double_dstr_e_has
        jmp     double_dstr_scale
double_dstr_e_has:
        cmp     #$45
        beq     double_dstr_esgn
        cmp     #$65
        beq     double_dstr_esgn
        jmp     double_dstr_scale
double_dstr_esgn:
        jsr     double_dstr_next
        stz     dstr_esign
        jsr     double_dstr_peek
        bcc     double_dstr_e_sgnok
        jmp     double_dstr_scale
double_dstr_e_sgnok:
        cmp     #$2D
        bne     double_dstr_eckp
        inc     dstr_esign
        jsr     double_dstr_next
        bra     double_dstr_edig
double_dstr_eckp:
        cmp     #$2B
        bne     double_dstr_edig
        jsr     double_dstr_next
double_dstr_edig:
        jsr     double_dstr_peek        ; exponent digits -> dstr_exp
        bcs     double_dstr_edone
        cmp     #$30
        bcc     double_dstr_edone
        cmp     #$3A
        bcs     double_dstr_edone
        sec
        sbc     #$30
        pha
        ; dstr_exp = dstr_exp*10 + digit
        lda     dstr_exp
        asl
        sta     dstr_t
        lda     dstr_exp+1
        rol
        sta     dstr_t+1                ; exp*2
        asl     dstr_t
        rol     dstr_t+1                ; exp*4
        asl     dstr_t
        rol     dstr_t+1                ; exp*8
        clc
        lda     dstr_t
        adc     dstr_exp
        sta     dstr_t
        lda     dstr_t+1
        adc     dstr_exp+1
        sta     dstr_t+1                ; exp*8 + exp = exp*9
        clc
        lda     dstr_t
        adc     dstr_exp
        sta     dstr_exp
        lda     dstr_t+1
        adc     dstr_exp+1
        sta     dstr_exp+1              ; exp*10
        pla
        clc
        adc     dstr_exp
        sta     dstr_exp
        lda     dstr_exp+1
        adc     #0
        sta     dstr_exp+1
        jsr     double_dstr_next
        bra     double_dstr_edig
double_dstr_edone:
        lda     dstr_esign
        beq     double_dstr_scale
        sec                             ; negate the explicit exponent
        lda     #0
        sbc     dstr_exp
        sta     dstr_exp
        lda     #0
        sbc     dstr_exp+1
        sta     dstr_exp+1
double_dstr_scale:
        sec                             ; finalexp = exp - fraction_digits
        lda     dstr_exp
        sbc     dstr_frac
        sta     dstr_exp
        lda     dstr_exp+1
        sbc     #0
        sta     dstr_exp+1
        lda     #<dstr_acc              ; d_ac = accumulator
        ldy     #>dstr_acc
        jsr     d_load
        lda     dstr_neg
        beq     double_dstr_ns
        jsr     d_neg
double_dstr_ns:
        lda     dstr_exp+1              ; scale by 10^finalexp
        bmi     double_dstr_neg_exp
        lda     dstr_exp                ; positive: multiply
        ora     dstr_exp+1
        beq     double_dstr_ret
        lda     dstr_exp
        sta     dstr_cnt
        lda     dstr_exp+1
        sta     dstr_cnt+1
double_dstr_ml:
        lda     #<d_ten
        ldy     #>d_ten
        jsr     d_mul
        jsr     double_dstr_deccnt
        bne     double_dstr_ml
        rts
double_dstr_neg_exp:
        sec                             ; count = -finalexp
        lda     #0
        sbc     dstr_exp
        sta     dstr_cnt
        lda     #0
        sbc     dstr_exp+1
        sta     dstr_cnt+1
double_dstr_dv:
        lda     #<d_ten
        ldy     #>d_ten
        jsr     d_div
        jsr     double_dstr_deccnt
        bne     double_dstr_dv
double_dstr_ret:
        rts

; peek: A = char at dstr_i, carry clear; carry set at end of string
double_dstr_peek:
        ldy     dstr_i
        cpy     dstr_len
        bcs     double_dstr_pend
        lda     (dstr_ptr),y
        clc
        rts
double_dstr_pend:
        sec
        rts

double_dstr_next:
        inc     dstr_i
        rts

; accumulator = accumulator * 10 + A  (A = digit 0..9)
double_dstr_muladd:
        pha
        lda     #<dstr_acc
        ldy     #>dstr_acc
        jsr     d_load
        lda     #<d_ten
        ldy     #>d_ten
        jsr     d_mul
        lda     #<dstr_acc
        ldy     #>dstr_acc
        jsr     d_store                 ; acc *= 10
        pla
        ldx     #0
        jsr     d_from_s16              ; d_ac = digit
        lda     #<dstr_acc
        ldy     #>dstr_acc
        jsr     d_add                   ; d_ac = digit + acc*10
        lda     #<dstr_acc
        ldy     #>dstr_acc
        jsr     d_store
        rts

; dstr_cnt--, returns Z set when it reaches zero
double_dstr_deccnt:
        lda     dstr_cnt
        bne     double_dstr_dcl
        dec     dstr_cnt+1
double_dstr_dcl:
        dec     dstr_cnt
        lda     dstr_cnt
        ora     dstr_cnt+1
        rts

; ---------------------------------------------------------------------
; d_to_str -- format d_ac as a NUL-terminated decimal string
;   out: A = low, X = high of the string (in d_strbuf)
;
; Scales |value| into [1,10) (repeated *10 / /10 -- so a very large or
; small exponent loses low digits), extracts 17 digits, rounds to 16,
; strips trailing zeros, and lays out fixed notation for -4 <= exp <= 20
; or scientific "d.dddddE+NN" beyond. Correctly-rounded shortest output
; (Grisu/Ryu) is a later refinement; exact short values print exactly.
; ---------------------------------------------------------------------
d_to_str:
        lda     #<d_ac
        sta     d_ptr
        lda     #>d_ac
        sta     d_ptr+1
        jsr     double_d_unpack
        stz     dts_bx
        lda     dac_c
        cmp     #D_NAN
        bne     double_dts_s0
        jmp     double_dts_nan
double_dts_s0:
        lda     dac_s
        bpl     double_dts_sok
        lda     #$2D
        jsr     double_dts_emit
double_dts_sok:
        lda     dac_c
        cmp     #D_INF
        bne     double_dts_s1
        jmp     double_dts_inf
double_dts_s1:
        cmp     #D_ZERO
        bne     double_dts_s2
        jmp     double_dts_zero
double_dts_s2:
        jsr     d_abs
        stz     dts_e
        stz     dts_e+1
double_dts_su:
        lda     #<d_ten
        ldy     #>d_ten
        jsr     d_cmp
        cmp     #$FF
        beq     double_dts_sd
        lda     #<d_ten
        ldy     #>d_ten
        jsr     d_div
        inc     dts_e
        bne     double_dts_su
        inc     dts_e+1
        bra     double_dts_su
double_dts_sd:
        lda     #<d_one
        ldy     #>d_one
        jsr     d_cmp
        cmp     #$FF
        bne     double_dts_ext
        lda     #<d_ten
        ldy     #>d_ten
        jsr     d_mul
        lda     dts_e
        bne     double_dts_sdl
        dec     dts_e+1
double_dts_sdl:
        dec     dts_e
        bra     double_dts_sd
double_dts_ext:
        ldx     #0
double_dts_extl:
        stx     dts_di
        jsr     d_to_s32
        ldx     dts_di
        lda     mos8(X16_P0)
        sta     dts_dig,x
        lda     #<dts_val
        ldy     #>dts_val
        jsr     d_store
        lda     mos8(X16_P0)
        ldx     #0
        jsr     d_from_s16
        lda     #<dts_digd
        ldy     #>dts_digd
        jsr     d_store
        lda     #<dts_val
        ldy     #>dts_val
        jsr     d_load
        lda     #<dts_digd
        ldy     #>dts_digd
        jsr     d_sub
        lda     #<d_ten
        ldy     #>d_ten
        jsr     d_mul
        ldx     dts_di
        inx
        cpx     #17
        bne     double_dts_extl
        ; round the 17th digit into the 16
        lda     dts_dig+16
        cmp     #5
        bcc     double_dts_strip
        ldx     #15
double_dts_rul:
        inc     dts_dig,x
        lda     dts_dig,x
        cmp     #10
        bcc     double_dts_strip
        lda     #0
        sta     dts_dig,x
        dex
        bpl     double_dts_rul
        lda     #1                      ; carried past the top: 9.99.. -> 10.0
        sta     dts_dig
        inc     dts_e
        bne     double_dts_strip
        inc     dts_e+1
double_dts_strip:
        ldx     #15
double_dts_strl:
        lda     dts_dig,x
        bne     double_dts_strd
        dex
        bpl     double_dts_strl
double_dts_strd:
        inx
        stx     dts_ndig
        ; choose fixed vs scientific
        lda     dts_e+1
        beq     double_dts_epos
        cmp     #$FF
        bne     double_dts_scij
        lda     dts_e
        cmp     #$FC                    ; E >= -4 ?
        bcs     double_dts_fixed
        bra     double_dts_scij
double_dts_epos:
        lda     dts_e
        cmp     #21                     ; E <= 20 ?
        bcc     double_dts_fixed
double_dts_scij:
        jmp     double_dts_sci
double_dts_fixed:
        clc                             ; P = E + 1 (point position)
        lda     dts_e
        adc     #1
        sta     dts_p
        lda     dts_e+1
        adc     #0
        sta     dts_p+1
        lda     dts_p+1
        bmi     double_dts_lead0
        lda     dts_p
        ora     dts_p+1
        beq     double_dts_lead0
        lda     dts_p                   ; P > 0
        cmp     dts_ndig
        bcc     double_dts_mid          ; P < ndig -> point in the middle
        ldx     #0                      ; P >= ndig -> integer + trailing zeros
double_dts_intl:
        cpx     dts_ndig
        beq     double_dts_intz
        lda     dts_dig,x
        jsr     double_dts_emitd
        inx
        bra     double_dts_intl
double_dts_intz:
        lda     dts_p
        sec
        sbc     dts_ndig
        tax
double_dts_intzl:
        cpx     #0
        beq     double_dts_donej
        lda     #$30
        jsr     double_dts_emit
        dex
        bra     double_dts_intzl
double_dts_donej:
        jmp     double_dts_done
double_dts_mid:
        ldx     #0
double_dts_mid1:
        cpx     dts_p
        beq     double_dts_middot
        lda     dts_dig,x
        jsr     double_dts_emitd
        inx
        bra     double_dts_mid1
double_dts_middot:
        lda     #$2E
        jsr     double_dts_emit
double_dts_mid2:
        cpx     dts_ndig
        beq     double_dts_donej
        lda     dts_dig,x
        jsr     double_dts_emitd
        inx
        bra     double_dts_mid2
double_dts_lead0:
        lda     #$30
        jsr     double_dts_emit
        lda     #$2E
        jsr     double_dts_emit
        sec                             ; (-P) leading zeros
        lda     #0
        sbc     dts_p
        tax
double_dts_l0l:
        cpx     #0
        beq     double_dts_l0d
        lda     #$30
        jsr     double_dts_emit
        dex
        bra     double_dts_l0l
double_dts_l0d:
        ldx     #0
double_dts_l0dl:
        cpx     dts_ndig
        beq     double_dts_donej
        lda     dts_dig,x
        jsr     double_dts_emitd
        inx
        bra     double_dts_l0dl
double_dts_sci:
        lda     dts_dig
        jsr     double_dts_emitd
        lda     dts_ndig
        cmp     #2
        bcc     double_dts_scie
        lda     #$2E
        jsr     double_dts_emit
        ldx     #1
double_dts_scil:
        cpx     dts_ndig
        beq     double_dts_scie
        lda     dts_dig,x
        jsr     double_dts_emitd
        inx
        bra     double_dts_scil
double_dts_scie:
        lda     #$45
        jsr     double_dts_emit
        lda     dts_e+1
        bpl     double_dts_scipos
        lda     #$2D
        jsr     double_dts_emit
        sec
        lda     #0
        sbc     dts_e
        sta     dts_e
        lda     #0
        sbc     dts_e+1
        sta     dts_e+1
        bra     double_dts_scimag
double_dts_scipos:
        lda     #$2B
        jsr     double_dts_emit
double_dts_scimag:
        jsr     double_dts_edec
        bra     double_dts_done
double_dts_nan:
        lda     #$4E
        jsr     double_dts_emit
        lda     #$41
        jsr     double_dts_emit
        lda     #$4E
        jsr     double_dts_emit
        bra     double_dts_done
double_dts_inf:
        lda     #$49
        jsr     double_dts_emit
        lda     #$4E
        jsr     double_dts_emit
        lda     #$46
        jsr     double_dts_emit
        bra     double_dts_done
double_dts_zero:
        lda     #$30
        jsr     double_dts_emit
double_dts_done:
        ldx     dts_bx
        lda     #0
        sta     d_strbuf,x
        lda     #<d_strbuf
        ldx     #>d_strbuf
        rts

; A -> d_strbuf[dts_bx], dts_bx++ . Uses Y (not X): the digit loops that
; call this keep their index in X.
double_dts_emit:
        ldy     dts_bx
        sta     d_strbuf,y
        iny
        sty     dts_bx
        rts

; A (0..9) -> emit as an ASCII digit
double_dts_emitd:
        clc
        adc     #$30
        bra     double_dts_emit

; emit dts_e (0..999) in decimal, no leading zeros
double_dts_edec:
        stz     dts_lead
        ldx     #0                      ; hundreds
double_dts_ed_h:
        lda     dts_e
        sec
        sbc     #100
        tay
        lda     dts_e+1
        sbc     #0
        bcc     double_dts_ed_hd
        sty     dts_e
        sta     dts_e+1
        inx
        bra     double_dts_ed_h
double_dts_ed_hd:
        cpx     #0
        beq     double_dts_ed_t
        txa
        jsr     double_dts_emitd
        inc     dts_lead
double_dts_ed_t:
        ldx     #0                      ; tens
double_dts_ed_tl:
        lda     dts_e
        cmp     #10
        bcc     double_dts_ed_td
        sbc     #10
        sta     dts_e
        inx
        bra     double_dts_ed_tl
double_dts_ed_td:
        cpx     #0
        bne     double_dts_ed_te
        lda     dts_lead
        beq     double_dts_ed_u
double_dts_ed_te:
        txa
        jsr     double_dts_emitd
double_dts_ed_u:
        lda     dts_e
        jsr     double_dts_emitd
        rts

; ---------------------------------------------------------------------
; internal: unpack, copy, normalise, pack
; ---------------------------------------------------------------------

; (d_ptr) 8 packed bytes -> dac_*  (dac_c/s/e/m)
double_d_unpack:
        ldy     #7
double_dun_cp:
        lda     (d_ptr),y
        sta     d_ub,y
        dey
        bpl     double_dun_cp

        lda     d_ub+7                  ; sign
        and     #$80
        sta     dac_s

        lda     d_ub+7                  ; biased exp = (b7&$7F)<<4 | (b6>>4)
        and     #$7F
        sta     dac_e
        stz     dac_e+1
        asl     dac_e
        rol     dac_e+1
        asl     dac_e
        rol     dac_e+1
        asl     dac_e
        rol     dac_e+1
        asl     dac_e
        rol     dac_e+1
        lda     d_ub+6
        lsr
        lsr
        lsr
        lsr
        ora     dac_e
        sta     dac_e                   ; dac_e:dac_e+1 = biased 0..2047

        lda     dac_e                   ; biased == 0 -> zero
        ora     dac_e+1
        bne     double_dun_notz
        lda     #D_ZERO
        sta     dac_c
        lda     #0
        ldy     #7
double_dun_zm:
        sta     dac_m,y
        dey
        bpl     double_dun_zm
        rts
double_dun_notz:
        lda     dac_e+1                 ; biased == 2047 -> inf/nan
        cmp     #>2047
        bne     double_dun_normal
        lda     dac_e
        cmp     #<2047
        bne     double_dun_normal
        lda     d_ub                    ; mantissa all zero -> inf
        ora     d_ub+1
        ora     d_ub+2
        ora     d_ub+3
        ora     d_ub+4
        ora     d_ub+5
        sta     d_t0
        lda     d_ub+6
        and     #$0F
        ora     d_t0
        bne     double_dun_nan
        lda     #D_INF
        sta     dac_c
        rts
double_dun_nan:
        lda     #D_NAN
        sta     dac_c
        rts
double_dun_normal:
        lda     #D_NORM
        sta     dac_c
        ; significand = (1<<52) | frac52, placed at bits 0..52, then << 11
        lda     d_ub
        sta     dac_m
        lda     d_ub+1
        sta     dac_m+1
        lda     d_ub+2
        sta     dac_m+2
        lda     d_ub+3
        sta     dac_m+3
        lda     d_ub+4
        sta     dac_m+4
        lda     d_ub+5
        sta     dac_m+5
        lda     d_ub+6
        and     #$0F
        ora     #$10                    ; implicit leading 1 (bit 52 -> byte6 bit4)
        sta     dac_m+6
        stz     dac_m+7
        ; << 8
        lda     dac_m+6
        sta     dac_m+7
        lda     dac_m+5
        sta     dac_m+6
        lda     dac_m+4
        sta     dac_m+5
        lda     dac_m+3
        sta     dac_m+4
        lda     dac_m+2
        sta     dac_m+3
        lda     dac_m+1
        sta     dac_m+2
        lda     dac_m
        sta     dac_m+1
        stz     dac_m
        ; << 3
        asl     dac_m
        rol     dac_m+1
        rol     dac_m+2
        rol     dac_m+3
        rol     dac_m+4
        rol     dac_m+5
        rol     dac_m+6
        rol     dac_m+7
        asl     dac_m
        rol     dac_m+1
        rol     dac_m+2
        rol     dac_m+3
        rol     dac_m+4
        rol     dac_m+5
        rol     dac_m+6
        rol     dac_m+7
        asl     dac_m
        rol     dac_m+1
        rol     dac_m+2
        rol     dac_m+3
        rol     dac_m+4
        rol     dac_m+5
        rol     dac_m+6
        rol     dac_m+7
        ; exponent = biased - 1086
        sec
        lda     dac_e
        sbc     #<1086
        sta     dac_e
        lda     dac_e+1
        sbc     #>1086
        sta     dac_e+1
        rts

; dac_* -> dbf_*
double_d_ac_to_bf:
        lda     dac_c
        sta     dbf_c
        lda     dac_s
        sta     dbf_s
        lda     dac_e
        sta     dbf_e
        lda     dac_e+1
        sta     dbf_e+1
        ldy     #7
double_dab_m:
        lda     dac_m,y
        sta     dbf_m,y
        dey
        bpl     double_dab_m
        rts

; normalise dac_m so bit 63 = 1, adjusting dac_e; all-zero -> true zero
double_d_norm:
double_dnm_chk:
        lda     dac_m+7
        bmi     double_dnm_done
        lda     dac_m
        ora     dac_m+1
        ora     dac_m+2
        ora     dac_m+3
        ora     dac_m+4
        ora     dac_m+5
        ora     dac_m+6
        ora     dac_m+7
        bne     double_dnm_sh
        lda     #D_ZERO
        sta     dac_c
        rts
double_dnm_sh:
        asl     dac_m
        rol     dac_m+1
        rol     dac_m+2
        rol     dac_m+3
        rol     dac_m+4
        rol     dac_m+5
        rol     dac_m+6
        rol     dac_m+7
        lda     dac_e
        bne     double_dnm_nolo
        dec     dac_e+1
double_dnm_nolo:
        dec     dac_e
        bra     double_dnm_chk
double_dnm_done:
        rts

; d_ac = +0
double_d_zero:
        lda     #0
        ldy     #7
double_dz_l:
        sta     d_ac,y
        dey
        bpl     double_dz_l
        rts

; d_ac = +/- 0 (keep dac_s)
double_d_zero_signed:
        lda     #0
        ldy     #6
double_dzs_l:
        sta     d_ac,y
        dey
        bpl     double_dzs_l
        lda     dac_s
        sta     d_ac+7
        rts

; pack dac_* (normalised) into d_ac with round-to-nearest-even.
; overflow -> infinity, underflow -> zero.
double_d_pack:
        lda     dac_c
        cmp     #D_NORM
        beq     double_dpk_norm
        cmp     #D_INF
        bne     double_dpk_notinf
        jmp     double_dpk_inf
double_dpk_notinf:
        cmp     #D_NAN
        bne     double_dpk_notnan
        jmp     double_dpk_nan
double_dpk_notnan:
        jmp     double_d_zero_signed
double_dpk_norm:
        ldy     #7                      ; work on a copy (a round carry may renorm)
double_dpk_cpm:
        lda     dac_m,y
        sta     d_work,y
        dey
        bpl     double_dpk_cpm
        ; drop bits 10..0. R = bit 10, S = OR(bits 9..0)
        lda     d_work+1
        and     #$04                    ; bit 10
        sta     d_t0                    ; R
        lda     d_work
        sta     d_t1
        lda     d_work+1
        and     #$03                    ; bits 9..8
        ora     d_t1
        ora     d_sticky                ; bits lost during alignment / mul / div
        sta     d_t1                    ; S
        lda     d_work+1                ; clear bits 10..8
        and     #$F8
        sta     d_work+1
        stz     d_work                  ; clear bits 7..0
        ; round to nearest even
        lda     d_t0
        beq     double_dpk_rounded      ; R = 0: truncate
        lda     d_t1
        bne     double_dpk_up           ; R=1, S!=0: up
        lda     d_work+1                ; tie: up only if bit 11 (lsb kept) is set
        and     #$08
        beq     double_dpk_rounded
double_dpk_up:
        clc
        lda     d_work+1
        adc     #$08
        sta     d_work+1
        lda     d_work+2
        adc     #0
        sta     d_work+2
        lda     d_work+3
        adc     #0
        sta     d_work+3
        lda     d_work+4
        adc     #0
        sta     d_work+4
        lda     d_work+5
        adc     #0
        sta     d_work+5
        lda     d_work+6
        adc     #0
        sta     d_work+6
        lda     d_work+7
        adc     #0
        sta     d_work+7
        bcc     double_dpk_rounded
        ror     d_work+7
        ror     d_work+6
        ror     d_work+5
        ror     d_work+4
        ror     d_work+3
        ror     d_work+2
        ror     d_work+1
        ror     d_work
        lda     #$80
        ora     d_work+7
        sta     d_work+7
        inc     dac_e
        bne     double_dpk_rounded
        inc     dac_e+1
double_dpk_rounded:
        clc                             ; biased = dac_e + 1086
        lda     dac_e
        adc     #<1086
        sta     d_bias
        lda     dac_e+1
        adc     #>1086
        sta     d_bias+1
        lda     d_bias+1
        bmi     double_dpk_under        ; biased < 0
        bne     double_dpk_maybe        ; >= 256
        bra     double_dpk_asm
double_dpk_maybe:
        lda     d_bias+1
        cmp     #>2047
        bcc     double_dpk_asm
        bne     double_dpk_ovf
        lda     d_bias
        cmp     #<2047
        bcc     double_dpk_asm
double_dpk_ovf:
        jmp     double_dpk_inf
double_dpk_under:
        jmp     double_d_zero_signed
double_dpk_asm:
        ; significand >> 11 (drop the low 11 bits already cleared): >>8 then >>3
        lda     d_work+1
        sta     d_work
        lda     d_work+2
        sta     d_work+1
        lda     d_work+3
        sta     d_work+2
        lda     d_work+4
        sta     d_work+3
        lda     d_work+5
        sta     d_work+4
        lda     d_work+6
        sta     d_work+5
        lda     d_work+7
        sta     d_work+6
        stz     d_work+7
        lsr     d_work+6
        ror     d_work+5
        ror     d_work+4
        ror     d_work+3
        ror     d_work+2
        ror     d_work+1
        ror     d_work
        lsr     d_work+6
        ror     d_work+5
        ror     d_work+4
        ror     d_work+3
        ror     d_work+2
        ror     d_work+1
        ror     d_work
        lsr     d_work+6
        ror     d_work+5
        ror     d_work+4
        ror     d_work+3
        ror     d_work+2
        ror     d_work+1
        ror     d_work
        ; d_work[0..6] = 53-bit significand; bit 52 (implicit) at byte6 bit4
        lda     d_work
        sta     d_ac
        lda     d_work+1
        sta     d_ac+1
        lda     d_work+2
        sta     d_ac+2
        lda     d_work+3
        sta     d_ac+3
        lda     d_work+4
        sta     d_ac+4
        lda     d_work+5
        sta     d_ac+5
        lda     d_work+6                ; frac 51..48 (drop implicit bit 4)
        and     #$0F
        sta     d_t0
        lda     d_bias                  ; byte6 = (biased low nibble << 4) | frac
        asl
        asl
        asl
        asl
        ora     d_t0
        sta     d_ac+6
        lda     d_bias                  ; byte7 = sign | (biased >> 4)
        lsr
        lsr
        lsr
        lsr
        sta     d_t0
        lda     d_bias+1
        asl
        asl
        asl
        asl
        ora     d_t0
        and     #$7F
        ora     dac_s
        sta     d_ac+7
        rts
double_dpk_inf:
        stz     d_ac
        stz     d_ac+1
        stz     d_ac+2
        stz     d_ac+3
        stz     d_ac+4
        stz     d_ac+5
        lda     #$F0
        sta     d_ac+6
        lda     dac_s
        ora     #$7F
        sta     d_ac+7
        rts
double_dpk_nan:
        lda     #$FF
        sta     d_ac
        sta     d_ac+1
        sta     d_ac+2
        sta     d_ac+3
        sta     d_ac+4
        sta     d_ac+5
        lda     #$F8
        sta     d_ac+6
        lda     #$7F
        sta     d_ac+7
        rts

; ---------------------------------------------------------------------
; Constants: packed binary64 values the algorithms lean on.
; ---------------------------------------------------------------------
        .section .rodata,"a",@progbits

d_ten:    .byte $00,$00,$00,$00,$00,$00,$24,$40   ; 10.0
d_one:    .byte $00,$00,$00,$00,$00,$00,$F0,$3F   ; 1.0
d_half:   .byte $00,$00,$00,$00,$00,$00,$E0,$3F   ; 0.5

d_ln2:    .byte $EF,$39,$FA,$FE,$42,$2E,$E6,$3F   ; ln 2  = 0.6931471805599453
d_log2e:  .byte $FE,$82,$2B,$65,$47,$15,$F7,$3F   ; 1/ln2 = 1.4426950408889634
d_1p5:    .byte $00,$00,$00,$00,$00,$00,$F8,$3F   ; 1.5
d_pihalf: .byte $18,$2D,$44,$54,$FB,$21,$F9,$3F   ; pi/2 = 1.5707963267948966
d_pi6:    .byte $66,$73,$2D,$38,$52,$C1,$E0,$3F   ; pi/6 = 0.5235987755982988
d_sqrt3:  .byte $AA,$4C,$58,$E8,$7A,$B6,$FB,$3F   ; sqrt3 = 1.7320508075688772
d_tan15:  .byte $56,$CD,$9E,$5E,$14,$26,$D1,$3F   ; tan(pi/12) = 0.26794919243112270
d_hyp20:  .byte $00,$00,$00,$00,$00,$00,$34,$40   ; 20.0 (tanh saturation cutoff)

; ---------------------------------------------------------------------
; State. crt0 zeroes BSS, so d_ac starts as +0.0 -- the same value the
; upstream file's initialisers gave it; everything else is written
; before it is read.
; ---------------------------------------------------------------------
        .section .bss,"aw",@nobits

d_ac:     .zero  8                        ; the packed accumulator

dac_c:    .zero  1
dac_s:    .zero  1
dac_e:    .zero  2
dac_m:    .zero  8

dbf_c:    .zero  1
dbf_s:    .zero  1
dbf_e:    .zero  2
dbf_m:    .zero  8

d_ub:     .zero  8
d_work:   .zero  8
d_cnt:    .zero  2
d_bias:   .zero  2
d_t0:     .zero  1
d_t1:     .zero  1
d_sticky: .zero  1
d_rsign:  .zero  1
d_prod:   .zero  16
d_rem:    .zero  8
d_diff:   .zero  8

d_sqv:    .zero  8
d_sqg:    .zero  8
d_sqi:    .zero  1

d_tv:     .zero  8                        ; transcendental scratch
d_tr:     .zero  8
d_tt:     .zero  8
d_tsum:   .zero  8
d_tterm:  .zero  8
d_tk:     .zero  8
d_tn16:   .zero  2
d_tkc:    .zero  1
d_powx:   .zero  8
d_powyp:  .zero  2
d_scq:    .zero  1
d_tanx:   .zero  8
d_tans:   .zero  8
d_tanc:   .zero  8
d_atflags: .zero  1
d_atx:    .zero  8
d_atn:    .zero  8
d_atd:    .zero  8
d_hypx:   .zero  8
d_hypa:   .zero  8
d_hypb:   .zero  8
d_hypn:   .zero  8
d_hypd:   .zero  8

dstr_len:   .zero  1
dstr_i:     .zero  1
dstr_neg:   .zero  1
dstr_frac:  .zero  1
dstr_esign: .zero  1
dstr_exp:   .zero  2
dstr_t:     .zero  2
dstr_cnt:   .zero  2
dstr_acc:   .zero  8

dts_bx:    .zero  1
dts_di:    .zero  1
dts_ndig:  .zero  1
dts_lead:  .zero  1
dts_e:     .zero  2
dts_p:     .zero  2
dts_dig:   .zero  18
dts_val:   .zero  8
dts_digd:  .zero  8
d_strbuf:  .zero  26
