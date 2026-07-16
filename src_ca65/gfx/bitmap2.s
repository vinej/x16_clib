; =====================================================================
; x16clib :: gfx/bitmap2.s -- 640x480x4 bitmap drawing (2bpp)
; =====================================================================
; The framebuffer is 2bpp at VRAM $00000: 4 pixels per byte packed
; MSB-first, rows of 160 bytes. A pixel byte is at y*160 + (x>>2).
; Colours are 0-3; x16_gfx2_init() loads white/gray/gray/black into
; palette entries 0-3 and programs the mode on bare VERA registers
; (no KERNAL screen mode exists for it).
;
; x16_gfx2_pset()/read() clip; the span/rect/line/blit primitives do
; NOT (the 8bpp module's policy). x16_gfx2_clear() runs through the
; VERA FX 32-bit cache and needs an FX-capable VERA (47.0.2+).
;
; The routine bodies are the x16_library gfx2 module, byte for byte;
; only the C shims in front are new. Every shim pops straight into the
; parameter block.
; =====================================================================

        .feature        labels_without_colons

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa, popax
        .import         vera_fill
        .import         fx_fill

        .export         _x16_gfx2_init
        .export         _x16_gfx2_clear
        .export         _x16_gfx2_setptr
        .export         _x16_gfx2_pset
        .export         _x16_gfx2_read
        .export         _x16_gfx2_hline
        .export         _x16_gfx2_vline
        .export         _x16_gfx2_rect
        .export         _x16_gfx2_frame
        .export         _x16_gfx2_line
        .export         _x16_gfx2_pattern_set
        .export         _x16_gfx2_pattern_rect
        .export         _x16_gfx2_blit
        .export         _x16_gfx2_blitm

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; void x16_gfx2_init(void)
_x16_gfx2_init:
        jmp     gfx2_init

; void __fastcall__ x16_gfx2_clear(unsigned char color)
_x16_gfx2_clear:
        jmp     gfx2_clear

; unsigned char __fastcall__ x16_gfx2_setptr(unsigned char inc,
;                                unsigned int x, unsigned int y)
_x16_gfx2_setptr:
        sta     X16_P2                  ; y
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jsr     popa                    ; increment index
        jsr     gfx2_setptr
        ldx     #0
        rts

; void __fastcall__ x16_gfx2_pset(unsigned int x, unsigned int y,
;                                 unsigned char color)
_x16_gfx2_pset:
        sta     g2sh_c
        jsr     popax
        sta     X16_P2                  ; y
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        lda     g2sh_c
        jmp     gfx2_pset

; unsigned char __fastcall__ x16_gfx2_read(unsigned int x, unsigned int y)
;   0-3, or $FF off screen
_x16_gfx2_read:
        sta     X16_P2                  ; y
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jsr     gfx2_read
        ldx     #0
        bcc     @on
        lda     #$FF
@on:    rts

; void __fastcall__ x16_gfx2_hline(unsigned int x, unsigned int y,
;                                  unsigned int len, unsigned char color)
_x16_gfx2_hline:
        sta     g2sh_c
        jsr     popax
        sta     X16_P4                  ; len
        stx     X16_P5
        jsr     popax
        sta     X16_P2                  ; y
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        lda     g2sh_c
        jmp     gfx2_hline

; void __fastcall__ x16_gfx2_vline(unsigned int x, unsigned int y,
;                                  unsigned int len, unsigned char color)
_x16_gfx2_vline:
        sta     g2sh_c
        jsr     popax
        sta     X16_P4                  ; len
        stx     X16_P5
        jsr     popax
        sta     X16_P2                  ; y
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        lda     g2sh_c
        jmp     gfx2_vline

; void __fastcall__ x16_gfx2_rect(unsigned int x, unsigned int y,
;                                 unsigned int w, unsigned int h,
;                                 unsigned char color)
_x16_gfx2_rect:
        sta     g2sh_c
        jsr     popax
        sta     X16_P6                  ; h
        stx     X16_P7
        jsr     popax
        sta     X16_P4                  ; w
        stx     X16_P5
        jsr     popax
        sta     X16_P2                  ; y
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        lda     g2sh_c
        jmp     gfx2_rect

; void __fastcall__ x16_gfx2_frame(unsigned int x, unsigned int y,
;                                  unsigned int w, unsigned int h,
;                                  unsigned char color)
_x16_gfx2_frame:
        sta     g2sh_c
        jsr     popax
        sta     X16_P6                  ; h
        stx     X16_P7
        jsr     popax
        sta     X16_P4                  ; w
        stx     X16_P5
        jsr     popax
        sta     X16_P2                  ; y
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        lda     g2sh_c
        jmp     gfx2_frame

; void __fastcall__ x16_gfx2_line(unsigned int x0, unsigned int y0,
;                                 unsigned int x1, unsigned int y1,
;                                 unsigned char color)
_x16_gfx2_line:
        sta     g2sh_c
        jsr     popax
        sta     X16_P6                  ; y1
        stx     X16_P7
        jsr     popax
        sta     X16_P4                  ; x1
        stx     X16_P5
        jsr     popax
        sta     X16_P2                  ; y0
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; x0
        stx     X16_P1
        lda     g2sh_c
        jmp     gfx2_line

; void __fastcall__ x16_gfx2_pattern_set(const unsigned char *pattern,
;                                        unsigned char colors)
;   colors = (background << 2) | foreground
_x16_gfx2_pattern_set:
        sta     g2sh_c                  ; colors
        jsr     popax                   ; pattern -> A/X
        ldy     g2sh_c
        jmp     gfx2_pattern_set

; void __fastcall__ x16_gfx2_pattern_rect(unsigned int x, unsigned int y,
;                                         unsigned int w, unsigned int h)
_x16_gfx2_pattern_rect:
        sta     X16_P6                  ; h
        stx     X16_P7
        jsr     popax
        sta     X16_P4                  ; w
        stx     X16_P5
        jsr     popax
        sta     X16_P2                  ; y
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx2_pattern_rect

; void __fastcall__ x16_gfx2_blit(unsigned int x, unsigned int y,
;                                 unsigned char wbytes, unsigned char h,
;                                 const unsigned char *src,
;                                 unsigned char op)
_x16_gfx2_blit:
        sta     g2sh_c                  ; op
        jsr     popax
        sta     X16_P6                  ; src
        stx     X16_P7
        jsr     popa
        sta     X16_P5                  ; h
        jsr     popa
        sta     X16_P4                  ; wbytes
        jsr     popax
        sta     X16_P2                  ; y
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        lda     g2sh_c
        jmp     gfx2_blit

; void __fastcall__ x16_gfx2_blitm(unsigned int x, unsigned int y,
;                                  unsigned char h, unsigned char cols,
;                                  const unsigned char *src)
_x16_gfx2_blitm:
        sta     X16_P6                  ; src
        stx     X16_P7
        jsr     popa
        sta     X16_P5                  ; cols
        jsr     popa
        sta     X16_P4                  ; h
        jsr     popax
        sta     X16_P2                  ; y
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx2_blitm

g2sh_c: .byte   0

; =====================================================================
; the x16_library gfx2 module, verbatim
; =====================================================================

GFX2_WIDTH  = 640
GFX2_HEIGHT = 480
GFX2_STRIDE = 160

; ---------------------------------------------------------------------
; gfx2_init -- program the 640x480@2bpp mode on bare VERA registers.
;
; Layer 0 becomes the bitmap and is enabled; layer 1 (the text screen,
; which would overlay garbage) is disabled; sprites are left as the
; caller had them. Palette entries 0-3 get the default ramp. The
; framebuffer contents are NOT cleared -- call gfx2_clear.
; ---------------------------------------------------------------------
gfx2_init
    vera_dcsel 0
    lda #$80                    ; 1:1 scale -> full 640x480
    sta VERA_DC_HSCALE
    sta VERA_DC_VSCALE
    stz VERA_DC_BORDER

    lda #(VERA_LAYER_BITMAP | VERA_LAYER_BPP_2)
    sta VERA_L0_CONFIG
    lda #$01                    ; bitmap base $00000, 640 pixels wide
    sta VERA_L0_TILEBASE
    stz VERA_L0_HSCROLL_L
    stz VERA_L0_HSCROLL_H       ; bits 3:0 = bitmap palette offset
    stz VERA_L0_VSCROLL_L
    stz VERA_L0_VSCROLL_H

    ; palette 0-3: white paper, two grays, black ink
    vera_addr 0, VRAM_PALETTE, VERA_INC_1
    ldx #0
@pal
    lda bitmap2_defpal,x
    sta VERA_DATA0
    inx
    cpx #8
    bne @pal

    lda #VERA_VIDEO_LAYER1_EN   ; layer 1 off, layer 0 on
    trb VERA_DC_VIDEO
    lda #VERA_VIDEO_LAYER0_EN
    tsb VERA_DC_VIDEO
    rts

bitmap2_defpal
    .byte $FF, $0F, $AA, $0A, $55, $05, $00, $00

; ---------------------------------------------------------------------
; gfx2_clear -- fill the whole framebuffer with one colour
;   in:  A = colour (0-3)
;
; Uses the FX 32-bit cache write (~4x a CPU byte loop; measured 1.25
; frames per full screen against 5.25). Clobbers X16_P0..P4.
; ---------------------------------------------------------------------
gfx2_clear
    and #3
    tax
    lda bitmap2_colbyte,x
    pha
    stz X16_P0                  ; first half: $00000, 38,400 bytes
    stz X16_P1
    stz X16_P2
    lda #<(GFX2_STRIDE * GFX2_HEIGHT / 2)
    sta X16_P3
    lda #>(GFX2_STRIDE * GFX2_HEIGHT / 2)
    sta X16_P4
    pla
    pha
    jsr fx_fill
    lda #<(GFX2_STRIDE * GFX2_HEIGHT / 2)
    sta X16_P0                  ; second half starts at $09600
    sta X16_P3
    lda #>(GFX2_STRIDE * GFX2_HEIGHT / 2)
    sta X16_P1
    sta X16_P4
    stz X16_P2
    pla
    jmp fx_fill

; ---------------------------------------------------------------------
; gfx2_setptr -- point data port 0 at the byte holding pixel (x,y)
;   in:  A = increment index (VERA_INC_*)
;        X16_P0/P1 = x, X16_P2/P3 = y
;   out: A = x & 3 (the pixel's position within the byte)
;
; y*160 = (y<<5) + (y<<5)<<2, so no multiply is needed; the result is
; 17-bit. Stepping by VERA_INC_160 then walks straight down a column.
; ---------------------------------------------------------------------
gfx2_setptr
    pha
    jsr bitmap2_addr_calc
    pla
    jsr bitmap2_aim0
    lda X16_P0
    and #3
    rts

; ---------------------------------------------------------------------
; gfx2_pset -- set one pixel, clipped
;   in:  A = colour (0-3), X16_P0/P1 = x, X16_P2/P3 = y
; ---------------------------------------------------------------------
gfx2_pset
    and #3
    sta g2_c
    jsr bitmap2_onscreen
    bcs @off

    jsr bitmap2_addr_calc
    lda #VERA_INC_0
    jsr bitmap2_aim0

    lda X16_P0
    and #3
    tax
    lda VERA_DATA0              ; INC_0: the read does not move the port
    and bitmap2_keep,x
    sta g2_t
    ldy g2_c
    lda bitmap2_colbyte,y
    and bitmap2_pix,x
    ora g2_t
    sta VERA_DATA0
@off
    rts

; ---------------------------------------------------------------------
; gfx2_read -- read one pixel
;   in:  X16_P0/P1 = x, X16_P2/P3 = y
;   out: carry clear, A = colour (0-3); carry set if (x,y) is off
;        screen (A undefined)
; ---------------------------------------------------------------------
gfx2_read
    jsr bitmap2_onscreen
    bcs @roff

    jsr bitmap2_addr_calc
    lda #VERA_INC_0
    jsr bitmap2_aim0

    lda X16_P0
    and #3
    tax
    lda VERA_DATA0
@shift
    cpx #3                      ; pixel 3 sits in bits 1:0 already
    beq @done
    lsr
    lsr
    inx
    bra @shift
@done
    and #3
    clc
@roff
    rts

; ---------------------------------------------------------------------
; gfx2_hline -- horizontal span (no clipping)
;   in:  A = colour (0-3), X16_P0/P1 = x, X16_P2/P3 = y,
;        X16_P4/P5 = length in pixels
;
; Head and tail partials are read-modify-write; the middle whole bytes
; are one vera_fill.
; ---------------------------------------------------------------------
gfx2_hline
    and #3
    tax
    lda bitmap2_colbyte,x
    sta g2_cb

    lda X16_P4
    sta g2_n
    ora X16_P5
    bne @hgo                    ; zero length: nothing to draw
    rts
@hgo
    lda X16_P5
    sta g2_n+1

    jsr bitmap2_addr_calc

    lda X16_P0
    and #3
    sta g2_p                    ; phase = x & 3
    bne @head
    ; phase 0: a head byte only exists when the span is shorter than
    ; one whole byte
    lda g2_n+1
    bne @middle
    lda g2_n
    cmp #4
    bcs @middle

@head
    ; q = last pixel of the head byte = min(3, p + n - 1)
    lda g2_n+1
    bne @qmax                   ; a long span always reaches pixel 3
    clc
    lda g2_p
    adc g2_n
    bcs @qmax                   ; p + n carried: certainly past pixel 3
    dec a
    cmp #4
    bcc @qgot
@qmax
    lda #3
@qgot
    tay                         ; Y = q
    sec                         ; head pixel count = q - p + 1
    iny
    tya
    sbc g2_p
    sta g2_t
    ; mask = from[p] AND upto[q]
    ldx g2_p
    lda bitmap2_from,x
    dey
    and bitmap2_upto,y
    jsr bitmap2_rmw                    ; ink = colour byte through this mask

    sec                         ; n -= head pixels
    lda g2_n
    sbc g2_t
    sta g2_n
    lda g2_n+1
    sbc #0
    sta g2_n+1
    jsr bitmap2_a_inc                  ; step to the first whole byte

@middle
    ; m = n >> 2 whole bytes
    lda g2_n+1
    sta g2_m+1
    lda g2_n
    lsr g2_m+1
    ror
    lsr g2_m+1
    ror
    sta g2_m
    ora g2_m+1
    beq @tail

    lda #VERA_INC_1
    jsr bitmap2_aim0
    lda g2_cb
    ldx g2_m
    ldy g2_m+1
    jsr vera_fill               ; clobbers X16_T0..T2, not g2_*

    clc                         ; addr += m
    lda g2_a0
    adc g2_m
    sta g2_a0
    lda g2_a1
    adc g2_m+1
    sta g2_a1
    lda g2_a2
    adc #0
    sta g2_a2

@tail
    lda g2_n
    and #3
    beq @hdone
    tay
    dey                         ; tail covers pixels 0..n-1
    lda bitmap2_upto,y
    jsr bitmap2_rmw
@hdone
    rts

; ---------------------------------------------------------------------
; gfx2_vline -- vertical span (no clipping)
;   in:  A = colour (0-3), X16_P0/P1 = x, X16_P2/P3 = y,
;        X16_P4/P5 = length in pixels
;
; One column of read-modify-writes: port 1 reads, port 0 writes, both
; stepping a whole row per access.
; ---------------------------------------------------------------------
gfx2_vline
    and #3
    tax
    lda bitmap2_colbyte,x
    sta g2_cb

    lda X16_P4
    sta g2_n
    ora X16_P5
    beq @vdone
    lda X16_P5
    sta g2_n+1

    jsr bitmap2_addr_calc
    lda #VERA_INC_160
    jsr bitmap2_aim1
    lda #VERA_INC_160
    jsr bitmap2_aim0

    lda X16_P0
    and #3
    tax
    lda g2_cb
    and bitmap2_pix,x
    sta g2_ink                  ; ink and keep are loop-invariant
    lda bitmap2_keep,x
    sta g2_msk

    ldx g2_n                    ; vera_fill's page-count idiom
    ldy g2_n+1
    txa
    beq @vfull                  ; low byte 0 -> exactly hi*256 rows
    iny                         ; otherwise one extra partial page
@vfull
@vloop
    lda VERA_DATA1
    and g2_msk
    ora g2_ink
    sta VERA_DATA0
    dex
    bne @vloop
    dey
    bne @vloop
@vdone
    rts

; ---------------------------------------------------------------------
; gfx2_rect -- filled rectangle (no clipping)
;   in:  A = colour (0-3), X16_P0/P1 = x, X16_P2/P3 = y,
;        X16_P4/P5 = width, X16_P6/P7 = height
; ---------------------------------------------------------------------
gfx2_rect
    sta g2_rc
    lda X16_P4
    sta g2_rw
    lda X16_P5
    sta g2_rw+1
    lda X16_P6
    sta g2_rh
    lda X16_P7
    sta g2_rh+1
@rrow
    lda g2_rh
    ora g2_rh+1
    beq @rdone
    lda g2_rw                   ; hline consumes the length: reload
    sta X16_P4
    lda g2_rw+1
    sta X16_P5
    lda g2_rc
    jsr gfx2_hline              ; leaves P0..P3 alone
    inc X16_P2                  ; y += 1
    bne @ry_ok
    inc X16_P3
@ry_ok
    lda g2_rh
    bne @rh_ok
    dec g2_rh+1
@rh_ok
    dec g2_rh
    bra @rrow
@rdone
    rts

; ---------------------------------------------------------------------
; gfx2_frame -- rectangle outline (no clipping)
;   same arguments as gfx2_rect
; ---------------------------------------------------------------------
gfx2_frame
    sta g2_rc
    lda X16_P0                  ; private copies: the edges reuse the
    sta g2_fx                   ; parameter block as they go
    lda X16_P1
    sta g2_fx+1
    lda X16_P2
    sta g2_fy
    lda X16_P3
    sta g2_fy+1
    lda X16_P4
    sta g2_rw
    lda X16_P5
    sta g2_rw+1
    lda X16_P6
    sta g2_rh
    lda X16_P7
    sta g2_rh+1

    jsr bitmap2_f_span                 ; top edge
    jsr gfx2_hline

    jsr bitmap2_f_span                 ; bottom edge: y + h - 1
    clc
    lda g2_fy
    adc g2_rh
    sta X16_P2
    lda g2_fy+1
    adc g2_rh+1
    sta X16_P3
    lda X16_P2
    bne @f_nb1
    dec X16_P3
@f_nb1
    dec X16_P2
    lda g2_rc
    jsr gfx2_hline

    jsr bitmap2_f_col                  ; left edge
    jsr gfx2_vline

    jsr bitmap2_f_col                  ; right edge: x + w - 1
    clc
    lda g2_fx
    adc g2_rw
    sta X16_P0
    lda g2_fx+1
    adc g2_rw+1
    sta X16_P1
    lda X16_P0
    bne @f_nb2
    dec X16_P1
@f_nb2
    dec X16_P0
    lda g2_rc
    jmp gfx2_vline

; x, y, width in the block, colour in A -- arguments for gfx2_hline
bitmap2_f_span
    lda g2_fx
    sta X16_P0
    lda g2_fx+1
    sta X16_P1
    lda g2_fy
    sta X16_P2
    lda g2_fy+1
    sta X16_P3
    lda g2_rw
    sta X16_P4
    lda g2_rw+1
    sta X16_P5
    lda g2_rc
    rts

; x, y, height in the block, colour in A -- arguments for gfx2_vline
bitmap2_f_col
    lda g2_fx
    sta X16_P0
    lda g2_fx+1
    sta X16_P1
    lda g2_fy
    sta X16_P2
    lda g2_fy+1
    sta X16_P3
    lda g2_rh
    sta X16_P4
    lda g2_rh+1
    sta X16_P5
    lda g2_rc
    rts

; ---------------------------------------------------------------------
; gfx2_line -- Bresenham, any direction; plots through gfx2_pset so
; the line clips at the screen edges
;   in:  A = colour (0-3)
;        X16_P0/P1 = x0, X16_P2/P3 = y0
;        X16_P4/P5 = x1, X16_P6/P7 = y1
; ---------------------------------------------------------------------
gfx2_line
    sta g2_lc
    lda X16_P0
    sta g2_lx0
    lda X16_P1
    sta g2_lx0+1
    lda X16_P2
    sta g2_ly0
    lda X16_P3
    sta g2_ly0+1
    lda X16_P4
    sta g2_lx1
    lda X16_P5
    sta g2_lx1+1
    lda X16_P6
    sta g2_ly1
    lda X16_P7
    sta g2_ly1+1

    ; dx = |x1 - x0|, sx = sign
    sec
    lda g2_lx1
    sbc g2_lx0
    sta g2_ldx
    lda g2_lx1+1
    sbc g2_lx0+1
    sta g2_ldx+1
    bpl @dx_pos
    sec
    lda #0
    sbc g2_ldx
    sta g2_ldx
    lda #0
    sbc g2_ldx+1
    sta g2_ldx+1
    lda #$FF
    sta g2_lsx
    sta g2_lsx+1
    bra @dx_done
@dx_pos
    lda #$01
    sta g2_lsx
    stz g2_lsx+1
@dx_done

    ; dy = -|y1 - y0|, sy = sign
    sec
    lda g2_ly1
    sbc g2_ly0
    sta g2_lt
    lda g2_ly1+1
    sbc g2_ly0+1
    sta g2_lt+1
    bpl @dy_pos
    sec
    lda #0
    sbc g2_lt
    sta g2_lt
    lda #0
    sbc g2_lt+1
    sta g2_lt+1
    lda #$FF
    sta g2_lsy
    sta g2_lsy+1
    bra @dy_done
@dy_pos
    lda #$01
    sta g2_lsy
    stz g2_lsy+1
@dy_done
    sec                         ; g2_ldy = -|dy|
    lda #0
    sbc g2_lt
    sta g2_ldy
    lda #0
    sbc g2_lt+1
    sta g2_ldy+1

    clc                         ; err = dx + dy
    lda g2_ldx
    adc g2_ldy
    sta g2_lerr
    lda g2_ldx+1
    adc g2_ldy+1
    sta g2_lerr+1

@loop
    lda g2_lx0                  ; plot (x0, y0)
    sta X16_P0
    lda g2_lx0+1
    sta X16_P1
    lda g2_ly0
    sta X16_P2
    lda g2_ly0+1
    sta X16_P3
    lda g2_lc
    jsr gfx2_pset

    lda g2_lx0                  ; reached the end point?
    cmp g2_lx1
    bne @step
    lda g2_lx0+1
    cmp g2_lx1+1
    bne @step
    lda g2_ly0
    cmp g2_ly1
    bne @step
    lda g2_ly0+1
    cmp g2_ly1+1
    bne @step
    rts

@step
    lda g2_lerr                 ; e2 = err * 2
    asl
    sta g2_le2
    lda g2_lerr+1
    rol
    sta g2_le2+1

    ; if e2 >= dy  ->  err += dy, x0 += sx
    sec
    lda g2_le2
    sbc g2_ldy
    lda g2_le2+1
    sbc g2_ldy+1
    bvc @nv1
    eor #$80                    ; signed compare: fold overflow into sign
@nv1
    bmi @skip_x
    clc
    lda g2_lerr
    adc g2_ldy
    sta g2_lerr
    lda g2_lerr+1
    adc g2_ldy+1
    sta g2_lerr+1
    clc
    lda g2_lx0
    adc g2_lsx
    sta g2_lx0
    lda g2_lx0+1
    adc g2_lsx+1
    sta g2_lx0+1
@skip_x

    ; if e2 <= dx  ->  err += dx, y0 += sy
    sec
    lda g2_ldx
    sbc g2_le2
    lda g2_ldx+1
    sbc g2_le2+1
    bvc @nv2
    eor #$80
@nv2
    bmi @skip_y
    clc
    lda g2_lerr
    adc g2_ldx
    sta g2_lerr
    lda g2_lerr+1
    adc g2_ldx+1
    sta g2_lerr+1
    clc
    lda g2_ly0
    adc g2_lsy
    sta g2_ly0
    lda g2_ly0+1
    adc g2_lsy+1
    sta g2_ly0+1
@skip_y
    jmp @loop

; ---------------------------------------------------------------------
; gfx2_pattern_set -- expand an 8x8 1bpp pattern for gfx2_pattern_rect
;   in:  A = pattern low, X = pattern high (8 row bytes, top first;
;            bit 7 is the leftmost pixel)
;        Y = colours: (background << 2) | foreground
;
; Patterns tile from the screen origin, so each row expands to exactly
; two 2bpp bytes (16 bits); which of the pair a framebuffer byte uses
; is the parity of its address. The expansion is cached in g2_pat.
; ---------------------------------------------------------------------
gfx2_pattern_set
    sta X16_T6                  ; T6/T7 = pattern pointer
    stx X16_T7
    tya
    and #3
    tax
    lda bitmap2_colbyte,x              ; replicated foreground
    sta g2_pfg
    tya
    lsr
    lsr
    and #3
    tax
    lda bitmap2_colbyte,x              ; replicated background
    sta g2_pbg

    ldx #0                      ; cache index (2 bytes per row)
    ldy #0                      ; pattern row
@prow
    sty g2_t
    lda (X16_T6),y
    sta g2_pr                   ; the row's 8 bits, consumed by asl
    jsr bitmap2_p_half                 ; pixels 0-3 -> even byte
    sta g2_pat,x
    inx
    jsr bitmap2_p_half                 ; pixels 4-7 -> odd byte
    sta g2_pat,x
    inx
    ldy g2_t
    iny
    cpy #8
    bne @prow
    rts

; expand the next 4 bits of g2_pr (MSB first) into one 2bpp byte:
; a set bit becomes the foreground colour, a clear one the background
bitmap2_p_half
    stz g2_t2
    ldy #0                      ; pixel 0..3 within the byte
@pbit
    asl g2_pr
    bcs @pfg
    lda g2_pbg
    bra @pmix
@pfg
    lda g2_pfg
@pmix
    and bitmap2_pix,y                  ; keep just this pixel's two bits
    ora g2_t2
    sta g2_t2
    iny
    cpy #4
    bne @pbit
    lda g2_t2
    rts

; ---------------------------------------------------------------------
; gfx2_pattern_rect -- fill a rectangle with the current pattern
;   in:  X16_P0/P1 = x, X16_P2/P3 = y, X16_P4/P5 = width,
;        X16_P6/P7 = height   (no clipping)
; ---------------------------------------------------------------------
gfx2_pattern_rect
    lda X16_P4
    sta g2_rw
    lda X16_P5
    sta g2_rw+1
    lda X16_P6
    sta g2_rh
    lda X16_P7
    sta g2_rh+1
@yrow
    lda g2_rh
    ora g2_rh+1
    beq @ydone
    jsr bitmap2_p_row
    inc X16_P2
    bne @py_ok
    inc X16_P3
@py_ok
    lda g2_rh
    bne @ph_ok
    dec g2_rh+1
@ph_ok
    dec g2_rh
    bra @yrow
@ydone
    rts

; one pattern row at (P0..P3), width g2_rw
bitmap2_p_row
    lda g2_rw
    sta g2_n
    ora g2_rw+1
    bne @prgo
    rts
@prgo
    lda g2_rw+1
    sta g2_n+1

    jsr bitmap2_addr_calc

    ; the row's two pattern bytes, in address-parity order
    lda X16_P2
    and #7
    asl
    tax
    lda g2_a0
    and #1
    beq @even
    inx                         ; an odd start address uses the odd
    lda g2_pat,x                ; byte first
    sta g2_pb0
    dex
    lda g2_pat,x
    sta g2_pb1
    bra @parity_done
@even
    lda g2_pat,x
    sta g2_pb0
    inx
    lda g2_pat,x
    sta g2_pb1
@parity_done

    lda X16_P0
    and #3
    sta g2_p
    bne @phead
    lda g2_n+1
    bne @pmiddle
    lda g2_n
    cmp #4
    bcs @pmiddle

@phead
    lda g2_n+1
    bne @pqmax
    clc
    lda g2_p
    adc g2_n
    bcs @pqmax                  ; p + n carried: certainly past pixel 3
    dec a
    cmp #4
    bcc @pqgot
@pqmax
    lda #3
@pqgot
    tay
    sec
    iny
    tya
    sbc g2_p
    sta g2_t
    ldx g2_p
    lda bitmap2_from,x
    dey
    and bitmap2_upto,y
    tax                         ; mask in X for bitmap2_rmwp
    lda g2_pb0
    jsr bitmap2_rmwp

    sec
    lda g2_n
    sbc g2_t
    sta g2_n
    lda g2_n+1
    sbc #0
    sta g2_n+1
    jsr bitmap2_a_inc
    lda g2_pb0                  ; next byte has the other parity
    ldx g2_pb1
    sta g2_pb1
    stx g2_pb0

@pmiddle
    lda g2_n+1
    sta g2_m+1
    lda g2_n
    lsr g2_m+1
    ror
    lsr g2_m+1
    ror
    sta g2_m
    ora g2_m+1
    beq @ptail

    lda #VERA_INC_1
    jsr bitmap2_aim0
    ldx g2_m                    ; vera_fill's page-count idiom
    ldy g2_m+1
    txa
    beq @pfull
    iny
@pfull
@ploop
    lda g2_pb0
    sta VERA_DATA0
    lda g2_pb0                  ; swap the parity pair
    pha
    lda g2_pb1
    sta g2_pb0
    pla
    sta g2_pb1
    dex
    bne @ploop
    dey
    bne @ploop

    clc                         ; addr += m
    lda g2_a0
    adc g2_m
    sta g2_a0
    lda g2_a1
    adc g2_m+1
    sta g2_a1
    lda g2_a2
    adc #0
    sta g2_a2

@ptail
    lda g2_n
    and #3
    beq @prdone
    tay
    dey
    lda bitmap2_upto,y
    tax
    lda g2_pb0
    jsr bitmap2_rmwp
@prdone
    rts

; ---------------------------------------------------------------------
; gfx2_blit -- copy a byte-aligned image from CPU RAM into the bitmap
;   in:  A = raster op: 0 copy, 1 OR, 2 AND, 3 XOR
;        X16_P0/P1 = x (bits 1:0 ignored: byte-aligned),
;        X16_P2/P3 = y, X16_P4 = width in BYTES (4-pixel units),
;        X16_P5 = height in rows, X16_P6/P7 = source (row-major)
;
; The source pointer is X16_PTR3 -- P6/P7 double as real zero page, so
; (PTR3),y addressing costs nothing extra. No clipping.
; ---------------------------------------------------------------------
gfx2_blit
    and #3
    sta g2_op
    jsr bitmap2_addr_calc
    lda X16_P5
    sta g2_h
@brow
    lda #VERA_INC_1
    jsr bitmap2_aim1                   ; ops read through port 1...
    lda #VERA_INC_1
    jsr bitmap2_aim0                   ; ...and everything writes port 0
    ldy #0
    lda g2_op
    beq @bcopy
    cmp #1
    beq @bor
    cmp #2
    beq @band
@bxor
    lda VERA_DATA1
    eor (X16_PTR3),y
    sta VERA_DATA0
    iny
    cpy X16_P4
    bne @bxor
    bra @brow_done
@bcopy
    lda (X16_PTR3),y
    sta VERA_DATA0
    iny
    cpy X16_P4
    bne @bcopy
    bra @brow_done
@bor
    lda VERA_DATA1
    ora (X16_PTR3),y
    sta VERA_DATA0
    iny
    cpy X16_P4
    bne @bor
    bra @brow_done
@band
    lda VERA_DATA1
    and (X16_PTR3),y
    sta VERA_DATA0
    iny
    cpy X16_P4
    bne @band
@brow_done
    clc                         ; src += width
    lda X16_PTR3
    adc X16_P4
    sta X16_PTR3
    bcc @bsrc_ok
    inc X16_PTR3+1
@bsrc_ok
    jsr bitmap2_a_row                  ; dest += one row
    dec g2_h
    bne @brow
    rts

; ---------------------------------------------------------------------
; gfx2_blitm -- masked blit of pre-shifted column-major data
;   in:  X16_P0/P1 = x (any pixel position), X16_P2/P3 = y,
;        X16_P4 = height in rows (1-127), X16_P5 = width in COLUMNS
;        (framebuffer bytes), X16_P6/P7 = source
;
; The source holds, for each of the P5 columns, P4 (mask, data) byte
; PAIRS walking down the rows: fb' = (fb AND mask) OR data. The caller
; supplies data already shifted for this x's pixel phase (x & 3) --
; pre-shifted glyph caches are the whole point: at 833 cycles per 8x8
; glyph this is what makes proportional text affordable (spike-proven;
; see the CXGEOS project). No clipping.
; ---------------------------------------------------------------------
gfx2_blitm
    jsr bitmap2_addr_calc
    lda X16_P5
    sta g2_w
@mcol
    lda #VERA_INC_160
    jsr bitmap2_aim1
    lda #VERA_INC_160
    jsr bitmap2_aim0
    ldy #0
    ldx X16_P4
@mrow
    lda VERA_DATA1
    and (X16_PTR3),y            ; mask byte
    iny
    ora (X16_PTR3),y            ; data byte
    iny
    sta VERA_DATA0
    dex
    bne @mrow

    clc                         ; src += 2 * height (one column)
    tya
    adc X16_PTR3
    sta X16_PTR3
    bcc @msrc_ok
    inc X16_PTR3+1
@msrc_ok
    jsr bitmap2_a_inc                  ; dest: next byte column
    dec g2_w
    bne @mcol
    rts

; ---------------------------------------------------------------------
; module plumbing
; ---------------------------------------------------------------------

; carry clear if (P0/P1, P2/P3) is on screen
bitmap2_onscreen
    lda X16_P1                  ; x < 640?
    cmp #>GFX2_WIDTH
    bcc @x_ok
    bne @bad
    lda X16_P0
    cmp #<GFX2_WIDTH
    bcs @bad
@x_ok
    lda X16_P3                  ; y < 480?
    cmp #>GFX2_HEIGHT
    bcc @ok
    bne @bad
    lda X16_P2
    cmp #<GFX2_HEIGHT
    bcs @bad
@ok
    clc
    rts
@bad
    sec
    rts

; g2_a2:a1:a0 = y*160 + (x>>2)   (from X16_P0..P3; clobbers T0..T2)
bitmap2_addr_calc
    lda X16_P2                  ; t = y << 5
    sta g2_a0
    lda X16_P3
    sta g2_a1
    asl g2_a0
    rol g2_a1
    asl g2_a0
    rol g2_a1
    asl g2_a0
    rol g2_a1
    asl g2_a0
    rol g2_a1
    asl g2_a0
    rol g2_a1

    lda g2_a0                   ; T2:T1:T0 = t << 2
    sta X16_T0
    lda g2_a1
    sta X16_T1
    stz X16_T2
    asl X16_T0
    rol X16_T1
    rol X16_T2
    asl X16_T0
    rol X16_T1
    rol X16_T2

    clc                         ; y*160 = t + (t << 2)
    lda g2_a0
    adc X16_T0
    sta g2_a0
    lda g2_a1
    adc X16_T1
    sta g2_a1
    lda #0
    adc X16_T2
    sta g2_a2

    lda X16_P1                  ; + x >> 2
    sta X16_T1
    lda X16_P0
    lsr X16_T1
    ror
    lsr X16_T1
    ror
    clc
    adc g2_a0
    sta g2_a0
    lda X16_T1
    adc g2_a1
    sta g2_a1
    lda #0
    adc g2_a2
    sta g2_a2
    rts

; point port 0 (write side) at g2_a; A = increment index.
; Scratch is g2_inc, NOT g2_t: hline/pattern hold a pixel count in
; g2_t across the bitmap2_rmw call, and bitmap2_rmw aims through here.
bitmap2_aim0
    asl
    asl
    asl
    asl
    sta g2_inc
    lda #VERA_CTRL_ADDRSEL
    trb VERA_CTRL
    lda g2_a0
    sta VERA_ADDR_L
    lda g2_a1
    sta VERA_ADDR_M
    lda g2_a2
    and #VERA_ADDR_H_BANK
    ora g2_inc
    sta VERA_ADDR_H
    rts

; point port 1 (read side) at g2_a; A = increment index
bitmap2_aim1
    asl
    asl
    asl
    asl
    sta g2_inc
    lda #VERA_CTRL_ADDRSEL
    tsb VERA_CTRL
    lda g2_a0
    sta VERA_ADDR_L
    lda g2_a1
    sta VERA_ADDR_M
    lda g2_a2
    and #VERA_ADDR_H_BANK
    ora g2_inc
    sta VERA_ADDR_H
    rts

; read-modify-write the byte at g2_a through a pixel mask:
; fb' = (fb AND NOT mask) OR (ink AND mask). INC_0 keeps the port in
; place, so one aim serves both the read and the write.
;   bitmap2_rmw:  A = mask, ink is the solid colour byte g2_cb
;   bitmap2_rmwp: A = ink byte, X = mask (the pattern-row variant)
bitmap2_rmw
    tax
    lda g2_cb
bitmap2_rmwp
    sta g2_ink
    stx g2_msk
    lda #VERA_INC_0
    jsr bitmap2_aim0
    lda g2_msk
    eor #$FF
    and VERA_DATA0
    sta g2_t2
    lda g2_ink
    and g2_msk
    ora g2_t2
    sta VERA_DATA0
    rts

; g2_a += 1 (24-bit)
bitmap2_a_inc
    inc g2_a0
    bne @ai_done
    inc g2_a1
    bne @ai_done
    inc g2_a2
@ai_done
    rts

; g2_a += one framebuffer row
bitmap2_a_row
    clc
    lda g2_a0
    adc #GFX2_STRIDE
    sta g2_a0
    lda g2_a1
    adc #0
    sta g2_a1
    lda g2_a2
    adc #0
    sta g2_a2
    rts

; ---------------------------------------------------------------------
; module variables (never live across a call boundary)
; ---------------------------------------------------------------------
g2_a0  .byte 0
g2_a1  .byte 0
g2_a2  .byte 0
g2_c   .byte 0
g2_cb  .byte 0
g2_p   .byte 0
g2_n   .word 0
g2_m   .word 0
g2_t   .byte 0
g2_t2  .byte 0
g2_inc .byte 0
g2_msk .byte 0
g2_ink .byte 0
g2_op  .byte 0
g2_h   .byte 0
g2_w   .byte 0

g2_rc  .byte 0
g2_rw  .word 0
g2_rh  .word 0
g2_fx  .word 0
g2_fy  .word 0

g2_pfg .byte 0
g2_pbg .byte 0
g2_pr  .byte 0
g2_pb0 .byte 0
g2_pb1 .byte 0
g2_pat .res 16, 0

g2_lc   .byte 0
g2_lx0  .word 0
g2_ly0  .word 0
g2_lx1  .word 0
g2_ly1  .word 0
g2_ldx  .word 0
g2_ldy  .word 0
g2_lerr .word 0
g2_le2  .word 0
g2_lsx  .word 0
g2_lsy  .word 0
g2_lt   .word 0

bitmap2_colbyte
    .byte $00, $55, $AA, $FF   ; a colour in all four pixels
bitmap2_pix
    .byte $C0, $30, $0C, $03   ; the bits of pixel 0..3
bitmap2_keep
    .byte $3F, $CF, $F3, $FC   ; everything but pixel 0..3
bitmap2_from
    .byte $FF, $3F, $0F, $03   ; pixels p..3
bitmap2_upto
    .byte $C0, $F0, $FC, $FF   ; pixels 0..q

; (end zone)
