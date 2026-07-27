; =====================================================================
; x16clib :: gfx/fb.s -- the KERNAL framebuffer driver API
; =====================================================================
; Wrappers over the stable FB_* jump table. The default driver is the
; ROM's 320x240@8bpp bitmap at VRAM $00000; GRAPH can install another.
;
; CALL x16_graph_init() FIRST. The FB entries dispatch through vectors
; that GRAPH_INIT installs; before that they point nowhere.
;
; The driver is a cursor machine: position the cursor, then get/set
; pixels advances it. That is what makes runs cheap -- no per-pixel
; address math.
;
; Zero counts are guarded here: the ROM driver's loops treat 0 as 256.
; =====================================================================

        .include        "macros.inc"

        .import         popa, popax
        .importzp       ptr1, ptr2

        .export         _x16_fb_init
        .export         _x16_fb_get_info
        .export         _x16_fb_set_palette
        .export         _x16_fb_cursor_position
        .export         _x16_fb_cursor_next_line
        .export         _x16_fb_get_pixel
        .export         _x16_fb_get_pixels
        .export         _x16_fb_set_pixel
        .export         _x16_fb_set_pixels
        .export         _x16_fb_set_8_pixels
        .export         _x16_fb_set_8_pixels_opaque
        .export         _x16_fb_fill_pixels
        .export         _x16_fb_filter_pixels
        .export         _x16_fb_move_pixels

        .segment        "CODE"

; ---------------------------------------------------------------------
; void x16_fb_init(void)
;
; Reinitialize the active framebuffer driver (mode registers, base).
; ---------------------------------------------------------------------
_x16_fb_init:
        jmp     FB_INIT

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_fb_get_info(unsigned int *width,
;                                            unsigned int *height)
;   returns the color depth in bits per pixel
; ---------------------------------------------------------------------
_x16_fb_get_info:
        sta     ptr2                    ; height* (rightmost arg: A/X)
        stx     ptr2+1
        jsr     popax                   ; width*
        sta     ptr1
        stx     ptr1+1

        jsr     FB_GET_INFO             ; r0 = width, r1 = height, A = depth
        pha
        ldy     #0
        lda     r0L
        sta     (ptr1),y
        lda     r1L
        sta     (ptr2),y
        iny
        lda     r0H
        sta     (ptr1),y
        lda     r1H
        sta     (ptr2),y
        pla
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_set_palette(const void *data,
;                                      unsigned char start,
;                                      unsigned char count)
;
; `data` is count*2 bytes of VERA GB/R words. count 0 means all 256.
; ---------------------------------------------------------------------
_x16_fb_set_palette:
        pha                             ; count (rightmost arg, in A)
        jsr     popa                    ; start
        pha
        jsr     popax                   ; data
        sta     r0L
        stx     r0H
        pla                             ; A = start
        plx                             ; X = count
        jmp     FB_SET_PALETTE

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_cursor_position(unsigned int x, unsigned int y)
; ---------------------------------------------------------------------
_x16_fb_cursor_position:
        sta     r1L                     ; y (rightmost arg: A/X)
        stx     r1H
        jsr     popax
        sta     r0L                     ; x
        stx     r0H
        jmp     FB_CURSOR_POSITION

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_cursor_next_line(unsigned int x)
;
; Move the cursor to the next scanline -- cheaper than a full
; cursor_position. The API passes x for drivers that need it; the
; default 320x240 driver keeps its own position and ignores it.
; ---------------------------------------------------------------------
_x16_fb_cursor_next_line:
        sta     r0L
        stx     r0H
        jmp     FB_CURSOR_NEXT_LINE

; ---------------------------------------------------------------------
; unsigned char x16_fb_get_pixel(void)
;   reads at the cursor and advances it
; ---------------------------------------------------------------------
_x16_fb_get_pixel:
        jsr     FB_GET_PIXEL
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_get_pixels(void *dst, unsigned int count)
; ---------------------------------------------------------------------
_x16_fb_get_pixels:
        sta     r1L                     ; count (rightmost arg: A/X)
        stx     r1H
        jsr     popax
        sta     r0L                     ; dst
        stx     r0H
        lda     r1L                     ; a zero count reads nothing
        ora     r1H
        beq     @done
        jmp     FB_GET_PIXELS
@done:
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_set_pixel(unsigned char color)
;   writes at the cursor and advances it
; ---------------------------------------------------------------------
_x16_fb_set_pixel:
        jmp     FB_SET_PIXEL

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_set_pixels(const void *src, unsigned int count)
; ---------------------------------------------------------------------
_x16_fb_set_pixels:
        sta     r1L                     ; count (rightmost arg: A/X)
        stx     r1H
        jsr     popax
        sta     r0L                     ; src
        stx     r0H
        lda     r1L                     ; a zero count writes nothing
        ora     r1H
        beq     @done
        jmp     FB_SET_PIXELS
@done:
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_set_8_pixels(unsigned char pattern,
;                                       unsigned char color)
;
; Draw the pattern's 1-bits in `color`, MSB first; 0-bits leave the
; underlying pixels alone. Always advances the cursor by 8.
; ---------------------------------------------------------------------
_x16_fb_set_8_pixels:
        tax                             ; X = color (rightmost arg, in A)
        jsr     popa                    ; A = pattern (popa preserves X)
        jmp     FB_SET_8_PIXELS

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_set_8_pixels_opaque(unsigned char pattern,
;                                              unsigned char mask,
;                                              unsigned char fg,
;                                              unsigned char bg)
;
; For each 1-bit of `mask` (MSB first): draw fg where the pattern bit is
; 1, bg where it is 0. 0-bits of the mask leave the pixel alone. Always
; advances the cursor by 8.
; ---------------------------------------------------------------------
_x16_fb_set_8_pixels_opaque:
        pha                             ; bg (rightmost arg, in A)
        jsr     popa                    ; fg
        pha
        jsr     popa                    ; mask
        pha
        jsr     popa                    ; pattern
        sta     r0L
        pla                             ; A = mask
        plx                             ; X = fg
        ply                             ; Y = bg
        jmp     FB_SET_8_PIXELS_OPAQUE

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_fill_pixels(unsigned int count,
;                                      unsigned int step,
;                                      unsigned char color)
;
; step 0 or 1 is a solid run (hardware-accelerated); larger steps space
; the pixels out -- vertical lines, dithers.
; ---------------------------------------------------------------------
_x16_fb_fill_pixels:
        pha                             ; color (rightmost arg, in A)
        jsr     popax                   ; step
        sta     r1L
        stx     r1H
        jsr     popax                   ; count
        sta     r0L
        stx     r0H
        pla
        ldx     r0L                     ; a zero count fills nothing
        bne     @go
        ldx     r0H
        beq     @done
@go:
        jmp     FB_FILL_PIXELS
@done:
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_filter_pixels(unsigned int count,
;                                        x16_fb_filter filter)
;
; Runs `filter` over count pixels from the cursor: it receives each
; color and returns the replacement. The ROM keeps its loop counters in
; X and Y across the callback, so the C function is called through a
; trampoline that preserves them. The callback must not touch VERA or
; call back into the fb/graph API.
; ---------------------------------------------------------------------
_x16_fb_filter_pixels:
        sta     filter_vec              ; filter (rightmost arg: A/X)
        stx     filter_vec+1
        jsr     popax                   ; count
        sta     r0L
        stx     r0H
        ora     r0H                     ; a zero count filters nothing
        beq     @done
        lda     #<filter_tramp
        sta     r1L
        lda     #>filter_tramp
        sta     r1H
        jmp     FB_FILTER_PIXELS
@done:
        rts

; The ROM calls r1's target with A = color and wants A = new color back,
; with X and Y intact -- they are its own loop counters. cc65 code
; clobbers both, so save them around the C call.
filter_tramp:
        phx
        phy
        jsr     @call
        ply
        plx
        rts
@call:
        jmp     (filter_vec)

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_move_pixels(unsigned int sx, unsigned int sy,
;                                      unsigned int tx, unsigned int ty,
;                                      unsigned int count)
;
; Copy a horizontal span of count pixels from (sx,sy) to (tx,ty).
; ---------------------------------------------------------------------
_x16_fb_move_pixels:
        sta     r4L                     ; count (rightmost arg: A/X)
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
        lda     r4L                     ; a zero count moves nothing
        ora     r4H
        beq     @done
        jmp     FB_MOVE_PIXELS
@done:
        rts

; =====================================================================

        .segment        "BSS"

filter_vec:
        .res    2
