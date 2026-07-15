; =====================================================================
; x16clib :: core/const_zp.inc -- zero page and bank registers
; =====================================================================
; Pure symbol file. Safe to .include any number of times.
;
; X16 zero page (Programmer's Reference / x16-rom-r49 inc/regs.inc):
;   $00       RAM_BANK   - bank visible at $A000-$BFFF
;   $01       ROM_BANK   - bank visible at $C000-$FFFF
;   $02-$21   r0..r15    - KERNAL virtual registers (argument passing)
;   $22-$7F   free for user programs   <-- cc65's ZEROPAGE segment
;   $80-$FF   KERNAL / BASIC / DOS     <-- never touch
;
; The library's own scratch block is NOT here: cc65's cx16.cfg maps its
; ZEROPAGE segment onto exactly $22-$7F, so the linker allocates it.
; See core/x16zp.s (the definitions) and core/x16zp.inc (the imports).
;
; This file does not include cc65's cx16.inc. That file defines RAM_BANK
; and r0..r15 too, and pulling both in would be a duplicate definition.
; =====================================================================

	ifndef X16_CONST_ZP_INC
X16_CONST_ZP_INC = 1

RAM_BANK        = $00
ROM_BANK        = $01

; KERNAL virtual registers. Caller-save: the library uses r0..r5 freely.
; They sit below $22, outside cc65's ZEROPAGE area, so cc65's runtime can
; never collide with them.
; (r-reg dropped: vbcc provides r0..r15)
r0L = $02
r0H = $03
; (r-reg dropped: vbcc provides r0..r15)
r1L = $04
r1H = $05
; (r-reg dropped: vbcc provides r0..r15)
r2L = $06
r2H = $07
; (r-reg dropped: vbcc provides r0..r15)
r3L = $08
r3H = $09
; (r-reg dropped: vbcc provides r0..r15)
r4L = $0A
r4H = $0B
; (r-reg dropped: vbcc provides r0..r15)
r5L = $0C
r5H = $0D
; (r-reg dropped: vbcc provides r0..r15)
r6L = $0E
r6H = $0F
; (r-reg dropped: vbcc provides r0..r15)
r7L = $10
r7H = $11
; (r-reg dropped: vbcc provides r0..r15)
r8L = $12
r8H = $13
; (r-reg dropped: vbcc provides r0..r15)
r9L = $14
r9H = $15
; (r-reg dropped: vbcc provides r0..r15)
r10L = $16
r10H = $17
; (r-reg dropped: vbcc provides r0..r15)
r11L = $18
r11H = $19
; (r-reg dropped: vbcc provides r0..r15)
r12L = $1A
r12H = $1B
; (r-reg dropped: vbcc provides r0..r15)
r13L = $1C
r13H = $1D
; (r-reg dropped: vbcc provides r0..r15)
r14L = $1E
r14H = $1F
; (r-reg dropped: vbcc provides r0..r15)
r15L = $20
r15H = $21

	endif
