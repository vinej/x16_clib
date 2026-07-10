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

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   POINTERS take aligned __rc PAIRS -- __rc2/__rc3, __rc4/__rc5, ... in
;   order, skipping any pair already partly used.
;   INTEGER bytes take A, then X, then single __rc bytes from __rc2 up,
;   skipping any byte a pointer already claimed.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_gfx_init
        .globl  x16_gfx_clear
        .globl  x16_gfx_pset
        .globl  x16_gfx_hline
        .globl  x16_gfx_vline
        .globl  x16_gfx_rect
        .globl  x16_gfx_frame
        .globl  x16_gfx_line
        .globl  x16_gfx_circle
        .globl  x16_gfx_disc
        .globl  x16_gfx_char
        .globl  x16_gfx_text
        .globl  x16_gfx_flood

GFX_WIDTH  = 320
GFX_HEIGHT = 240

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_gfx_init(void)
;   320x240@256c bitmap on layer 0, 40x30 text on layer 1.
;   Returns 1 on success, 0 if the mode is unsupported.
; ---------------------------------------------------------------------
x16_gfx_init:
        jsr     gfx_init                ; carry set = unsupported
        lda     #0
        rol     a
        eor     #1                      ; report success, not failure
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_clear(unsigned char color)
;   Single argument, already in A: no shim.
; ---------------------------------------------------------------------
x16_gfx_clear:
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
; x -> A/X, y -> __rc2, color -> __rc3.
x16_gfx_pset:
        sta     X16_P0                  ; x lo
        stx     X16_P1                  ; x hi
        lda     __rc2
        sta     X16_P2                  ; y
        lda     __rc3
        sta     X16_P3                  ; color
        jmp     gfx_pset

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_hline(unsigned int x, unsigned char y,
;                                 unsigned int len, unsigned char color)
; ---------------------------------------------------------------------
; x -> A/X, y -> __rc2, len -> __rc3/__rc4, color -> __rc5.
x16_gfx_hline:
        sta     X16_P0                  ; x lo
        stx     X16_P1                  ; x hi
        lda     __rc2
        sta     X16_P2                  ; y
        lda     __rc3
        sta     X16_P4                  ; len lo
        lda     __rc4
        sta     X16_P5                  ; len hi
        lda     __rc5
        sta     X16_P3                  ; color
        jmp     gfx_hline

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_vline(unsigned int x, unsigned char y,
;                                 unsigned char len, unsigned char color)
;   len is 1-255: a column of a 240-row screen never needs more.
; ---------------------------------------------------------------------
; x -> A/X, y -> __rc2, len -> __rc3, color -> __rc4.
x16_gfx_vline:
        sta     X16_P0                  ; x lo
        stx     X16_P1                  ; x hi
        lda     __rc2
        sta     X16_P2                  ; y
        lda     __rc3
        sta     X16_P4                  ; len
        lda     __rc4
        sta     X16_P3                  ; color
        jmp     gfx_vline

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_rect(unsigned int x, unsigned char y,
;                                unsigned int w, unsigned char h,
;                                unsigned char color)      -- filled
; void __fastcall__ x16_gfx_frame(... same ...)            -- outline
; ---------------------------------------------------------------------
x16_gfx_rect:
        jsr     rect_marshal
        jmp     gfx_rect

x16_gfx_frame:
        jsr     rect_marshal
        jmp     gfx_frame

; in: x -> A/X, y -> __rc2, w -> __rc3/__rc4, h -> __rc5, color -> __rc6
rect_marshal:
        sta     X16_P0                  ; x lo
        stx     X16_P1                  ; x hi
        lda     __rc2
        sta     X16_P2                  ; y
        lda     __rc3
        sta     X16_P4                  ; w lo
        lda     __rc4
        sta     X16_P5                  ; w hi
        lda     __rc5
        sta     X16_P6                  ; h
        lda     __rc6
        sta     X16_P3                  ; color
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_line(unsigned int x0, unsigned char y0,
;                                unsigned int x1, unsigned char y1,
;                                unsigned char color)
; ---------------------------------------------------------------------
; x0 -> A/X, y0 -> __rc2, x1 -> __rc3/__rc4, y1 -> __rc5, color -> __rc6.
x16_gfx_line:
        sta     X16_P0                  ; x0 lo
        stx     X16_P1                  ; x0 hi
        lda     __rc2
        sta     X16_P2                  ; y0
        lda     __rc3
        sta     X16_P3                  ; x1 lo
        lda     __rc4
        sta     X16_P4                  ; x1 hi
        lda     __rc5
        sta     X16_P5                  ; y1
        lda     __rc6
        sta     X16_P6                  ; color
        jmp     gfx_line

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_circle(unsigned int cx, unsigned char cy,
;                                  unsigned char r, unsigned char color)
; void __fastcall__ x16_gfx_disc(...same...)
;
; Both plot through gfx_pset / gfx_hline, so unlike the line and rect
; primitives they clip at every screen edge for free.
; ---------------------------------------------------------------------
x16_gfx_circle:
        jsr     circle_marshal
        jmp     gfx_circle

x16_gfx_disc:
        jsr     circle_marshal
        jmp     gfx_disc

; in: cx -> A/X, cy -> __rc2, r -> __rc3, color -> __rc4
circle_marshal:
        sta     X16_P0                  ; cx lo
        stx     X16_P1                  ; cx hi
        lda     __rc2
        sta     X16_P2                  ; cy
        lda     __rc3
        sta     X16_P4                  ; radius
        lda     __rc4
        sta     X16_P3                  ; color
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_char(unsigned int x, unsigned char y,
;                                unsigned char color, unsigned char code)
;   `code` is a SCREEN code, not PETSCII.
; ---------------------------------------------------------------------
; x -> A/X, y -> __rc2, color -> __rc3, code -> __rc4. gfx_char wants the
; screen code in A.
x16_gfx_char:
        sta     X16_P0                  ; x lo
        stx     X16_P1                  ; x hi
        lda     __rc2
        sta     X16_P2                  ; y
        lda     __rc3
        sta     X16_P3                  ; color
        lda     __rc4                   ; A = screen code
        jmp     gfx_char

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx_text(unsigned int x, unsigned char y,
;                                unsigned char color, const char *s)
;   The pointer already arrives as A = low, X = high.
; ---------------------------------------------------------------------
; Arguments are allocated STRICTLY LEFT TO RIGHT. x takes A and X, y takes
; __rc2, colour takes __rc3 -- and only then does `s`, a pointer, take the
; next free aligned pair, __rc4/__rc5. A pointer does not jump the queue.
; gfx_text wants the string in A/X.
x16_gfx_text:
        sta     X16_P0                  ; x lo
        stx     X16_P1                  ; x hi
        lda     __rc2
        sta     X16_P2                  ; y
        lda     __rc3
        sta     X16_P3                  ; color
        lda     __rc4                   ; A/X = string
        ldx     __rc5
        jmp     gfx_text

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_gfx_flood(unsigned int x, unsigned char y,
;                                          unsigned char color)
;   1 if the region was filled completely; 0 if the span stack overflowed
;   and the fill is incomplete.
; ---------------------------------------------------------------------
; x -> A/X, y -> __rc2, color -> __rc3.
x16_gfx_flood:
        sta     X16_P0                  ; x lo
        stx     X16_P1                  ; x hi
        lda     __rc2
        sta     X16_P2                  ; y
        lda     __rc3
        sta     X16_P3                  ; color
        jsr     gfx_flood               ; carry set = incomplete
        lda     #0
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
        bcs     .Lgfx_pset_off                    ; y >= 240

        lda     X16_P1                  ; x high byte
        beq     .Lgfx_pset_on                     ; x < 256, always on screen
        cmp     #1
        bne     .Lgfx_pset_off                    ; x >= 512
        lda     X16_P0
        cmp     #<GFX_WIDTH             ; 320 = $140, so x low must be < $40
        bcs     .Lgfx_pset_off
.Lgfx_pset_on:
        lda     #VERA_INC_0
        jsr     gfx_setptr
        lda     X16_P3
        sta     VERA_DATA0
.Lgfx_pset_off:
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
.Lgfx_rect_row:
        lda     X16_P6
        beq     .Lgfx_rect_done
        jsr     gfx_hline               ; leaves P0..P5 alone
        inc     X16_P2
        dec     X16_P6
        bra     .Lgfx_rect_row
.Lgfx_rect_done:
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
        bne     .Lgfx_frame_no_borrow
        dec     X16_P1
.Lgfx_frame_no_borrow:
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
        bpl     .Lgfx_line_dx_pos
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
        bra     .Lgfx_line_dx_done
.Lgfx_line_dx_pos:
        lda     #$01
        sta     gl_sx
        stz     gl_sx+1
.Lgfx_line_dx_done:

        ; dy = -|y1 - y0|, sy = sign
        sec
        lda     gl_y1
        sbc     gl_y0
        bpl     .Lgfx_line_dy_pos
        eor     #$FF
        clc
        adc     #1                      ; absolute value
        sta     gl_tmp
        lda     #$FF
        sta     gl_sy
        bra     .Lgfx_line_dy_done
.Lgfx_line_dy_pos:
        sta     gl_tmp
        lda     #$01
        sta     gl_sy
.Lgfx_line_dy_done:
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

.Lgfx_line_loop:
        jsr     plot

        lda     gl_x0                   ; reached the end point?
        cmp     gl_x1
        bne     .Lgfx_line_step
        lda     gl_x0+1
        cmp     gl_x1+1
        bne     .Lgfx_line_step
        lda     gl_y0
        cmp     gl_y1
        bne     .Lgfx_line_step
        rts

.Lgfx_line_step:
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
        bvc     .Lgfx_line_nv1
        eor     #$80                    ; signed compare: fold overflow into sign
.Lgfx_line_nv1:
        bmi     .Lgfx_line_skip_x
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
.Lgfx_line_skip_x:

        ; if e2 <= dx  ->  err += dx, y0 += sy
        sec
        lda     gl_dx
        sbc     gl_e2
        lda     gl_dx+1
        sbc     gl_e2+1
        bvc     .Lgfx_line_nv2
        eor     #$80
.Lgfx_line_nv2:
        bmi     .Lgfx_line_skip_y
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
.Lgfx_line_skip_y:
        jmp     .Lgfx_line_loop

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
        bne     .Lgfx_circle_go
        lda     gc_cx                   ; radius 0: a single point
        sta     X16_P0
        lda     gc_cx+1
        sta     X16_P1
        lda     gc_cy
        sta     X16_P2
        jsr     gfx_pset
        jmp     c_restore
.Lgfx_circle_go:
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

.Lgfx_circle_loop:
        lda     gc_x                    ; the 8 octant points
        ldy     gc_y
        jsr     c_plot4
        lda     gc_y
        ldy     gc_x
        jsr     c_plot4

        inc     gc_y
        lda     gc_err+1                ; err < 0 ?
        bmi     .Lgfx_circle_err_neg
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
        bne     .Lgfx_circle_add_err
        inc     X16_T1
.Lgfx_circle_add_err:
        clc
        lda     gc_err
        adc     X16_T0
        sta     gc_err
        lda     gc_err+1
        adc     X16_T1
        sta     gc_err+1
        bra     .Lgfx_circle_cont
.Lgfx_circle_err_neg:
        lda     gc_y                    ; err += 2*y + 1
        stz     X16_T1
        asl     a
        rol     X16_T1
        inc     a
        bne     .Lgfx_circle_add2
        inc     X16_T1
.Lgfx_circle_add2:
        clc
        adc     gc_err
        sta     gc_err
        lda     gc_err+1
        adc     X16_T1
        sta     gc_err+1
.Lgfx_circle_cont:
        lda     gc_y
        cmp     gc_x
        bcc     .Lgfx_circle_loop
        beq     .Lgfx_circle_loop_last
        jmp     c_restore
.Lgfx_circle_loop_last:
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

.Lgfx_disc_dloop:
        lda     gc_x                    ; spans at cy+/-y, half-width x
        ldy     gc_y
        jsr     c_span2
        lda     gc_y                    ; spans at cy+/-x, half-width y
        ldy     gc_x
        jsr     c_span2

        inc     gc_y
        lda     gc_err+1
        bmi     .Lgfx_disc_derr_neg
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
        bne     .Lgfx_disc_dadd
        inc     X16_T1
.Lgfx_disc_dadd:
        clc
        lda     gc_err
        adc     X16_T0
        sta     gc_err
        lda     gc_err+1
        adc     X16_T1
        sta     gc_err+1
        bra     .Lgfx_disc_dcont
.Lgfx_disc_derr_neg:
        lda     gc_y
        stz     X16_T1
        asl     a
        rol     X16_T1
        inc     a
        bne     .Lgfx_disc_dadd2
        inc     X16_T1
.Lgfx_disc_dadd2:
        clc
        adc     gc_err
        sta     gc_err
        lda     gc_err+1
        adc     X16_T1
        sta     gc_err+1
.Lgfx_disc_dcont:
        lda     gc_y
        cmp     gc_x
        bcc     .Lgfx_disc_dloop
        beq     .Lgfx_disc_dloop_last
        jmp     c_restore
.Lgfx_disc_dloop_last:
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
        bcs     .Lc_plot4_p4_low
        jsr     c_xpl
        jsr     gfx_pset
        jsr     c_xmi
        jsr     gfx_pset
.Lc_plot4_p4_low:
        jsr     c_ymi
        bcs     .Lc_plot4_p4_done
        jsr     c_xpl
        jsr     gfx_pset
        jsr     c_xmi
        jsr     gfx_pset
.Lc_plot4_p4_done:
        rts

; two clamped horizontal spans: rows cy +/- Y, half-width A
c_span2:
        sta     gc_ox
        sty     gc_oy
        jsr     c_ypl
        bcs     .Lc_span2_s2_lower
        jsr     c_hspan
.Lc_span2_s2_lower:
        lda     gc_oy
        beq     .Lc_span2_s2_done                ; same row twice: skip the mirror
        jsr     c_ymi
        bcs     .Lc_span2_s2_done
        jsr     c_hspan
.Lc_span2_s2_done:
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
        bpl     .Lc_hspan_left_ok
        stz     X16_T0
        stz     X16_T1
.Lc_hspan_left_ok:
        clc                             ; right = cx + ox, clamped to 319
        lda     gc_cx
        adc     gc_ox
        sta     X16_T2
        lda     gc_cx+1
        adc     #0
        sta     X16_T3
        lda     X16_T3                  ; right >= 320 ?
        cmp     #>320
        bcc     .Lc_hspan_right_ok
        bne     .Lc_hspan_clamp_r
        lda     X16_T2
        cmp     #<320
        bcc     .Lc_hspan_right_ok
.Lc_hspan_clamp_r:
        lda     #<319
        sta     X16_T2
        lda     #>319
        sta     X16_T3
.Lc_hspan_right_ok:
        sec                             ; entirely off screen?
        lda     X16_T2
        sbc     X16_T0
        sta     X16_T4
        lda     X16_T3
        sbc     X16_T1
        sta     X16_T5
        bmi     .Lc_hspan_off
        inc     X16_T4                  ; length = right - left + 1
        bne     .Lc_hspan_len_ok
        inc     X16_T5
.Lc_hspan_len_ok:
        lda     X16_T0
        sta     X16_P0
        lda     X16_T1
        sta     X16_P1
        lda     X16_T4
        sta     X16_P4
        lda     X16_T5
        sta     X16_P5
        jmp     gfx_hline
.Lc_hspan_off:
        rts

; X16_P2 = cy + gc_oy; carry set if the row is off screen.
c_ypl:
        clc
        lda     gc_cy
        adc     gc_oy
        bcs     .Lc_ypl_ypl_bad                ; past 255
        cmp     #GFX_HEIGHT
        bcs     .Lc_ypl_ypl_bad                ; 240..255
        sta     X16_P2
        clc
        rts
.Lc_ypl_ypl_bad:
        sec
        rts

; X16_P2 = cy - gc_oy; carry set if above the screen.
c_ymi:
        sec
        lda     gc_cy
        sbc     gc_oy
        bcc     .Lc_ymi_ymi_bad
        sta     X16_P2
        clc
        rts
.Lc_ymi_ymi_bad:
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
.Lgfx_char_fetch:
        lda     VERA_DATA1
        sta     gt_glyph,x
        inx
        cpx     #8
        bne     .Lgfx_char_fetch
        vera_addrsel 0

        lda     X16_P0                  ; park the caller's position
        sta     gt_bx
        lda     X16_P1
        sta     gt_bx+1
        lda     X16_P2
        sta     gt_by

        stz     gt_row
.Lgfx_char_rows:
        ldx     gt_row
        lda     gt_glyph,x
        sta     gt_bits
        beq     .Lgfx_char_next_row               ; a blank row: nothing to plot
        stz     gt_col
.Lgfx_char_cols:
        asl     gt_bits                 ; leftmost pixel first
        bcc     .Lgfx_char_next_col
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
        bcs     .Lgfx_char_next_col               ; wrapped past 255: off screen
        sta     X16_P2
        jsr     gfx_pset
.Lgfx_char_next_col:
        inc     gt_col
        lda     gt_col
        cmp     #8
        bne     .Lgfx_char_cols
.Lgfx_char_next_row:
        inc     gt_row
        lda     gt_row
        cmp     #8
        bne     .Lgfx_char_rows

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
        bcs     .Lgfx_flood_bail
        lda     X16_P1
        beq     .Lgfx_flood_seed_ok
        cmp     #1
        bne     .Lgfx_flood_bail
        lda     X16_P0
        cmp     #<GFX_WIDTH
        bcc     .Lgfx_flood_seed_ok
.Lgfx_flood_bail:
        clc
        rts
.Lgfx_flood_seed_ok:
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
        beq     .Lgfx_flood_bail                   ; already the fill colour: no-op

        stz     ff_sp
        stz     ff_ovf
        lda     ff_x
        sta     ff_px
        lda     ff_x+1
        sta     ff_px+1
        lda     ff_y
        sta     ff_ny
        jsr     f_push

.Lgfx_flood_main:
        lda     ff_sp
        bne     .Lgfx_flood_have_work
        jmp     .Lgfx_flood_finish
.Lgfx_flood_have_work:
        jsr     f_pop                   ; -> ff_x / ff_y
        jsr     f_rd
        cmp     ff_tgt
        bne     .Lgfx_flood_main                   ; painted over since it was queued

        ; grow the span left: xl = leftmost target pixel
        lda     ff_x
        sta     ff_xl
        lda     ff_x+1
        sta     ff_xl+1
        lda     ff_xl
        ora     ff_xl+1
        beq     .Lgfx_flood_left_done
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
.Lgfx_flood_left_scan:
        lda     VERA_DATA1
        cmp     ff_tgt
        bne     .Lgfx_flood_left_done
        lda     ff_xl
        bne     .Lgfx_flood_left_dec
        dec     ff_xl+1
.Lgfx_flood_left_dec:
        dec     ff_xl
        lda     ff_xl
        ora     ff_xl+1
        bne     .Lgfx_flood_left_scan
.Lgfx_flood_left_done:

        ; grow the span right: xr = rightmost target pixel
        lda     ff_x
        sta     ff_xr
        lda     ff_x+1
        sta     ff_xr+1
        jsr     f_at_right
        bcs     .Lgfx_flood_right_done
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
.Lgfx_flood_right_scan:
        lda     VERA_DATA1
        cmp     ff_tgt
        bne     .Lgfx_flood_right_done
        inc     ff_xr
        bne     .Lgfx_flood_right_chk
        inc     ff_xr+1
.Lgfx_flood_right_chk:
        jsr     f_at_right
        bcc     .Lgfx_flood_right_scan
.Lgfx_flood_right_done:

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
        bne     .Lgfx_flood_len_ok
        inc     X16_P5
.Lgfx_flood_len_ok:
        jsr     gfx_hline

        ; queue fresh spans in the rows above and below
        lda     ff_y
        beq     .Lgfx_flood_no_up
        dec     a
        sta     ff_ny
        jsr     f_scanrow
.Lgfx_flood_no_up:
        lda     ff_y
        cmp     #(GFX_HEIGHT - 1)
        bcs     .Lgfx_flood_no_down
        inc     a
        sta     ff_ny
        jsr     f_scanrow
.Lgfx_flood_no_down:
        jmp     .Lgfx_flood_main

.Lgfx_flood_finish:
        vera_addrsel 0
        lda     ff_ovf                  ; carry = "the fill may be incomplete"
        lsr     a
        rts

; carry set when ff_xr is the last column (319)
f_at_right:
        lda     ff_xr+1
        cmp     #>(GFX_WIDTH - 1)
        bne     .Lf_at_right_below
        lda     ff_xr
        cmp     #<(GFX_WIDTH - 1)
        bcs     .Lf_at_right_at
.Lf_at_right_below:
        clc
        rts
.Lf_at_right_at:
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
        bne     .Lf_scanrow_counted
        inc     ff_cnt+1
.Lf_scanrow_counted:
        stz     ff_seg
.Lf_scanrow_cell:
        lda     VERA_DATA1
        cmp     ff_tgt
        bne     .Lf_scanrow_break
        lda     ff_seg
        bne     .Lf_scanrow_step                   ; already inside a run
        jsr     f_push                  ; a run begins here: remember its start
        lda     #1
        sta     ff_seg
        bra     .Lf_scanrow_step
.Lf_scanrow_break:
        stz     ff_seg
.Lf_scanrow_step:
        inc     ff_px
        bne     .Lf_scanrow_count
        inc     ff_px+1
.Lf_scanrow_count:
        lda     ff_cnt
        bne     .Lf_scanrow_declo
        dec     ff_cnt+1
.Lf_scanrow_declo:
        dec     ff_cnt
        lda     ff_cnt
        ora     ff_cnt+1
        bne     .Lf_scanrow_cell
        rts

; push (ff_px, ff_ny); a full stack sets ff_ovf instead
f_push:
        lda     ff_sp
        cmp     #FF_DEPTH
        bcc     .Lf_push_room
        lda     #1
        sta     ff_ovf
        rts
.Lf_push_room:
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
        .section .bss,"aw",@nobits

gc_cx:     .zero  2
gc_cy:     .zero  1
gc_r:      .zero  1
gc_x:      .zero  1
gc_y:      .zero  1
gc_ox:     .zero  1
gc_oy:     .zero  1
gc_err:    .zero  2
gc_sav:    .zero  4

gt_code:   .zero  1
gt_hi:     .zero  1
gt_glyph:  .zero  8
gt_bx:     .zero  2
gt_by:     .zero  1
gt_row:    .zero  1
gt_col:    .zero  1
gt_bits:   .zero  1

ff_x:      .zero  2
ff_y:      .zero  1
ff_xl:     .zero  2
ff_xr:     .zero  2
ff_px:     .zero  2
ff_ny:     .zero  1
ff_ax:     .zero  2
ff_ay:     .zero  1
ff_h:      .zero  1
ff_tgt:    .zero  1
ff_col:    .zero  1
ff_seg:    .zero  1
ff_cnt:    .zero  2
ff_sp:     .zero  1
ff_ovf:    .zero  1
ff_stk:    .zero  FF_DEPTH * 3

gb_x:      .zero  2
gb_y:      .zero  1
gb_w:      .zero  2
gb_h:      .zero  1
gb_c:      .zero  1

gl_x0:     .zero  2
gl_y0:     .zero  1
gl_x1:     .zero  2
gl_y1:     .zero  1
gl_color:  .zero  1
gl_dx:     .zero  2
gl_dy:     .zero  2
gl_err:    .zero  2
gl_e2:     .zero  2
gl_sx:     .zero  2
gl_sy:     .zero  1
gl_tmp:    .zero  1
