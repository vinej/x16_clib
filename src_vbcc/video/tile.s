; =====================================================================
; x16clib :: video/tile.s -- tilemap cells and layer configuration
; =====================================================================
; The tile_* routines address layer 1, which in the default text modes is
; the text screen. They read L1_CONFIG and L1_MAPBASE at run time rather
; than assuming a screen mode, so they keep working after a mode change.
;
; The KERNAL's default text setup is L1_CONFIG = $60 (map 128x64, 1bpp)
; with MAPBASE = $D8, i.e. the map at $1B000. A cell is two bytes:
; screen code, then colour attribute (fg | bg<<4).
;
; tile_setptr leaves ADDRSEL = 0, so it is safe to call the KERNAL
; afterwards -- see the note at the top of video/screen.s.
;
; tile_setptr uses every one of X16_T0..T7, so the shims that reach it
; stage their arguments on the hardware stack instead.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers. layer/col/value chars ride one per even
; register (r0, r2, r4, r6); a 16-bit scroll value takes r2/r3.
        zpage	r0
        zpage	r2
        zpage	r3
        zpage	r4
        zpage	r6

        global	_x16_layer_on
        global	_x16_layer_off
        global	_x16_layer_set_config
        global	_x16_layer_set_mapbase
        global	_x16_layer_set_tilebase
        global	_x16_layer_scroll_x
        global	_x16_layer_scroll_y
        global	_x16_tile_setptr
        global	_x16_tile_put
        global	_x16_tile_get

        section text

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_layer_on(unsigned char layer)
; void __fastcall__ x16_layer_off(unsigned char layer)
;   Single argument, already in A: no shim.
; ---------------------------------------------------------------------
_x16_layer_on:
layer_on:
        tax
        vera_dcsel 0
        lda     #VERA_VIDEO_LAYER0_EN
        cpx     #0
        beq     .go
        lda     #VERA_VIDEO_LAYER1_EN
.go:
        tsb     VERA_DC_VIDEO
        rts

_x16_layer_off:
layer_off:
        tax
        vera_dcsel 0
        lda     #VERA_VIDEO_LAYER0_EN
        cpx     #0
        beq     .go
        lda     #VERA_VIDEO_LAYER1_EN
.go:
        trb     VERA_DC_VIDEO
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_layer_set_config  (unsigned char layer, unsigned char config)
; void __fastcall__ x16_layer_set_mapbase (unsigned char layer, unsigned char mapbase)
; void __fastcall__ x16_layer_set_tilebase(unsigned char layer, unsigned char tilebase)
; ---------------------------------------------------------------------
_x16_layer_set_config:
        jsr     layer_marshal
        jmp     layer_set_config

_x16_layer_set_mapbase:
        jsr     layer_marshal
        jmp     layer_set_mapbase

_x16_layer_set_tilebase:
        jsr     layer_marshal
        jmp     layer_set_tilebase

; layer in r0, value in r2.  out: A = value, X = layer
layer_marshal:
        ldx     r0                      ; X = layer
        lda     r2                      ; A = value
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_layer_scroll_x(unsigned char layer, unsigned int value)
; void __fastcall__ x16_layer_scroll_y(unsigned char layer, unsigned int value)
; ---------------------------------------------------------------------
_x16_layer_scroll_x:
        jsr     scroll_marshal
        jmp     layer_scroll_x

_x16_layer_scroll_y:
        jsr     scroll_marshal
        jmp     layer_scroll_y

; layer in r0, value int in r2/r3.  out: X = layer, P0/P1 = value
scroll_marshal:
        lda     r2
        sta     X16_P0                  ; value lo
        lda     r3
        sta     X16_P1                  ; value hi
        ldx     r0                      ; X = layer
        rts

; ---------------------------------------------------------------------
; void x16_tile_setptr(__reg("r0") unsigned char col, __reg("r2") unsigned char row)
;   tile_setptr wants X = col, Y = row.
; ---------------------------------------------------------------------
_x16_tile_setptr:
        ldx     r0                      ; X = col
        ldy     r2                      ; Y = row
        jmp     tile_setptr

; ---------------------------------------------------------------------
; void x16_tile_put(__reg("r0") unsigned char col, __reg("r2") unsigned char row,
;                   __reg("r4") unsigned char code, __reg("r6") unsigned char attr)
;   tile_put wants X = col, Y = row, P0 = code, P1 = attr.
; ---------------------------------------------------------------------
_x16_tile_put:
        lda     r4
        sta     X16_P0                  ; code
        lda     r6
        sta     X16_P1                  ; attr
        ldx     r0                      ; X = col
        ldy     r2                      ; Y = row
        jmp     tile_put

; ---------------------------------------------------------------------
; unsigned int x16_tile_get(__reg("r0") unsigned char col, __reg("r2") unsigned char row)
;
; tile_get returns the screen code in A and the attribute in X -- exactly
; vbcc's 16-bit return: code | attr<<8. tile_get wants X = col, Y = row.
; ---------------------------------------------------------------------
_x16_tile_get:
        ldx     r0                      ; X = col
        ldy     r2                      ; Y = row
        jmp     tile_get

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; layer_index -- turn a layer number into the register offset.
;   in:  X = layer (0 or 1)
;   out: X = 0 or 7   (L1_CONFIG is 7 bytes past L0_CONFIG)
;   Preserves A.
; ---------------------------------------------------------------------
layer_index:
        cpx     #0
        beq     .zero
        ldx     #(VERA_L1_CONFIG - VERA_L0_CONFIG)
        rts
.zero:
        ldx     #0
        rts

; ---------------------------------------------------------------------
; layer_set_config  -- in: X = layer, A = config byte
;   map height (7:6) | map width (5:4) | T256C (3) | bitmap (2) | bpp (1:0)
; layer_set_mapbase -- in: X = layer, A = VRAM address >> 9  (512-aligned)
; layer_set_tilebase-- in: X = layer, A = base>>11<<2 | tile size bits
; ---------------------------------------------------------------------
layer_set_config:
        pha
        jsr     layer_index
        pla
        sta     VERA_L0_CONFIG,x
        rts

layer_set_mapbase:
        pha
        jsr     layer_index
        pla
        sta     VERA_L0_MAPBASE,x
        rts

layer_set_tilebase:
        pha
        jsr     layer_index
        pla
        sta     VERA_L0_TILEBASE,x
        rts

; ---------------------------------------------------------------------
; layer_scroll_x / layer_scroll_y -- 12-bit hardware scroll
;   in:  X = layer, X16_P0/P1 = value (0-4095)
; ---------------------------------------------------------------------
layer_scroll_x:
        jsr     layer_index
        lda     X16_P0
        sta     VERA_L0_HSCROLL_L,x
        lda     X16_P1
        and     #$0F
        sta     VERA_L0_HSCROLL_H,x
        rts

layer_scroll_y:
        jsr     layer_index
        lda     X16_P0
        sta     VERA_L0_VSCROLL_L,x
        lda     X16_P1
        and     #$0F
        sta     VERA_L0_VSCROLL_H,x
        rts

; ---------------------------------------------------------------------
; tile_setptr -- point data port 0 at a layer-1 tilemap cell.
;   in:  X = column, Y = row
;
; address = (L1_MAPBASE << 9) + (row * mapwidth + col) * 2
;
; mapwidth is 32 << ((L1_CONFIG >> 4) & 3), always a power of two, so
; (row * mapwidth) * 2 is just row shifted left by 6 + that field. The
; product needs 17 bits, hence the three-byte shift.
; ---------------------------------------------------------------------
tile_setptr:
        stx     X16_T4                  ; column
        sty     X16_T5                  ; row

        lda     VERA_L1_CONFIG
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        and     #$03                    ; map width code 0..3
        clc
        adc     #6
        tax                             ; shift count 6..9

        stz     X16_T1
        stz     X16_T2
        lda     X16_T5
        sta     X16_T0
.shift:
        asl     X16_T0
        rol     X16_T1
        rol     X16_T2
        dex
        bne     .shift

        ; + column * 2  (up to 9 bits)
        lda     X16_T4
        asl     a
        sta     X16_T6
        lda     #0
        rol     a
        sta     X16_T7

        clc
        lda     X16_T0
        adc     X16_T6
        sta     X16_T0
        lda     X16_T1
        adc     X16_T7
        sta     X16_T1
        lda     X16_T2
        adc     #0
        sta     X16_T2

        ; + mapbase, which is (register << 9): low byte is always zero.
        lda     VERA_L1_MAPBASE
        asl     a                       ; carry = VRAM address bit 16
        sta     X16_T6
        lda     #0
        rol     a
        sta     X16_T7

        clc
        lda     X16_T1
        adc     X16_T6
        sta     X16_T1
        lda     X16_T2
        adc     X16_T7
        sta     X16_T2

        lda     #VERA_CTRL_ADDRSEL
        trb     VERA_CTRL               ; ADDRSEL = 0, DCSEL untouched
        lda     X16_T0
        sta     VERA_ADDR_L
        lda     X16_T1
        sta     VERA_ADDR_M
        lda     X16_T2
        and     #VERA_ADDR_H_BANK
        ora     #(VERA_INC_1 << 4)
        sta     VERA_ADDR_H
        rts

; ---------------------------------------------------------------------
; tile_put -- write one cell
;   in:  X = column, Y = row, X16_P0 = screen code, X16_P1 = attribute
; ---------------------------------------------------------------------
tile_put:
        jsr     tile_setptr
        lda     X16_P0
        sta     VERA_DATA0
        lda     X16_P1
        sta     VERA_DATA0
        rts

; ---------------------------------------------------------------------
; tile_get -- read one cell
;   in:  X = column, Y = row
;   out: A = screen code, X = attribute
; ---------------------------------------------------------------------
tile_get:
        jsr     tile_setptr
        lda     VERA_DATA0
        tay
        lda     VERA_DATA0
        tax
        tya
        rts
