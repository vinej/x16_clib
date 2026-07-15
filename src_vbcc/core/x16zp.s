; =====================================================================
; x16clib :: core/x16zp.s -- the library's zero-page scratch block
; =====================================================================
; The ONLY file that reserves the P/T block. Everything else pulls the
; symbols in with core/x16zp.inc.
;
; The ACME original hardcoded this block at $22. cc65's cx16.cfg maps
;       ZP: start = $0022, size = $0080 - $0022
; onto its ZEROPAGE segment -- exactly the same window -- so here the
; linker places the block instead, packing it alongside cc65's own
; runtime zero page (sp, sreg, ptr1-4, tmp1-4, regsave: ~26 bytes).
; 16 more leaves the 94-byte window with room to spare.
;
; P0..P7 carry arguments for routines that need more than A/X/Y.
; T0..T7 are private scratch, never live across a call boundary.
;
; The block is shared and NOT reentrant. An interrupt handler must not
; call any x16_* routine that touches it -- in practice, anything taking
; more than three arguments, or any 16-bit argument. See include/x16/irq.h.
; =====================================================================

        global	X16_P0
        global	X16_P1
        global	X16_P2
        global	X16_P3
        global	X16_P4
        global	X16_P5
        global	X16_P6
        global	X16_P7
        global	X16_T0
        global	X16_T1
        global	X16_T2
        global	X16_T3
        global	X16_T4
        global	X16_T5
        global	X16_T6
        global	X16_T7

        global	_x16_zp_base

        section text

; ---------------------------------------------------------------------
; unsigned char x16_zp_base(void)
;
; The address the linker chose for X16_P0. Diagnostic only: nothing in
; the library depends on the value, and it moves as cc65's own runtime
; zero-page footprint changes. The test suite asserts it landed inside
; the $22-$7F user window.
; ---------------------------------------------------------------------
_x16_zp_base:
        lda     #<X16_P0
        ldx     #$00                    ; high byte, for int-promoting callers
        rts

        section zpage

X16_P0: reserve    1
X16_P1: reserve    1
X16_P2: reserve    1
X16_P3: reserve    1
X16_P4: reserve    1
X16_P5: reserve    1
X16_P6: reserve    1
X16_P7: reserve    1

X16_T0: reserve    1
X16_T1: reserve    1
X16_T2: reserve    1
X16_T3: reserve    1
X16_T4: reserve    1
X16_T5: reserve    1
X16_T6: reserve    1
X16_T7: reserve    1
