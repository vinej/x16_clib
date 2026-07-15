; =====================================================================
; x16clib :: video/palette.s -- VERA palette
; =====================================================================
; 256 entries of two bytes at $1FA00:
;       byte 0 = Green<<4 | Blue
;       byte 1 = Red             (high nibble unused)
;
; So a 12-bit $0RGB colour stores little-endian exactly as written:
; $0F00 is pure red, $00F0 pure green, $000F pure blue. That is why the
; C API takes the colour as one 16-bit word.
;
; Caution: $1F9C0-$1FFFF is write-only. Reading an entry returns the
; last value the host wrote there, not what the hardware holds -- fine
; for reading back your own writes, useless for discovering the state
; after a reset.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers. pal_set: index in r0, colour int in r2/r3.
; pal_load: src pointer in r0/r1, and the two char args land one per EVEN
; register -- first in r2, count in r4.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3
        zpage	r4

        global	_x16_pal_set
        global	_x16_pal_load

        section text

; ---------------------------------------------------------------------
; void x16_pal_set(__reg("r0") unsigned char index,
;                  __reg("r2/r3") unsigned int color)
;   pal_set wants X = index, A = colour low, Y = colour high.
; ---------------------------------------------------------------------
_x16_pal_set:
        ldx     r0                      ; X = index
        lda     r2                      ; A = colour low
        ldy     r3                      ; Y = colour high
        ; fall through

; pal_set
;   in:  X = palette index (0-255)
;        A = low byte  (Green<<4 | Blue)
;        Y = high byte (Red)
pal_set:
        sta     X16_T0                  ; colour low
        sty     X16_T1                  ; colour high

        lda     #VERA_CTRL_ADDRSEL
        trb     VERA_CTRL               ; ADDRSEL = 0 (leaves DCSEL alone)

        txa
        asl     a                       ; index * 2; carry = address bit 8
        tax
        lda     #>VRAM_PALETTE          ; $FA
        adc     #0                      ; carry from the asl rolls it to $FB
        stx     VERA_ADDR_L
        sta     VERA_ADDR_M
        lda     #(VERA_ADDR_H_BANK | (VERA_INC_1 << 4))   ; $1FA00 is in bank 1
        sta     VERA_ADDR_H

        lda     X16_T0
        sta     VERA_DATA0
        lda     X16_T1
        sta     VERA_DATA0
        rts

; ---------------------------------------------------------------------
; void x16_pal_load(__reg("r0/r1") const void *src, __reg("r2") unsigned char first,
;                   __reg("r4") unsigned char count)
;
; src is two bytes per entry, low byte first. count is 1-128; 0 loads
; nothing. pal_load wants X16_PTR0 = src, A = first, X = count.
; ---------------------------------------------------------------------
_x16_pal_load:
        lda     r0
        sta     X16_PTR0
        lda     r1
        sta     X16_PTR0+1
        lda     r2                      ; A = first index
        ldx     r4                      ; X = count
        ; fall through

; pal_load -- bulk-load palette entries from RAM.
;   in:  X16_PTR0 = source address (2 bytes per entry, low byte first)
;        A = first palette index
;        X = entry count (1-128; 0 loads nothing)
pal_load:
        cpx     #0                      ; count 0 loads nothing -- without
        beq     .done                   ; this guard the loop would run 256
        stx     X16_T2                  ; times and shred the whole palette

        tax                             ; X = first index
        lda     #VERA_CTRL_ADDRSEL
        trb     VERA_CTRL
        txa
        asl     a
        tax
        lda     #>VRAM_PALETTE
        adc     #0
        stx     VERA_ADDR_L
        sta     VERA_ADDR_M
        lda     #(VERA_ADDR_H_BANK | (VERA_INC_1 << 4))
        sta     VERA_ADDR_H

        ldy     #0
.loop:
        lda     (X16_PTR0),y
        sta     VERA_DATA0              ; low byte
        iny
        lda     (X16_PTR0),y
        sta     VERA_DATA0              ; high byte
        iny
        dec     X16_T2
        bne     .loop
.done:
        rts
