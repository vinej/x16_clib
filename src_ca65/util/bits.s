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

        .import         popa, popax

        .export         _x16_bit_set
        .export         _x16_bit_clr
        .export         _x16_bit_put
        .export         _x16_bit_test
        .export         _x16_hinib
        .export         _x16_lonib
        .export         _x16_catnib

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_bit_set (unsigned char *addr, unsigned char mask)
; void __fastcall__ x16_bit_clr (unsigned char *addr, unsigned char mask)
; ---------------------------------------------------------------------
_x16_bit_set:
        pha                             ; mask (rightmost arg, in A)
        jsr     popax                   ; addr
        sta     X16_P0
        stx     X16_P1
        pla
        jmp     bit_set

_x16_bit_clr:
        pha
        jsr     popax
        sta     X16_P0
        stx     X16_P1
        pla
        jmp     bit_clr

; ---------------------------------------------------------------------
; void __fastcall__ x16_bit_put (unsigned char *addr, unsigned char mask,
;                                unsigned char on)
;   on != 0 sets the masked bits, on == 0 clears them.
; ---------------------------------------------------------------------
_x16_bit_put:
        sta     bt_on                   ; on (rightmost arg, in A)
        jsr     popa                    ; mask
        sta     bt_mask
        jsr     popax                   ; addr
        sta     X16_P0
        stx     X16_P1
        ldx     bt_on
        lda     bt_mask
        jmp     bit_put

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_bit_test (const unsigned char *addr,
;                                          unsigned char mask)
;   Returns *addr & mask -- nonzero iff any masked bit is set.
; ---------------------------------------------------------------------
_x16_bit_test:
        pha
        jsr     popax
        sta     X16_P0
        stx     X16_P1
        pla
        jsr     bit_test
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_hinib (unsigned char v)   -- v >> 4
; unsigned char __fastcall__ x16_lonib (unsigned char v)   -- v & $0F
;
; The value already arrives in A: only the return convention.
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
; unsigned char __fastcall__ x16_catnib (unsigned char hi, unsigned char lo)
;   (hi << 4) | lo, both masked to their nibble first.
;
; popa leaves X alone, so lo goes to X before hi is popped into A.
; ---------------------------------------------------------------------
_x16_catnib:
        tax                             ; X = lo (rightmost arg, in A)
        jsr     popa                    ; A = hi
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

        .segment        "BSS"

bt_on:   .res 1
bt_mask: .res 1
