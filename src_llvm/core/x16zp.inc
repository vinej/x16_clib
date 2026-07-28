; =====================================================================
; x16clib :: core/x16zp.inc (llvm-mos) -- the library's zero-page block
; =====================================================================
; The cc65 build needs a matching x16zp.inc full of .importzp directives,
; because ca65 decides addressing mode at assembly time and must be told
; a symbol lives in page zero. llvm-mos has no equivalent directive, so
; this file only carries the 16-bit aliases; the block itself is defined
; once, in core/x16zp.s.
;
; SAY mos8() AT EVERY USE. An external symbol's address is a relocation,
; the assembler cannot know it is under $100, and it emits the THREE-byte
; absolute form. Nothing shrinks it afterwards -- ld.lld does not re-lay
; out the section, and the relocation records prove it: every plain
; `sta X16_P0` carries R_MOS_ADDR16 and is still three bytes in the
; finished binary.
;
;       lda     mos8(X16_P0)    ; a5 xx      -- say this
;       lda     X16_P0          ; ad xx xx   -- not this
;
; That is a byte and a cycle per access, over a thousand accesses. It
; also keeps routines short enough that their branches stay in range,
; which matters more than it sounds: mos-as truncates an out-of-range
; branch displacement without a diagnostic. tools/llvm_zp_narrow.py
; applies the modifier and tools/llvm_branch_check.py catches the
; fallout when something still grows too big.
; =====================================================================

.ifndef X16_ZP_INC
X16_ZP_INC = 1

; 16-bit aliases over the same bytes, for (indirect),y addressing.
; There is only one (zp),y addressing mode, so the assembler needs no
; hint that these are zero page.
X16_PTR0  = X16_P0      ; P0/P1 as a pointer
X16_PTR1  = X16_P2      ; P2/P3 as a pointer
X16_PTR2  = X16_P4
X16_PTR3  = X16_P6
X16_TPTR0 = X16_T0
X16_TPTR1 = X16_T2
X16_TPTR2 = X16_T4
X16_TPTR3 = X16_T6

.endif
