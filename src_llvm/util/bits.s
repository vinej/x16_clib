; =====================================================================
; x16clib :: util/bits.s -- bit and nibble helpers
; =====================================================================
; Masked read-modify-write on a byte in memory, plus nibble packing.
; The C compiler can of course do `*p |= mask` itself; these exist so C
; and assembly callers share one implementation, and because bit_put
; turns a flag into a set-or-clear without a branch at the call site.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine (see gfx/bitmap4l.s):
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X. Returns: char in A.

        .globl  x16_bit_set
        .globl  x16_bit_clr
        .globl  x16_bit_put
        .globl  x16_bit_test
        .globl  x16_hinib
        .globl  x16_lonib
        .globl  x16_catnib

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void x16_bit_set (unsigned char *addr, unsigned char mask)
; void x16_bit_clr (unsigned char *addr, unsigned char mask)
; addr -> __rc2/__rc3, mask -> A. The asm wants PTR0 = addr, A = mask;
; the loads below go through Y so the mask never leaves A.
; ---------------------------------------------------------------------
x16_bit_set:
        ldy     __rc2                   ; addr
        sty     X16_P0
        ldy     __rc3
        sty     X16_P1
        jmp     bit_set

x16_bit_clr:
        ldy     __rc2
        sty     X16_P0
        ldy     __rc3
        sty     X16_P1
        jmp     bit_clr

; ---------------------------------------------------------------------
; void x16_bit_put (unsigned char *addr, unsigned char mask,
;                   unsigned char on)
;   on != 0 sets the masked bits, on == 0 clears them.
; addr -> __rc2/__rc3, mask -> A, on -> X -- and the asm wants exactly
; A = mask, X = the flag, so only the pointer moves.
; ---------------------------------------------------------------------
x16_bit_put:
        ldy     __rc2                   ; addr
        sty     X16_P0
        ldy     __rc3
        sty     X16_P1
        jmp     bit_put

; ---------------------------------------------------------------------
; unsigned char x16_bit_test (const unsigned char *addr,
;                             unsigned char mask)
;   Returns *addr & mask -- nonzero iff any masked bit is set.
; ---------------------------------------------------------------------
x16_bit_test:
        ldy     __rc2
        sty     X16_P0
        ldy     __rc3
        sty     X16_P1
        jmp     bit_test                ; A = *addr & mask

; ---------------------------------------------------------------------
; unsigned char x16_hinib (unsigned char v)   -- v >> 4
; unsigned char x16_lonib (unsigned char v)   -- v & $0F
;
; The value arrives in A and the nibble returns in A: the asm entry IS
; the C entry.
; ---------------------------------------------------------------------
x16_hinib:
        jmp     hinib

x16_lonib:
        jmp     lonib

; ---------------------------------------------------------------------
; unsigned char x16_catnib (unsigned char hi, unsigned char lo)
;   (hi << 4) | lo, both masked to their nibble first.
; hi -> A, lo -> X: exactly what the asm wants.
; ---------------------------------------------------------------------
x16_catnib:
        jmp     catnib

; =====================================================================
; Internal routines (the upstream x16_library body, verbatim)
; =====================================================================

; ---------------------------------------------------------------------
; catnib -- in: A = high nibble, X = low nibble.  out: A = (A<<4)|X
; ---------------------------------------------------------------------
catnib:
        and     #$0F
        asl
        asl
        asl
        asl
        sta     X16_T0
        txa
        and     #$0F
        ora     X16_T0
        rts

; ---------------------------------------------------------------------
; hinib / lonib -- in: A = byte.  out: A = that nibble, in bits 3:0
; ---------------------------------------------------------------------
hinib:
        lsr
        lsr
        lsr
        lsr
        rts

lonib:
        and     #$0F
        rts

; ---------------------------------------------------------------------
; bit_set / bit_clr -- in: X16_PTR0 = address, A = mask
; bit_put          -- in: X16_PTR0 = address, A = mask,
;                        X != 0 to set, X = 0 to clear
; ---------------------------------------------------------------------
bit_set:
        ldy     #0
        ora     (X16_PTR0),y
        sta     (X16_PTR0),y
        rts

bit_clr:
        eor     #$FF
        ldy     #0
        and     (X16_PTR0),y
        sta     (X16_PTR0),y
        rts

bit_put:
        cpx     #0
        beq     bit_clr
        bra     bit_set

; ---------------------------------------------------------------------
; bit_test -- in: X16_PTR0 = address, A = mask
;             out: Z clear if any masked bit is set
; ---------------------------------------------------------------------
bit_test:
        ldy     #0
        and     (X16_PTR0),y
        rts

; (The cc65 build stages bit_put's arguments through a bt_on/bt_mask
; BSS pair; llvm-mos delivers them in A/X directly, so no BSS here.)
