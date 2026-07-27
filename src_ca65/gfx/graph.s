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

        .include        "macros.inc"

        .import         popa, popax
        .importzp       ptr1, ptr2

        .export         _x16_graph_init
        .export         _x16_graph_clear
        .export         _x16_graph_set_window
        .export         _x16_graph_set_colors
        .export         _x16_graph_draw_line
        .export         _x16_graph_draw_rect
        .export         _x16_graph_move_rect
        .export         _x16_graph_draw_oval
        .export         _x16_graph_draw_image
        .export         _x16_graph_set_font
        .export         _x16_graph_get_char_size
        .export         _x16_graph_put_char

        .segment        "CODE"

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_init(const void *driver)
;
; `driver` points to an FB_* vector table, or NULL for the default
; 320x240@8bpp driver. Restore text with x16_screen_set_mode().
; ---------------------------------------------------------------------
_x16_graph_init:
        sta     r0L
        stx     r0H
        jmp     GRAPH_INIT

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
        sta     r3L                     ; height (rightmost arg: A/X)
        stx     r3H
        jsr     popax
        sta     r2L                     ; width
        stx     r2H
        jsr     popax
        sta     r1L                     ; y
        stx     r1H
        jsr     popax
        sta     r0L                     ; x
        stx     r0H
        jmp     GRAPH_SET_WINDOW

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_set_colors(unsigned char stroke,
;                                        unsigned char fill,
;                                        unsigned char background)
; ---------------------------------------------------------------------
_x16_graph_set_colors:
        pha                             ; background (rightmost arg, in A)
        jsr     popa                    ; fill
        pha
        jsr     popa                    ; A = stroke
        plx                             ; X = fill
        ply                             ; Y = background
        jmp     GRAPH_SET_COLORS

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_draw_line(unsigned int x1, unsigned int y1,
;                                       unsigned int x2, unsigned int y2)
; ---------------------------------------------------------------------
_x16_graph_draw_line:
        sta     r3L                     ; y2 (rightmost arg: A/X)
        stx     r3H
        jsr     popax
        sta     r2L                     ; x2
        stx     r2H
        jsr     popax
        sta     r1L                     ; y1
        stx     r1H
        jsr     popax
        sta     r0L                     ; x1
        stx     r0H
        jmp     GRAPH_DRAW_LINE

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
        pha                             ; fill flag (rightmost arg, in A)
        jsr     popax
        sta     r4L                     ; radius
        stx     r4H
        jsr     popax
        sta     r3L                     ; height
        stx     r3H
        jsr     popax
        sta     r2L                     ; width
        stx     r2H
        jsr     popax
        sta     r1L                     ; y
        stx     r1H
        jsr     popax
        sta     r0L                     ; x
        stx     r0H
        pla
        cmp     #1                      ; carry set iff fill != 0
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
        sta     r5L                     ; height (rightmost arg: A/X)
        stx     r5H
        jsr     popax
        sta     r4L                     ; width
        stx     r4H
        jsr     popax
        sta     r3L                     ; ty
        stx     r3H
        jsr     popax
        sta     r2L                     ; tx
        stx     r2H
        jsr     popax
        sta     r1L                     ; sy
        stx     r1H
        jsr     popax
        sta     r0L                     ; sx
        stx     r0H
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
        pha                             ; fill flag (rightmost arg, in A)
        jsr     popax
        sta     r3L                     ; height
        stx     r3H
        jsr     popax
        sta     r2L                     ; width
        stx     r2H
        jsr     popax
        sta     r1L                     ; y
        stx     r1H
        jsr     popax
        sta     r0L                     ; x
        stx     r0H
        pla
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
        sta     r4L                     ; height (rightmost arg: A/X)
        stx     r4H
        jsr     popax
        sta     r3L                     ; width
        stx     r3H
        jsr     popax
        sta     r2L                     ; image
        stx     r2H
        jsr     popax
        sta     r1L                     ; y
        stx     r1H
        jsr     popax
        sta     r0L                     ; x
        stx     r0H
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
        sta     ptr1                    ; out* (rightmost arg: A/X)
        stx     ptr1+1
        jsr     popa                    ; style
        pha
        jsr     popa                    ; A = c
        plx                             ; X = style
        jsr     GRAPH_GET_CHAR_SIZE
        bcs     @control

        phy                             ; height
        phx                             ; width
        ldy     #0
        sta     (ptr1),y                ; baseline
        iny
        pla
        sta     (ptr1),y                ; width
        iny
        pla
        sta     (ptr1),y                ; height
        lda     #1
        ldx     #0
        rts

@control:
        txa                             ; the new style
        ldy     #3
        sta     (ptr1),y
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
        pha                             ; c (rightmost arg, in A)
        jsr     popax                   ; y*
        sta     ptr2
        stx     ptr2+1
        jsr     popax                   ; x*
        sta     ptr1
        stx     ptr1+1

        ldy     #0
        lda     (ptr1),y
        sta     r0L
        lda     (ptr2),y
        sta     r1L
        iny
        lda     (ptr1),y
        sta     r0H
        lda     (ptr2),y
        sta     r1H

        pla                             ; A = c
        jsr     GRAPH_PUT_CHAR          ; r0/r1 advance, carry = clipped

        ldy     #0                      ; nothing below disturbs the carry
        lda     r0L
        sta     (ptr1),y
        lda     r1L
        sta     (ptr2),y
        iny
        lda     r0H
        sta     (ptr1),y
        lda     r1H
        sta     (ptr2),y

        lda     #0
        ldx     #0
        bcs     @clipped
        lda     #1
@clipped:
        rts
