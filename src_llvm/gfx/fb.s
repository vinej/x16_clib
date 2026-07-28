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
        .include        "x16zp.inc"

; (import dropped: popa, popax)
; (import dropped: X16_TPTR0, X16_TPTR1)

        .globl  x16_fb_init
        .globl  x16_fb_get_info
        .globl  x16_fb_set_palette
        .globl  x16_fb_cursor_position
        .globl  x16_fb_cursor_next_line
        .globl  x16_fb_get_pixel
        .globl  x16_fb_get_pixels
        .globl  x16_fb_set_pixel
        .globl  x16_fb_set_pixels
        .globl  x16_fb_set_8_pixels
        .globl  x16_fb_set_8_pixels_opaque
        .globl  x16_fb_fill_pixels
        .globl  x16_fb_filter_pixels
        .globl  x16_fb_move_pixels

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void x16_fb_init(void)
;
; Reinitialize the active framebuffer driver (mode registers, base).
; ---------------------------------------------------------------------
x16_fb_init:
        jmp     FB_INIT

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_fb_get_info(unsigned int *width,
;                                            unsigned int *height)
;   returns the color depth in bits per pixel
; ---------------------------------------------------------------------
x16_fb_get_info:
        lda     mos8(__rc2)             ; width*; staged into the library's
        sta     mos8(X16_TPTR0)         ; own block, which does not alias
        lda     mos8(__rc3)             ; the KERNAL registers the call
        sta     mos8(X16_TPTR0+1)       ; is about to fill
        lda     mos8(__rc4)             ; height*
        sta     mos8(X16_TPTR1)
        lda     mos8(__rc5)
        sta     mos8(X16_TPTR1+1)

        jsr     FB_GET_INFO             ; r0 = width, r1 = height, A = depth
        pha
        ldy     #0
        lda     r0L
        sta     (X16_TPTR0),y
        lda     r1L
        sta     (X16_TPTR1),y
        iny
        lda     r0H
        sta     (X16_TPTR0),y
        lda     r1H
        sta     (X16_TPTR1),y
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
x16_fb_set_palette:
        jmp     FB_SET_PALETTE          ; data* already sits in r0 (it is
                                        ; __rc2/3), start in A, count in X

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_cursor_position(unsigned int x, unsigned int y)
; ---------------------------------------------------------------------
x16_fb_cursor_position:
        pha                             ; A and X hold the first
        phx                             ; argument; the loads below
                                        ; clobber both, so park them
        lda     mos8(__rc3)
        sta     r1H
        lda     mos8(__rc2)
        sta     r1L                ; y (rightmost arg: A/X)
        plx
        pla
        sta     r0L                ; x
        stx     r0H
        jmp     FB_CURSOR_POSITION

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_cursor_next_line(unsigned int x)
;
; Move the cursor to the next scanline -- cheaper than a full
; cursor_position. The API passes x for drivers that need it; the
; default 320x240 driver keeps its own position and ignores it.
; ---------------------------------------------------------------------
x16_fb_cursor_next_line:
        sta     r0L
        stx     r0H
        jmp     FB_CURSOR_NEXT_LINE

; ---------------------------------------------------------------------
; unsigned char x16_fb_get_pixel(void)
;   reads at the cursor and advances it
; ---------------------------------------------------------------------
x16_fb_get_pixel:
        jsr     FB_GET_PIXEL
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_get_pixels(void *dst, unsigned int count)
; ---------------------------------------------------------------------
x16_fb_get_pixels:
        sta     r1L                     ; count arrives in A/X; the
        stx     r1H                     ; dst pointer is already in r0
        lda     r1L                     ; a zero count reads nothing
        ora     r1H
        beq     .Lx16_fb_get_pixels_done
        jmp     FB_GET_PIXELS
.Lx16_fb_get_pixels_done:
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_set_pixel(unsigned char color)
;   writes at the cursor and advances it
; ---------------------------------------------------------------------
x16_fb_set_pixel:
        jmp     FB_SET_PIXEL

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_set_pixels(const void *src, unsigned int count)
; ---------------------------------------------------------------------
x16_fb_set_pixels:
        sta     r1L                     ; count arrives in A/X; the
        stx     r1H                     ; src pointer is already in r0
        lda     r1L                     ; a zero count writes nothing
        ora     r1H
        beq     .Lx16_fb_set_pixels_done
        jmp     FB_SET_PIXELS
.Lx16_fb_set_pixels_done:
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_set_8_pixels(unsigned char pattern,
;                                       unsigned char color)
;
; Draw the pattern's 1-bits in `color`, MSB first; 0-bits leave the
; underlying pixels alone. Always advances the cursor by 8.
; ---------------------------------------------------------------------
x16_fb_set_8_pixels:
        jmp     FB_SET_8_PIXELS         ; pattern in A, color in X already

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
x16_fb_set_8_pixels_opaque:
        sta     mos8(X16_T0)            ; park the pattern: writing it to
        ldy     mos8(__rc3)             ; r0L would bury fg, because r0L
        lda     mos8(__rc2)             ; and __rc2 are the same byte
        pha                             ; fg
        lda     mos8(X16_T0)
        sta     r0L                     ; pattern -- fg is safe now
        txa                             ; A = mask
        plx                             ; X = fg, Y already holds bg
        jmp     FB_SET_8_PIXELS_OPAQUE

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_fill_pixels(unsigned int count,
;                                      unsigned int step,
;                                      unsigned char color)
;
; step 0 or 1 is a solid run (hardware-accelerated); larger steps space
; the pixels out -- vertical lines, dithers.
; ---------------------------------------------------------------------
x16_fb_fill_pixels:
        pha                             ; park count low
        lda     mos8(__rc4)             ; color, read before r1H buries it
        pha
        lda     mos8(__rc3)             ; step high -- __rc2/3 IS r0, so
        sta     r1H                     ; step must move up to r1 before
        lda     mos8(__rc2)             ; count is written into r0
        sta     r1L
        pla                             ; A = color
        tay
        pla                             ; A = count low
        sta     r0L
        stx     r0H
        tya
        ldx     r0L                     ; a zero count fills nothing
        bne     .Lx16_fb_fill_pixels_go
        ldx     r0H
        beq     .Lx16_fb_fill_pixels_done
.Lx16_fb_fill_pixels_go:
        jmp     FB_FILL_PIXELS
.Lx16_fb_fill_pixels_done:
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
x16_fb_filter_pixels:
        ldy     mos8(__rc2)             ; the filter pointer lives in r0,
        sty     filter_vec              ; which count is about to occupy
        ldy     mos8(__rc3)
        sty     filter_vec+1
        sta     r0L                     ; count
        stx     r0H
        ora     r0H                     ; a zero count filters nothing
        beq     .Lx16_fb_filter_pixels_done
        lda     #<filter_tramp
        sta     r1L
        lda     #>filter_tramp
        sta     r1H
        jmp     FB_FILTER_PIXELS
.Lx16_fb_filter_pixels_done:
        rts

; The ROM calls r1's target with A = color and wants A = new color back,
; with X and Y intact -- they are its own loop counters. cc65 code
; clobbers both, so save them around the C call.
filter_tramp:
        phx
        phy
        jsr     .Lfilter_tramp_call
        ply
        plx
        rts
.Lfilter_tramp_call:
        jmp     (filter_vec)

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_move_pixels(unsigned int sx, unsigned int sy,
;                                      unsigned int tx, unsigned int ty,
;                                      unsigned int count)
;
; Copy a horizontal span of count pixels from (sx,sy) to (tx,ty).
; ---------------------------------------------------------------------
x16_fb_move_pixels:
        pha                             ; sx: A and X are needed last but
        phx                             ; the loads below clobber them
        lda     mos8(__rc9)             ; highest destination first: the
        sta     r4H                     ; sources are the same bytes one
        lda     mos8(__rc8)             ; KERNAL register lower down
        sta     r4L                     ; count
        lda     mos8(__rc7)
        sta     r3H
        lda     mos8(__rc6)
        sta     r3L                     ; ty
        lda     mos8(__rc5)
        sta     r2H
        lda     mos8(__rc4)
        sta     r2L                     ; tx
        lda     mos8(__rc3)
        sta     r1H
        lda     mos8(__rc2)
        sta     r1L                     ; sy
        plx
        pla
        sta     r0L                     ; sx
        stx     r0H
        lda     r4L                     ; a zero count moves nothing
        ora     r4H
        beq     .Lx16_fb_move_pixels_done
        jmp     FB_MOVE_PIXELS
.Lx16_fb_move_pixels_done:
        rts

; =====================================================================

        .section .bss,"aw",@nobits

filter_vec:
        .zero  2
