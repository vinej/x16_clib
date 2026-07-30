; =====================================================================
; x16clib :: gfx/bitmap8l.s -- 320x240x256 bitmap drawing
; =====================================================================
; The framebuffer is 8bpp at VRAM $00000, one byte per pixel, rows of
; 320. A pixel is at y*320 + x.
;
; gfx8l_pset clips. The line/rect primitives do NOT: they assume their
; arguments are on screen. Clipping every span would cost more than it
; saves for a caller that already knows its geometry.
;
; Nothing here changes the screen mode. Call x16_gfx8l_init() once to
; switch the display to bitmap mode; the drawing routines only touch
; VRAM, so they also work on an off-screen buffer.
;
; Every C shim pops straight into the parameter block. popa and popax
; touch only A, X and Y, so no staging temporaries are needed -- unlike
; the sprite module, whose internal routines want arguments in X.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa, popax
        .import         vera_fill
        .import         screen_set_mode

        .export         _x16_gfx8l_init
        .export         _x16_gfx8l_clear
        .export         _x16_gfx8l_setptr
        .export         _x16_gfx8l_read
        .export         _x16_gfx8l_pset
        .export         _x16_gfx8l_hline
        .export         _x16_gfx8l_vline
        .export         _x16_gfx8l_rect
        .export         _x16_gfx8l_frame
        .export         _x16_gfx8l_line
        .export         _x16_gfx8l_char
        .export         _x16_gfx8l_text
        .export         _x16_gfx8l_pattern_set
        .export         _x16_gfx8l_pattern_rect
        .export         _x16_gfx8l_blit
        .export         _x16_gfx8l_blitm

        ; primitives the shared shape module (gfx/shapes.s) plots through
        .export         gfx8l_pset, gfx8l_hline, gfx8l_read

GFX8L_WIDTH  = 320
GFX8L_HEIGHT = 240

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_gfx8l_init(void)
;   320x240@256c bitmap on layer 0, 40x30 text on layer 1.
;   Returns 1 on success, 0 if the mode is unsupported.
; ---------------------------------------------------------------------
_x16_gfx8l_init:
        jsr     gfx8l_init                ; carry set = unsupported
        lda     #0
        ldx     #0
        rol     a
        eor     #1                      ; report success, not failure
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx8l_clear(unsigned char color)
;   Single argument, already in A: no shim.
; ---------------------------------------------------------------------
_x16_gfx8l_clear:
gfx8l_clear:
        ; 320*240 = 76800 = $12C00 bytes does not fit vera_fill's 16-bit
        ; count. Neither ACME nor ca65 complains: `ldy #>$12C00` quietly
        ; takes byte 1, so the count becomes $2C00 and only the top 35
        ; rows clear. Hence two halves; port 0 keeps auto-incrementing
        ; between the calls. Regression-tested by GFX8L_CLEAR, which checks
        ; the LAST pixel rather than the first.
        pha
        vera_addr 0, VRAM_BITMAP, VERA_INC_1
        pla
        pha
        ldx     #<(GFX8L_WIDTH * GFX8L_HEIGHT / 2)
        ldy     #>(GFX8L_WIDTH * GFX8L_HEIGHT / 2)
        jsr     vera_fill
        pla
        ldx     #<(GFX8L_WIDTH * GFX8L_HEIGHT / 2)
        ldy     #>(GFX8L_WIDTH * GFX8L_HEIGHT / 2)
        jmp     vera_fill

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx8l_setptr(unsigned char inc, unsigned int x,
;                                  unsigned char y)
;
; The packed planes return x's position within the byte; at 8bpp a pixel
; IS a byte, so there is nothing to hand back and this is void.
; No clipping -- the caller keeps (x,y) on screen.
; ---------------------------------------------------------------------
_x16_gfx8l_setptr:
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jsr     popa                    ; increment index
        jmp     gfx8l_setptr

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_gfx8l_read(unsigned int x,
;                                         unsigned char y)
;
; Unclipped, unlike the packed planes' read(): off-screen coordinates
; read whatever VRAM the wrapped address lands on.
; ---------------------------------------------------------------------
_x16_gfx8l_read:
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jsr     gfx8l_read
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx8l_pset(unsigned int x, unsigned char y,
;                                unsigned char color)
; ---------------------------------------------------------------------
_x16_gfx8l_pset:
        sta     X16_P3                  ; color
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx8l_pset

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx8l_hline(unsigned int x, unsigned char y,
;                                 unsigned int len, unsigned char color)
; ---------------------------------------------------------------------
_x16_gfx8l_hline:
        sta     X16_P3                  ; color
        jsr     popax
        sta     X16_P4                  ; len
        stx     X16_P5
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx8l_hline

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx8l_vline(unsigned int x, unsigned char y,
;                                 unsigned char len, unsigned char color)
;   len is 1-255: a column of a 240-row screen never needs more.
; ---------------------------------------------------------------------
_x16_gfx8l_vline:
        sta     X16_P3                  ; color
        jsr     popa
        sta     X16_P4                  ; len
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx8l_vline

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx8l_rect(unsigned int x, unsigned char y,
;                                unsigned int w, unsigned char h,
;                                unsigned char color)      -- filled
; void __fastcall__ x16_gfx8l_frame(... same ...)            -- outline
; ---------------------------------------------------------------------
_x16_gfx8l_rect:
        jsr     rect_marshal
        jmp     gfx8l_rect

_x16_gfx8l_frame:
        jsr     rect_marshal
        jmp     gfx8l_frame

rect_marshal:
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
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx8l_line(unsigned int x0, unsigned char y0,
;                                unsigned int x1, unsigned char y1,
;                                unsigned char color)
; ---------------------------------------------------------------------
_x16_gfx8l_line:
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
        jmp     gfx8l_line

; circle / disc / flood now live in the engine-agnostic gfx/shapes.s,
; which plots through the gfx8l_pset / gfx8l_hline / gfx8l_read exported above.

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx8l_char(unsigned int x, unsigned char y,
;                                unsigned char color, unsigned char code)
;   `code` is a SCREEN code, not PETSCII.
; ---------------------------------------------------------------------
_x16_gfx8l_char:
        pha                             ; screen code (rightmost arg, in A)
        jsr     popa
        sta     X16_P3                  ; color
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        pla
        jmp     gfx8l_char

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx8l_text(unsigned int x, unsigned char y,
;                                unsigned char color, const char *s)
;   The pointer already arrives as A = low, X = high.
; ---------------------------------------------------------------------
_x16_gfx8l_text:
        pha                             ; string lo
        phx                             ; string hi
        jsr     popa
        sta     X16_P3                  ; color
        jsr     popa
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        plx
        pla
        jmp     gfx8l_text

; =====================================================================
; Internal routines
; =====================================================================

gfx8l_init:
        lda     #$80
        jmp     screen_set_mode

; ---------------------------------------------------------------------
; gfx8l_setptr -- point data port 0 at pixel (x,y)
;   in:  A = increment index (VERA_INC_*)
;        X16_P0/P1 = x, X16_P2 = y
;
; y*320 = (y<<8) + (y<<6), so no multiply is needed. Result is 17-bit.
; Stepping by VERA_INC_320 then walks straight down a column.
; ---------------------------------------------------------------------
gfx8l_setptr:
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
; gfx8l_ld1 -- point VERA port 1 at the address gfx8l_setptr just computed
;
; gfx8l_blit's read-modify-write ops read the screen back while writing it,
; and DATA1 always reads port 1 whatever ADDRSEL says. T0/T1/T2 and T5
; still hold gfx8l_setptr's answer, so this is the same store sequence
; against the other port.
; ---------------------------------------------------------------------
gfx8l_ld1:
        lda     #VERA_CTRL_ADDRSEL
        tsb     VERA_CTRL
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
; gfx8l_pset -- set one pixel, clipped
;   in:  X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour
; ---------------------------------------------------------------------
gfx8l_pset:
        lda     X16_P2
        cmp     #GFX8L_HEIGHT
        bcs     @off                    ; y >= 240

        lda     X16_P1                  ; x high byte
        beq     @on                     ; x < 256, always on screen
        cmp     #1
        bne     @off                    ; x >= 512
        lda     X16_P0
        cmp     #<GFX8L_WIDTH             ; 320 = $140, so x low must be < $40
        bcs     @off
@on:
        lda     #VERA_INC_0
        jsr     gfx8l_setptr
        lda     X16_P3
        sta     VERA_DATA0
@off:
        rts

; ---------------------------------------------------------------------
; gfx8l_read -- read one pixel (no clip; caller keeps it on screen)
;   in:  X16_P0/P1 = x, X16_P2 = y   out: A = colour
; ---------------------------------------------------------------------
gfx8l_read:
        lda     #VERA_INC_0
        jsr     gfx8l_setptr
        lda     VERA_DATA0
        rts

; ---------------------------------------------------------------------
; gfx8l_hline -- in: X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour,
;                  X16_P4/P5 = length
; ---------------------------------------------------------------------
gfx8l_hline:
        lda     #VERA_INC_1
        jsr     gfx8l_setptr
        lda     X16_P3
        ldx     X16_P4
        ldy     X16_P5
        jmp     vera_fill

; ---------------------------------------------------------------------
; gfx8l_vline -- in: X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour,
;                  X16_P4 = length (1-255)
;
; VERA_INC_320 is one of the hardware's odd increments, so a vertical
; line is the same tight loop as a horizontal one.
; ---------------------------------------------------------------------
gfx8l_vline:
        lda     #VERA_INC_320
        jsr     gfx8l_setptr
        lda     X16_P3
        ldx     X16_P4
        ldy     #0
        jmp     vera_fill

; ---------------------------------------------------------------------
; gfx8l_rect -- filled rectangle
;   in:  X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour,
;        X16_P4/P5 = width, X16_P6 = height
; ---------------------------------------------------------------------
gfx8l_rect:
@row:
        lda     X16_P6
        beq     @done
        jsr     gfx8l_hline               ; leaves P0..P5 alone
        inc     X16_P2
        dec     X16_P6
        bra     @row
@done:
        rts

; ---------------------------------------------------------------------
; gfx8l_frame -- rectangle outline, same arguments as gfx8l_rect
; ---------------------------------------------------------------------
gfx8l_frame:
        ; Take a private copy of everything: gfx8l_vline reuses P4 as its
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
        jsr     gfx8l_hline

        jsr     restore_span            ; bottom edge, y + h - 1
        clc
        lda     gb_y
        adc     gb_h
        sec
        sbc     #1
        sta     X16_P2
        jsr     gfx8l_hline

        jsr     restore_col             ; left edge
        jsr     gfx8l_vline

        jsr     restore_col             ; right edge, x + w - 1
        clc
        lda     gb_x
        adc     gb_w
        sta     X16_P0
        lda     gb_x+1
        adc     gb_w+1
        sta     X16_P1
        lda     X16_P0
        bne     @no_borrow
        dec     X16_P1
@no_borrow:
        dec     X16_P0
        jsr     gfx8l_vline

        rts

; x, y, colour, width -- arguments for gfx8l_hline
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

; x, y, colour, height -- arguments for gfx8l_vline
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
; gfx8l_line -- Bresenham, any direction
;   in:  X16_P0/P1 = x0, X16_P2 = y0
;        X16_P3/P4 = x1, X16_P5 = y1
;        X16_P6    = colour
;
; Works entirely from its own variables, because gfx8l_pset wants the
; colour in X16_P3 -- which is where x1 lives on entry.
; ---------------------------------------------------------------------
gfx8l_line:
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
        bpl     @dx_pos
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
        bra     @dx_done
@dx_pos:
        lda     #$01
        sta     gl_sx
        stz     gl_sx+1
@dx_done:

        ; dy = -|y1 - y0|, sy = sign
        sec
        lda     gl_y1
        sbc     gl_y0
        bpl     @dy_pos
        eor     #$FF
        clc
        adc     #1                      ; absolute value
        sta     gl_tmp
        lda     #$FF
        sta     gl_sy
        bra     @dy_done
@dy_pos:
        sta     gl_tmp
        lda     #$01
        sta     gl_sy
@dy_done:
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

@loop:
        jsr     plot

        lda     gl_x0                   ; reached the end point?
        cmp     gl_x1
        bne     @step
        lda     gl_x0+1
        cmp     gl_x1+1
        bne     @step
        lda     gl_y0
        cmp     gl_y1
        bne     @step
        rts

@step:
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
        bvc     @nv1
        eor     #$80                    ; signed compare: fold overflow into sign
@nv1:
        bmi     @skip_x
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
@skip_x:

        ; if e2 <= dx  ->  err += dx, y0 += sy
        sec
        lda     gl_dx
        sbc     gl_e2
        lda     gl_dx+1
        sbc     gl_e2+1
        bvc     @nv2
        eor     #$80
@nv2:
        bmi     @skip_y
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
@skip_y:
        jmp     @loop

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
        jmp     gfx8l_pset

; circle / disc now live in the engine-agnostic gfx/shapes.s, which
; plots through the exported gfx8l_pset / gfx8l_hline / gfx8l_read.

; ---------------------------------------------------------------------
; gfx8l_char -- draw one glyph from the VRAM charset into the bitmap
;   in:  A = screen code (0-255)
;        X16_P0/P1 = x, X16_P2 = y, X16_P3 = colour
;
; Reads the 8-byte 1bpp glyph from the charset the KERNAL keeps at VRAM
; $1F000; set bits become colour pixels through gfx8l_pset (so text clips),
; clear bits stay transparent. Preserves X16_P0..P3.
; ---------------------------------------------------------------------
gfx8l_char:
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
@fetch:
        lda     VERA_DATA1
        sta     gt_glyph,x
        inx
        cpx     #8
        bne     @fetch
        vera_addrsel 0

        lda     X16_P0                  ; park the caller's position
        sta     gt_bx
        lda     X16_P1
        sta     gt_bx+1
        lda     X16_P2
        sta     gt_by

        stz     gt_row
@rows:
        ldx     gt_row
        lda     gt_glyph,x
        sta     gt_bits
        beq     @next_row               ; a blank row: nothing to plot
        stz     gt_col
@cols:
        asl     gt_bits                 ; leftmost pixel first
        bcc     @next_col
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
        bcs     @next_col               ; wrapped past 255: off screen
        sta     X16_P2
        jsr     gfx8l_pset
@next_col:
        inc     gt_col
        lda     gt_col
        cmp     #8
        bne     @cols
@next_row:
        inc     gt_row
        lda     gt_row
        cmp     #8
        bne     @rows

        lda     gt_bx                   ; restore the caller's block
        sta     X16_P0
        lda     gt_bx+1
        sta     X16_P1
        lda     gt_by
        sta     X16_P2
        rts

; ---------------------------------------------------------------------
; gfx8l_text -- a NUL-terminated string, 8 pixels per character
;   in:  A = string low, X = string high; X16_P0..P3 as gfx8l_char.
;   ASCII letters are converted to screen codes ('A'-'Z' work as
;   expected); X16_P0/P1 are left one past the final character.
;
; gt_lda must be a plain label so the pointer can live in its own operand
; -- and in ca65 a plain label ends the enclosing cheap-local scope, so
; this loop's labels are plain too. ACME's zone-local `.gt_lda` did not.
; ---------------------------------------------------------------------
gfx8l_text:
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
        jsr     gfx8l_char
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
; Module variables. Kept out of zero page: these are only touched by the
; routine that owns them, never across a call boundary.
; ---------------------------------------------------------------------
        .segment        "BSS"

gt_code:   .res 1
gt_hi:     .res 1
gt_glyph:  .res 8
gt_bx:     .res 2
gt_by:     .res 1
gt_row:    .res 1
gt_col:    .res 1
gt_bits:   .res 1

gb_x:      .res 2
gb_y:      .res 1
gb_w:      .res 2
gb_h:      .res 1
gb_c:      .res 1

gl_x0:     .res 2
gl_y0:     .res 1
gl_x1:     .res 2
gl_y1:     .res 1
gl_color:  .res 1
gl_dx:     .res 2
gl_dy:     .res 2
gl_err:    .res 2
gl_e2:     .res 2
gl_sx:     .res 2
gl_sy:     .res 1
gl_tmp:    .res 1

        .segment        "CODE"          ; the block above ended in BSS

; =====================================================================
; Patterns and blits -- the same surface the 2bpp module has
; =====================================================================
; These are the 8bpp halves of the two-way parity between the bitmap
; engines: a program can move between 320x240x256 and 640x480x4 without
; losing a primitive. Two of them differ from their 2bpp counterparts,
; and both differences come from one byte being one pixel here:
;
;   pattern_set takes background and foreground as whole bytes, where
;   the 2bpp version packs two 2-bit colours into one argument.
;
;   blitm needs no mask plane. At 8bpp colour 0 IS the mask -- a source
;   byte of zero means "leave the screen alone" -- so the source is
;   plain row-major pixels, not the 2bpp module's interleaved
;   (mask, data) pairs.
;
; Neither blit clips; keep them on screen.
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx8l_pattern_set(const unsigned char *pattern,
;                                       unsigned char bg, unsigned char fg)
; ---------------------------------------------------------------------
_x16_gfx8l_pattern_set:
        sta     X16_P5                  ; fg (rightmost arg, in A)
        jsr     popa
        sta     X16_P4                  ; bg
        jsr     popax                   ; A/X = pattern
        jmp     gfx8l_pattern_set

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx8l_pattern_rect(unsigned int x, unsigned int y,
;                                        unsigned int w, unsigned int h)
;
; gfx8l_pattern_rect wants P0/P1 = x, P2 = y, P4/P5 = width, P6 = height.
; ---------------------------------------------------------------------
_x16_gfx8l_pattern_rect:
        sta     X16_P6                  ; h (rightmost arg: A/X, low byte)
        jsr     popax
        sta     X16_P4                  ; w
        stx     X16_P5
        jsr     popax
        sta     X16_P2                  ; y (low byte: rows are 0-239)
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx8l_pattern_rect

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx8l_blit(unsigned int x, unsigned int y,
;                                unsigned char w, unsigned char h,
;                                const unsigned char *src,
;                                unsigned char op)
;   op: 0 copy, 1 OR, 2 AND, 3 XOR. w is in PIXELS (1-255).
; ---------------------------------------------------------------------
_x16_gfx8l_blit:
        pha                             ; op (rightmost arg, in A)
        jsr     popax
        sta     X16_P6                  ; src -- P6/P7 is X16_PTR3
        stx     X16_P7
        jsr     popa
        sta     X16_P5                  ; h
        jsr     popa
        sta     X16_P4                  ; w
        jsr     popax
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        pla                             ; A = op
        jmp     gfx8l_blit

; ---------------------------------------------------------------------
; void __fastcall__ x16_gfx8l_blitm(unsigned int x, unsigned int y,
;                                 unsigned char w, unsigned char h,
;                                 const unsigned char *src)
;   Masked: a source byte of 0 leaves the screen alone.
; ---------------------------------------------------------------------
_x16_gfx8l_blitm:
        sta     X16_P6                  ; src (rightmost arg: A/X)
        stx     X16_P7
        jsr     popa
        sta     X16_P5                  ; h
        jsr     popa
        sta     X16_P4                  ; w
        jsr     popax
        sta     X16_P2                  ; y
        jsr     popax
        sta     X16_P0                  ; x
        stx     X16_P1
        jmp     gfx8l_blitm

; ---------------------------------------------------------------------
; gfx8l_pattern_set -- cache an 8x8 1bpp pattern for gfx8l_pattern_rect
;   in:  A = pattern low, X = pattern high (8 row bytes, top first;
;            bit 7 is the leftmost pixel)
;        X16_P4 = background colour, X16_P5 = foreground colour
; ---------------------------------------------------------------------
gfx8l_pattern_set:
        sta     X16_T0
        stx     X16_T0+1
        ldy     #7
gp8_cp:
        lda     (X16_T0),y
        sta     gp8_pat,y
        dey
        bpl     gp8_cp
        lda     X16_P4
        sta     gp8_bg
        lda     X16_P5
        sta     gp8_fg
        rts

; ---------------------------------------------------------------------
; gfx8l_pattern_rect -- fill a rectangle with the cached pattern
;   in:  X16_P0/P1 = x, X16_P2 = y, X16_P4/P5 = width, X16_P6 = height
;   (P2 and P6 are consumed)
;
; Tiles from the screen origin, like the 2bpp module: the pattern cell
; under a pixel depends only on the pixel, not the rectangle.
; ---------------------------------------------------------------------
gfx8l_pattern_rect:
        lda     X16_P4                  ; zero width or height: draw nothing
        ora     X16_P5
        beq     gp8_done
        lda     X16_P6
        beq     gp8_done
        lda     X16_P0                  ; the column phase: x & 7, fixed for
        and     #7                      ; every row
        sta     gp8_rot
gp8_row:
        lda     #VERA_INC_1
        jsr     gfx8l_setptr
        lda     X16_P2                  ; the pattern row: y & 7
        and     #7
        tay
        lda     gp8_pat,y
        ldy     gp8_rot                 ; pre-rotate to the column phase
        beq     gp8_go
gp8_pre:
        asl     a
        adc     #0                      ; circular left: bit 7 wraps to bit 0
        dey
        bne     gp8_pre
gp8_go:
        sta     gp8_cur
        lda     X16_P4                  ; the width countdown, 16-bit
        sta     gb_t
        lda     X16_P5
        sta     gb_t+1
gp8_px:
        lda     gp8_cur                 ; bit 7 = this pixel
        bmi     gp8_fgpix
        lda     gp8_bg
        bra     gp8_out
gp8_fgpix:
        lda     gp8_fg
gp8_out:
        sta     VERA_DATA0
        lda     gp8_cur                 ; rotate to the next column
        asl     a
        adc     #0
        sta     gp8_cur
        lda     gb_t                    ; width--
        bne     gp8_nodec
        dec     gb_t+1
gp8_nodec:
        dec     gb_t
        lda     gb_t
        ora     gb_t+1
        bne     gp8_px
        inc     X16_P2                  ; the next row
        dec     X16_P6
        bne     gp8_row
gp8_done:
        rts

; ---------------------------------------------------------------------
; gfx8l_blit -- rows of pixel bytes from RAM to the framebuffer
;   in:  A = raster op: 0 copy, 1 OR, 2 AND, 3 XOR
;        X16_P0/P1 = x, X16_P2 = y, X16_P4 = width in PIXELS (1-255),
;        X16_P5 = height in rows, X16_P6/P7 = source (row-major)
;
; The source pointer is X16_PTR3 -- P6/P7 double as real zero page, the
; 2bpp module's own trick. No clipping. P2 and P5 are consumed.
;
; The three RMW ops share one loop: the opcode of the instruction at
; gb_opcode is patched from gb_optab (ora/and/eor abs), the gfx8l_text
; trick one byte earlier.
; ---------------------------------------------------------------------
gfx8l_blit:
        and     #3
        sta     gb_op
        beq     gb_row                  ; copy: no opcode to patch
        tax
        lda     gb_optab-1,x
        sta     gb_opcode
gb_row:
        lda     #VERA_INC_1
        jsr     gfx8l_setptr
        lda     gb_op
        beq     gb_copy
        jsr     gfx8l_ld1                 ; the RMW ops read through port 1
        ldy     #0
gb_oploop:
        lda     (X16_PTR3),y
gb_opcode:
        ora     VERA_DATA1              ; patched: op 1/2/3 = ora/and/eor
        sta     VERA_DATA0
        iny
        cpy     X16_P4
        bne     gb_oploop
        bra     gb_next
gb_copy:
        ldy     #0
gb_copyloop:
        lda     (X16_PTR3),y
        sta     VERA_DATA0
        iny
        cpy     X16_P4
        bne     gb_copyloop
gb_next:
        clc                             ; the next source row
        lda     X16_PTR3
        adc     X16_P4
        sta     X16_PTR3
        bcc     gb_nocarry
        inc     X16_PTR3+1
gb_nocarry:
        inc     X16_P2
        dec     X16_P5
        bne     gb_row
        rts

gb_optab: .byte $0D, $2D, $4D           ; ora / and / eor absolute

; ---------------------------------------------------------------------
; gfx8l_blitm -- a masked blit: byte $00 is transparent
;   in:  X16_P0/P1 = x, X16_P2 = y, X16_P4 = width in PIXELS (1-255),
;        X16_P5 = height, X16_P6/P7 = source (row-major)
;
; At 8bpp the mask IS the data: colour 0 means "leave the screen alone"
; (a read still advances the port, which is the whole trick). The 2bpp
; module needs interleaved mask bytes; one byte per pixel does not.
; P2 and P5 are consumed.
; ---------------------------------------------------------------------
gfx8l_blitm:
gm_row:
        lda     #VERA_INC_1
        jsr     gfx8l_setptr
        ldy     #0
gm_px:
        lda     (X16_PTR3),y
        beq     gm_skip
        sta     VERA_DATA0
        bra     gm_next
gm_skip:
        lda     VERA_DATA0              ; advance without writing
gm_next:
        iny
        cpy     X16_P4
        bne     gm_px
        clc
        lda     X16_PTR3
        adc     X16_P4
        sta     X16_PTR3
        bcc     gm_nocarry
        inc     X16_PTR3+1
gm_nocarry:
        inc     X16_P2
        dec     X16_P5
        bne     gm_row
        rts

        .segment        "BSS"

gp8_pat:  .res 8
gp8_bg:   .res 1
gp8_fg:   .res 1
gp8_rot:  .res 1
gp8_cur:  .res 1
gb_op:    .res 1
gb_t:     .res 2

        .segment        "CODE"
