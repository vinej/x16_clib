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
        .include        "x16zp.inc"

; (import dropped: popa, popax)
; (import dropped: X16_TPTR0, X16_TPTR1)

        .globl  x16_graph_init
        .globl  x16_graph_clear
        .globl  x16_graph_set_window
        .globl  x16_graph_set_colors
        .globl  x16_graph_draw_line
        .globl  x16_graph_draw_rect
        .globl  x16_graph_move_rect
        .globl  x16_graph_draw_oval
        .globl  x16_graph_draw_image
        .globl  x16_graph_set_font
        .globl  x16_graph_get_char_size
        .globl  x16_graph_put_char

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_init(const void *driver)
;
; `driver` points to an FB_* vector table, or NULL for the default
; 320x240@8bpp driver. Restore text with x16_screen_set_mode().
; ---------------------------------------------------------------------
x16_graph_init:
        jmp     GRAPH_INIT              ; the pointer already sits in r0

; ---------------------------------------------------------------------
; void x16_graph_clear(void)
;   clears the current window to the background color
; ---------------------------------------------------------------------
x16_graph_clear:
        jmp     GRAPH_CLEAR

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_set_window(unsigned int x, unsigned int y,
;                                        unsigned int width,
;                                        unsigned int height)
;
; Clip everything to this rectangle. All zeroes resets to full screen.
; ---------------------------------------------------------------------
x16_graph_set_window:
        pha                             ; A and X hold the first
        phx                             ; argument; the loads below
                                        ; clobber both, so park them
        lda     mos8(__rc7)
        sta     r3H
        lda     mos8(__rc6)
        sta     r3L                ; height (rightmost arg: A/X)
        lda     mos8(__rc5)
        sta     r2H
        lda     mos8(__rc4)
        sta     r2L                ; width
        lda     mos8(__rc3)
        sta     r1H
        lda     mos8(__rc2)
        sta     r1L                ; y
        plx
        pla
        sta     r0L                ; x
        stx     r0H
        jmp     GRAPH_SET_WINDOW

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_set_colors(unsigned char stroke,
;                                        unsigned char fill,
;                                        unsigned char background)
; ---------------------------------------------------------------------
x16_graph_set_colors:
        ldy     mos8(__rc2)             ; Y = background; stroke and fill
        jmp     GRAPH_SET_COLORS        ; already sit in A and X

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_draw_line(unsigned int x1, unsigned int y1,
;                                       unsigned int x2, unsigned int y2)
; ---------------------------------------------------------------------
x16_graph_draw_line:
        pha                             ; A and X hold the first
        phx                             ; argument; the loads below
                                        ; clobber both, so park them
        lda     mos8(__rc7)
        sta     r3H
        lda     mos8(__rc6)
        sta     r3L                ; y2 (rightmost arg: A/X)
        lda     mos8(__rc5)
        sta     r2H
        lda     mos8(__rc4)
        sta     r2L                ; x2
        lda     mos8(__rc3)
        sta     r1H
        lda     mos8(__rc2)
        sta     r1L                ; y1
        plx
        pla
        sta     r0L                ; x1
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
x16_graph_draw_rect:
        pha                             ; park x low
        phx                             ; and x high
        lda     mos8(__rc10)            ; the fill flag, before r4H lands
        pha                             ; on top of __rc10's neighbour
        lda     mos8(__rc9)             ; highest destination first: every
        sta     r4H                     ; source is one KERNAL register
        lda     mos8(__rc8)             ; below its destination
        sta     r4L                     ; radius
        lda     mos8(__rc7)
        sta     r3H
        lda     mos8(__rc6)
        sta     r3L                     ; height
        lda     mos8(__rc5)
        sta     r2H
        lda     mos8(__rc4)
        sta     r2L                     ; width
        lda     mos8(__rc3)
        sta     r1H
        lda     mos8(__rc2)
        sta     r1L                     ; y
        pla                             ; A = fill flag
        tay
        plx                             ; X = x high
        pla                             ; A = x low
        sta     r0L
        stx     r0H
        tya
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
x16_graph_move_rect:
        pha                             ; A and X hold the first
        phx                             ; argument; the loads below
                                        ; clobber both, so park them
        lda     mos8(__rc11)
        sta     r5H
        lda     mos8(__rc10)
        sta     r5L                ; height (rightmost arg: A/X)
        lda     mos8(__rc9)
        sta     r4H
        lda     mos8(__rc8)
        sta     r4L                ; width
        lda     mos8(__rc7)
        sta     r3H
        lda     mos8(__rc6)
        sta     r3L                ; ty
        lda     mos8(__rc5)
        sta     r2H
        lda     mos8(__rc4)
        sta     r2L                ; tx
        lda     mos8(__rc3)
        sta     r1H
        lda     mos8(__rc2)
        sta     r1L                ; sy
        plx
        pla
        sta     r0L                ; sx
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
x16_graph_draw_oval:
        pha                             ; park x low
        phx                             ; and x high
        lda     mos8(__rc8)             ; the fill flag, read first
        pha
        lda     mos8(__rc7)             ; highest destination first
        sta     r3H
        lda     mos8(__rc6)
        sta     r3L                     ; height
        lda     mos8(__rc5)
        sta     r2H
        lda     mos8(__rc4)
        sta     r2L                     ; width
        lda     mos8(__rc3)
        sta     r1H
        lda     mos8(__rc2)
        sta     r1L                     ; y
        pla                             ; A = fill flag
        tay
        plx                             ; X = x high
        pla                             ; A = x low
        sta     r0L
        stx     r0H
        tya
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
x16_graph_draw_image:
        pha                             ; A and X hold the first
        phx                             ; argument; the loads below
                                        ; clobber both, so park them
        lda     mos8(__rc9)
        sta     r4H
        lda     mos8(__rc8)
        sta     r4L                ; height (rightmost arg: A/X)
        lda     mos8(__rc7)
        sta     r3H
        lda     mos8(__rc6)
        sta     r3L                ; width
        lda     mos8(__rc5)
        sta     r2H
        lda     mos8(__rc4)
        sta     r2L                ; image
        lda     mos8(__rc3)
        sta     r1H
        lda     mos8(__rc2)
        sta     r1L                ; y
        plx
        pla
        sta     r0L                ; x
        stx     r0H
        jmp     GRAPH_DRAW_IMAGE

; ---------------------------------------------------------------------
; void __fastcall__ x16_graph_set_font(const void *font)
;   NULL restores the system font
; ---------------------------------------------------------------------
x16_graph_set_font:
        jmp     GRAPH_SET_FONT              ; the pointer already sits in r0

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_graph_get_char_size(unsigned char c,
;                                                    unsigned char style,
;                                                    x16_char_size *out)
;   returns 1 for a printable character: out->baseline/width/height set
;   returns 0 for a control code:        out->style = the style it selects
; ---------------------------------------------------------------------
x16_graph_get_char_size:
        ldy     mos8(__rc2)             ; out*: staged out of r0 before
        sty     mos8(X16_TPTR0)         ; the ROM call overwrites it
        ldy     mos8(__rc3)
        sty     mos8(X16_TPTR0+1)       ; c stays in A, style in X
        jsr     GRAPH_GET_CHAR_SIZE
        bcs     .Lx16_graph_get_char_size_control

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

.Lx16_graph_get_char_size_control:
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
x16_graph_put_char:
        pha                             ; park c
        lda     mos8(__rc2)             ; x* and y* live in r0/r1, which
        sta     mos8(X16_TPTR0)         ; the ROM call is about to use
        lda     mos8(__rc3)
        sta     mos8(X16_TPTR0+1)
        lda     mos8(__rc4)             ; y*
        sta     mos8(X16_TPTR1)
        lda     mos8(__rc5)
        sta     mos8(X16_TPTR1+1)

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
        bcs     .Lx16_graph_put_char_clipped
        lda     #1
.Lx16_graph_put_char_clipped:
        rts
