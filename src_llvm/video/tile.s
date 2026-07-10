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

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos: integer argument bytes fill A, then X, then __rc2, __rc3, ...
; A 16-bit return comes back in A (low) and X (high), which is what
; tile_get already produces.

        .globl  x16_layer_on
        .globl  x16_layer_off
        .globl  x16_layer_set_config
        .globl  x16_layer_set_mapbase
        .globl  x16_layer_set_tilebase
        .globl  x16_layer_scroll_x
        .globl  x16_layer_scroll_y
        .globl  x16_tile_setptr
        .globl  x16_tile_put
        .globl  x16_tile_get

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_layer_on(unsigned char layer)
; void __fastcall__ x16_layer_off(unsigned char layer)
;   Single argument, already in A: no shim.
; ---------------------------------------------------------------------
x16_layer_on:
layer_on:
        tax
        vera_dcsel 0
        lda     #VERA_VIDEO_LAYER0_EN
        cpx     #0
        beq     .Llayer_on_go
        lda     #VERA_VIDEO_LAYER1_EN
.Llayer_on_go:
        tsb     VERA_DC_VIDEO
        rts

x16_layer_off:
layer_off:
        tax
        vera_dcsel 0
        lda     #VERA_VIDEO_LAYER0_EN
        cpx     #0
        beq     .Llayer_off_go
        lda     #VERA_VIDEO_LAYER1_EN
.Llayer_off_go:
        trb     VERA_DC_VIDEO
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_layer_set_config  (unsigned char layer, unsigned char config)
; void __fastcall__ x16_layer_set_mapbase (unsigned char layer, unsigned char mapbase)
; void __fastcall__ x16_layer_set_tilebase(unsigned char layer, unsigned char tilebase)
; ---------------------------------------------------------------------
x16_layer_set_config:
        jsr     layer_marshal
        jmp     layer_set_config

x16_layer_set_mapbase:
        jsr     layer_marshal
        jmp     layer_set_mapbase

x16_layer_set_tilebase:
        jsr     layer_marshal
        jmp     layer_set_tilebase

; in:  A = layer, X = value
; out: A = value, X = layer
;
; cc65 needed a pop here; llvm-mos already has both bytes in registers, so
; this is a three-instruction transpose.
layer_marshal:
        pha                             ; layer
        txa                             ; A = value
        plx                             ; X = layer
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_layer_scroll_x(unsigned char layer, unsigned int value)
; void __fastcall__ x16_layer_scroll_y(unsigned char layer, unsigned int value)
; ---------------------------------------------------------------------
x16_layer_scroll_x:
        jsr     scroll_marshal
        jmp     layer_scroll_x

x16_layer_scroll_y:
        jsr     scroll_marshal
        jmp     layer_scroll_y

; in:  A = layer, X = value lo, __rc2 = value hi
; out: X = layer, X16_P0/P1 = value
scroll_marshal:
        pha                             ; layer
        stx     X16_P0                  ; value lo
        lda     __rc2
        sta     X16_P1                  ; value hi
        plx                             ; X = layer
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_tile_setptr(unsigned char col, unsigned char row)
;
; popa clobbers Y and tile_setptr owns all eight T registers, so `row`
; rides the hardware stack across the pop.
; ---------------------------------------------------------------------
; A = col, X = row; tile_setptr wants X = col, Y = row.
x16_tile_setptr:
        phx                             ; row
        tax                             ; X = col
        ply                             ; Y = row
        jmp     tile_setptr

; ---------------------------------------------------------------------
; void __fastcall__ x16_tile_put(unsigned char col, unsigned char row,
;                                unsigned char code, unsigned char attr)
; ---------------------------------------------------------------------
; A = col, X = row, __rc2 = code, __rc3 = attr.
x16_tile_put:
        pha                             ; col
        phx                             ; row
        lda     __rc2
        sta     X16_P0                  ; code
        lda     __rc3
        sta     X16_P1                  ; attr
        ply                             ; Y = row
        plx                             ; X = col
        jmp     tile_put

; ---------------------------------------------------------------------
; unsigned int __fastcall__ x16_tile_get(unsigned char col, unsigned char row)
;
; The internal routine already returns the screen code in A and the
; attribute in X, which is exactly cc65's 16-bit return: the result reads
; as code | attr<<8. See X16_TILE_CODE / X16_TILE_ATTR in the header.
; ---------------------------------------------------------------------
; A = col, X = row. tile_get returns A = code, X = attr, and llvm-mos reads
; a 16-bit return as A (low) | X (high) -- exactly cc65's convention, so
; the result still reads as code | attr<<8.
x16_tile_get:
        phx                             ; row
        tax                             ; X = col
        ply                             ; Y = row
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
        beq     .Llayer_index_zero
        ldx     #(VERA_L1_CONFIG - VERA_L0_CONFIG)
        rts
.Llayer_index_zero:
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
.Ltile_setptr_shift:
        asl     X16_T0
        rol     X16_T1
        rol     X16_T2
        dex
        bne     .Ltile_setptr_shift

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
