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

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos passes argument bytes left to right in A, then X, then __rc2,
; __rc3, ... There is no software stack and nothing is popped.

        .globl  x16_pal_set
        .globl  x16_pal_load

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void x16_pal_set(unsigned char index, unsigned int color)
;
; A = index, X = colour lo, __rc2 = colour hi. pal_set wants X = index,
; A = colour lo, Y = colour hi -- so A and X trade places while the high
; byte comes out of __rc2. Load Y first: __rc2 is r0L, and nothing here
; disturbs it, but reading it before the A/X shuffle keeps the dependency
; obvious.
; ---------------------------------------------------------------------
x16_pal_set:
        ldy     __rc2                   ; Y = colour hi
        pha                             ; index
        txa                             ; A = colour lo
        plx                             ; X = index
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
; void x16_pal_load(const void *src, unsigned char first,
;                   unsigned char count)
;
; src is two bytes per entry, low byte first. count is 1-128; 0 loads
; nothing.
; ---------------------------------------------------------------------
; A POINTER DOES NOT ARRIVE IN A/X. llvm-mos passes integer bytes in A, X,
; __rc2, ... but gives every pointer argument a whole __rc pair, because
; only zero page can be indirected through. So here:
;
;       src   -> __rc2/__rc3      (the first pointer gets the first pair)
;       first -> A                (the byte-class arguments still get A, X)
;       count -> X
;
; pal_load wants X16_PTR0 = src, A = first, X = count. X is already right;
; A has to survive the pointer copy, hence the push.
x16_pal_load:
        pha                             ; first
        lda     __rc2
        sta     X16_PTR0                ; src lo
        lda     __rc3
        sta     X16_PTR0+1              ; src hi
        pla                             ; A = first; X = count, untouched
        ; fall through

; pal_load -- bulk-load palette entries from RAM.
;   in:  X16_PTR0 = source address (2 bytes per entry, low byte first)
;        A = first palette index
;        X = entry count (1-128; 0 loads nothing)
pal_load:
        cpx     #0                      ; count 0 loads nothing -- without
        beq     .Lpal_load_done                   ; this guard the loop would run 256
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
.Lpal_load_loop:
        lda     (X16_PTR0),y
        sta     VERA_DATA0              ; low byte
        iny
        lda     (X16_PTR0),y
        sta     VERA_DATA0              ; high byte
        iny
        dec     X16_T2
        bne     .Lpal_load_loop
.Lpal_load_done:
        rts
