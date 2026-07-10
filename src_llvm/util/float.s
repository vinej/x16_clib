; =====================================================================
; x16clib :: util/float.s -- floating point, via the ROM's FP library
; =====================================================================
; The X16 ROM already carries a complete C128/C65-compatible floating
; point library in BANK_BASIC, reachable through a stable jump table at
; $FE00. This module is a binding, not a reimplementation: several
; thousand lines of 6502 we do not have to write, test, or carry -- and
; several thousand bytes cc65's own float support would have to link.
;
; Everything works on FAC, the floating accumulator in zero page, which
; is why the C API reads as a sequence of operations on an implicit
; accumulator rather than as expressions. A float in memory is 5 bytes.
;
; FAC and ARG live at $C3-$D1, above cc65's zero-page window, so there is
; no conflict. BASIC is dormant while a SYSed program runs, so disturbing
; them is safe.
;
; --- on operand order ------------------------------------------------
; The ROM's fp_fsub and fp_fdiv are backwards from what jumptab.s claims:
; both load ARG from memory and then subtract or divide FAC INTO it, so
; you get `mem - FAC` and `mem / FAC`. f_sub and f_div below present the
; intuitive direction by stashing FAC in ARG first and running the
; ARG-first form. f_rsub and f_rdiv expose the raw order, which is what
; you want for `1/x` and similar.
;
; --- cost ------------------------------------------------------------
; Every call crosses a ROM bank via jsrfar, which is not free. For hot
; per-frame maths prefer x16_mul88() from <x16/fixed.h>.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X, then whatever __rc bytes are left.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_f_zero
        .globl  x16_f_neg
        .globl  x16_f_abs
        .globl  x16_f_int
        .globl  x16_f_sgn
        .globl  x16_f_from_u8
        .globl  x16_f_from_s16
        .globl  x16_f_to_s16
        .globl  x16_f_load
        .globl  x16_f_store
        .globl  x16_f_add
        .globl  x16_f_sub
        .globl  x16_f_mul
        .globl  x16_f_div
        .globl  x16_f_rsub
        .globl  x16_f_rdiv
        .globl  x16_f_pow
        .globl  x16_f_rpow
        .globl  x16_f_cmp
        .globl  x16_f_sqrt
        .globl  x16_f_ln
        .globl  x16_f_exp
        .globl  x16_f_sin
        .globl  x16_f_cos
        .globl  x16_f_tan
        .globl  x16_f_atan
        .globl  x16_f_to_str
        .globl  x16_f_to_str_trim
        .globl  x16_f_from_str

; A caller's operand address, stashed across the bank crossings. Must be
; in zero page: f_to_str_trim dereferences it with (zp),y. Nothing here
; calls another library routine, so borrowing the shared scratch pointer
; cannot collide with anything.
f_ptr = X16_TPTR0                       ; T0/T1
c_dst = X16_TPTR1                       ; T2/T3 -- the C string destination
c_src = X16_TPTR2                       ; T4/T5

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; A pointer argument arrives in __rc2/__rc3, not in A/X. The ROM wants the
; address in A (low) / Y (high). Every operand-taking entry point below
; funnels through this macro, so the module's whole pointer handling is
; these two instructions. (cc65 received the pointer in A/X and only had
; to phx/ply.)
.macro  ptr_ax_to_ay
        lda     __rc2
        ldy     __rc3
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

; --- no arguments ----------------------------------------------------
x16_f_zero:
        jsrfar  fp_zerofc, BANK_BASIC
        rts

; fp_negop is the true unary minus. fp_negfac, despite its name, is an
; internal helper of the ROM's add/subtract path that two's-complements
; the mantissa in place -- calling it on a normalised FAC denormalises it
; (5.0 comes back as garbage that reads about -2.5).
x16_f_neg:
        jsrfar  fp_negop, BANK_BASIC
        rts

x16_f_abs:
        jsrfar  fp_abs, BANK_BASIC
        rts

x16_f_int:
        jsrfar  fp_int, BANK_BASIC
        rts

x16_f_sqrt:
        jsrfar  fp_sqr, BANK_BASIC
        rts

x16_f_ln:
        jsrfar  fp_log, BANK_BASIC
        rts

x16_f_exp:
        jsrfar  fp_exp, BANK_BASIC
        rts

x16_f_sin:
        jsrfar  fp_sin, BANK_BASIC
        rts

x16_f_cos:
        jsrfar  fp_cos, BANK_BASIC
        rts

x16_f_tan:
        jsrfar  fp_tan, BANK_BASIC
        rts

x16_f_atan:
        jsrfar  fp_atn, BANK_BASIC
        rts

; ---------------------------------------------------------------------
; signed char x16_f_sgn(void)  --  -1, 0 or 1
; ---------------------------------------------------------------------
; A signed char comes back in A alone; llvm-mos widens it at the call site.
x16_f_sgn:
        jsrfar  fp_sign, BANK_BASIC
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_f_from_u8(unsigned char v)
;
; Through givayf with a zero high byte. The ROM's fp_float converts a
; SIGNED byte, so 200 through it would come out as -56.
; ---------------------------------------------------------------------
x16_f_from_u8:
        tay                             ; Y = low byte
        lda     #0                      ; A = high byte: zero-extend
        jsrfar  fp_givayf, BANK_BASIC
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_f_from_s16(int v)
; int x16_f_to_s16(void)               -- rounds toward zero
;
; fp_givayf wants the high byte in A and the low byte in Y, the reverse
; of the usual A = low convention, so swap on the way in. fp_ayint leaves
; the result big-endian in FACMO (high) and FACLO (low).
; ---------------------------------------------------------------------
x16_f_from_s16:
        sta     f_ptr                   ; stash the low byte
        txa                             ; A = high
        ldy     f_ptr                   ; Y = low
        jsrfar  fp_givayf, BANK_BASIC
        rts

x16_f_to_s16:
        jsrfar  fp_ayint, BANK_BASIC
        lda     FP_FACLO
        ldx     FP_FACMO
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_f_load(const x16_float m)   -- FAC = *m
; void __fastcall__ x16_f_store(x16_float m)        -- *m = round(FAC)
;
; fp_movmf takes its pointer in X/Y rather than A/Y. Only this one does.
; ---------------------------------------------------------------------
x16_f_load:
        ptr_ax_to_ay
        jsrfar  fp_movfm, BANK_BASIC
        rts

x16_f_store:
        ptr_ax_to_ay
        tax                             ; X = low, Y = high
        jsrfar  fp_movmf, BANK_BASIC
        rts

; ---------------------------------------------------------------------
; FAC = FAC + m,  FAC = FAC * m.  Both commute, so the ROM's order does
; not matter.
; ---------------------------------------------------------------------
x16_f_add:
        ptr_ax_to_ay
        jsrfar  fp_fadd, BANK_BASIC
        rts

x16_f_mul:
        ptr_ax_to_ay
        jsrfar  fp_fmult, BANK_BASIC
        rts

; ---------------------------------------------------------------------
; FAC = FAC - m,  FAC = FAC / m,  FAC = FAC ^ m.
;
; The ROM only offers mem-first. Move FAC into ARG, load mem into FAC,
; then use the ARG-first entry, which computes ARG (op) FAC. Three bank
; crossings instead of one -- the price of the intuitive direction.
; ---------------------------------------------------------------------
x16_f_sub:
        ptr_ax_to_ay
        jsr     arg_gets_fac_fac_gets_mem
        jsrfar  fp_fsubt, BANK_BASIC    ; FAC = ARG - FAC
        rts

x16_f_div:
        ptr_ax_to_ay
        jsr     arg_gets_fac_fac_gets_mem
        jsrfar  fp_fdivt, BANK_BASIC    ; FAC = ARG / FAC
        rts

x16_f_pow:
        ptr_ax_to_ay
        jsr     arg_gets_fac_fac_gets_mem
        jsrfar  fp_fpwrt, BANK_BASIC    ; FAC = ARG ^ FAC
        rts

; in: A = operand low, Y = operand high
arg_gets_fac_fac_gets_mem:
        sta     f_ptr
        sty     f_ptr+1
        jsrfar  fp_movef, BANK_BASIC    ; ARG = FAC
        lda     f_ptr
        ldy     f_ptr+1
        jsrfar  fp_movfm, BANK_BASIC    ; FAC = mem
        rts

; ---------------------------------------------------------------------
; FAC = m - FAC,  FAC = m / FAC,  FAC = m ^ FAC.
; The ROM's native order: one bank crossing instead of three. f_rdiv is
; the reciprocal form, 1/x.
; ---------------------------------------------------------------------
x16_f_rsub:
        ptr_ax_to_ay
        jsrfar  fp_fsub, BANK_BASIC
        rts

x16_f_rdiv:
        ptr_ax_to_ay
        jsrfar  fp_fdiv, BANK_BASIC
        rts

x16_f_rpow:
        ptr_ax_to_ay
        jsrfar  fp_fpwr, BANK_BASIC
        rts

; ---------------------------------------------------------------------
; signed char __fastcall__ x16_f_cmp(const x16_float m)
;   -1 if FAC < m, 0 if equal, 1 if FAC > m
; ---------------------------------------------------------------------
x16_f_cmp:
        ptr_ax_to_ay
        jsrfar  fp_fcomp, BANK_BASIC
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_f_to_str(char *buf)
; void __fastcall__ x16_f_to_str_trim(char *buf)
;
; The ROM writes its result to FP_FBUFFR ($0100 -- the bottom of the
; STACK PAGE, which BASIC also uses for this). It would not survive the
; next deep call, nor the next conversion, so these copy it into the
; caller's buffer before returning. Sixteen bytes is always enough.
;
; Positive numbers get a leading space, exactly as BASIC's PRINT shows
; them; the _trim form skips it.
; ---------------------------------------------------------------------
; buf is a pointer -> __rc2/__rc3.
x16_f_to_str:
        lda     __rc2
        sta     c_dst
        lda     __rc3
        sta     c_dst+1
        jsr     f_to_str                ; A/X = the ROM's buffer
        jmp     copy_asciiz

x16_f_to_str_trim:
        lda     __rc2
        sta     c_dst
        lda     __rc3
        sta     c_dst+1
        jsr     f_to_str
        sta     c_src
        stx     c_src+1
        ldy     #0
        lda     (c_src),y
        cmp     #32                     ; a leading sign space
        bne     copy_from_src
        inc     c_src                   ; skip it
        bne     copy_from_src
        inc     c_src+1
        ; fall through

; in: c_src = source, c_dst = destination
copy_from_src:
        ldy     #0
.Lcopy_from_src_loop:
        lda     (c_src),y
        sta     (c_dst),y
        beq     .Lcopy_from_src_done
        iny
        bne     .Lcopy_from_src_loop
.Lcopy_from_src_done:
        rts

; in: A/X = source, c_dst = destination
copy_asciiz:
        sta     c_src
        stx     c_src+1
        bra     copy_from_src

; f_to_str -- out: A = low, X = high of the ROM's NUL-terminated buffer
f_to_str:
        jsrfar  fp_fout, BANK_BASIC
        pha                             ; the ROM returns A = low, Y = high
        tya
        tax                             ; X = high
        pla                             ; A = low
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_f_from_str(const char *s, unsigned char len)
;
; fp_val wants X = address LOW, Y = address high, A = length. Note the
; low byte in X: jumptab.s writes the argument as ".X:.Y", which
; everywhere else in that file means high:low, but the code is
; `stx index / sty index+1` -- low first. The comment is wrong.
; ---------------------------------------------------------------------
; s is a pointer -> __rc2/__rc3; len is the only integer -> A.
x16_f_from_str:
        ldx     __rc2                   ; X = address low
        ldy     __rc3                   ; Y = address high
        jsrfar  fp_val, BANK_BASIC      ; A is already the length
        rts
