; =====================================================================
; x16clib :: gfx/graph.s -- the KERNAL GRAPH drawing API
; =====================================================================
; Wrappers over the ROM's higher-level drawing layer (GEOS-derived). It
; draws through whatever framebuffer driver is active -- see gfx/fb.s.
;
; x16_graph_init(NULL) switches the display to 320x240@8bpp, installs
; the default driver, resets the window, colors and font, and clears.
; It is the required entry point for this whole family, fb.s included.
;
; Colors: stroke is the pen for lines, outlines, and glyphs; filled
; rects use it too. fill is the secondary color (rect frames in filled
; mode); background is what clear/erase paints.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3
        zpage	r4
        zpage	r5
        zpage	sp


        global	_x16_graph_init
        global	_x16_graph_clear
        global	_x16_graph_set_window
        global	_x16_graph_set_colors
        global	_x16_graph_draw_line
        global	_x16_graph_draw_rect
        global	_x16_graph_move_rect
        global	_x16_graph_draw_oval
        global	_x16_graph_draw_image
        global	_x16_graph_set_font
        global	_x16_graph_get_char_size
        global	_x16_graph_put_char

        section text

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_init(const void *driver)
;
; `driver` points to an FB_* vector table, or NULL for the default
; 320x240@8bpp driver. Restore text with x16_screen_set_mode().
; ---------------------------------------------------------------------
_x16_graph_init:
        jmp     GRAPH_INIT              ; driver already rides r0/r1 = r0

; ---------------------------------------------------------------------
; void x16_graph_clear(void)
;   clears the current window to the background color
; ---------------------------------------------------------------------
_x16_graph_clear:
        jmp     GRAPH_CLEAR

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_set_window(unsigned int x, unsigned int y,
;                                        unsigned int width,
;                                        unsigned int height)
;
; Clip everything to this rectangle. All zeroes resets to full screen.
; ---------------------------------------------------------------------
_x16_graph_set_window:
        jmp     GRAPH_SET_WINDOW        ; x, y, width, height already sit in
                                        ; the KERNAL's r0..r3

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_set_colors(unsigned char stroke,
;                                        unsigned char fill,
;                                        unsigned char background)
; ---------------------------------------------------------------------
_x16_graph_set_colors:
        lda     r0                      ; stroke
        ldx     r2                      ; fill
        ldy     r4                      ; background
        jmp     GRAPH_SET_COLORS

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_draw_line(unsigned int x1, unsigned int y1,
;                                       unsigned int x2, unsigned int y2)
; ---------------------------------------------------------------------
_x16_graph_draw_line:
        jmp     GRAPH_DRAW_LINE         ; x1, y1, x2, y2 are already r0..r3

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_draw_rect(unsigned int x, unsigned int y,
;                                       unsigned int width,
;                                       unsigned int height,
;                                       unsigned int radius,
;                                       unsigned char fill)
;
; fill 0 draws the outline in the stroke color; nonzero fills. The
; corner radius is accepted for API parity but the ROM ignores it.
; ---------------------------------------------------------------------
_x16_graph_draw_rect:
        ldy     #2                      ; x, y, width and height already sit
        lda     (sp),y                  ; in r0..r3; radius and fill spilled
        cmp     #1                      ; carry set iff fill != 0
        dey                             ; -- and lda/dey/sta leave it alone
        lda     (sp),y
        sta     r4H                     ; radius
        dey
        lda     (sp),y
        sta     r4L
        jmp     GRAPH_DRAW_RECT

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_move_rect(unsigned int sx, unsigned int sy,
;                                       unsigned int tx, unsigned int ty,
;                                       unsigned int width,
;                                       unsigned int height)
;
; Copies the rectangle; source and target may overlap.
; ---------------------------------------------------------------------
_x16_graph_move_rect:
        ldy     #3                      ; sx, sy, tx, ty are already r0..r3;
        lda     (sp),y                  ; width and height spilled
        sta     r5H
        dey
        lda     (sp),y
        sta     r5L                     ; height
        dey
        lda     (sp),y
        sta     r4H
        dey
        lda     (sp),y
        sta     r4L                     ; width
        jmp     GRAPH_MOVE_RECT

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_draw_oval(unsigned int x, unsigned int y,
;                                       unsigned int width,
;                                       unsigned int height,
;                                       unsigned char fill)
;
; The oval inscribes the bounding box. fill as in draw_rect.
; ---------------------------------------------------------------------
_x16_graph_draw_oval:
        ldy     #0                      ; x, y, width, height are r0..r3
        lda     (sp),y                  ; and the fill flag spilled
        cmp     #1                      ; carry set iff fill != 0
        jmp     GRAPH_DRAW_OVAL

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_draw_image(unsigned int x, unsigned int y,
;                                        const unsigned char *image,
;                                        unsigned int width,
;                                        unsigned int height)
;
; `image` is width*height 8-bit pixels, row by row.
; ---------------------------------------------------------------------
_x16_graph_draw_image:
        ldy     #1                      ; x, y, image and width already sit
        lda     (sp),y                  ; in r0..r3; only height spilled
        sta     r4H
        dey
        lda     (sp),y
        sta     r4L
        jmp     GRAPH_DRAW_IMAGE

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_set_font(const void *font)
;   NULL restores the system font
; ---------------------------------------------------------------------
_x16_graph_set_font:
        sta     r0L
        stx     r0H
        jmp     GRAPH_SET_FONT

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_graph_get_char_size(unsigned char c,
;                                                    unsigned char style,
;                                                    x16_char_size *out)
;   returns 1 for a printable character: out->baseline/width/height set
;   returns 0 for a control code:        out->style = the style it selects
; ---------------------------------------------------------------------
_x16_graph_get_char_size:
        lda     r4                      ; out*, staged clear of the KERNAL
        sta     X16_TPTR0               ; registers this call uses
        lda     r5
        sta     X16_TPTR0+1
        lda     r0                      ; c
        ldx     r2                      ; style
        jsr     GRAPH_GET_CHAR_SIZE
        bcs     .control

        phy                             ; height
        phx                             ; width
        ldy     #0
        sta     (X16_TPTR0),y                ; baseline
        iny
        pla
        sta     (X16_TPTR0),y                ; width
        iny
        pla
        sta     (X16_TPTR0),y                ; height
        lda     #1
        ldx     #0
        rts

.control:
        txa                             ; the new style
        ldy     #3
        sta     (X16_TPTR0),y
        lda     #0
        tax
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_graph_put_char(unsigned int *x,
;                                               unsigned int *y,
;                                               unsigned char c)
;   returns 1 if the character landed inside the window, 0 if clipped;
;   *x/*y advance to the next character position either way
;
; `y` is the BASELINE, not the glyph top. Control codes ($01-$1F and
; the CON_ATTR_*/style codes) move the position or restyle the pen.
; ---------------------------------------------------------------------
_x16_graph_put_char:
        lda     r0                      ; x* and y* ride r0/r1 and r2/r3,
        sta     X16_TPTR0               ; which ARE the KERNAL r0 and r1
        lda     r1                      ; this call reads and advances
        sta     X16_TPTR0+1
        lda     r2
        sta     X16_TPTR1
        lda     r3
        sta     X16_TPTR1+1
        lda     r4                      ; c
        pha

        ldy     #0
        lda     (X16_TPTR0),y
        sta     r0L
        lda     (X16_TPTR1),y
        sta     r1L
        iny
        lda     (X16_TPTR0),y
        sta     r0H
        lda     (X16_TPTR1),y
        sta     r1H

        pla                             ; A = c
        jsr     GRAPH_PUT_CHAR          ; r0/r1 advance, carry = clipped

        ldy     #0                      ; nothing below disturbs the carry
        lda     r0L
        sta     (X16_TPTR0),y
        lda     r1L
        sta     (X16_TPTR1),y
        iny
        lda     r0H
        sta     (X16_TPTR0),y
        lda     r1H
        sta     (X16_TPTR1),y

        lda     #0
        ldx     #0
        bcs     .clipped
        lda     #1
.clipped:
        rts
