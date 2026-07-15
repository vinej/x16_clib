; =====================================================================
; x16clib :: gfx/bitmap.s -- 320x240x256 bitmap drawing
; =====================================================================
; The framebuffer is 8bpp at VRAM $00000, one byte per pixel, rows of
; 320. A pixel is at y*320 + x.
;
; gfx_pset clips. The line/rect primitives do NOT: they assume their
; arguments are on screen. Clipping every span would cost more than it
; saves for a caller that already knows its geometry.
;
; Nothing here changes the screen mode. Call x16_gfx_init() once to
; switch the display to bitmap mode; the drawing routines only touch
; VRAM, so they also work on an off-screen buffer.
;
; Every C shim pops straight into the parameter block. popa and popax
; touch only A, X and Y, so no staging temporaries are needed -- unlike
; the sprite module, whose internal routines want arguments in X.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

; (import: vera_fill)
; (import: screen_set_mode)
; vbcc argument registers, plus the C soft-stack pointer for the fifth
; argument of gfx_rect/frame/line. The x/cx coordinate is a 16-bit int in
; r0/r1; y/color and other chars each take an even register (r2, r4, r6);
; a 16-bit len/width or a string pointer takes r4/r5 (or r6/r7).
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r4
        zpage	r5
        zpage	r6
        zpage	r7
        zpage	sp

        global	_x16_gfx_init
        global	_x16_gfx_clear
        global	_x16_gfx_pset
        global	_x16_gfx_hline
        global	_x16_gfx_vline
        global	_x16_gfx_rect
        global	_x16_gfx_frame
        global	_x16_gfx_line
        global	_x16_gfx_circle
        global	_x16_gfx_disc
        global	_x16_gfx_char
        global	_x16_gfx_text
        global	_x16_gfx_flood

GFX_WIDTH  = 320
GFX_HEIGHT = 240

        section text

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_gfx_init(void)
;   320x240@256c bitmap on layer 0, 40x30 text on layer 1.
;   Returns 1 on success, 0 if the mode is unsupported.
; ---------------------------------------------------------------------
_x16_gfx_init:
        jsr     gfx_init                ; carry set = unsupported
        lda     #0
        ldx     #0
        rol     a
        eor     #1                      ; report success, not failure
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_clear(unsigned char color)
;   Single argument, already in A: no shim.
; ---------------------------------------------------------------------
_x16_gfx_clear:
gfx_clear:
        ; 320*240 = 76800 = $12C00 bytes does not fit vera_fill's 16-bit
        ; count. Neither ACME nor ca65 complains: `ldy #>$12C00` quietly
        ; takes byte 1, so the count becomes $2C00 and only the top 35
        ; rows clear. Hence two halves; port 0 keeps auto-incrementing
        ; between the calls. Regression-tested by GFX_CLEAR, which checks
        ; the LAST pixel rather than the first.
        pha
        vera_addr 0, VRAM_BITMAP, VERA_INC_1
        pla
        pha
        ldx     #<(GFX_WIDTH * GFX_HEIGHT / 2)
        ldy     #>(GFX_WIDTH * GFX_HEIGHT / 2)
        jsr     vera_fill
        pla
        ldx     #<(GFX_WIDTH * GFX_HEIGHT / 2)
        ldy     #>(GFX_WIDTH * GFX_HEIGHT / 2)
        jmp     vera_fill

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_pset(unsigned int x, unsigned char y,
;                                unsigned char color)
; ---------------------------------------------------------------------
; x16_gfx_pset(__reg("r0/r1") x, __reg("r2") y, __reg("r4") color)
;   gfx_pset wants P0/P1 = x, P2 = y, P3 = color.
_x16_gfx_pset:
        lda     r0
        sta     X16_P0                  ; x
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; y
        lda     r4
        sta     X16_P3                  ; color
        jmp     gfx_pset

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_hline(unsigned int x, unsigned char y,
;                                 unsigned int len, unsigned char color)
; ---------------------------------------------------------------------
; x16_gfx_hline(__reg("r0/r1") x, __reg("r2") y, __reg("r4/r5") len, __reg("r6") color)
;   gfx_hline wants P0/P1 = x, P2 = y, P4/P5 = len, P3 = color.
_x16_gfx_hline:
        lda     r0
        sta     X16_P0                  ; x
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; y
        lda     r4
        sta     X16_P4                  ; len
        lda     r5
        sta     X16_P5
        lda     r6
        sta     X16_P3                  ; color
        jmp     gfx_hline

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_vline(unsigned int x, unsigned char y,
;                                 unsigned char len, unsigned char color)
;   len is 1-255: a column of a 240-row screen never needs more.
; ---------------------------------------------------------------------
; x16_gfx_vline(__reg("r0/r1") x, __reg("r2") y, __reg("r4") len, __reg("r6") color)
;   gfx_vline wants P0/P1 = x, P2 = y, P4 = len, P3 = color.
_x16_gfx_vline:
        lda     r0
        sta     X16_P0                  ; x
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; y
        lda     r4
        sta     X16_P4                  ; len
        lda     r6
        sta     X16_P3                  ; color
        jmp     gfx_vline

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_rect(unsigned int x, unsigned char y,
;                                unsigned int w, unsigned char h,
;                                unsigned char color)      -- filled
; void __fastcall__ x16_gfx_frame(... same ...)            -- outline
; ---------------------------------------------------------------------
_x16_gfx_rect:
        jsr     rect_marshal
        jmp     gfx_rect

_x16_gfx_frame:
        jsr     rect_marshal
        jmp     gfx_frame

; gfx_rect/frame(x, y, w, h, color): x->r0/r1, y->r2, w->r4/r5, h->r6, and
; color (5th arg) spills to the C soft stack at (sp)+0.
; out: P0/P1 = x, P2 = y, P4/P5 = w, P6 = h, P3 = color.
rect_marshal:
        lda     r0
        sta     X16_P0                  ; x
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; y
        lda     r4
        sta     X16_P4                  ; w
        lda     r5
        sta     X16_P5
        lda     r6
        sta     X16_P6                  ; h
        ldy     #0
        lda     (sp),y
        sta     X16_P3                  ; color (stacked 5th arg)
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_line(unsigned int x0, unsigned char y0,
;                                unsigned int x1, unsigned char y1,
;                                unsigned char color)
; ---------------------------------------------------------------------
; x16_gfx_line(x0, y0, x1, y1, color): x0->r0/r1, y0->r2, x1->r4/r5,
; y1->r6, color (5th) on the C soft stack. gfx_line wants P0/P1 = x0,
; P2 = y0, P3/P4 = x1, P5 = y1, P6 = color.
_x16_gfx_line:
        lda     r0
        sta     X16_P0                  ; x0
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; y0
        lda     r4
        sta     X16_P3                  ; x1
        lda     r5
        sta     X16_P4
        lda     r6
        sta     X16_P5                  ; y1
        ldy     #0
        lda     (sp),y
        sta     X16_P6                  ; color (stacked 5th arg)
        jmp     gfx_line

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_circle(unsigned int cx, unsigned char cy,
;                                  unsigned char r, unsigned char color)
; void __fastcall__ x16_gfx_disc(...same...)
;
; Both plot through gfx_pset / gfx_hline, so unlike the line and rect
; primitives they clip at every screen edge for free.
; ---------------------------------------------------------------------
_x16_gfx_circle:
        jsr     circle_marshal
        jmp     gfx_circle

_x16_gfx_disc:
        jsr     circle_marshal
        jmp     gfx_disc

; gfx_circle/disc(cx, cy, r, color): cx->r0/r1, cy->r2, r->r4, color->r6.
; out: P0/P1 = cx, P2 = cy, P4 = radius, P3 = color.
circle_marshal:
        lda     r0
        sta     X16_P0                  ; cx
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; cy
        lda     r4
        sta     X16_P4                  ; radius
        lda     r6
        sta     X16_P3                  ; color
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_char(unsigned int x, unsigned char y,
;                                unsigned char color, unsigned char code)
;   `code` is a SCREEN code, not PETSCII.
; ---------------------------------------------------------------------
; x16_gfx_char(x, y, color, code): x->r0/r1, y->r2, color->r4, code->r6.
;   gfx_char wants P0/P1 = x, P2 = y, P3 = color, A = screen code.
_x16_gfx_char:
        lda     r0
        sta     X16_P0                  ; x
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; y
        lda     r4
        sta     X16_P3                  ; color
        lda     r6                      ; A = screen code
        jmp     gfx_char

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_text(unsigned int x, unsigned char y,
;                                unsigned char color, const char *s)
;   The pointer already arrives as A = low, X = high.
; ---------------------------------------------------------------------
; x16_gfx_text(x, y, color, s): x->r0/r1, y->r2, color->r4, s->r6/r7.
;   gfx_text wants P0/P1 = x, P2 = y, P3 = color, A/X = string pointer.
_x16_gfx_text:
        lda     r0
        sta     X16_P0                  ; x
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; y
        lda     r4
        sta     X16_P3                  ; color
        lda     r6                      ; A = string low
        ldx     r7                      ; X = string high
        jmp     gfx_text

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_gfx_flood(unsigned int x, unsigned char y,
;                                          unsigned char color)
;   1 if the region was filled completely; 0 if the span stack overflowed
;   and the fill is incomplete.
; ---------------------------------------------------------------------
; x16_gfx_flood(__reg("r0/r1") x, __reg("r2") y, __reg("r4") color)
;   gfx_flood wants P0/P1 = x, P2 = y, P3 = color.
_x16_gfx_flood:
        lda     r0
        sta     X16_P0                  ; x
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; y
        lda     r4
        sta     X16_P3                  ; color
        jsr     gfx_flood               ; carry set = incomplete
        lda     #0
        ldx     #0
        rol     a
        eor     #1                      ; report success, not overflow
        rts

; =====================================================================
; Internal routines
; =====================================================================

gfx_init:
        lda     #$80
        jmp     screen_set_mode

; ---------------------------------------------------------------------
; gfx_setptr -- point data port 0 at pixel (x,y)
;   in:  A = increment index (VERA_INC_*)
;        X16_P0/P1 = x, X16_P2 = y
;
; y*320 = (y<<8) + (y<<6), so no multiply is needed. Result is 17-bit.
; Stepping by VERA_INC_320 then walks straight down a column.
; ---------------------------------------------------------------------
gfx_setptr:
        asl     a
        asl     a
        asl     a
        asl     a
        sta     X16_T5                  ; increment field, pre-shifted

        lda     X16_P2                  ; y << 6
        stz     X16_T3
        asl     a
        rol     X16_T3
        asl     a
        rol     X16_T3
        asl     a
        rol     X16_T3
        asl     a
        rol     X16_T3
        asl     a
        rol     X16_T3
        asl     a
        rol     X16_T3
        sta     X16_T4                  ; T4/T3 = y*64

        clc                             ; + y<<8, whose low byte is zero
        lda     X16_T4
        sta     X16_T0
        lda     X16_P2
        adc     X16_T3
        sta     X16_T1
        lda     #0
        adc     #0
        sta     X16_T2                  ; T2:T1:T0 = y*320

        clc                             ; + x
        lda     X16_T0
        adc     X16_P0
        sta     X16_T0
        lda     X16_T1
        adc     X16_P1
        sta     X16_T1
        lda     X16_T2
        adc     #0
        sta     X16_T2

        lda     #VERA_CTRL_ADDRSEL
        trb     VERA_CTRL
        lda     X16_T0
        sta     VERA_ADDR_L
        lda     X16_T1
        sta     VERA_ADDR_M
        lda     X16_T2
        and     #VERA_ADDR_H_BANK
        ora     X16_T5
        sta     VERA_ADDR_H
        rts

; ---------------------------------------------------------------------
; gfx_pset -- set one pixel, clipped
;   in:  X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour
; ---------------------------------------------------------------------
gfx_pset:
        lda     X16_P2
        cmp     #GFX_HEIGHT
        bcs     .off                    ; y >= 240

        lda     X16_P1                  ; x high byte
        beq     .on                     ; x < 256, always on screen
        cmp     #1
        bne     .off                    ; x >= 512
        lda     X16_P0
        cmp     #<GFX_WIDTH             ; 320 = $140, so x low must be < $40
        bcs     .off
.on:
        lda     #VERA_INC_0
        jsr     gfx_setptr
        lda     X16_P3
        sta     VERA_DATA0
.off:
        rts

; ---------------------------------------------------------------------
; gfx_hline -- in: X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour,
;                  X16_P4/P5 = length
; ---------------------------------------------------------------------
gfx_hline:
        lda     #VERA_INC_1
        jsr     gfx_setptr
        lda     X16_P3
        ldx     X16_P4
        ldy     X16_P5
        jmp     vera_fill

; ---------------------------------------------------------------------
; gfx_vline -- in: X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour,
;                  X16_P4 = length (1-255)
;
; VERA_INC_320 is one of the hardware's odd increments, so a vertical
; line is the same tight loop as a horizontal one.
; ---------------------------------------------------------------------
gfx_vline:
        lda     #VERA_INC_320
        jsr     gfx_setptr
        lda     X16_P3
        ldx     X16_P4
        ldy     #0
        jmp     vera_fill

; ---------------------------------------------------------------------
; gfx_rect -- filled rectangle
;   in:  X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour,
;        X16_P4/P5 = width, X16_P6 = height
; ---------------------------------------------------------------------
gfx_rect:
.row:
        lda     X16_P6
        beq     .done
        jsr     gfx_hline               ; leaves P0..P5 alone
        inc     X16_P2
        dec     X16_P6
        bra     .row
.done:
        rts

; ---------------------------------------------------------------------
; gfx_frame -- rectangle outline, same arguments as gfx_rect
; ---------------------------------------------------------------------
gfx_frame:
        ; Take a private copy of everything: gfx_vline reuses P4 as its
        ; length, which is where the caller's width lives.
        lda     X16_P0
        sta     gb_x
        lda     X16_P1
        sta     gb_x+1
        lda     X16_P2
        sta     gb_y
        lda     X16_P3
        sta     gb_c
        lda     X16_P4
        sta     gb_w
        lda     X16_P5
        sta     gb_w+1
        lda     X16_P6
        sta     gb_h

        jsr     restore_span            ; top edge
        jsr     gfx_hline

        jsr     restore_span            ; bottom edge, y + h - 1
        clc
        lda     gb_y
        adc     gb_h
        sec
        sbc     #1
        sta     X16_P2
        jsr     gfx_hline

        jsr     restore_col             ; left edge
        jsr     gfx_vline

        jsr     restore_col             ; right edge, x + w - 1
        clc
        lda     gb_x
        adc     gb_w
        sta     X16_P0
        lda     gb_x+1
        adc     gb_w+1
        sta     X16_P1
        lda     X16_P0
        bne     .no_borrow
        dec     X16_P1
.no_borrow:
        dec     X16_P0
        jsr     gfx_vline

        rts

; x, y, colour, width -- arguments for gfx_hline
restore_span:
        lda     gb_x
        sta     X16_P0
        lda     gb_x+1
        sta     X16_P1
        lda     gb_y
        sta     X16_P2
        lda     gb_c
        sta     X16_P3
        lda     gb_w
        sta     X16_P4
        lda     gb_w+1
        sta     X16_P5
        rts

; x, y, colour, height -- arguments for gfx_vline
restore_col:
        lda     gb_x
        sta     X16_P0
        lda     gb_x+1
        sta     X16_P1
        lda     gb_y
        sta     X16_P2
        lda     gb_c
        sta     X16_P3
        lda     gb_h
        sta     X16_P4
        rts

; ---------------------------------------------------------------------
; gfx_line -- Bresenham, any direction
;   in:  X16_P0/P1 = x0, X16_P2 = y0
;        X16_P3/P4 = x1, X16_P5 = y1
;        X16_P6    = colour
;
; Works entirely from its own variables, because gfx_pset wants the
; colour in X16_P3 -- which is where x1 lives on entry.
; ---------------------------------------------------------------------
gfx_line:
        lda     X16_P0
        sta     gl_x0
        lda     X16_P1
        sta     gl_x0+1
        lda     X16_P2
        sta     gl_y0
        lda     X16_P3
        sta     gl_x1
        lda     X16_P4
        sta     gl_x1+1
        lda     X16_P5
        sta     gl_y1
        lda     X16_P6
        sta     gl_color

        ; dx = |x1 - x0|, sx = sign
        sec
        lda     gl_x1
        sbc     gl_x0
        sta     gl_dx
        lda     gl_x1+1
        sbc     gl_x0+1
        sta     gl_dx+1
        bpl     .dx_pos
        sec
        lda     #0
        sbc     gl_dx
        sta     gl_dx
        lda     #0
        sbc     gl_dx+1
        sta     gl_dx+1
        lda     #$FF
        sta     gl_sx
        sta     gl_sx+1                 ; -1, sign extended
        bra     .dx_done
.dx_pos:
        lda     #$01
        sta     gl_sx
        stz     gl_sx+1
.dx_done:

        ; dy = -|y1 - y0|, sy = sign
        sec
        lda     gl_y1
        sbc     gl_y0
        bpl     .dy_pos
        eor     #$FF
        clc
        adc     #1                      ; absolute value
        sta     gl_tmp
        lda     #$FF
        sta     gl_sy
        bra     .dy_done
.dy_pos:
        sta     gl_tmp
        lda     #$01
        sta     gl_sy
.dy_done:
        sec
        lda     #0
        sbc     gl_tmp
        sta     gl_dy
        lda     #0
        sbc     #0
        sta     gl_dy+1                 ; gl_dy = -|dy|, 16-bit signed

        clc                             ; err = dx + dy
        lda     gl_dx
        adc     gl_dy
        sta     gl_err
        lda     gl_dx+1
        adc     gl_dy+1
        sta     gl_err+1

.loop:
        jsr     plot

        lda     gl_x0                   ; reached the end point?
        cmp     gl_x1
        bne     .step
        lda     gl_x0+1
        cmp     gl_x1+1
        bne     .step
        lda     gl_y0
        cmp     gl_y1
        bne     .step
        rts

.step:
        lda     gl_err                  ; e2 = err * 2
        asl     a
        sta     gl_e2
        lda     gl_err+1
        rol     a
        sta     gl_e2+1

        ; if e2 >= dy  ->  err += dy, x0 += sx
        sec
        lda     gl_e2
        sbc     gl_dy
        lda     gl_e2+1
        sbc     gl_dy+1
        bvc     .nv1
        eor     #$80                    ; signed compare: fold overflow into sign
.nv1:
        bmi     .skip_x
        clc
        lda     gl_err
        adc     gl_dy
        sta     gl_err
        lda     gl_err+1
        adc     gl_dy+1
        sta     gl_err+1
        clc
        lda     gl_x0
        adc     gl_sx
        sta     gl_x0
        lda     gl_x0+1
        adc     gl_sx+1
        sta     gl_x0+1
.skip_x:

        ; if e2 <= dx  ->  err += dx, y0 += sy
        sec
        lda     gl_dx
        sbc     gl_e2
        lda     gl_dx+1
        sbc     gl_e2+1
        bvc     .nv2
        eor     #$80
.nv2:
        bmi     .skip_y
        clc
        lda     gl_err
        adc     gl_dx
        sta     gl_err
        lda     gl_err+1
        adc     gl_dx+1
        sta     gl_err+1
        clc
        lda     gl_y0
        adc     gl_sy
        sta     gl_y0
.skip_y:
        jmp     .loop

; plot (gl_x0, gl_y0) in gl_color
plot:
        lda     gl_x0
        sta     X16_P0
        lda     gl_x0+1
        sta     X16_P1
        lda     gl_y0
        sta     X16_P2
        lda     gl_color
        sta     X16_P3
        jmp     gfx_pset

; ---------------------------------------------------------------------
; gfx_circle -- midpoint circle outline
;   in:  X16_P0/P1 = centre x, X16_P2 = centre y, X16_P3 = colour,
;        X16_P4 = radius (0-120)
;
; Plots through gfx_pset, so the circle clips at every screen edge for
; free. Preserves X16_P0..P4.
; ---------------------------------------------------------------------
gfx_circle:
        jsr     c_setup
        lda     gc_r
        bne     .go
        lda     gc_cx                   ; radius 0: a single point
        sta     X16_P0
        lda     gc_cx+1
        sta     X16_P1
        lda     gc_cy
        sta     X16_P2
        jsr     gfx_pset
        jmp     c_restore
.go:
        ; x = r, y = 0, err = 1 - r
        lda     gc_r
        sta     gc_x
        stz     gc_y
        sec
        lda     #1
        sbc     gc_r
        sta     gc_err
        lda     #0
        sbc     #0
        sta     gc_err+1

.loop:
        lda     gc_x                    ; the 8 octant points
        ldy     gc_y
        jsr     c_plot4
        lda     gc_y
        ldy     gc_x
        jsr     c_plot4

        inc     gc_y
        lda     gc_err+1                ; err < 0 ?
        bmi     .err_neg
        dec     gc_x                    ; no: also step x inward,
        sec                             ; err += 2*(y - x) + 1
        lda     gc_y
        sbc     gc_x
        sta     X16_T0
        lda     #0
        sbc     #0
        sta     X16_T1
        asl     X16_T0
        rol     X16_T1
        inc     X16_T0
        bne     .add_err
        inc     X16_T1
.add_err:
        clc
        lda     gc_err
        adc     X16_T0
        sta     gc_err
        lda     gc_err+1
        adc     X16_T1
        sta     gc_err+1
        bra     .cont
.err_neg:
        lda     gc_y                    ; err += 2*y + 1
        stz     X16_T1
        asl     a
        rol     X16_T1
        inc     a
        bne     .add2
        inc     X16_T1
.add2:
        clc
        adc     gc_err
        sta     gc_err
        lda     gc_err+1
        adc     X16_T1
        sta     gc_err+1
.cont:
        lda     gc_y
        cmp     gc_x
        bcc     .loop
        beq     .loop_last
        jmp     c_restore
.loop_last:
        lda     gc_x                    ; the final x == y diagonal points
        ldy     gc_y
        jsr     c_plot4
        jmp     c_restore

; ---------------------------------------------------------------------
; gfx_disc -- filled circle, same arguments as gfx_circle.
; Draws horizontal spans clamped to the screen, so it clips too.
; ---------------------------------------------------------------------
gfx_disc:
        jsr     c_setup
        lda     gc_r
        sta     gc_x
        stz     gc_y
        sec
        lda     #1
        sbc     gc_r
        sta     gc_err
        lda     #0
        sbc     #0
        sta     gc_err+1

.dloop:
        lda     gc_x                    ; spans at cy+/-y, half-width x
        ldy     gc_y
        jsr     c_span2
        lda     gc_y                    ; spans at cy+/-x, half-width y
        ldy     gc_x
        jsr     c_span2

        inc     gc_y
        lda     gc_err+1
        bmi     .derr_neg
        dec     gc_x
        sec
        lda     gc_y
        sbc     gc_x
        sta     X16_T0
        lda     #0
        sbc     #0
        sta     X16_T1
        asl     X16_T0
        rol     X16_T1
        inc     X16_T0
        bne     .dadd
        inc     X16_T1
.dadd:
        clc
        lda     gc_err
        adc     X16_T0
        sta     gc_err
        lda     gc_err+1
        adc     X16_T1
        sta     gc_err+1
        bra     .dcont
.derr_neg:
        lda     gc_y
        stz     X16_T1
        asl     a
        rol     X16_T1
        inc     a
        bne     .dadd2
        inc     X16_T1
.dadd2:
        clc
        adc     gc_err
        sta     gc_err
        lda     gc_err+1
        adc     X16_T1
        sta     gc_err+1
.dcont:
        lda     gc_y
        cmp     gc_x
        bcc     .dloop
        beq     .dloop_last
        jmp     c_restore
.dloop_last:
        lda     gc_x
        ldy     gc_y
        jsr     c_span2
        jmp     c_restore

; --- circle plumbing --------------------------------------------------

c_setup:                                ; park the caller's block
        lda     X16_P0
        sta     gc_cx
        sta     gc_sav
        lda     X16_P1
        sta     gc_cx+1
        sta     gc_sav+1
        lda     X16_P2
        sta     gc_cy
        sta     gc_sav+2
        lda     X16_P4
        sta     gc_r
        sta     gc_sav+3
        rts

c_restore:                              ; put the caller's block back
        lda     gc_sav
        sta     X16_P0
        lda     gc_sav+1
        sta     X16_P1
        lda     gc_sav+2
        sta     X16_P2
        lda     gc_sav+3
        sta     X16_P4
        rts

; plot (cx +/- A, cy +/- Y): the four reflections of one octant point
c_plot4:
        sta     gc_ox
        sty     gc_oy
        jsr     c_ypl
        bcs     .p4_low
        jsr     c_xpl
        jsr     gfx_pset
        jsr     c_xmi
        jsr     gfx_pset
.p4_low:
        jsr     c_ymi
        bcs     .p4_done
        jsr     c_xpl
        jsr     gfx_pset
        jsr     c_xmi
        jsr     gfx_pset
.p4_done:
        rts

; two clamped horizontal spans: rows cy +/- Y, half-width A
c_span2:
        sta     gc_ox
        sty     gc_oy
        jsr     c_ypl
        bcs     .s2_lower
        jsr     c_hspan
.s2_lower:
        lda     gc_oy
        beq     .s2_done                ; same row twice: skip the mirror
        jsr     c_ymi
        bcs     .s2_done
        jsr     c_hspan
.s2_done:
        rts

; X16_P2 already holds the row: draw cx-gc_ox .. cx+gc_ox clamped
c_hspan:
        sec                             ; left = cx - ox, clamped to 0
        lda     gc_cx
        sbc     gc_ox
        sta     X16_T0
        lda     gc_cx+1
        sbc     #0
        sta     X16_T1
        bpl     .left_ok
        stz     X16_T0
        stz     X16_T1
.left_ok:
        clc                             ; right = cx + ox, clamped to 319
        lda     gc_cx
        adc     gc_ox
        sta     X16_T2
        lda     gc_cx+1
        adc     #0
        sta     X16_T3
        lda     X16_T3                  ; right >= 320 ?
        cmp     #>320
        bcc     .right_ok
        bne     .clamp_r
        lda     X16_T2
        cmp     #<320
        bcc     .right_ok
.clamp_r:
        lda     #<319
        sta     X16_T2
        lda     #>319
        sta     X16_T3
.right_ok:
        sec                             ; entirely off screen?
        lda     X16_T2
        sbc     X16_T0
        sta     X16_T4
        lda     X16_T3
        sbc     X16_T1
        sta     X16_T5
        bmi     .off
        inc     X16_T4                  ; length = right - left + 1
        bne     .len_ok
        inc     X16_T5
.len_ok:
        lda     X16_T0
        sta     X16_P0
        lda     X16_T1
        sta     X16_P1
        lda     X16_T4
        sta     X16_P4
        lda     X16_T5
        sta     X16_P5
        jmp     gfx_hline
.off:
        rts

; X16_P2 = cy + gc_oy; carry set if the row is off screen.
c_ypl:
        clc
        lda     gc_cy
        adc     gc_oy
        bcs     .ypl_bad                ; past 255
        cmp     #GFX_HEIGHT
        bcs     .ypl_bad                ; 240..255
        sta     X16_P2
        clc
        rts
.ypl_bad:
        sec
        rts

; X16_P2 = cy - gc_oy; carry set if above the screen.
c_ymi:
        sec
        lda     gc_cy
        sbc     gc_oy
        bcc     .ymi_bad
        sta     X16_P2
        clc
        rts
.ymi_bad:
        sec
        rts

c_xpl:                                  ; X16_P0/P1 = cx + gc_ox
        clc
        lda     gc_cx
        adc     gc_ox
        sta     X16_P0
        lda     gc_cx+1
        adc     #0
        sta     X16_P1
        rts

c_xmi:                                  ; X16_P0/P1 = cx - gc_ox
        sec
        lda     gc_cx
        sbc     gc_ox
        sta     X16_P0
        lda     gc_cx+1
        sbc     #0
        sta     X16_P1
        rts

; ---------------------------------------------------------------------
; gfx_char -- draw one glyph from the VRAM charset into the bitmap
;   in:  A = screen code (0-255)
;        X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour
;
; Reads the 8-byte 1bpp glyph from the charset the KERNAL keeps at VRAM
; $1F000; set bits become colour pixels through gfx_pset (so text clips),
; clear bits stay transparent. Preserves X16_P0..P3.
; ---------------------------------------------------------------------
gfx_char:
        ; glyph address = VRAM_CHARSET + code * 8  (17-bit)
        sta     gt_code
        stz     gt_hi
        asl     a
        rol     gt_hi
        asl     a
        rol     gt_hi
        asl     a
        rol     gt_hi                   ; gt_hi:A = code * 8
        pha
        vera_addrsel 1
        pla
        sta     VERA_ADDR_L
        lda     gt_hi
        clc
        adc     #<(VRAM_CHARSET >> 8)
        sta     VERA_ADDR_M
        lda     #(VERA_ADDR_H_BANK | (VERA_INC_1 << 4))  ; $1F000 is in bank 1
        sta     VERA_ADDR_H
        ldx     #0
.fetch:
        lda     VERA_DATA1
        sta     gt_glyph,x
        inx
        cpx     #8
        bne     .fetch
        vera_addrsel 0

        lda     X16_P0                  ; park the caller's position
        sta     gt_bx
        lda     X16_P1
        sta     gt_bx+1
        lda     X16_P2
        sta     gt_by

        stz     gt_row
.rows:
        ldx     gt_row
        lda     gt_glyph,x
        sta     gt_bits
        beq     .next_row               ; a blank row: nothing to plot
        stz     gt_col
.cols:
        asl     gt_bits                 ; leftmost pixel first
        bcc     .next_col
        clc
        lda     gt_bx
        adc     gt_col
        sta     X16_P0
        lda     gt_bx+1
        adc     #0
        sta     X16_P1
        clc
        lda     gt_by
        adc     gt_row
        bcs     .next_col               ; wrapped past 255: off screen
        sta     X16_P2
        jsr     gfx_pset
.next_col:
        inc     gt_col
        lda     gt_col
        cmp     #8
        bne     .cols
.next_row:
        inc     gt_row
        lda     gt_row
        cmp     #8
        bne     .rows

        lda     gt_bx                   ; restore the caller's block
        sta     X16_P0
        lda     gt_bx+1
        sta     X16_P1
        lda     gt_by
        sta     X16_P2
        rts

; ---------------------------------------------------------------------
; gfx_text -- a NUL-terminated string, 8 pixels per character
;   in:  A = string low, X = string high; X16_P0..P3 as gfx_char.
;   ASCII letters are converted to screen codes ('A'-'Z' work as
;   expected); X16_P0/P1 are left one past the final character.
;
; gt_lda must be a plain label so the pointer can live in its own operand
; -- and in ca65 a plain label ends the enclosing cheap-local scope, so
; this loop's labels are plain too. ACME's zone-local `.gt_lda` did not.
; ---------------------------------------------------------------------
gfx_text:
        sta     gt_lda+1                ; the string pointer lives in the lda's
        stx     gt_lda+2                ; own operand (no zero page needed)
gt_loop:
gt_lda:
        lda     $FFFF                   ; operand patched above and stepped below
        beq     gt_done
        ; ASCII -> screen code: bit 6 set means the letters/at-sign block
        bit     #%01000000
        beq     gt_code_ok
        and     #$1F
gt_code_ok:
        jsr     gfx_char
        clc                             ; advance the pen 8 pixels
        lda     X16_P0
        adc     #8
        sta     X16_P0
        lda     X16_P1
        adc     #0
        sta     X16_P1
        inc     gt_lda+1
        bne     gt_loop
        inc     gt_lda+2
        bra     gt_loop
gt_done:
        rts

; ---------------------------------------------------------------------
; gfx_flood -- scanline flood fill
;   in:  X16_P0/P1 = seed x, X16_P2 = seed y, X16_P3 = fill colour
;   out: carry clear = filled completely; carry set = the span stack
;        overflowed and the fill is INCOMPLETE (pathological shapes: the
;        stack holds FF_DEPTH pending spans)
;
; Fills the 4-connected region of the seed's colour. Filling with the
; colour already under the seed is a no-op. Spans are painted with
; gfx_hline; both VERA ports get repointed freely.
; ---------------------------------------------------------------------
FF_DEPTH = 170

gfx_flood:
        lda     X16_P2                  ; a seed off screen fills nothing
        cmp     #GFX_HEIGHT
        bcs     .bail
        lda     X16_P1
        beq     .seed_ok
        cmp     #1
        bne     .bail
        lda     X16_P0
        cmp     #<GFX_WIDTH
        bcc     .seed_ok
.bail:
        clc
        rts
.seed_ok:
        lda     X16_P3
        sta     ff_col
        lda     X16_P0
        sta     ff_x
        lda     X16_P1
        sta     ff_x+1
        lda     X16_P2
        sta     ff_y

        jsr     f_rd                    ; the colour being replaced
        sta     ff_tgt
        cmp     ff_col
        beq     .bail                   ; already the fill colour: no-op

        stz     ff_sp
        stz     ff_ovf
        lda     ff_x
        sta     ff_px
        lda     ff_x+1
        sta     ff_px+1
        lda     ff_y
        sta     ff_ny
        jsr     f_push

.main:
        lda     ff_sp
        bne     .have_work
        jmp     .finish
.have_work:
        jsr     f_pop                   ; -> ff_x / ff_y
        jsr     f_rd
        cmp     ff_tgt
        bne     .main                   ; painted over since it was queued

        ; grow the span left: xl = leftmost target pixel
        lda     ff_x
        sta     ff_xl
        lda     ff_x+1
        sta     ff_xl+1
        lda     ff_xl
        ora     ff_xl+1
        beq     .left_done
        sec                             ; walk from xl-1 downwards
        lda     ff_xl
        sbc     #1
        sta     ff_ax
        lda     ff_xl+1
        sbc     #0
        sta     ff_ax+1
        lda     ff_y
        sta     ff_ay
        lda     #VERA_ADDR_H_DECR
        jsr     f_addr1
.left_scan:
        lda     VERA_DATA1
        cmp     ff_tgt
        bne     .left_done
        lda     ff_xl
        bne     .left_dec
        dec     ff_xl+1
.left_dec:
        dec     ff_xl
        lda     ff_xl
        ora     ff_xl+1
        bne     .left_scan
.left_done:

        ; grow the span right: xr = rightmost target pixel
        lda     ff_x
        sta     ff_xr
        lda     ff_x+1
        sta     ff_xr+1
        jsr     f_at_right
        bcs     .right_done
        clc                             ; walk from xr+1 upwards
        lda     ff_xr
        adc     #1
        sta     ff_ax
        lda     ff_xr+1
        adc     #0
        sta     ff_ax+1
        lda     ff_y
        sta     ff_ay
        lda     #0
        jsr     f_addr1
.right_scan:
        lda     VERA_DATA1
        cmp     ff_tgt
        bne     .right_done
        inc     ff_xr
        bne     .right_chk
        inc     ff_xr+1
.right_chk:
        jsr     f_at_right
        bcc     .right_scan
.right_done:

        ; paint it
        lda     ff_xl
        sta     X16_P0
        lda     ff_xl+1
        sta     X16_P1
        lda     ff_y
        sta     X16_P2
        lda     ff_col
        sta     X16_P3
        sec                             ; length = xr - xl + 1
        lda     ff_xr
        sbc     ff_xl
        sta     X16_P4
        lda     ff_xr+1
        sbc     ff_xl+1
        sta     X16_P5
        inc     X16_P4
        bne     .len_ok
        inc     X16_P5
.len_ok:
        jsr     gfx_hline

        ; queue fresh spans in the rows above and below
        lda     ff_y
        beq     .no_up
        dec     a
        sta     ff_ny
        jsr     f_scanrow
.no_up:
        lda     ff_y
        cmp     #(GFX_HEIGHT - 1)
        bcs     .no_down
        inc     a
        sta     ff_ny
        jsr     f_scanrow
.no_down:
        jmp     .main

.finish:
        vera_addrsel 0
        lda     ff_ovf                  ; carry = "the fill may be incomplete"
        lsr     a
        rts

; carry set when ff_xr is the last column (319)
f_at_right:
        lda     ff_xr+1
        cmp     #>(GFX_WIDTH - 1)
        bne     .below
        lda     ff_xr
        cmp     #<(GFX_WIDTH - 1)
        bcs     .at
.below:
        clc
        rts
.at:
        sec
        rts

; scan row ff_ny across columns ff_xl..ff_xr, pushing the start of every
; run of target-coloured pixels
f_scanrow:
        lda     ff_xl
        sta     ff_ax
        sta     ff_px
        lda     ff_xl+1
        sta     ff_ax+1
        sta     ff_px+1
        lda     ff_ny
        sta     ff_ay
        lda     #0
        jsr     f_addr1
        sec                             ; count = xr - xl + 1
        lda     ff_xr
        sbc     ff_xl
        sta     ff_cnt
        lda     ff_xr+1
        sbc     ff_xl+1
        sta     ff_cnt+1
        inc     ff_cnt
        bne     .counted
        inc     ff_cnt+1
.counted:
        stz     ff_seg
.cell:
        lda     VERA_DATA1
        cmp     ff_tgt
        bne     .break
        lda     ff_seg
        bne     .step                   ; already inside a run
        jsr     f_push                  ; a run begins here: remember its start
        lda     #1
        sta     ff_seg
        bra     .step
.break:
        stz     ff_seg
.step:
        inc     ff_px
        bne     .count
        inc     ff_px+1
.count:
        lda     ff_cnt
        bne     .declo
        dec     ff_cnt+1
.declo:
        dec     ff_cnt
        lda     ff_cnt
        ora     ff_cnt+1
        bne     .cell
        rts

; push (ff_px, ff_ny); a full stack sets ff_ovf instead
f_push:
        lda     ff_sp
        cmp     #FF_DEPTH
        bcc     .room
        lda     #1
        sta     ff_ovf
        rts
.room:
        jsr     f_slot
        ldy     #0
        lda     ff_px
        sta     (X16_T6),y
        iny
        lda     ff_px+1
        sta     (X16_T6),y
        iny
        lda     ff_ny
        sta     (X16_T6),y
        inc     ff_sp
        rts

; pop -> ff_x, ff_y
f_pop:
        dec     ff_sp
        jsr     f_slot
        ldy     #0
        lda     (X16_T6),y
        sta     ff_x
        iny
        lda     (X16_T6),y
        sta     ff_x+1
        iny
        lda     (X16_T6),y
        sta     ff_y
        rts

; X16_T6/T7 = &ff_stk[ff_sp * 3]
f_slot:
        lda     ff_sp
        sta     X16_T6
        stz     X16_T7
        asl     X16_T6
        rol     X16_T7
        clc
        lda     X16_T6
        adc     ff_sp
        sta     X16_T6
        lda     X16_T7
        adc     #0
        sta     X16_T7
        clc
        lda     X16_T6
        adc     #<ff_stk
        sta     X16_T6
        lda     X16_T7
        adc     #>ff_stk
        sta     X16_T7
        rts

; A = the pixel at (ff_x, ff_y)
f_rd:
        lda     ff_x
        sta     ff_ax
        lda     ff_x+1
        sta     ff_ax+1
        lda     ff_y
        sta     ff_ay
        lda     #0
        jsr     f_addr1
        lda     VERA_DATA1
        rts

; point port 1 at (ff_ax, ff_ay), INC_1, with A's DECR flag
f_addr1:
        ora     #(VERA_INC_1 << 4)
        sta     ff_h
        lda     ff_ay                   ; ay*320 = ay*64 + ay*256
        stz     X16_T1
        asl     a
        rol     X16_T1
        asl     a
        rol     X16_T1
        asl     a
        rol     X16_T1
        asl     a
        rol     X16_T1
        asl     a
        rol     X16_T1
        asl     a
        rol     X16_T1
        sta     X16_T0
        clc
        lda     ff_ay
        adc     X16_T1
        sta     X16_T1
        lda     #0
        adc     #0
        sta     X16_T2
        clc                             ; + ax
        lda     X16_T0
        adc     ff_ax
        sta     X16_T0
        lda     X16_T1
        adc     ff_ax+1
        sta     X16_T1
        lda     X16_T2
        adc     #0
        sta     X16_T2
        lda     #VERA_CTRL_ADDRSEL
        tsb     VERA_CTRL
        lda     X16_T0
        sta     VERA_ADDR_L
        lda     X16_T1
        sta     VERA_ADDR_M
        lda     X16_T2
        and     #VERA_ADDR_H_BANK
        ora     ff_h
        sta     VERA_ADDR_H
        rts

; ---------------------------------------------------------------------
; Module variables. Kept out of zero page: these are only touched by the
; routine that owns them, never across a call boundary.
; ---------------------------------------------------------------------
        section bss

gc_cx:     reserve 2
gc_cy:     reserve 1
gc_r:      reserve 1
gc_x:      reserve 1
gc_y:      reserve 1
gc_ox:     reserve 1
gc_oy:     reserve 1
gc_err:    reserve 2
gc_sav:    reserve 4

gt_code:   reserve 1
gt_hi:     reserve 1
gt_glyph:  reserve 8
gt_bx:     reserve 2
gt_by:     reserve 1
gt_row:    reserve 1
gt_col:    reserve 1
gt_bits:   reserve 1

ff_x:      reserve 2
ff_y:      reserve 1
ff_xl:     reserve 2
ff_xr:     reserve 2
ff_px:     reserve 2
ff_ny:     reserve 1
ff_ax:     reserve 2
ff_ay:     reserve 1
ff_h:      reserve 1
ff_tgt:    reserve 1
ff_col:    reserve 1
ff_seg:    reserve 1
ff_cnt:    reserve 2
ff_sp:     reserve 1
ff_ovf:    reserve 1
ff_stk:    reserve FF_DEPTH * 3

gb_x:      reserve 2
gb_y:      reserve 1
gb_w:      reserve 2
gb_h:      reserve 1
gb_c:      reserve 1

gl_x0:     reserve 2
gl_y0:     reserve 1
gl_x1:     reserve 2
gl_y1:     reserve 1
gl_color:  reserve 1
gl_dx:     reserve 2
gl_dy:     reserve 2
gl_err:    reserve 2
gl_e2:     reserve 2
gl_sx:     reserve 2
gl_sy:     reserve 1
gl_tmp:    reserve 1
