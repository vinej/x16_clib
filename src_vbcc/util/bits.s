; =====================================================================
; x16clib :: util/bits.s -- bit and nibble helpers
; =====================================================================
; Masked read-modify-write on a byte in memory, plus nibble packing.
; The C compiler can of course do `*p |= mask` itself; these exist so C
; and assembly callers share one implementation, and because bit_put
; turns a flag into a set-or-clear without a branch at the call site.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers: the address rides r0/r1, the mask r2, and
; bit_put's flag r4; a lone byte (hinib/lonib) rides the accumulator.
; Returns: char in a.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r4

        global	_x16_bit_set
        global	_x16_bit_clr
        global	_x16_bit_put
        global	_x16_bit_test
        global	_x16_hinib
        global	_x16_lonib
        global	_x16_catnib

        section text

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void x16_bit_set (__reg("r0/r1") unsigned char *addr,
;                   __reg("r2") unsigned char mask)
; void x16_bit_clr (same)
;   The asm wants PTR0 = addr, A = mask.
; ---------------------------------------------------------------------
_x16_bit_set:
        lda     r0
        sta     X16_P0                  ; addr
        lda     r1
        sta     X16_P1
        lda     r2                      ; A = mask
        jmp     bit_set

_x16_bit_clr:
        lda     r0
        sta     X16_P0
        lda     r1
        sta     X16_P1
        lda     r2
        jmp     bit_clr

; ---------------------------------------------------------------------
; void x16_bit_put (__reg("r0/r1") unsigned char *addr,
;                   __reg("r2") unsigned char mask,
;                   __reg("r4") unsigned char on)
;   on != 0 sets the masked bits, on == 0 clears them.
;   The asm adds X = the flag.
; ---------------------------------------------------------------------
_x16_bit_put:
        lda     r0
        sta     X16_P0                  ; addr
        lda     r1
        sta     X16_P1
        ldx     r4                      ; X != 0 to set
        lda     r2                      ; A = mask
        jmp     bit_put

; ---------------------------------------------------------------------
; unsigned char x16_bit_test (__reg("r0/r1") const unsigned char *addr,
;                             __reg("r2") unsigned char mask)
;   Returns *addr & mask -- nonzero iff any masked bit is set.
; ---------------------------------------------------------------------
_x16_bit_test:
        lda     r0
        sta     X16_P0
        lda     r1
        sta     X16_P1
        lda     r2
        jsr     bit_test
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; unsigned char x16_hinib (__reg("a") unsigned char v)   -- v >> 4
; unsigned char x16_lonib (__reg("a") unsigned char v)   -- v & $0F
;
; The value arrives in A: only the return convention.
; ---------------------------------------------------------------------
_x16_hinib:
        jsr     hinib
        ldx     #0
        rts

_x16_lonib:
        jsr     lonib
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char x16_catnib (__reg("r0") unsigned char hi,
;                           __reg("r2") unsigned char lo)
;   (hi << 4) | lo, both masked to their nibble first.
;   The asm wants A = hi, X = lo.
; ---------------------------------------------------------------------
_x16_catnib:
        ldx     r2                      ; X = lo
        lda     r0                      ; A = hi
        jsr     catnib
        ldx     #0
        rts

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
; BSS pair; vbcc delivers them in registers, so no BSS here.)
