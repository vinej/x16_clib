; =====================================================================
; x16clib :: gfx/bitmap4l.s -- 320x240x16 bitmap drawing (4bpp)
; =====================================================================
; The framebuffer is 4bpp at VRAM $00000: 2 pixels per byte packed
; MSB-first, rows of 160 bytes, 38,400 bytes in all. A pixel byte is
; at y*160 + (x>>1); the left pixel is the high nibble. Colours are
; 0-15; x16_gfx4l_init() loads a default 16-entry palette and programs
; the mode on bare VERA registers (no KERNAL screen mode exists).
;
; x16_gfx4l_pset()/read() clip. The span/rect/line/blit primitives do
; NOT: they assume their arguments are on screen (the 8bpp module's
; policy).
;
; The routine bodies are the x16_library bitmap4l module, byte for
; byte; only the C shims in front are new. Every shim pops straight
; into the parameter block.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa, popax
        .import         vera_fill

        .export         _x16_gfx4l_init
        .export         _x16_gfx4l_clear
        .export         _x16_gfx4l_setptr
        .export         _x16_gfx4l_pset
        .export         _x16_gfx4l_read
        .export         _x16_gfx4l_hline
        .export         _x16_gfx4l_vline
        .export         _x16_gfx4l_rect
        .export         _x16_gfx4l_frame
        .export         _x16_gfx4l_line
        .export         _x16_gfx4l_char
        .export         _x16_gfx4l_text
        .export         _x16_gfx4l_pattern_set
        .export         _x16_gfx4l_pattern_rect
        .export         _x16_gfx4l_blit
        .export         _x16_gfx4l_blitm

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; void x16_gfx4l_init(void)
_x16_gfx4l_init:
        jmp     gfx4l_init

; void __fastcall__ x16_gfx4l_clear(unsigned char color)
_x16_gfx4l_clear:
        jmp     gfx4l_clear

; void __fastcall__ x16_gfx4l_setptr(unsigned char inc,
;                                   unsigned int x, unsigned char y)
_x16_gfx4l_setptr:
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jsr     popa                    ; increment index
        jmp     gfx4l_setptr

; void __fastcall__ x16_gfx4l_pset(unsigned int x, unsigned char y,
;                                 unsigned char color)
_x16_gfx4l_pset:
        sta     X16_P3                  ; color
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx4l_pset

; unsigned char __fastcall__ x16_gfx4l_read(unsigned int x, unsigned char y)
;   0-15, or $FF off screen
_x16_gfx4l_read:
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jsr     gfx4l_read
        ldx     #0
        bcc     @on
        lda     #$FF
@on:    rts

; void __fastcall__ x16_gfx4l_hline(unsigned int x, unsigned char y,
;                                  unsigned int len, unsigned char color)
_x16_gfx4l_hline:
        sta     X16_P3                  ; color
        jsr     popax
        sta     X16_P4                  ; len
        stx     X16_P5
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx4l_hline

; void __fastcall__ x16_gfx4l_vline(unsigned int x, unsigned char y,
;                                  unsigned char len, unsigned char color)
_x16_gfx4l_vline:
        sta     X16_P3                  ; color
        jsr     popa
        sta     X16_P4                  ; len
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx4l_vline

; void __fastcall__ x16_gfx4l_rect(unsigned int x, unsigned char y,
;                                 unsigned int w, unsigned char h,
;                                 unsigned char color)
_x16_gfx4l_rect:
        sta     X16_P3                  ; color
        jsr     popa
        sta     X16_P6                  ; h
        jsr     popax
        sta     X16_P4                  ; w
        stx     X16_P5
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx4l_rect

; void __fastcall__ x16_gfx4l_frame(unsigned int x, unsigned char y,
;                                  unsigned int w, unsigned char h,
;                                  unsigned char color)
_x16_gfx4l_frame:
        sta     X16_P3                  ; color
        jsr     popa
        sta     X16_P6                  ; h
        jsr     popax
        sta     X16_P4                  ; w
        stx     X16_P5
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx4l_frame

; void __fastcall__ x16_gfx4l_line(unsigned int x0, unsigned char y0,
;                                 unsigned int x1, unsigned char y1,
;                                 unsigned char color)
;   the engine takes x1 in P3/P4, y1 in P5, colour in P6
_x16_gfx4l_line:
        sta     X16_P6                  ; color
        jsr     popa
        sta     X16_P5                  ; y1
        jsr     popax
        sta     X16_P3                  ; x1
        stx     X16_P4
        jsr     popa
        sta     X16_P2                  ; y0
        jsr     popax
        sta     X16_P0                  ; x0
        stx     X16_P1
        jmp     gfx4l_line

; void __fastcall__ x16_gfx4l_char(unsigned int x, unsigned char y,
;                                 unsigned char color, unsigned char code)
;   code is a SCREEN code; the glyph comes from the VERA charset
_x16_gfx4l_char:
        sta     g4sl_c                  ; code
        jsr     popa
        sta     X16_P3                  ; color
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        lda     g4sl_c
        jmp     gfx4l_char

; void __fastcall__ x16_gfx4l_text(unsigned int x, unsigned char y,
;                                 unsigned char color, const char *s)
_x16_gfx4l_text:
        sta     g4sl_c                  ; s, low
        stx     g4sl_c2                 ; s, high
        jsr     popa
        sta     X16_P3                  ; color
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        lda     g4sl_c
        ldx     g4sl_c2
        jmp     gfx4l_text

; void __fastcall__ x16_gfx4l_pattern_set(const unsigned char *pattern,
;                                        unsigned char bg, unsigned char fg)
_x16_gfx4l_pattern_set:
        sta     X16_P5                  ; fg
        jsr     popa
        sta     X16_P4                  ; bg
        jsr     popax                   ; pattern -> A/X
        jmp     gfx4l_pattern_set

; void __fastcall__ x16_gfx4l_pattern_rect(unsigned int x, unsigned char y,
;                                         unsigned int w, unsigned char h)
_x16_gfx4l_pattern_rect:
        sta     X16_P6                  ; h
        jsr     popax
        sta     X16_P4                  ; w
        stx     X16_P5
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx4l_pattern_rect

; void __fastcall__ x16_gfx4l_blit(unsigned int x, unsigned char y,
;                                 unsigned char w, unsigned char h,
;                                 const unsigned char *src,
;                                 unsigned char op)
;   w is in PIXELS (1-255); src is row-major, one byte per pixel pair
_x16_gfx4l_blit:
        sta     g4sl_c                  ; op
        jsr     popax
        sta     X16_P6                  ; src
        stx     X16_P7
        jsr     popa
        sta     X16_P5                  ; h
        jsr     popa
        sta     X16_P4                  ; w
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        lda     g4sl_c
        jmp     gfx4l_blit

; void __fastcall__ x16_gfx4l_blitm(unsigned int x, unsigned char y,
;                                  unsigned char w, unsigned char h,
;                                  const unsigned char *src)
;   colour 0 is transparent
_x16_gfx4l_blitm:
        sta     X16_P6                  ; src
        stx     X16_P7
        jsr     popa
        sta     X16_P5                  ; h
        jsr     popa
        sta     X16_P4                  ; w
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx4l_blitm

g4sl_c:  .byte  0
g4sl_c2: .byte  0

; =====================================================================
; the x16_library bitmap4l module, verbatim
; =====================================================================
; =====================================================================
; x16lib :: gfx/bitmap4l.asm -- 320x240x16 bitmap drawing
; =====================================================================
; This file EMITS CODE. Source it exactly once (x16_code.asm does).
; Requires X16_USE_VERA (uses vera_fill).
;
; The framebuffer is 4bpp at VRAM $00000: 2 pixels per byte packed
; MSB-first (the leftmost pixel is bits 7:4), rows of 160 bytes,
; 38,400 bytes in all. A pixel byte is at y*160 + (x>>1); the left
; pixel is the high nibble, the right pixel is the low nibble.
;
; gfx4l_pset and gfx4l_read clip. The span/rect/line primitives do NOT:
; they assume their arguments are on screen. Clipping every span would
; cost more than it saves for a caller that already knows its geometry.
;
; Nothing here changes the screen mode. Call gfx4l_init once to switch
; the display to 320x240@4bpp; the drawing routines only touch VRAM.
; =====================================================================

; (zone: file scope in ca65)

GFX4L_WIDTH  = 320
GFX4L_HEIGHT = 240
GFX4L_STRIDE = 160

; ---------------------------------------------------------------------
; gfx4l_init -- program 320x240@4bpp on bare VERA registers.
; ---------------------------------------------------------------------
gfx4l_init:
    vera_dcsel 0
    lda #$80
    sta VERA_DC_HSCALE
    sta VERA_DC_VSCALE
    stz VERA_DC_BORDER

    lda #(VERA_LAYER_BITMAP | VERA_LAYER_BPP_4)
    sta VERA_L0_CONFIG
    lda #$01
    sta VERA_L0_TILEBASE
    stz VERA_L0_HSCROLL_L
    stz VERA_L0_HSCROLL_H
    stz VERA_L0_VSCROLL_L
    stz VERA_L0_VSCROLL_H

    ; Default palette entries 0-15: a simple grayscale ramp.
    vera_addr 0, VRAM_PALETTE, VERA_INC_1
    ldx #0
@pal:
    lda bitmap4l_defpal,x
    sta VERA_DATA0
    inx
    cpx #32
    bne @pal

    lda #VERA_VIDEO_LAYER1_EN   ; layer 1 off, layer 0 on
    trb VERA_DC_VIDEO
    lda #VERA_VIDEO_LAYER0_EN
    tsb VERA_DC_VIDEO
    rts

; ---------------------------------------------------------------------
; gfx4l_clear -- fill the whole framebuffer with one colour
;   in:  A = colour (0-15)
; ---------------------------------------------------------------------
gfx4l_clear:
    and #$0F
    tay
    lda bitmap4l_colbyte,y
    pha
    vera_addr 0, VRAM_BITMAP, VERA_INC_1
    pla
    ldx #<(GFX4L_STRIDE * GFX4L_HEIGHT)
    ldy #>(GFX4L_STRIDE * GFX4L_HEIGHT)
    jmp vera_fill

; ---------------------------------------------------------------------
; gfx4l_setptr -- point data port 0 at the byte holding pixel (x,y)
;   in:  A = increment index (VERA_INC_*)
;        X16_P0/P1 = x, X16_P2 = y
; ---------------------------------------------------------------------
gfx4l_setptr:
    asl
    asl
    asl
    asl
    sta X16_T5                  ; increment field, pre-shifted

    lda X16_P2                  ; y << 5
    stz X16_T3
    asl
    rol X16_T3
    asl
    rol X16_T3
    asl
    rol X16_T3
    asl
    rol X16_T3
    asl
    rol X16_T3
    sta X16_T4                  ; T4:T3 = y*32

    lda X16_T4                  ; T2:T1:T0 = y*128
    sta X16_T0
    lda X16_T3
    sta X16_T1
    stz X16_T2
    asl X16_T0
    rol X16_T1
    rol X16_T2
    asl X16_T0
    rol X16_T1
    rol X16_T2

    clc                         ; y*160 = y*32 + y*128
    lda X16_T4
    adc X16_T0
    sta X16_T0
    lda X16_T3
    adc X16_T1
    sta X16_T1
    lda #0
    adc X16_T2
    sta X16_T2

    lda X16_P1                  ; + x >> 1
    lsr
    sta X16_T4
    lda X16_P0
    ror
    sta X16_T3
    clc
    lda X16_T0
    adc X16_T3
    sta X16_T0
    lda X16_T1
    adc X16_T4
    sta X16_T1
    lda X16_T2
    adc #0
    sta X16_T2

    lda #VERA_CTRL_ADDRSEL
    trb VERA_CTRL
    lda X16_T0
    sta VERA_ADDR_L
    lda X16_T1
    sta VERA_ADDR_M
    lda X16_T2
    and #VERA_ADDR_H_BANK
    ora X16_T5
    sta VERA_ADDR_H
    rts

; ---------------------------------------------------------------------
; gfx4l_pset -- set one pixel, clipped
;   in:  X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour
; ---------------------------------------------------------------------
gfx4l_pset:
    lda X16_P2
    cmp #GFX4L_HEIGHT
    bcs @off

    lda X16_P1
    beq @on
    cmp #1
    bne @off
    lda X16_P0
    cmp #<GFX4L_WIDTH
    bcs @off
@on:
    lda #VERA_INC_0
    jsr gfx4l_setptr
    lda VERA_DATA0
    sta g4l_t
    lda X16_P0
    and #1
    beq @even
    lda g4l_t
    and #$F0
    sta g4l_t
    lda X16_P3
    and #$0F
    ora g4l_t
    sta VERA_DATA0
    rts
@even:
    lda g4l_t
    and #$0F
    sta g4l_t
    lda X16_P3
    and #$0F
    asl
    asl
    asl
    asl
    ora g4l_t
    sta VERA_DATA0
    rts
@off:
    rts

; ---------------------------------------------------------------------
; gfx4l_read -- read one pixel
;   in:  X16_P0/P1 = x, X16_P2 = y
;   out: A = the colour
; ---------------------------------------------------------------------
gfx4l_read:
    lda X16_P2
    cmp #GFX4L_HEIGHT
    bcs @off

    lda X16_P1
    beq @on
    cmp #1
    bne @off
    lda X16_P0
    cmp #<GFX4L_WIDTH
    bcs @off
@on:
    lda #VERA_INC_0
    jsr gfx4l_setptr
    lda VERA_DATA0
    sta g4l_t
    lda X16_P0
    and #1
    beq @even
    lda g4l_t
    and #$0F
    rts
@even:
    lda g4l_t
    and #$F0
    lsr
    lsr
    lsr
    lsr
    rts
@off:
    rts

; ---------------------------------------------------------------------
; gfx4l_hline -- horizontal span (no clipping)
;   in:  X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour,
;        X16_P4/P5 = length in pixels
; ---------------------------------------------------------------------
gfx4l_hline:
    lda X16_P4
    sta g4l_n
    lda X16_P5
    sta g4l_n+1
    ora g4l_n
    beq @done
@gh4l_loop:
    jsr gfx4l_pset
    inc X16_P0
    bne @nextx
    inc X16_P1
@nextx:
    sec
    lda g4l_n
    sbc #1
    sta g4l_n
    lda g4l_n+1
    sbc #0
    sta g4l_n+1
    ora g4l_n
    bne @gh4l_loop
@done:
    rts

; ---------------------------------------------------------------------
; gfx4l_vline -- vertical span (no clipping)
;   in:  X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour,
;        X16_P4 = length (1-255)
; ---------------------------------------------------------------------
gfx4l_vline:
    lda X16_P4
    beq @done
@gv4l_loop:
    jsr gfx4l_pset
    inc X16_P2
    dec X16_P4
    bne @gv4l_loop
@done:
    rts

; ---------------------------------------------------------------------
; gfx4l_rect -- filled rectangle
;   in:  X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour,
;        X16_P4/P5 = width, X16_P6 = height
; ---------------------------------------------------------------------
gfx4l_rect:
@row:
    lda X16_P6
    beq @done
    jsr gfx4l_hline             ; advances P0/P1 by the width -- reset it,
    sec                        ; or every row starts where the last ended
    lda X16_P0                 ; (a staircase instead of a filled rect)
    sbc X16_P4
    sta X16_P0
    lda X16_P1
    sbc X16_P5
    sta X16_P1
    inc X16_P2
    dec X16_P6
    bra @row
@done:
    rts

; ---------------------------------------------------------------------
; gfx4l_frame -- rectangle outline
;   same arguments as gfx4l_rect
; ---------------------------------------------------------------------
gfx4l_frame:
    ldx #6
@gf4l_take:
    lda X16_P0,x
    sta gb4l_x,x
    dex
    bpl @gf4l_take

    jsr bitmap4l_gf4l_restore_span
    jsr gfx4l_hline

    jsr bitmap4l_gf4l_restore_span
    clc
    lda gb4l_y
    adc gb4l_h
    sec
    sbc #1
    sta X16_P2
    jsr gfx4l_hline

    jsr bitmap4l_gf4l_restore_col
    jsr gfx4l_vline

    jsr bitmap4l_gf4l_restore_col
    clc
    lda gb4l_x
    adc gb4l_w
    sta X16_P0
    lda gb4l_x+1
    adc gb4l_w+1
    sta X16_P1
    lda X16_P0
    bne @gf4l_no_borrow
    dec X16_P1
@gf4l_no_borrow:
    dec X16_P0
    jsr gfx4l_vline
    rts

bitmap4l_gf4l_restore_span:
    ldx #5
bitmap4l_gf4l_rsp_l:
    lda gb4l_x,x
    sta X16_P0,x
    dex
    bpl bitmap4l_gf4l_rsp_l
    rts

bitmap4l_gf4l_restore_col:
    ldx #3
bitmap4l_gf4l_rcl_l:
    lda gb4l_x,x
    sta X16_P0,x
    dex
    bpl bitmap4l_gf4l_rcl_l
    lda gb4l_h
    sta X16_P4
    rts

; ---------------------------------------------------------------------
; gfx4l_blit -- rows of pixels from RAM to the framebuffer
;   in:  A = raster op: 0 copy, 1 OR, 2 AND, 3 XOR
;        X16_P0/P1 = x, X16_P2 = y, X16_P4 = width in pixels (1-255),
;        X16_P5 = height in rows, X16_P6/P7 = source (row-major)
; ---------------------------------------------------------------------
gfx4l_blit:
    and #3
    sta g4l_op
    lda X16_P4
    sta g4l_w
    lda X16_P5
    sta g4l_h
    lda X16_P6
    sta g4l_src
    lda X16_P7
    sta g4l_src+1
    lda g4l_w
    bne @gb4l_nonzero
    rts

@gb4l_nonzero:
    clc
    lda g4l_w
    adc #1
    lsr
    sta g4l_rowbytes

    lda X16_P0
    sta g4l_x0
    lda X16_P1
    sta g4l_x0+1
    lda X16_P2
    sta g4l_y0

@gb4l_row:
    lda g4l_x0
    sta X16_P0
    lda g4l_x0+1
    sta X16_P1
    lda g4l_y0
    sta X16_P2
    lda g4l_src
    sta X16_PTR3
    lda g4l_src+1
    sta X16_PTR3+1
    lda #0
    sta g4l_phase
    lda g4l_w
    sta g4l_n
@gb4l_col:
    ldy #0
    lda (X16_PTR3),y
    sta g4l_ink
    lda g4l_phase
    beq @gb4l_bit_even
    lda g4l_ink
    and #$0F
    bra @gb4l_bit_done
@gb4l_bit_even:
    lda g4l_ink
    and #$F0
    lsr
    lsr
    lsr
    lsr
@gb4l_bit_done:
    sta g4l_ink
    jsr gfx4l_read
    sta g4l_t
    lda g4l_op
    beq @gb4l_copy
    cmp #1
    beq @gb4l_or
    cmp #2
    beq @gb4l_and
@gb4l_xor:
    lda g4l_ink
    eor g4l_t
    bra @gb4l_store
@gb4l_and:
    lda g4l_ink
    and g4l_t
    bra @gb4l_store
@gb4l_or:
    lda g4l_ink
    ora g4l_t
@gb4l_store:
    sta X16_P3
    jsr gfx4l_pset
    bra @gb4l_next_x
@gb4l_copy:
    lda g4l_ink
    sta X16_P3
    jsr gfx4l_pset
    bra @gb4l_next_x
@gb4l_next_x:
    inc X16_P0
    bne @gb4l_carry1
    inc X16_P1
@gb4l_carry1:
    lda g4l_phase
    and #1
    beq @gb4l_flip_phase
    inc X16_PTR3
    bne @gb4l_flip_phase
    inc X16_PTR3+1
@gb4l_flip_phase:
    eor #1
    sta g4l_phase
    dec g4l_n
    beq @gb4l_end_row
    jmp @gb4l_col
@gb4l_end_row:
    lda g4l_src
    clc
    adc g4l_rowbytes
    sta g4l_src
    lda g4l_src+1
    adc #0
    sta g4l_src+1
    lda g4l_y0
    inc a
    sta g4l_y0
    dec g4l_h
    beq @gb4l_done
    jmp @gb4l_row
@gb4l_done:
    rts

; ---------------------------------------------------------------------
; gfx4l_blitm -- a masked blit: colour 0 is transparent
;   in:  X16_P0/P1 = x, X16_P2 = y, X16_P4 = width (1-255),
;        X16_P5 = height, X16_P6/P7 = source (row-major)
; ---------------------------------------------------------------------
gfx4l_blitm:
    lda X16_P4
    sta g4l_w
    lda X16_P5
    sta g4l_h
    lda X16_P6
    sta g4l_src
    lda X16_P7
    sta g4l_src+1
    lda g4l_w
    bne @gm4l_nonzero
    rts

@gm4l_nonzero:
    clc
    lda g4l_w
    adc #1
    lsr
    sta g4l_rowbytes

    lda X16_P0
    sta g4l_x0
    lda X16_P1
    sta g4l_x0+1
    lda X16_P2
    sta g4l_y0

@gm4l_row:
    lda g4l_x0
    sta X16_P0
    lda g4l_x0+1
    sta X16_P1
    lda g4l_y0
    sta X16_P2
    lda g4l_src
    sta X16_PTR3
    lda g4l_src+1
    sta X16_PTR3+1
    lda #0
    sta g4l_phase
    lda g4l_w
    sta g4l_n
@gm4l_col:
    ldy #0
    lda (X16_PTR3),y
    sta g4l_ink
    lda g4l_phase
    beq @gm4l_px_even
    lda g4l_ink
    and #$0F
    bra @gm4l_px_done
@gm4l_px_even:
    lda g4l_ink
    and #$F0
    lsr
    lsr
    lsr
    lsr
@gm4l_px_done:
    beq @gm4l_skip
    sta X16_P3
    jsr gfx4l_pset
@gm4l_skip:
    inc X16_P0
    bne @gm4l_carry1
    inc X16_P1
@gm4l_carry1:
    lda g4l_phase
    and #1
    beq @gm4l_flip_phase
    inc X16_PTR3
    bne @gm4l_flip_phase
    inc X16_PTR3+1
@gm4l_flip_phase:
    eor #1
    sta g4l_phase
    dec g4l_n
    bne @gm4l_col
    lda g4l_src
    clc
    adc g4l_rowbytes
    sta g4l_src
    lda g4l_src+1
    adc #0
    sta g4l_src+1
    lda g4l_y0
    inc a
    sta g4l_y0
    dec g4l_h
    beq @gm4l_done
    jmp @gm4l_row
@gm4l_done:
    rts

; ---------------------------------------------------------------------
; gfx4l_pattern_set -- cache an 8x8 1bpp pattern for gfx4l_pattern_rect
;   in:  A = pattern low, X = pattern high
;        X16_P4 = background colour, X16_P5 = foreground colour
; ---------------------------------------------------------------------
gfx4l_pattern_set:
    sta X16_T0
    stx X16_T0+1
    ldy #7
bitmap4l_gp4l_copy:
    lda (X16_T0),y
    sta gp4l_pat,y
    dey
    bpl bitmap4l_gp4l_copy
    lda X16_P4
    and #$0F
    sta gp4l_bg
    lda X16_P5
    and #$0F
    sta gp4l_fg
    rts

; ---------------------------------------------------------------------
; gfx4l_pattern_rect -- fill a rectangle with the cached pattern
;   in:  X16_P0/P1 = x, X16_P2 = y, X16_P4/P5 = width, X16_P6 = height
; ---------------------------------------------------------------------
gfx4l_pattern_rect:
    lda X16_P4
    ora X16_P5
    bne :+
    rts
:
    lda X16_P6
    bne :+
    rts
:
    lda X16_P0
    and #7
    sta gp4l_rot
    lda X16_P0
    sta gp4l_bx
    lda X16_P1
    sta gp4l_bx+1
    lda X16_P2
    sta gp4l_by
bitmap4l_gp4l_row:
    lda X16_P2
    and #7
    tay
    lda gp4l_pat,y
    sta gp4l_cur
    lda gp4l_rot
    beq bitmap4l_gp4l_rot_ok
    tay
    lda gp4l_cur
bitmap4l_gp4l_rot:
    asl
    adc #0
    dey
    bne bitmap4l_gp4l_rot
    sta gp4l_cur
bitmap4l_gp4l_rot_ok:
    lda X16_P0
    sta g4l_x0
    lda X16_P1
    sta g4l_x0+1
    lda X16_P4
    sta g4l_n
    lda X16_P5
    sta g4l_n+1
bitmap4l_gp4l_col:
    lda gp4l_cur
    bmi bitmap4l_gp4l_use_fg
    lda gp4l_bg
    bra bitmap4l_gp4l_out
bitmap4l_gp4l_use_fg:
    lda gp4l_fg
bitmap4l_gp4l_out:
    sta X16_P3
    lda g4l_x0
    sta X16_P0
    lda g4l_x0+1
    sta X16_P1
    lda X16_P2
    sta X16_P2
    jsr gfx4l_pset
    inc g4l_x0
    bne bitmap4l_gp4l_tail
    inc g4l_x0+1
bitmap4l_gp4l_tail:
    lda gp4l_cur
    asl
    adc #0
    sta gp4l_cur
    lda g4l_n
    bne :+
    dec g4l_n+1
:	dec g4l_n
    lda g4l_n
    ora g4l_n+1
    bne bitmap4l_gp4l_col
    lda gp4l_bx
    sta X16_P0
    lda gp4l_bx+1
    sta X16_P1
    inc X16_P2
    dec X16_P6
    beq bitmap4l_gp4l_done
    jmp bitmap4l_gp4l_row
bitmap4l_gp4l_done:
    rts

; ---------------------------------------------------------------------
; gfx4l_line -- Bresenham, any direction
;   in:  X16_P0/P1 = x0, X16_P2 = y0
;        X16_P4/P5 = x1, y1 in P6/P7?  (compatible with gfx4l_line macros)
;        X16_P6 = colour
; ---------------------------------------------------------------------
gfx4l_line:
    ldx #6
@gl4l_take:
    lda X16_P0,x
    sta gl4l_x0,x
    dex
    bpl @gl4l_take

    sec
    lda gl4l_x1
    sbc gl4l_x0
    sta gl4l_dx
    lda gl4l_x1+1
    sbc gl4l_x0+1
    sta gl4l_dx+1
    bpl @gl4l_dx_pos
    sec
    lda #0
    sbc gl4l_dx
    sta gl4l_dx
    lda #0
    sbc gl4l_dx+1
    sta gl4l_dx+1
    lda #$FF
    sta gl4l_sx
    sta gl4l_sx+1
    bra @gl4l_dx_done
@gl4l_dx_pos:
    lda #$01
    sta gl4l_sx
    stz gl4l_sx+1
@gl4l_dx_done:

    sec                         ; dy = -|y1 - y0|, sy = sign (y is 8-bit)
    lda gl4l_y1
    sbc gl4l_y0
    bpl @gl4l_dy_pos
    eor #$FF
    clc
    adc #1                      ; absolute value
    sta gl4l_ldy
    lda #$FF
    sta gl4l_sy
    bra @gl4l_dy_done
@gl4l_dy_pos:
    sta gl4l_ldy
    lda #$01
    sta gl4l_sy
@gl4l_dy_done:
    sec
    lda #0
    sbc gl4l_ldy
    sta gl4l_dy
    lda #0
    sbc #0
    sta gl4l_dy+1               ; gl4l_dy = -|dy|, 16-bit signed

    clc
    lda gl4l_dx
    adc gl4l_dy
    sta gl4l_err
    lda gl4l_dx+1
    adc gl4l_dy+1
    sta gl4l_err+1

@gl4l_loop:
    jsr bitmap4l_gl4l_plot
    lda gl4l_x0
    cmp gl4l_x1
    bne @gl4l_step
    lda gl4l_x0+1
    cmp gl4l_x1+1
    bne @gl4l_step
    lda gl4l_y0
    cmp gl4l_y1
    bne @gl4l_step
    rts

@gl4l_step:
    lda gl4l_err
    asl
    sta gl4l_e2
    lda gl4l_err+1
    rol
    sta gl4l_e2+1
    sec
    lda gl4l_e2
    sbc gl4l_dy
    lda gl4l_e2+1
    sbc gl4l_dy+1
    bvc @gl4l_nv1
    eor #$80
@gl4l_nv1:
    bmi @gl4l_skip_x
    clc
    lda gl4l_err
    adc gl4l_dy
    sta gl4l_err
    lda gl4l_err+1
    adc gl4l_dy+1
    sta gl4l_err+1
    clc
    lda gl4l_x0
    adc gl4l_sx
    sta gl4l_x0
    lda gl4l_x0+1
    adc gl4l_sx+1
    sta gl4l_x0+1
@gl4l_skip_x:
    sec
    lda gl4l_dx
    sbc gl4l_e2
    lda gl4l_dx+1
    sbc gl4l_e2+1
    bvc @gl4l_nv2
    eor #$80
@gl4l_nv2:
    bmi @gl4l_skip_y
    clc
    lda gl4l_err
    adc gl4l_dx
    sta gl4l_err
    lda gl4l_err+1
    adc gl4l_dx+1
    sta gl4l_err+1
    clc
    lda gl4l_y0
    adc gl4l_sy
    sta gl4l_y0
@gl4l_skip_y:
    jmp @gl4l_loop

bitmap4l_gl4l_plot:
    lda gl4l_x0
    sta X16_P0
    lda gl4l_x0+1
    sta X16_P1
    lda gl4l_y0
    sta X16_P2
    lda gl4l_color
    sta X16_P3
    jmp gfx4l_pset

; ---------------------------------------------------------------------
; gfx4l_char / gfx4l_text
; ---------------------------------------------------------------------
gfx4l_char:
    sta gt4l_code
    stz gt4l_hi
    asl
    rol gt4l_hi
    asl
    rol gt4l_hi
    asl
    rol gt4l_hi
    pha
    vera_addrsel 1
    pla
    sta VERA_ADDR_L
    lda gt4l_hi
    clc
    adc #<(VRAM_CHARSET >> 8)
    sta VERA_ADDR_M
    lda #(VERA_ADDR_H_BANK | (VERA_INC_1 << 4))
    sta VERA_ADDR_H
    ldx #0
@gt4l_fetch:
    lda VERA_DATA1
    sta gt4l_glyph,x
    inx
    cpx #8
    bne @gt4l_fetch
    vera_addrsel 0

    lda X16_P0
    sta gt4l_bx
    lda X16_P1
    sta gt4l_bx+1
    lda X16_P2
    sta gt4l_by
    stz gt4l_row
@gt4l_rows:
    ldx gt4l_row
    lda gt4l_glyph,x
    sta gt4l_bits
    beq @gt4l_next_row
    stz gt4l_col
@gt4l_cols:
    asl gt4l_bits
    bcc @gt4l_next_col
    clc
    lda gt4l_bx
    adc gt4l_col
    sta X16_P0
    lda gt4l_bx+1
    adc #0
    sta X16_P1
    clc
    lda gt4l_by
    adc gt4l_row
    bcs @gt4l_next_col
    sta X16_P2
    jsr gfx4l_pset
@gt4l_next_col:
    inc gt4l_col
    lda gt4l_col
    cmp #8
    bne @gt4l_cols
@gt4l_next_row:
    inc gt4l_row
    lda gt4l_row
    cmp #8
    bne @gt4l_rows
    lda gt4l_bx
    sta X16_P0
    lda gt4l_bx+1
    sta X16_P1
    lda gt4l_by
    sta X16_P2
    rts

gfx4l_text:
    sta bitmap4l_gt4l_lda+1
    stx bitmap4l_gt4l_lda+2
gtx4l_gt4l_loop:
bitmap4l_gt4l_lda:
    lda $FFFF
    beq gtx4l_gt4l_done
    bit #%01000000
    beq gtx4l_gt4l_code_ok
    and #$1F
gtx4l_gt4l_code_ok:
    jsr gfx4l_char
    clc
    lda X16_P0
    adc #8
    sta X16_P0
    lda X16_P1
    adc #0
    sta X16_P1
    inc bitmap4l_gt4l_lda+1
    bne gtx4l_gt4l_loop
    inc bitmap4l_gt4l_lda+2
    bra gtx4l_gt4l_loop
gtx4l_gt4l_done:
    rts

; ---------------------------------------------------------------------
; Data
; ---------------------------------------------------------------------
bitmap4l_defpal:
    .byte $FF, $0F, $AA, $0A, $55, $05, $00, $00
        .byte $F0, $00, $0F, $00, $F8, $08, $88, $00
        .byte $8F, $00, $0F, $0F, $F0, $0F, $FF, $00
        .byte $0F, $0F, $F0, $00, $99, $09, $66, $06

bitmap4l_colbyte:
    .byte $00, $11, $22, $33, $44, $55, $66, $77, $88, $99, $AA, $BB, $CC, $DD, $EE, $FF

gp4l_pat:.res 8, 0
gp4l_bg: .byte 0
gp4l_fg: .byte 0
gp4l_rot:.byte 0
gp4l_cur:.byte 0
gp4l_bx: .word 0
gp4l_by: .byte 0

g4l_t:   .byte 0

g4l_n:   .word 0
g4l_w:   .byte 0
g4l_h:   .byte 0

g4l_rowbytes:.byte 0

g4l_src:.word 0

g4l_x0:.word 0

g4l_y0:.byte 0

g4l_phase:.byte 0

g4l_op:.byte 0
g4l_ink:.byte 0

; Line helpers
gb4l_x:  .word 0
gb4l_y:  .byte 0
gb4l_c:  .byte 0
gb4l_w:  .word 0
gb4l_h:  .byte 0

gl4l_x0: .word 0
gl4l_y0: .byte 0
gl4l_x1: .word 0
gl4l_y1: .byte 0
gl4l_color:.byte 0
gl4l_dx: .word 0
gl4l_ldy:.word 0
gl4l_dy: .word 0
gl4l_err:.word 0
gl4l_e2: .word 0
gl4l_sx: .word 0
gl4l_sy: .byte 0

gt4l_code:.byte 0
gt4l_hi:  .byte 0
gt4l_glyph:.res 8, 0
gt4l_bx: .word 0
gt4l_by: .byte 0
gt4l_row:.byte 0
gt4l_col:.byte 0
gt4l_bits:.byte 0

; (end zone)
