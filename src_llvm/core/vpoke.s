; =====================================================================
; x16clib :: core/vpoke.s (llvm-mos) -- a working vpoke()
; =====================================================================
; llvm-mos-sdk v23.0.1's cx16 vpoke() is broken, and this file replaces
; it. The SDK's is a normal archive member (libc.a(vpoke.s.obj)) whose
; only defined symbol is `vpoke`, and libx16c.a is named ahead of the
; platform libraries on the link line -- so the linker resolves `vpoke`
; here and never pulls the SDK's member in. Nothing else changes: link
; libx16c.a and every existing vpoke() call starts landing where the
; caller asked.
;
; The bug, read off the SDK's own disassembly:
;
;   void vpoke(unsigned char data, unsigned long addr)
;
; puts data in A and addr in X (byte 0), __rc2 (byte 1), __rc3 (byte 2)
; and __rc4 (byte 3). The SDK's code stores __rc4 -> ADDR_H, __rc3 ->
; ADDR_M and __rc2 -> ADDR_L -- address bytes 3, 2 and 1. It never reads
; X, so the low byte is dropped and every write lands at addr >> 8:
; vpoke(0xAB, 0x08000) stores at $00080, vpoke(0xCD, 0x01234) at $00012.
; It is off by one register: the author appears to have assumed addr
; began at __rc2 rather than in X.
;
; The SDK's vpeek() is correct, and is what this mirrors -- it reads the
; same address from A (byte 0), X (byte 1) and __rc2 (byte 2). Note both
; write address byte 2 into ADDR_H unmasked, so it carries the bank bit
; and leaves the increment field at zero, which is what a single access
; wants. Passing an address above $1FFFF is the caller's mistake in
; either function.
;
; stz VERA_CTRL selects data port 0 (and resets DCSEL to 0), exactly as
; the SDK's vpeek() and vpoke() both do.
; =====================================================================

        .include        "macros.inc"

        .globl  vpoke

        .section .text.vpoke,"ax",@progbits

; void vpoke(unsigned char data, unsigned long addr)
;   data -> A; addr -> X (lo), __rc2 (mid), __rc3 (hi), __rc4 (unused).
vpoke:
        stz     VERA_CTRL               ; ADDRSEL = 0
        ldy     __rc3                   ; addr byte 2: bank bit
        sty     VERA_ADDR_H
        ldy     __rc2                   ; addr byte 1
        sty     VERA_ADDR_M
        stx     VERA_ADDR_L             ; addr byte 0 -- the byte the
        sta     VERA_DATA0              ; SDK's version never reads
        rts
