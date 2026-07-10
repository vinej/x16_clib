; =====================================================================
; x16clib :: core/x16zp.s (llvm-mos) -- the library's zero-page block
; =====================================================================
; The ONLY file that reserves the P/T block.
;
; Zero page on this target is TIGHT. cx16/lib/imag-regs.ld aliases
; llvm-mos's imaginary registers onto the KERNAL's virtual registers, so
; $02-$21 is r0-r15 (== __rc2-__rc19 and __rc20-__rc29), $22-$25 holds
; __rc0 and __rc30, and commodore.ld then hands the linker
;       MEMORY { zp : ORIGIN = __rc31 + 1, LENGTH = $80 - (__rc31 + 1) }
; which is $26-$7F: NINETY bytes, shared between this block and whatever
; the compiler decides to put in .zp.bss for the user's own program.
;
; Sixteen of those ninety are ours. That is affordable -- but note that
; llvm-mos's printf alone needs all ninety, so a program cannot use
; printf and this library's scratch block at the same time. The test
; harness therefore writes its output through CHROUT. See test_llvm.
;
; P0..P7 carry arguments for routines that need more than A/X/Y.
; T0..T7 are private scratch, never live across a call boundary.
;
; The block is shared and NOT reentrant. An interrupt handler must not
; call any x16_* routine that touches it -- in practice, anything taking
; more than three arguments, or any 16-bit argument. See x16/irq.h.
; =====================================================================

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; unsigned char x16_zp_base(void)
;
; The address the linker chose for X16_P0. Diagnostic only. Returns in A:
; llvm-mos returns a one-byte value in A alone, so unlike the cc65 build
; there is no high byte to zero.
; ---------------------------------------------------------------------
        .globl  x16_zp_base
x16_zp_base:
        lda     #<X16_P0
        rts

; ---------------------------------------------------------------------
; .zp.bss is the zero-page BSS section; ld.lld places it in the `zp`
; memory region. Contiguity is not guaranteed between objects, but it is
; within one, and nothing here depends on P and T being adjacent.
; ---------------------------------------------------------------------
        .section .zp.bss,"aw",@nobits

        .globl  X16_P0, X16_P1, X16_P2, X16_P3
        .globl  X16_P4, X16_P5, X16_P6, X16_P7
        .globl  X16_T0, X16_T1, X16_T2, X16_T3
        .globl  X16_T4, X16_T5, X16_T6, X16_T7

X16_P0: .zero   1
X16_P1: .zero   1
X16_P2: .zero   1
X16_P3: .zero   1
X16_P4: .zero   1
X16_P5: .zero   1
X16_P6: .zero   1
X16_P7: .zero   1

X16_T0: .zero   1
X16_T1: .zero   1
X16_T2: .zero   1
X16_T3: .zero   1
X16_T4: .zero   1
X16_T5: .zero   1
X16_T6: .zero   1
X16_T7: .zero   1
