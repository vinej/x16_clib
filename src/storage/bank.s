; =====================================================================
; x16clib :: storage/bank.s -- banked RAM ($A000-$BFFF window)
; =====================================================================
; RAM_BANK ($00) selects which 8 KB bank appears at $A000-$BFFF.
; Bank 0 holds KERNAL variables; banks 1..255 are yours.
;
; Offsets are 0..8191 into the window. The bulk copies auto-advance
; across bank boundaries, so a run may start near the end of one bank
; and finish in the next.
;
; All routines here save and restore RAM_BANK, so they are safe to call
; without disturbing whatever bank the caller had mapped in.
;
; cc65's BANK_RAM[] array in <cx16.h> gives raw access to the window at
; $A000 once you have set RAM_BANK yourself. These add the bank save and
; restore, the offset arithmetic, and the boundary-crossing copies.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa, popax

        .export         _x16_bank_set
        .export         _x16_bank_get
        .export         _x16_bank_peek
        .export         _x16_bank_poke
        .export         _x16_mem_to_bank
        .export         _x16_bank_to_mem

BANK_WINDOW     = $A000
BANK_WINDOW_END = $C000

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_bank_set(unsigned char bank)
; unsigned char x16_bank_get(void)
;   Both safe from an ISR: neither touches the shared scratch block.
; ---------------------------------------------------------------------
_x16_bank_set:
        sta     RAM_BANK
        rts

_x16_bank_get:
        lda     RAM_BANK
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_bank_peek(unsigned char bank,
;                                          unsigned int offset)
; ---------------------------------------------------------------------
_x16_bank_peek:
        sta     X16_P0                  ; offset lo (rightmost arg: A/X)
        stx     X16_P1                  ; offset hi
        jsr     popa                    ; A = bank
        jsr     bank_peek
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_bank_poke(unsigned char bank, unsigned int offset,
;                                 unsigned char value)
; ---------------------------------------------------------------------
_x16_bank_poke:
        pha                             ; value (rightmost arg, in A)
        jsr     popax                   ; offset
        sta     X16_P0
        stx     X16_P1
        jsr     popa                    ; bank
        tax
        pla                             ; A = value, X = bank
        jmp     bank_poke

; ---------------------------------------------------------------------
; void __fastcall__ x16_mem_to_bank(const void *src, unsigned char bank,
;                                   unsigned int offset, unsigned int count)
; ---------------------------------------------------------------------
_x16_mem_to_bank:
        sta     X16_P5                  ; count lo
        stx     X16_P6                  ; count hi
        jsr     popax
        sta     X16_P3                  ; offset
        stx     X16_P4
        jsr     popa
        sta     X16_P2                  ; bank
        jsr     popax
        sta     X16_P0                  ; src
        stx     X16_P1
        jmp     mem_to_bank

; ---------------------------------------------------------------------
; void __fastcall__ x16_bank_to_mem(unsigned char bank, unsigned int offset,
;                                   void *dst, unsigned int count)
; ---------------------------------------------------------------------
_x16_bank_to_mem:
        sta     X16_P5                  ; count lo
        stx     X16_P6                  ; count hi
        jsr     popax
        sta     X16_P3                  ; dst
        stx     X16_P4
        jsr     popax
        sta     X16_P1                  ; offset
        stx     X16_P2
        jsr     popa
        sta     X16_P0                  ; bank
        jmp     bank_to_mem

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; bank_peek -- in:  A = bank, X16_P0/P1 = offset (0..8191)
;              out: A = byte
; bank_poke -- in:  A = byte, X = bank, X16_P0/P1 = offset
; ---------------------------------------------------------------------
bank_peek:
        ldx     RAM_BANK
        phx
        sta     RAM_BANK
        jsr     window_ptr
        ldy     #0
        lda     (X16_T0),y
        plx
        stx     RAM_BANK
        rts

bank_poke:
        pha                             ; [byte]
        lda     RAM_BANK
        pha                             ; [byte][caller bank]
        stx     RAM_BANK
        jsr     window_ptr
        ldy     #0
        pla
        tax                             ; X = caller bank
        pla                             ; A = byte to store
        sta     (X16_T0),y
        stx     RAM_BANK
        rts

; T0/T1 = BANK_WINDOW + offset. Preserves A.
;
; A shared helper, so it gets a plain label rather than a cheap local: a
; cheap local only reaches from one plain label to the next, and bank_peek
; could not then see a helper defined after bank_poke.
window_ptr:
        pha
        lda     X16_P0
        clc
        adc     #<BANK_WINDOW
        sta     X16_T0
        lda     X16_P1
        adc     #>BANK_WINDOW
        sta     X16_T1
        pla
        rts

; ---------------------------------------------------------------------
; mem_to_bank -- copy low RAM into banked RAM
;   in:  X16_P0/P1 = source address
;        X16_P2    = destination bank
;        X16_P3/P4 = destination offset (0..8191)
;        X16_P5/P6 = byte count
;
; bank_to_mem -- the inverse
;   in:  X16_P0    = source bank
;        X16_P1/P2 = source offset
;        X16_P3/P4 = destination address
;        X16_P5/P6 = byte count
;
; Both auto-advance: when the window pointer reaches $C000 it snaps back
; to $A000 and RAM_BANK increments.
; ---------------------------------------------------------------------
mem_to_bank:
        lda     RAM_BANK
        pha
        lda     X16_P2
        sta     RAM_BANK

        lda     X16_P0                  ; T2/T3 = source
        sta     X16_T2
        lda     X16_P1
        sta     X16_T3
        lda     X16_P3                  ; T0/T1 = window pointer
        clc
        adc     #<BANK_WINDOW
        sta     X16_T0
        lda     X16_P4
        adc     #>BANK_WINDOW
        sta     X16_T1
        jsr     load_count

@copy_out:
        lda     X16_T4
        ora     X16_T5
        beq     @out_done
        ldy     #0
        lda     (X16_T2),y
        sta     (X16_T0),y
        jsr     advance_src
        jsr     advance_window
        jsr     dec_count
        bra     @copy_out
@out_done:
        pla
        sta     RAM_BANK
        rts

bank_to_mem:
        lda     RAM_BANK
        pha
        lda     X16_P0
        sta     RAM_BANK

        lda     X16_P3                  ; T2/T3 = destination
        sta     X16_T2
        lda     X16_P4
        sta     X16_T3
        lda     X16_P1                  ; T0/T1 = window pointer
        clc
        adc     #<BANK_WINDOW
        sta     X16_T0
        lda     X16_P2
        adc     #>BANK_WINDOW
        sta     X16_T1
        jsr     load_count

@copy_in:
        lda     X16_T4
        ora     X16_T5
        beq     @in_done
        ldy     #0
        lda     (X16_T0),y
        sta     (X16_T2),y
        jsr     advance_src
        jsr     advance_window
        jsr     dec_count
        bra     @copy_in
@in_done:
        pla
        sta     RAM_BANK
        rts

; --- shared helpers --------------------------------------------------
load_count:
        lda     X16_P5
        sta     X16_T4
        lda     X16_P6
        sta     X16_T5
        rts

; T2/T3 is whichever side lives in low RAM.
advance_src:
        inc     X16_T2
        bne     @done
        inc     X16_T3
@done:
        rts

; T0/T1 walks the $A000 window and rolls into the next bank at $C000.
advance_window:
        inc     X16_T0
        bne     @done
        inc     X16_T1
        lda     X16_T1
        cmp     #>BANK_WINDOW_END
        bne     @done
        lda     #>BANK_WINDOW
        sta     X16_T1
        inc     RAM_BANK
@done:
        rts

dec_count:
        lda     X16_T4
        bne     @low
        dec     X16_T5
@low:
        dec     X16_T4
        rts
