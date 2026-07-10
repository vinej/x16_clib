; =====================================================================
; x16clib :: core/x16zp.inc (llvm-mos) -- the library's zero-page block
; =====================================================================
; The cc65 build needs a matching x16zp.inc full of .importzp directives,
; because ca65 decides addressing mode at assembly time and must be told
; a symbol lives in page zero. llvm-mos does not work that way: the
; assembler emits a relaxable instruction, and ld.lld shrinks it to the
; two-byte zero-page form once the address is known. So an external
; symbol needs no declaration at all here, and this file exists only for
; the 16-bit aliases and to keep the two trees looking alike.
;
; The block itself is defined once, in core/x16zp.s.
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
