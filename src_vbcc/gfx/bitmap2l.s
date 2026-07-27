; =====================================================================
; x16clib :: gfx/bitmap2l.s -- 320x240x4 bitmap drawing (2bpp)
; =====================================================================
; The framebuffer is 2bpp at VRAM $00000: 4 pixels per byte packed
; MSB-first, rows of 80 bytes, 19,200 bytes in all. A pixel byte is at
; y*80 + (x>>2). Colours are 0-3; x16_gfx2l_init() loads the default
; paper-and-ink ramp into palette entries 0-3 and programs the mode on
; bare VERA registers (no KERNAL screen mode exists for it).
;
; x16_gfx2l_pset()/read() clip; the span/rect/line/blit primitives do
; NOT (the 8bpp module's policy). x16_gfx2l_clear() runs through the
; VERA FX 32-bit cache and needs an FX-capable VERA (47.0.2+).
;
; The routine bodies are the x16_library bitmap2l module, byte for
; byte; only the C shims in front are new. Every shim pops straight
; into the parameter block.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers. Every gfx2l coordinate is a 16-bit int, so
; x takes r0/r1 and y takes r2/r3; the remaining arguments fill r4..r7
; in order and anything past them spills to the C soft stack at (sp).
; Only rect, frame, line and blit are wide enough to need that.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3
        zpage	r4
        zpage	r5
        zpage	r6
        zpage	r7
        zpage	sp

        global	_x16_gfx2l_init
        global	_x16_gfx2l_clear
        global	_x16_gfx2l_setptr
        global	_x16_gfx2l_pset
        global	_x16_gfx2l_read
        global	_x16_gfx2l_hline
        global	_x16_gfx2l_vline
        global	_x16_gfx2l_rect
        global	_x16_gfx2l_frame
        global	_x16_gfx2l_line
        global	_x16_gfx2l_pattern_set
        global	_x16_gfx2l_pattern_rect
        global	_x16_gfx2l_blit
        global	_x16_gfx2l_blitm

        section text

; =====================================================================
; C entry points
; =====================================================================

; void x16_gfx2l_init(void)
_x16_gfx2l_init:
        jmp     gfx2l_init

; void x16_gfx2l_clear(__reg("a") unsigned char color)
;   already in A: no shim.
_x16_gfx2l_clear:
        jmp     gfx2l_clear

; unsigned char x16_gfx2l_setptr(__reg("r4") unsigned char inc,
;                               __reg("r0/r1") unsigned int x,
;                               __reg("r2/r3") unsigned int y)
_x16_gfx2l_setptr:
        jsr     xy_marshal
        lda     r4                      ; increment index
        jmp     gfx2l_setptr             ; A = x & 3 out

; void x16_gfx2l_pset(__reg("r0/r1") unsigned int x,
;                    __reg("r2/r3") unsigned int y,
;                    __reg("r4") unsigned char color)
_x16_gfx2l_pset:
        jsr     xy_marshal
        lda     r4                      ; colour
        jmp     gfx2l_pset

; unsigned char x16_gfx2l_read(__reg("r0/r1") unsigned int x,
;                             __reg("r2/r3") unsigned int y)
;   0-3, or $FF off screen
_x16_gfx2l_read:
        jsr     xy_marshal
        jsr     gfx2l_read
        bcc     rd_on
        lda     #$FF
rd_on:
        rts

; void x16_gfx2l_hline(__reg("r0/r1") unsigned int x,
;                     __reg("r2/r3") unsigned int y,
;                     __reg("r4/r5") unsigned int len,
;                     __reg("r6") unsigned char color)
_x16_gfx2l_hline:
        jsr     span_marshal
        lda     r6                      ; colour
        jmp     gfx2l_hline

; void x16_gfx2l_vline(... same arguments ...)
_x16_gfx2l_vline:
        jsr     span_marshal
        lda     r6                      ; colour
        jmp     gfx2l_vline

; void x16_gfx2l_rect(__reg("r0/r1") unsigned int x,
;                    __reg("r2/r3") unsigned int y,
;                    __reg("r4/r5") unsigned int w,
;                    __reg("r6/r7") unsigned int h,
;                    unsigned char color)
;   colour is the fifth argument: it spills to the soft stack at (sp)+0.
_x16_gfx2l_rect:
        jsr     quad_marshal
        jmp     gfx2l_rect

; void x16_gfx2l_frame(... same arguments ...)
_x16_gfx2l_frame:
        jsr     quad_marshal
        jmp     gfx2l_frame

; void x16_gfx2l_line(__reg("r0/r1") unsigned int x0,
;                    __reg("r2/r3") unsigned int y0,
;                    __reg("r4/r5") unsigned int x1,
;                    __reg("r6/r7") unsigned int y1,
;                    unsigned char color)
;   the same four words land in the same four parameter slots
_x16_gfx2l_line:
        jsr     quad_marshal
        jmp     gfx2l_line

; void x16_gfx2l_pattern_set(__reg("r0/r1") const unsigned char *pattern,
;                           __reg("r2") unsigned char colors)
;   the body wants A/X = pattern, Y = colors.
_x16_gfx2l_pattern_set:
        ldy     r2                      ; colours
        lda     r0
        ldx     r1
        jmp     gfx2l_pattern_set

; void x16_gfx2l_pattern_rect(__reg("r0/r1") unsigned int x,
;                            __reg("r2/r3") unsigned int y,
;                            __reg("r4/r5") unsigned int w,
;                            __reg("r6/r7") unsigned int h)
_x16_gfx2l_pattern_rect:
        jsr     wh_marshal
        jmp     gfx2l_pattern_rect

; void x16_gfx2l_blit(__reg("r0/r1") unsigned int x,
;                    __reg("r2/r3") unsigned int y,
;                    __reg("r4") unsigned char wbytes,
;                    __reg("r5") unsigned char h,
;                    __reg("r6/r7") const unsigned char *src,
;                    unsigned char op)
;   op is the sixth argument: it spills to the soft stack at (sp)+0.
_x16_gfx2l_blit:
        jsr     blit_marshal
        ldy     #0
        lda     (sp),y                  ; op (stacked)
        jmp     gfx2l_blit

; void x16_gfx2l_blitm(__reg("r0/r1") unsigned int x,
;                     __reg("r2/r3") unsigned int y,
;                     __reg("r4") unsigned char h,
;                     __reg("r5") unsigned char cols,
;                     __reg("r6/r7") const unsigned char *src)
;   h and cols land in the same slots blit uses for wbytes and h, so the
;   marshal is shared.
_x16_gfx2l_blitm:
        jsr     blit_marshal
        jmp     gfx2l_blitm

; --- the marshals ----------------------------------------------------

; x -> P0/P1, y -> P2/P3
xy_marshal:
        lda     r0
        sta     X16_P0                  ; x
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; y
        lda     r3
        sta     X16_P3
        rts

; x, y and one more 16-bit argument -> P0..P5
span_marshal:
        jsr     xy_marshal
        lda     r4
        sta     X16_P4                  ; len / w
        lda     r5
        sta     X16_P5
        rts

; four 16-bit arguments -> P0..P7
wh_marshal:
        jsr     span_marshal
        lda     r6
        sta     X16_P6                  ; h / y1
        lda     r7
        sta     X16_P7
        rts

; four 16-bit arguments, plus the stacked colour -> A
quad_marshal:
        jsr     wh_marshal
        ldy     #0
        lda     (sp),y                  ; colour (stacked 5th arg)
        rts

; x, y, two bytes and a pointer -> P0..P7
blit_marshal:
        jsr     xy_marshal
        lda     r4
        sta     X16_P4                  ; wbytes / h
        lda     r5
        sta     X16_P5                  ; h / cols
        lda     r6
        sta     X16_P6                  ; src
        lda     r7
        sta     X16_P7
        rts

; =====================================================================
; the x16_library bitmap2l module, verbatim
; =====================================================================
; =====================================================================
; x16lib :: gfx/bitmap2l.asm -- 320x240x4 bitmap drawing (2bpp)
; =====================================================================
; This file EMITS CODE. Source it exactly once (x16_code.asm does).
; Requires X16_USE_VERA (vera_fill) and X16_USE_VERAFX (fx_fill).
;
; The framebuffer is 2bpp at VRAM $00000: 4 pixels per byte packed
; MSB-first (the leftmost pixel is bits 7:6), rows of 80 bytes,
; 19,200 bytes in all. A pixel byte is at y*80 + (x>>2); its position
; within the byte is x & 3. VERA renders it as layer-0 bitmap, 2bpp,
; 320 wide, HSCALE = VSCALE = $80 -- gfx2l_init programs exactly that
; (there is no KERNAL screen mode for it).
;
; Colours are 0-3 out of the first four palette entries. gfx2l_init
; loads a paper-and-ink default: 0 white, 1 light gray, 2 dark gray,
; 3 black. pal_set/pal_load re-colour without touching the pixels.
;
; gfx2l_pset and gfx2l_read clip. The span/rect/line/blit primitives do
; NOT: they assume their arguments are on screen (the 8bpp module's
; policy, for the same reason -- a caller that knows its geometry
; should not pay for a clip on every span).
;
; Sub-byte pixels make 2bpp spans three-phase: a partial head byte, a
; run of whole bytes, a partial tail byte. The partial bytes are
; read-modify-write through data port 0 with INC_0; the middle run is
; a plain vera_fill. Column walks (vline, blitm) pair port 1 (read)
; with port 0 (write), both stepping VERA_INC_80.
; =====================================================================

; (zone: file scope in ca65)

GFX2L_WIDTH  = 320
GFX2L_HEIGHT = 240
GFX2L_STRIDE = 80

; ---------------------------------------------------------------------
; gfx2l_init -- program the 320x240@2bpp mode on bare VERA registers.
;
; Layer 0 becomes the bitmap and is enabled; layer 1 (the text screen,
; which would overlay garbage) is disabled; sprites are left as the
; caller had them. Palette entries 0-3 get the default ramp. The
; framebuffer contents are NOT cleared -- call gfx2l_clear.
; ---------------------------------------------------------------------
gfx2l_init:
    vera_dcsel 0
    lda #$80                    ; 1:1 scale -> 1:1 scale
    sta VERA_DC_HSCALE
    sta VERA_DC_VSCALE
    stz VERA_DC_BORDER

    lda #(VERA_LAYER_BITMAP | VERA_LAYER_BPP_2)
    sta VERA_L0_CONFIG
    lda #$00                    ; bitmap base $00000, 320 pixels wide
    sta VERA_L0_TILEBASE
    stz VERA_L0_HSCROLL_L
    stz VERA_L0_HSCROLL_H       ; bits 3:0 = bitmap palette offset
    stz VERA_L0_VSCROLL_L
    stz VERA_L0_VSCROLL_H

    ; palette 0-3: white paper, two grays, black ink
    vera_addr 0, VRAM_PALETTE, VERA_INC_1
    ldx #0
.pal:
    lda bitmap2l_defpal,x
    sta VERA_DATA0
    inx
    cpx #8
    bne .pal

    lda #VERA_VIDEO_LAYER1_EN   ; layer 1 off, layer 0 on
    trb VERA_DC_VIDEO
    lda #VERA_VIDEO_LAYER0_EN
    tsb VERA_DC_VIDEO
    rts

bitmap2l_defpal:
    byte $FF, $0F, $AA, $0A, $55, $05, $00, $00

; ---------------------------------------------------------------------
; gfx2l_clear -- fill the whole framebuffer with one colour
;   in:  A = colour (0-3)
;
; Uses the FX 32-bit cache write (~4x a CPU byte loop; measured 1.25
; frames per full screen against 5.25). Clobbers X16_P0..P4.
; ---------------------------------------------------------------------
gfx2l_clear:
    and #3
    tax
    lda bitmap2l_colbyte,x
    pha
    stz X16_P0                  ; first half: $00000, 9,600 bytes
    stz X16_P1
    stz X16_P2
    lda #<(GFX2L_STRIDE * GFX2L_HEIGHT / 2)
    sta X16_P3
    lda #>(GFX2L_STRIDE * GFX2L_HEIGHT / 2)
    sta X16_P4
    pla
    pha
    jsr fx_fill
    lda #<(GFX2L_STRIDE * GFX2L_HEIGHT / 2)
    sta X16_P0                  ; second half starts at $02580
    sta X16_P3
    lda #>(GFX2L_STRIDE * GFX2L_HEIGHT / 2)
    sta X16_P1
    sta X16_P4
    stz X16_P2
    pla
    jmp fx_fill

; ---------------------------------------------------------------------
; gfx2l_setptr -- point data port 0 at the byte holding pixel (x,y)
;   in:  A = increment index (VERA_INC_*)
;        X16_P0/P1 = x, X16_P2/P3 = y
;   out: A = x & 3 (the pixel's position within the byte)
;
; y*80 = (y<<4) + (y<<4)<<2, so no multiply is needed; the result is
; 17-bit. Stepping by VERA_INC_80 then walks straight down a column.
; ---------------------------------------------------------------------
gfx2l_setptr:
    pha
    jsr bitmap2l_addr_calc
    pla
    jsr bitmap2l_aim0
    lda X16_P0
    and #3
    rts

; ---------------------------------------------------------------------
; gfx2l_pset -- set one pixel, clipped
;   in:  A = colour (0-3), X16_P0/P1 = x, X16_P2/P3 = y
; ---------------------------------------------------------------------
gfx2l_pset:
    and #3
    sta g2l_c
    jsr bitmap2l_onscreen
    bcs .off

    jsr bitmap2l_addr_calc
    lda #VERA_INC_0
    jsr bitmap2l_aim0

    lda X16_P0
    and #3
    tax
    lda VERA_DATA0              ; INC_0: the read does not move the port
    and bitmap2l_keep,x
    sta g2l_t
    ldy g2l_c
    lda bitmap2l_colbyte,y
    and bitmap2l_pix,x
    ora g2l_t
    sta VERA_DATA0
.off:
    rts

; ---------------------------------------------------------------------
; gfx2l_read -- read one pixel
;   in:  X16_P0/P1 = x, X16_P2/P3 = y
;   out: carry clear, A = colour (0-3); carry set if (x,y) is off
;        screen (A undefined)
; ---------------------------------------------------------------------
gfx2l_read:
    jsr bitmap2l_onscreen
    bcs .roff

    jsr bitmap2l_addr_calc
    lda #VERA_INC_0
    jsr bitmap2l_aim0

    lda X16_P0
    and #3
    tax
    lda VERA_DATA0
.shift:
    cpx #3                      ; pixel 3 sits in bits 1:0 already
    beq .done
    lsr
    lsr
    inx
    bra .shift
.done:
    and #3
    clc
.roff:
    rts

; ---------------------------------------------------------------------
; gfx2l_hline -- horizontal span (no clipping)
;   in:  A = colour (0-3), X16_P0/P1 = x, X16_P2/P3 = y,
;        X16_P4/P5 = length in pixels
;
; Head and tail partials are read-modify-write; the middle whole bytes
; are one vera_fill.
; ---------------------------------------------------------------------
gfx2l_hline:
    and #3
    tax
    lda bitmap2l_colbyte,x
    sta g2l_cb

    lda X16_P4
    sta g2l_n
    ora X16_P5
    bne .hgo                    ; zero length: nothing to draw
    rts
.hgo:
    lda X16_P5
    sta g2l_n+1

    jsr bitmap2l_addr_calc

    lda X16_P0
    and #3
    sta g2l_p                    ; phase = x & 3
    bne .head
    ; phase 0: a head byte only exists when the span is shorter than
    ; one whole byte
    lda g2l_n+1
    bne .middle
    lda g2l_n
    cmp #4
    bcs .middle

.head:
    jsr bitmap2l_headmask               ; mask -> A, head pixel count -> g2l_t
    jsr bitmap2l_rmw                    ; ink = colour byte through this mask
    jsr bitmap2l_headadv                ; n -= head pixels, on to the whole bytes

.middle:
    jsr bitmap2l_quadcount              ; m = n >> 2 whole bytes
    beq .tail

    lda #VERA_INC_1
    jsr bitmap2l_aim0
    lda g2l_cb
    ldx g2l_m
    ldy g2l_m+1
    jsr vera_fill               ; clobbers X16_T0..T2, not g2l_*
    jsr bitmap2l_a_addm                 ; addr += m

.tail:
    jsr bitmap2l_tailmask
    beq .hdone
    jsr bitmap2l_rmw
.hdone:
    rts

; ---------------------------------------------------------------------
; gfx2l_vline -- vertical span (no clipping)
;   in:  A = colour (0-3), X16_P0/P1 = x, X16_P2/P3 = y,
;        X16_P4/P5 = length in pixels
;
; One column of read-modify-writes: port 1 reads, port 0 writes, both
; stepping a whole row per access.
; ---------------------------------------------------------------------
gfx2l_vline:
    and #3
    tax
    lda bitmap2l_colbyte,x
    sta g2l_cb

    lda X16_P4
    sta g2l_n
    ora X16_P5
    beq .vdone
    lda X16_P5
    sta g2l_n+1

    jsr bitmap2l_addr_calc
    lda #VERA_INC_80
    jsr bitmap2l_aim1
    lda #VERA_INC_80
    jsr bitmap2l_aim0

    lda X16_P0
    and #3
    tax
    lda g2l_cb
    and bitmap2l_pix,x
    sta g2l_ink                  ; ink and keep are loop-invariant
    lda bitmap2l_keep,x
    sta g2l_msk

    ldx g2l_n                    ; vera_fill's page-count idiom
    ldy g2l_n+1
    txa
    beq .vfull                  ; low byte 0 -> exactly hi*256 rows
    iny                         ; otherwise one extra partial page
.vfull:
.vloop:
    lda VERA_DATA1
    and g2l_msk
    ora g2l_ink
    sta VERA_DATA0
    dex
    bne .vloop
    dey
    bne .vloop
.vdone:
    rts

; ---------------------------------------------------------------------
; gfx2l_rect -- filled rectangle (no clipping)
;   in:  A = colour (0-3), X16_P0/P1 = x, X16_P2/P3 = y,
;        X16_P4/P5 = width, X16_P6/P7 = height
; ---------------------------------------------------------------------
gfx2l_rect:
    sta g2l_rc
    lda X16_P4
    sta g2l_rw
    lda X16_P5
    sta g2l_rw+1
    lda X16_P6
    sta g2l_rh
    lda X16_P7
    sta g2l_rh+1
.rrow:
    lda g2l_rh
    ora g2l_rh+1
    beq .rdone
    lda g2l_rw                   ; hline consumes the length: reload
    sta X16_P4
    lda g2l_rw+1
    sta X16_P5
    lda g2l_rc
    jsr gfx2l_hline              ; leaves P0..P3 alone
    inc X16_P2                  ; y += 1
    bne .ry_ok
    inc X16_P3
.ry_ok:
    lda g2l_rh
    bne .rh_ok
    dec g2l_rh+1
.rh_ok:
    dec g2l_rh
    bra .rrow
.rdone:
    rts

; ---------------------------------------------------------------------
; gfx2l_frame -- rectangle outline (no clipping)
;   same arguments as gfx2l_rect
; ---------------------------------------------------------------------
gfx2l_frame:
    sta g2l_rc
    ldx #7                      ; private copies: the edges reuse the
.take:                           ; parameter block as they go; g2l_fx..
    lda X16_P0,x                ; g2l_rh are laid out in P0..P7 order
    sta g2l_fx,x
    dex
    bpl .take

    jsr bitmap2l_f_span                 ; top edge
    jsr gfx2l_hline

    jsr bitmap2l_f_span                 ; bottom edge: y + h - 1
    clc
    lda g2l_fy
    adc g2l_rh
    sta X16_P2
    lda g2l_fy+1
    adc g2l_rh+1
    sta X16_P3
    lda X16_P2
    bne .f_nb1
    dec X16_P3
.f_nb1:
    dec X16_P2
    lda g2l_rc
    jsr gfx2l_hline

    jsr bitmap2l_f_col                  ; left edge
    jsr gfx2l_vline

    jsr bitmap2l_f_col                  ; right edge: x + w - 1
    clc
    lda g2l_fx
    adc g2l_rw
    sta X16_P0
    lda g2l_fx+1
    adc g2l_rw+1
    sta X16_P1
    lda X16_P0
    bne .f_nb2
    dec X16_P1
.f_nb2:
    dec X16_P0
    lda g2l_rc
    jmp gfx2l_vline

; x, y, width in the block, colour in A -- arguments for gfx2l_hline
bitmap2l_f_span:
    ldx #5
bitmap2l_fsp_l:
    lda g2l_fx,x
    sta X16_P0,x
    dex
    bpl bitmap2l_fsp_l
    lda g2l_rc
    rts

; x, y, height in the block, colour in A -- arguments for gfx2l_vline
bitmap2l_f_col:
    ldx #3
bitmap2l_fcl_l:
    lda g2l_fx,x
    sta X16_P0,x
    dex
    bpl bitmap2l_fcl_l
    lda g2l_rh
    sta X16_P4
    lda g2l_rh+1
    sta X16_P5
    lda g2l_rc
    rts

; ---------------------------------------------------------------------
; gfx2l_line -- Bresenham, any direction; plots through gfx2l_pset so
; the line clips at the screen edges
;   in:  A = colour (0-3)
;        X16_P0/P1 = x0, X16_P2/P3 = y0
;        X16_P4/P5 = x1, X16_P6/P7 = y1
; ---------------------------------------------------------------------
gfx2l_line:
    sta g2l_lc
    ldx #7                      ; P0..P7 -> g2l_lx0..g2l_ly1, which are
.take:                           ; laid out in the same order
    lda X16_P0,x
    sta g2l_lx0,x
    dex
    bpl .take

    ; dx = |x1 - x0|, sx = sign
    sec
    lda g2l_lx1
    sbc g2l_lx0
    sta g2l_ldx
    lda g2l_lx1+1
    sbc g2l_lx0+1
    sta g2l_ldx+1
    bpl .dx_pos
    sec
    lda #0
    sbc g2l_ldx
    sta g2l_ldx
    lda #0
    sbc g2l_ldx+1
    sta g2l_ldx+1
    lda #$FF
    sta g2l_lsx
    sta g2l_lsx+1
    bra .dx_done
.dx_pos:
    lda #$01
    sta g2l_lsx
    stz g2l_lsx+1
.dx_done:

    ; dy = -|y1 - y0|, sy = sign
    sec
    lda g2l_ly1
    sbc g2l_ly0
    sta g2l_lt
    lda g2l_ly1+1
    sbc g2l_ly0+1
    sta g2l_lt+1
    bpl .dy_pos
    sec
    lda #0
    sbc g2l_lt
    sta g2l_lt
    lda #0
    sbc g2l_lt+1
    sta g2l_lt+1
    lda #$FF
    sta g2l_lsy
    sta g2l_lsy+1
    bra .dy_done
.dy_pos:
    lda #$01
    sta g2l_lsy
    stz g2l_lsy+1
.dy_done:
    sec                         ; g2l_ldy = -|dy|
    lda #0
    sbc g2l_lt
    sta g2l_ldy
    lda #0
    sbc g2l_lt+1
    sta g2l_ldy+1

    clc                         ; err = dx + dy
    lda g2l_ldx
    adc g2l_ldy
    sta g2l_lerr
    lda g2l_ldx+1
    adc g2l_ldy+1
    sta g2l_lerr+1

.loop:
    lda g2l_lx0                  ; plot (x0, y0)
    sta X16_P0
    lda g2l_lx0+1
    sta X16_P1
    lda g2l_ly0
    sta X16_P2
    lda g2l_ly0+1
    sta X16_P3
    lda g2l_lc
    jsr gfx2l_pset

    lda g2l_lx0                  ; reached the end point?
    cmp g2l_lx1
    bne .step
    lda g2l_lx0+1
    cmp g2l_lx1+1
    bne .step
    lda g2l_ly0
    cmp g2l_ly1
    bne .step
    lda g2l_ly0+1
    cmp g2l_ly1+1
    bne .step
    rts

.step:
    lda g2l_lerr                 ; e2 = err * 2
    asl
    sta g2l_le2
    lda g2l_lerr+1
    rol
    sta g2l_le2+1

    ; if e2 >= dy  ->  err += dy, x0 += sx
    sec
    lda g2l_le2
    sbc g2l_ldy
    lda g2l_le2+1
    sbc g2l_ldy+1
    bvc .nv1
    eor #$80                    ; signed compare: fold overflow into sign
.nv1:
    bmi .skip_x
    clc
    lda g2l_lerr
    adc g2l_ldy
    sta g2l_lerr
    lda g2l_lerr+1
    adc g2l_ldy+1
    sta g2l_lerr+1
    clc
    lda g2l_lx0
    adc g2l_lsx
    sta g2l_lx0
    lda g2l_lx0+1
    adc g2l_lsx+1
    sta g2l_lx0+1
.skip_x:

    ; if e2 <= dx  ->  err += dx, y0 += sy
    sec
    lda g2l_ldx
    sbc g2l_le2
    lda g2l_ldx+1
    sbc g2l_le2+1
    bvc .nv2
    eor #$80
.nv2:
    bmi .skip_y
    clc
    lda g2l_lerr
    adc g2l_ldx
    sta g2l_lerr
    lda g2l_lerr+1
    adc g2l_ldx+1
    sta g2l_lerr+1
    clc
    lda g2l_ly0
    adc g2l_lsy
    sta g2l_ly0
    lda g2l_ly0+1
    adc g2l_lsy+1
    sta g2l_ly0+1
.skip_y:
    jmp .loop

; ---------------------------------------------------------------------
; gfx2l_pattern_set -- expand an 8x8 1bpp pattern for gfx2l_pattern_rect
;   in:  A = pattern low, X = pattern high (8 row bytes, top first;
;            bit 7 is the leftmost pixel)
;        Y = colours: (background << 2) | foreground
;
; Patterns tile from the screen origin, so each row expands to exactly
; two 2bpp bytes (16 bits); which of the pair a framebuffer byte uses
; is the parity of its address. The expansion is cached in g2l_pat.
; ---------------------------------------------------------------------
gfx2l_pattern_set:
    sta X16_T6                  ; T6/T7 = pattern pointer
    stx X16_T7
    tya
    and #3
    tax
    lda bitmap2l_colbyte,x              ; replicated foreground
    sta g2l_pfg
    tya
    lsr
    lsr
    and #3
    tax
    lda bitmap2l_colbyte,x              ; replicated background
    sta g2l_pbg

    ldx #0                      ; cache index (2 bytes per row)
    ldy #0                      ; pattern row
.prow:
    sty g2l_t
    lda (X16_T6),y
    sta g2l_pr                   ; the row's 8 bits, consumed by asl
    jsr bitmap2l_p_half                 ; pixels 0-3 -> even byte
    sta g2l_pat,x
    inx
    jsr bitmap2l_p_half                 ; pixels 4-7 -> odd byte
    sta g2l_pat,x
    inx
    ldy g2l_t
    iny
    cpy #8
    bne .prow
    rts

; expand the next 4 bits of g2l_pr (MSB first) into one 2bpp byte:
; a set bit becomes the foreground colour, a clear one the background
bitmap2l_p_half:
    stz g2l_t2
    ldy #0                      ; pixel 0..3 within the byte
.pbit:
    asl g2l_pr
    bcs .pfg
    lda g2l_pbg
    bra .pmix
.pfg:
    lda g2l_pfg
.pmix:
    and bitmap2l_pix,y                  ; keep just this pixel's two bits
    ora g2l_t2
    sta g2l_t2
    iny
    cpy #4
    bne .pbit
    lda g2l_t2
    rts

; ---------------------------------------------------------------------
; gfx2l_pattern_rect -- fill a rectangle with the current pattern
;   in:  X16_P0/P1 = x, X16_P2/P3 = y, X16_P4/P5 = width,
;        X16_P6/P7 = height   (no clipping)
; ---------------------------------------------------------------------
gfx2l_pattern_rect:
    lda X16_P4
    sta g2l_rw
    lda X16_P5
    sta g2l_rw+1
    lda X16_P6
    sta g2l_rh
    lda X16_P7
    sta g2l_rh+1
.yrow:
    lda g2l_rh
    ora g2l_rh+1
    beq .ydone
    jsr bitmap2l_p_row
    inc X16_P2
    bne .py_ok
    inc X16_P3
.py_ok:
    lda g2l_rh
    bne .ph_ok
    dec g2l_rh+1
.ph_ok:
    dec g2l_rh
    bra .yrow
.ydone:
    rts

; one pattern row at (P0..P3), width g2l_rw
bitmap2l_p_row:
    lda g2l_rw
    sta g2l_n
    ora g2l_rw+1
    bne .prgo
    rts
.prgo:
    lda g2l_rw+1
    sta g2l_n+1

    jsr bitmap2l_addr_calc

    ; the row's two pattern bytes, in address-parity order
    lda X16_P2
    and #7
    asl
    tax
    lda g2l_a0
    and #1
    beq .even
    inx                         ; an odd start address uses the odd
    lda g2l_pat,x                ; byte first
    sta g2l_pb0
    dex
    lda g2l_pat,x
    sta g2l_pb1
    bra .parity_done
.even:
    lda g2l_pat,x
    sta g2l_pb0
    inx
    lda g2l_pat,x
    sta g2l_pb1
.parity_done:

    lda X16_P0
    and #3
    sta g2l_p
    bne .phead
    lda g2l_n+1
    bne .pmiddle
    lda g2l_n
    cmp #4
    bcs .pmiddle

.phead:
    jsr bitmap2l_headmask               ; mask -> A, head pixel count -> g2l_t
    tax                         ; mask in X for bitmap2l_rmwp
    lda g2l_pb0
    jsr bitmap2l_rmwp
    jsr bitmap2l_headadv
    lda g2l_pb0                  ; next byte has the other parity
    ldx g2l_pb1
    sta g2l_pb1
    stx g2l_pb0

.pmiddle:
    jsr bitmap2l_quadcount
    beq .ptail

    lda #VERA_INC_1
    jsr bitmap2l_aim0
    ldx g2l_m                    ; vera_fill's page-count idiom
    ldy g2l_m+1
    txa
    beq .pfull
    iny
.pfull:
.ploop:
    lda g2l_pb0
    sta VERA_DATA0
    lda g2l_pb0                  ; swap the parity pair
    pha
    lda g2l_pb1
    sta g2l_pb0
    pla
    sta g2l_pb1
    dex
    bne .ploop
    dey
    bne .ploop
    jsr bitmap2l_a_addm                 ; addr += m

.ptail:
    jsr bitmap2l_tailmask
    beq .prdone
    tax
    lda g2l_pb0
    jsr bitmap2l_rmwp
.prdone:
    rts

; ---------------------------------------------------------------------
; gfx2l_blit -- copy a byte-aligned image from CPU RAM into the bitmap
;   in:  A = raster op: 0 copy, 1 OR, 2 AND, 3 XOR
;        X16_P0/P1 = x (bits 1:0 ignored: byte-aligned),
;        X16_P2/P3 = y, X16_P4 = width in BYTES (4-pixel units),
;        X16_P5 = height in rows, X16_P6/P7 = source (row-major)
;
; The source pointer is X16_PTR3 -- P6/P7 double as real zero page, so
; (PTR3),y addressing costs nothing extra. No clipping.
; ---------------------------------------------------------------------
; The three RMW ops share one loop whose opcode at bitmap2l_g2l_blit_op is patched
; from bitmap2l_g2l_optab (ora/and/eor (zp),y) -- the 8bpp module's gfx8l_blit
; does the same.
gfx2l_blit:
    and #3
    sta g2l_op                   ; copy (op 0) needs no opcode patch
    beq .anon0
    tax
    lda bitmap2l_g2l_optab-1,x
    sta bitmap2l_g2l_blit_op
.anon0:	jsr bitmap2l_addr_calc
    lda X16_P5
    sta g2l_h
bitmap2l_g2l_blit_row:
    lda #VERA_INC_1
    jsr bitmap2l_aim1                   ; ops read through port 1...
    lda #VERA_INC_1
    jsr bitmap2l_aim0                   ; ...and everything writes port 0
    ldy #0
    lda g2l_op
    beq bitmap2l_g2l_blit_copy
bitmap2l_g2l_blit_rmw:
    lda VERA_DATA1
bitmap2l_g2l_blit_op:
    ora (X16_PTR3),y            ; opcode patched: op 1/2/3 = ora/and/eor
    sta VERA_DATA0
    iny
    cpy X16_P4
    bne bitmap2l_g2l_blit_rmw
    bra bitmap2l_g2l_blit_done
bitmap2l_g2l_blit_copy:
    lda (X16_PTR3),y
    sta VERA_DATA0
    iny
    cpy X16_P4
    bne bitmap2l_g2l_blit_copy
bitmap2l_g2l_blit_done:
    clc                         ; src += width
    lda X16_PTR3
    adc X16_P4
    sta X16_PTR3
    bcc .anon1
    inc X16_PTR3+1
.anon1:	jsr bitmap2l_a_row                  ; dest += one row
    dec g2l_h
    bne bitmap2l_g2l_blit_row
    rts

; ---------------------------------------------------------------------
; gfx2l_blitm -- masked blit of pre-shifted column-major data
;   in:  X16_P0/P1 = x (any pixel position), X16_P2/P3 = y,
;        X16_P4 = height in rows (1-127), X16_P5 = width in COLUMNS
;        (framebuffer bytes), X16_P6/P7 = source
;
; The source holds, for each of the P5 columns, P4 (mask, data) byte
; PAIRS walking down the rows: fb' = (fb AND mask) OR data. The caller
; supplies data already shifted for this x's pixel phase (x & 3) --
; pre-shifted glyph caches are the whole point: at 833 cycles per 8x8
; glyph this is what makes proportional text affordable (spike-proven;
; see the CXRF project). No clipping.
; ---------------------------------------------------------------------
gfx2l_blitm:
    jsr bitmap2l_addr_calc
    lda X16_P5
    sta g2l_w
.mcol:
    lda #VERA_INC_80
    jsr bitmap2l_aim1
    lda #VERA_INC_80
    jsr bitmap2l_aim0
    ldy #0
    ldx X16_P4
.mrow:
    lda VERA_DATA1
    and (X16_PTR3),y            ; mask byte
    iny
    ora (X16_PTR3),y            ; data byte
    iny
    sta VERA_DATA0
    dex
    bne .mrow

    clc                         ; src += 2 * height (one column)
    tya
    adc X16_PTR3
    sta X16_PTR3
    bcc .msrc_ok
    inc X16_PTR3+1
.msrc_ok:
    jsr bitmap2l_a_inc                  ; dest: next byte column
    dec g2l_w
    bne .mcol
    rts

; ---------------------------------------------------------------------
; module plumbing
; ---------------------------------------------------------------------

; carry clear if (P0/P1, P2/P3) is on screen
bitmap2l_onscreen:
    lda X16_P1                  ; x < 320?
    cmp #>GFX2L_WIDTH
    bcc .x_ok
    bne .bad
    lda X16_P0
    cmp #<GFX2L_WIDTH
    bcs .bad
.x_ok:
    lda X16_P3                  ; y < 240?
    cmp #>GFX2L_HEIGHT
    bcc .ok
    bne .bad
    lda X16_P2
    cmp #<GFX2L_HEIGHT
    bcs .bad
.ok:
    clc
    rts
.bad:
    sec
    rts

; g2l_a2:a1:a0 = y*80 + (x>>2)   (from X16_P0..P3; clobbers T0..T2)
bitmap2l_addr_calc:
    lda X16_P2                  ; t = y << 4
    sta g2l_a0
    lda X16_P3
    sta g2l_a1
    asl g2l_a0
    rol g2l_a1
    asl g2l_a0
    rol g2l_a1
    asl g2l_a0
    rol g2l_a1
    asl g2l_a0
    rol g2l_a1

    lda g2l_a0                   ; T2:T1:T0 = t << 2
    sta X16_T0
    lda g2l_a1
    sta X16_T1
    stz X16_T2
    asl X16_T0
    rol X16_T1
    rol X16_T2
    asl X16_T0
    rol X16_T1
    rol X16_T2

    clc                         ; y*80 = t + (t << 2)
    lda g2l_a0
    adc X16_T0
    sta g2l_a0
    lda g2l_a1
    adc X16_T1
    sta g2l_a1
    lda #0
    adc X16_T2
    sta g2l_a2

    lda X16_P1                  ; + x >> 2
    sta X16_T1
    lda X16_P0
    lsr X16_T1
    ror
    lsr X16_T1
    ror
    clc
    adc g2l_a0
    sta g2l_a0
    lda X16_T1
    adc g2l_a1
    sta g2l_a1
    lda #0
    adc g2l_a2
    sta g2l_a2
    rts

; point port 0 (write side) at g2l_a; A = increment index.
; Scratch is g2l_inc, NOT g2l_t: hline/pattern hold a pixel count in
; g2l_t across the bitmap2l_rmw call, and bitmap2l_rmw aims through here.
bitmap2l_aim0:
    asl
    asl
    asl
    asl
    sta g2l_inc
    lda #VERA_CTRL_ADDRSEL
    trb VERA_CTRL
    bra bitmap2l_aimgo

; point port 1 (read side) at g2l_a; A = increment index
bitmap2l_aim1:
    asl
    asl
    asl
    asl
    sta g2l_inc
    lda #VERA_CTRL_ADDRSEL
    tsb VERA_CTRL
bitmap2l_aimgo:
    lda g2l_a0
    sta VERA_ADDR_L
    lda g2l_a1
    sta VERA_ADDR_M
    lda g2l_a2
    and #VERA_ADDR_H_BANK
    ora g2l_inc
    sta VERA_ADDR_H
    rts

; the three-phase span geometry, shared by gfx2l_hline and bitmap2l_p_row:
;   bitmap2l_headmask:  from phase g2l_p and count g2l_n, the head pixel count
;               -> g2l_t and the pixel mask (from[p] AND upto[q]) -> A
;   bitmap2l_headadv:   n -= the head pixels; step g2l_a to the whole bytes
;   bitmap2l_quadcount: g2l_m = n >> 2 whole bytes; Z set when there are none
;   bitmap2l_a_addm:    g2l_a += m (skip what vera_fill / the pair loop wrote)
;   bitmap2l_tailmask:  the pixels 0..n-1 tail mask -> A; Z set when no tail
bitmap2l_headmask:
    lda g2l_n+1                  ; q = last head pixel = min(3, p+n-1)
    bne bitmap2l_hmqmax                 ; a long span always reaches pixel 3
    clc
    lda g2l_p
    adc g2l_n
    bcs bitmap2l_hmqmax                 ; p + n carried: certainly past pixel 3
    dec a
    cmp #4
    bcc bitmap2l_hmqgot
bitmap2l_hmqmax:
    lda #3
bitmap2l_hmqgot:
    tay                         ; Y = q
    sec                         ; head pixel count = q - p + 1
    iny
    tya
    sbc g2l_p
    sta g2l_t
    ldx g2l_p
    lda bitmap2l_from,x
    dey
    and bitmap2l_upto,y
    rts

bitmap2l_headadv:
    sec                         ; n -= head pixels
    lda g2l_n
    sbc g2l_t
    sta g2l_n
    lda g2l_n+1
    sbc #0
    sta g2l_n+1
    jmp bitmap2l_a_inc                  ; step to the first whole byte

bitmap2l_quadcount:
    lda g2l_n+1
    sta g2l_m+1
    lda g2l_n
    lsr g2l_m+1
    ror
    lsr g2l_m+1
    ror
    sta g2l_m
    ora g2l_m+1
    rts

bitmap2l_a_addm:
    clc
    lda g2l_a0
    adc g2l_m
    sta g2l_a0
    lda g2l_a1
    adc g2l_m+1
    sta g2l_a1
    lda g2l_a2
    adc #0
    sta g2l_a2
    rts

bitmap2l_tailmask:
    lda g2l_n
    and #3
    beq bitmap2l_tmnone
    tay
    dey                         ; tail covers pixels 0..n-1
    lda bitmap2l_upto,y                 ; never zero, so Z stays clear
bitmap2l_tmnone:
    rts

; read-modify-write the byte at g2l_a through a pixel mask:
; fb' = (fb AND NOT mask) OR (ink AND mask). INC_0 keeps the port in
; place, so one aim serves both the read and the write.
;   bitmap2l_rmw:  A = mask, ink is the solid colour byte g2l_cb
;   bitmap2l_rmwp: A = ink byte, X = mask (the pattern-row variant)
bitmap2l_rmw:
    tax
    lda g2l_cb
bitmap2l_rmwp:
    sta g2l_ink
    stx g2l_msk
    lda #VERA_INC_0
    jsr bitmap2l_aim0
    lda g2l_msk
    eor #$FF
    and VERA_DATA0
    sta g2l_t2
    lda g2l_ink
    and g2l_msk
    ora g2l_t2
    sta VERA_DATA0
    rts

; g2l_a += 1 (24-bit)
bitmap2l_a_inc:
    inc g2l_a0
    bne .ai_done
    inc g2l_a1
    bne .ai_done
    inc g2l_a2
.ai_done:
    rts

; g2l_a += one framebuffer row
bitmap2l_a_row:
    clc
    lda g2l_a0
    adc #GFX2L_STRIDE
    sta g2l_a0
    lda g2l_a1
    adc #0
    sta g2l_a1
    lda g2l_a2
    adc #0
    sta g2l_a2
    rts

; ---------------------------------------------------------------------
; module variables (never live across a call boundary)
; ---------------------------------------------------------------------
g2l_a0: byte 0
g2l_a1: byte 0
g2l_a2: byte 0
g2l_c:  byte 0
g2l_cb: byte 0
g2l_p:  byte 0
g2l_n:  word 0
g2l_m:  word 0
g2l_t:  byte 0
g2l_t2: byte 0
g2l_inc:byte 0
g2l_msk:byte 0
g2l_ink:byte 0
g2l_op: byte 0
g2l_h:  byte 0
g2l_w:  byte 0

; g2l_fx..g2l_rh are laid out in X16_P0..P7 order so gfx2l_frame can take
; and restore the block with a loop
g2l_fx: word 0
g2l_fy: word 0
g2l_rw: word 0
g2l_rh: word 0
g2l_rc: byte 0

g2l_pfg:byte 0
g2l_pbg:byte 0
g2l_pr: byte 0
g2l_pb0:byte 0
g2l_pb1:byte 0
g2l_pat:reserve 16, 0

g2l_lc:  byte 0
g2l_lx0: word 0
g2l_ly0: word 0
g2l_lx1: word 0
g2l_ly1: word 0
g2l_ldx: word 0
g2l_ldy: word 0
g2l_lerr:word 0
g2l_le2: word 0
g2l_lsx: word 0
g2l_lsy: word 0
g2l_lt:  word 0

bitmap2l_colbyte:
    byte $00, $55, $AA, $FF   ; a colour in all four pixels
bitmap2l_pix:
    byte $C0, $30, $0C, $03   ; the bits of pixel 0..3
bitmap2l_keep:
    byte $3F, $CF, $F3, $FC   ; everything but pixel 0..3
bitmap2l_from:
    byte $FF, $3F, $0F, $03   ; pixels p..3
bitmap2l_upto:
    byte $C0, $F0, $FC, $FF   ; pixels 0..q
bitmap2l_g2l_optab:
    byte $11, $31, $51        ; ora/and/eor (zp),y, for gfx2l_blit

; (end zone)
