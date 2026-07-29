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

        include        "macros.inc"
        include        "x16zp.inc"

        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3
        zpage	r4
        zpage	r5
        zpage	r6
        zpage	sp


        global	_x16_fb_init
        global	_x16_fb_get_info
        global	_x16_fb_set_palette
        global	_x16_fb_cursor_position
        global	_x16_fb_cursor_next_line
        global	_x16_fb_get_pixel
        global	_x16_fb_get_pixels
        global	_x16_fb_set_pixel
        global	_x16_fb_set_pixels
        global	_x16_fb_set_8_pixels
        global	_x16_fb_set_8_pixels_opaque
        global	_x16_fb_fill_pixels
        global	_x16_fb_filter_pixels
        global	_x16_fb_move_pixels

        section text

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
        lda     r0                      ; width* and height* ride r0/r1 and
        sta     X16_TPTR0               ; r2/r3, which ARE the KERNAL r0 and
        lda     r1                      ; r1 this call answers in
        sta     X16_TPTR0+1
        lda     r2
        sta     X16_TPTR1
        lda     r3
        sta     X16_TPTR1+1

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
_x16_fb_set_palette:
        lda     r2                      ; start; data already rides r0/r1,
        ldx     r4                      ; which IS the KERNAL r0
        jmp     FB_SET_PALETTE

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_cursor_position(unsigned int x, unsigned int y)
; ---------------------------------------------------------------------
_x16_fb_cursor_position:
        jmp     FB_CURSOR_POSITION      ; x and y are already r0 and r1

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_cursor_next_line(unsigned int x)
;
; Move the cursor to the next scanline -- cheaper than a full
; cursor_position. The API passes x for drivers that need it; the
; default 320x240 driver keeps its own position and ignores it.
; ---------------------------------------------------------------------
_x16_fb_cursor_next_line:
        jmp     FB_CURSOR_NEXT_LINE     ; x already rides r0

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
        lda     r2                      ; the pointer already rides r0/r1 =
        sta     r1L                     ; KERNAL r0; only the count moves
        lda     r3
        sta     r1H
        ora     r1L                     ; a zero count reads nothing
        beq     .done
        jmp     FB_GET_PIXELS
.done:
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
        lda     r2                      ; the pointer already rides r0/r1 =
        sta     r1L                     ; KERNAL r0; only the count moves
        lda     r3
        sta     r1H
        ora     r1L                     ; a zero count writes nothing
        beq     .done
        jmp     FB_SET_PIXELS
.done:
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_set_8_pixels(unsigned char pattern,
;                                       unsigned char color)
;
; Draw the pattern's 1-bits in `color`, MSB first; 0-bits leave the
; underlying pixels alone. Always advances the cursor by 8.
; ---------------------------------------------------------------------
_x16_fb_set_8_pixels:
        lda     r0                      ; pattern
        ldx     r2                      ; color
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
        ldy     r6                      ; bg
        ldx     r4                      ; fg
        lda     r2                      ; mask -- read before r0L is set,
        pha                             ; because vbcc's r2 is not r0L but
        lda     r0                      ; A is the only way to move it
        sta     r0L                     ; pattern (r0 IS r0L: a no-op store
        pla                             ; kept for the reader's benefit)
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
        lda     r2                      ; step, up from KERNAL r1 to r1 --
        sta     r1L                     ; count already occupies r0
        lda     r3
        sta     r1H
        lda     r4                      ; A = color
        ldx     r0L                     ; a zero count fills nothing
        bne     .go
        ldx     r0H
        beq     .done
.go:
        jmp     FB_FILL_PIXELS
.done:
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
        lda     r2                      ; the filter pointer
        sta     filter_vec
        lda     r3
        sta     filter_vec+1
        lda     r0L                     ; count already occupies r0
        ora     r0H                     ; a zero count filters nothing
        beq     .done
        lda     #<filter_tramp
        sta     r1L
        lda     #>filter_tramp
        sta     r1H
        jmp     FB_FILTER_PIXELS
.done:
        rts

; The ROM calls r1's target with A = color and wants A = new color back,
; with X and Y intact -- they are its own loop counters. C code clobbers
; both, so they are saved around the call.
;
; AND SO IS THE WHOLE KERNAL REGISTER BLOCK. This is the vbcc-only
; hazard: vbcc's register variables r0..r15 ARE the KERNAL's virtual
; registers, the same bytes at $02-$21. FB_FILTER_PIXELS keeps its
; remaining count in r0 and this trampoline's own address in r1, so a C
; filter that writes vbcc's r0 -- and one taking its argument in A does
; exactly that, spilling A to r0 on entry -- destroys the ROM's loop
; counter and the call never returns. cc65 had no such clash: its
; scratch is ptr1/tmp1/sreg, well clear of the KERNAL block.
;
; Saving r0/r1 alone is not enough in practice, so all sixteen go. That
; is 32 pushes and 32 pulls per pixel, which is why the header says to
; keep a filter small -- but a wrong answer would be worse than a slow
; one, and the library cannot know how many registers a caller's filter
; will touch.
;
; The colour goes over in r0, where vbcc passes a lone char argument,
; and NOT in A. Pinning the parameter with __reg("a") looks tidier and
; does not work: a vbcc function that needs soft-stack space opens with
;       lda sp : bne : dec sp+1 : dec sp
; and that first `lda` destroys the incoming A before anything saves it.
; The symptom is a filter that sees $FF for every pixel.
filter_tramp:
        phx                             ; the ROM's loop counters
        phy
        sta     X16_T7                  ; the colour -- the saves below all
                                        ; go through A, and burying it under
                                        ; 32 stack bytes would be worse
        ldx     #0                      ; save the WHOLE KERNAL register
.save:                                  ; block; see the note above
        lda     r0L,x
        pha
        inx
        cpx     #32
        bne     .save

        lda     X16_T7                  ; hand the colour over in vbcc's r0,
        sta     r0L                     ; NOT in A -- see the note above
        jsr     .call                   ; A = the replacement
        sta     X16_T7

        ldx     #31
.restore:
        pla
        sta     r0L,x
        dex
        bpl     .restore

        lda     X16_T7
        ply
        plx
        rts
.call:
        jmp     (filter_vec)

; ---------------------------------------------------------------------
; void __fastcall__ x16_fb_move_pixels(unsigned int sx, unsigned int sy,
;                                      unsigned int tx, unsigned int ty,
;                                      unsigned int count)
;
; Copy a horizontal span of count pixels from (sx,sy) to (tx,ty).
; ---------------------------------------------------------------------
_x16_fb_move_pixels:
        ldy     #1                      ; sx, sy, tx, ty are already r0..r3;
        lda     (sp),y                  ; only the count spilled
        sta     r4H
        dey
        lda     (sp),y
        sta     r4L
        lda     r4L                     ; a zero count moves nothing
        ora     r4H
        beq     .done
        jmp     FB_MOVE_PIXELS
.done:
        rts

; =====================================================================

        section bss

filter_vec:
        reserve    2
