; =====================================================================
; x16clib :: core/x16zp.inc -- import the library's zero-page block
; =====================================================================
; .include this in any module that touches the P/T block. It pulls in
; core/x16zp.s from the library at link time.
;
; IMPORTANT: the symbols arrive via .importzp, never .import.
;
; With a plain .import, ca65 has no way to know the symbol lives in page
; zero -- it assumes absolute addressing and emits a three-byte
; `sta $xxxx` against an address the linker later resolves below $0100.
; Depending on the instruction that is either a range error or, worse,
; silently the wrong opcode. .importzp marks the symbol zeropage so
; `sta X16_P0` assembles to the two-byte form.
; =====================================================================

.ifndef X16_ZP_INC
X16_ZP_INC = 1

        .importzp       X16_P0, X16_P1, X16_P2, X16_P3
        .importzp       X16_P4, X16_P5, X16_P6, X16_P7
        .importzp       X16_T0, X16_T1, X16_T2, X16_T3
        .importzp       X16_T4, X16_T5, X16_T6, X16_T7

; 16-bit aliases over the same bytes, for (indirect),y addressing.
X16_PTR0  = X16_P0      ; P0/P1 as a pointer
X16_PTR1  = X16_P2      ; P2/P3 as a pointer
X16_PTR2  = X16_P4
X16_PTR3  = X16_P6
X16_TPTR0 = X16_T0
X16_TPTR1 = X16_T2
X16_TPTR2 = X16_T4
X16_TPTR3 = X16_T6

.endif
