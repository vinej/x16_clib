; =====================================================================
; x16clib :: gfx/bitmap8h.s -- VERA_2 640x480x256 SDRAM bitmap drawing
; =====================================================================
; Requires the MiSTer VERA_2 bitmap layer: the framebuffer is NOT VERA
; VRAM but the VERA_2 20-bit SDRAM byte address space behind $9F60-
; $9F6F. Feature-detect with x16_gfx8h_has() before relying on it --
; on stock hardware (and the emulator) every routine here writes into
; open bus.
;
; The framebuffer is 8bpp, one byte per pixel, rows of 640 bytes:
; offset = y*640 + x, 307,200 bytes in all.
;
; x16_gfx8h_pset()/read() clip; the span/rect/line/blit primitives do
; NOT (the 8bpp module's policy).
;
; The routine bodies are the x16_library bitmap8h module, byte for
; byte; only the C shims in front are new.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; (import dropped: popa, popax, popeax)
; (import dropped: sreg)

        .globl  x16_gfx8h_has
        .globl  x16_gfx8h_init
        .globl  x16_gfx8h_off
        .globl  x16_gfx8h_passthru_on
        .globl  x16_gfx8h_passthru_off
        .globl  x16_gfx8h_pal_gray
        .globl  x16_gfx8h_pal_set
        .globl  x16_gfx8h_pal_load
        .globl  x16_gfx8h_setptr
        .globl  x16_gfx8h_clear
        .globl  x16_gfx8h_pset
        .globl  x16_gfx8h_read
        .globl  x16_gfx8h_hline
        .globl  x16_gfx8h_vline
        .globl  x16_gfx8h_rect
        .globl  x16_gfx8h_frame
        .globl  x16_gfx8h_line
        .globl  x16_gfx8h_pattern_set
        .globl  x16_gfx8h_pattern_rect
        .globl  x16_gfx8h_blit
        .globl  x16_gfx8h_blitm
        .globl  x16_gfx8h_copy
        .globl  x16_gfx8h_copy_wait

        .section .text,"ax",@progbits

; llvm-mos argument placement, measured on the machine (see gfx/bitmap8l.s):
;   INTEGER bytes take A, then X, then single __rc bytes from __rc2 up;
;   a 16-bit int simply takes the next two byte slots; a LONG takes the
;   next four (measured for x16_gfx8h_copy with clang -Os -S). POINTERS
;   take aligned __rc PAIRS, skipping any pair already partly used.
; Returns: char in A; int in A/X.
; The x/y/len/w/h words land exactly as in gfx/bitmap2h.s.

; =====================================================================
; C entry points
; =====================================================================

; unsigned char x16_gfx8h_has(void)
;   1 if the VERA_2 bitmap layer answers, 0 otherwise
x16_gfx8h_has:
        jsr     gfx8h_has               ; carry set = present
        lda     #0
        rol     a
        rts

; void x16_gfx8h_init(void)
x16_gfx8h_init:
        jmp     gfx8h_init

; void x16_gfx8h_off(void)
x16_gfx8h_off:
        jmp     gfx8h_off

; void x16_gfx8h_passthru_on(void)
x16_gfx8h_passthru_on:
        jmp     gfx8h_passthru_on

; void x16_gfx8h_passthru_off(void)
x16_gfx8h_passthru_off:
        jmp     gfx8h_passthru_off

; void x16_gfx8h_pal_gray(void)
x16_gfx8h_pal_gray:
        jmp     gfx8h_pal_gray

; void x16_gfx8h_pal_set(unsigned char index,
;                       unsigned char lo, unsigned char hi)
;   lo = (G << 4) | B, hi = R
; index -> A, lo -> X, hi -> __rc2. The body wants X = index, A = lo,
; Y = hi.
x16_gfx8h_pal_set:
        ldy     mos8(__rc2)             ; hi
        pha                             ; park the index...
        txa                             ; lo -> A
        plx                             ; ...and pull it into X
        jmp     gfx8h_pal_set

; void x16_gfx8h_pal_load(const unsigned char *src,
;                        unsigned char first, unsigned char count)
; src (pointer) -> __rc2/__rc3; first -> A, count -> X -- exactly what
; the body wants, so only the pointer needs a store.
x16_gfx8h_pal_load:
        ldy     mos8(__rc2)
        sty     mos8(X16_P0)            ; src -- P0/P1 is X16_PTR0
        ldy     mos8(__rc3)
        sty     mos8(X16_P1)
        jmp     gfx8h_pal_load

; void x16_gfx8h_setptr(unsigned char inc, unsigned int x, unsigned int y)
;   inc is a VERA2_INC_* stride index
; inc -> A, x -> X/__rc2, y -> __rc3/__rc4.
x16_gfx8h_setptr:
        stx     mos8(X16_P0)            ; x lo
        ldx     mos8(__rc2)
        stx     mos8(X16_P1)            ; x hi
        ldx     mos8(__rc3)
        stx     mos8(X16_P2)            ; y lo
        ldx     mos8(__rc4)
        stx     mos8(X16_P3)            ; y hi
        jmp     gfx8h_setptr            ; A = stride index

; void x16_gfx8h_clear(unsigned char color)
;   color is already in A: no shim.
x16_gfx8h_clear:
        jmp     gfx8h_clear

; void x16_gfx8h_pset(unsigned int x, unsigned int y, unsigned char color)
; x -> A/X, y -> __rc2/__rc3, color -> __rc4.
x16_gfx8h_pset:
        sta     mos8(X16_P0)            ; x lo
        stx     mos8(X16_P1)            ; x hi
        lda     mos8(__rc2)
        sta     mos8(X16_P2)            ; y lo
        lda     mos8(__rc3)
        sta     mos8(X16_P3)            ; y hi
        lda     mos8(__rc4)             ; color
        jmp     gfx8h_pset

; unsigned int x16_gfx8h_read(unsigned int x, unsigned int y)
;   0-255, or 0xFFFF off screen: every 8-bit value is a valid colour,
;   so the sentinel needs the high byte. An int returns in A/X.
; x -> A/X, y -> __rc2/__rc3.
x16_gfx8h_read:
        sta     mos8(X16_P0)
        stx     mos8(X16_P1)
        lda     mos8(__rc2)
        sta     mos8(X16_P2)
        lda     mos8(__rc3)
        sta     mos8(X16_P3)
        jsr     gfx8h_read
        ldx     #0
        bcc     .Lg8hread_on
        lda     #$FF
        tax
.Lg8hread_on:
        rts

; void x16_gfx8h_hline(unsigned int x, unsigned int y,
;                     unsigned int len, unsigned char color)
; x -> A/X, y -> __rc2/__rc3, len -> __rc4/__rc5, color -> __rc6.
x16_gfx8h_hline:
        jsr     span_marshal
        lda     mos8(__rc6)             ; color
        jmp     gfx8h_hline

; void x16_gfx8h_vline(unsigned int x, unsigned int y,
;                     unsigned int len, unsigned char color)
x16_gfx8h_vline:
        jsr     span_marshal
        lda     mos8(__rc6)             ; color
        jmp     gfx8h_vline

; x, y and one more 16-bit argument into P0..P5
span_marshal:
        sta     mos8(X16_P0)            ; x lo
        stx     mos8(X16_P1)            ; x hi
        lda     mos8(__rc2)
        sta     mos8(X16_P2)            ; y lo
        lda     mos8(__rc3)
        sta     mos8(X16_P3)            ; y hi
        lda     mos8(__rc4)
        sta     mos8(X16_P4)            ; len/w lo
        lda     mos8(__rc5)
        sta     mos8(X16_P5)            ; len/w hi
        rts

; void x16_gfx8h_rect(unsigned int x, unsigned int y,
;                    unsigned int w, unsigned int h, unsigned char color)
; x -> A/X, y -> __rc2/__rc3, w -> __rc4/__rc5, h -> __rc6/__rc7,
; color -> __rc8.
x16_gfx8h_rect:
        jsr     quad_marshal
        lda     mos8(__rc8)             ; color
        jmp     gfx8h_rect

; void x16_gfx8h_frame(... same arguments ...)
x16_gfx8h_frame:
        jsr     quad_marshal
        lda     mos8(__rc8)             ; color
        jmp     gfx8h_frame

; void x16_gfx8h_line(unsigned int x0, unsigned int y0,
;                    unsigned int x1, unsigned int y1, unsigned char color)
;   the same four words land in the same four parameter slots
x16_gfx8h_line:
        jsr     quad_marshal
        lda     mos8(__rc8)             ; color
        jmp     gfx8h_line

; four 16-bit arguments into P0..P7
quad_marshal:
        jsr     span_marshal
        lda     mos8(__rc6)
        sta     mos8(X16_P6)            ; h/y1 lo
        lda     mos8(__rc7)
        sta     mos8(X16_P7)            ; h/y1 hi
        rts

; void x16_gfx8h_pattern_set(const unsigned char *pattern,
;                           unsigned char bg, unsigned char fg)
;   pattern is a pointer -> __rc2/__rc3; bg and fg take A and X. The
;   body wants A/X = pattern, P4 = bg, P5 = fg.
x16_gfx8h_pattern_set:
        sta     mos8(X16_P4)            ; bg
        stx     mos8(X16_P5)            ; fg
        lda     mos8(__rc2)
        ldx     mos8(__rc3)             ; A/X = pattern
        jmp     gfx8h_pattern_set

; void x16_gfx8h_pattern_rect(unsigned int x, unsigned int y,
;                            unsigned int w, unsigned int h)
x16_gfx8h_pattern_rect:
        jsr     quad_marshal
        jmp     gfx8h_pattern_rect

; void x16_gfx8h_blit(unsigned int x, unsigned int y,
;                    unsigned char w, unsigned char h,
;                    const unsigned char *src, unsigned char op)
; x -> A/X, y -> __rc2/__rc3, w -> __rc4, h -> __rc5,
; src -> the __rc6/__rc7 pair, op -> __rc8.
x16_gfx8h_blit:
        jsr     blit_marshal
        lda     mos8(__rc8)             ; op
        jmp     gfx8h_blit

; void x16_gfx8h_blitm(unsigned int x, unsigned int y,
;                     unsigned char w, unsigned char h,
;                     const unsigned char *src)
;   The same placement, without the trailing op.
x16_gfx8h_blitm:
        jsr     blit_marshal
        jmp     gfx8h_blitm

; x, y, two bytes, and a pointer into P0..P7
blit_marshal:
        sta     mos8(X16_P0)            ; x lo
        stx     mos8(X16_P1)            ; x hi
        lda     mos8(__rc2)
        sta     mos8(X16_P2)            ; y lo
        lda     mos8(__rc3)
        sta     mos8(X16_P3)            ; y hi
        lda     mos8(__rc4)
        sta     mos8(X16_P4)            ; w
        lda     mos8(__rc5)
        sta     mos8(X16_P5)            ; h
        lda     mos8(__rc6)
        sta     mos8(X16_P6)            ; src lo
        lda     mos8(__rc7)
        sta     mos8(X16_P7)            ; src hi
        rts

; void x16_gfx8h_copy(unsigned long src, unsigned long dst,
;                    unsigned long len)
;   VERA_2 hardware SDRAM-to-SDRAM copy; waits for completion
; A long is four byte slots in order (measured): src -> A/X/__rc2/__rc3,
; dst -> __rc4..__rc7, len -> __rc8..__rc11. The addresses are 20-bit,
; so each long's top byte is ignored. The body wants P0/P1/P2 = source,
; P3/P4/P5 = destination, A/X/Y = length.
x16_gfx8h_copy:
        sta     mos8(X16_P0)            ; src lo
        stx     mos8(X16_P1)            ; src mid
        lda     mos8(__rc2)
        sta     mos8(X16_P2)            ; src hi (bits 16-19)
        lda     mos8(__rc4)
        sta     mos8(X16_P3)            ; dst lo
        lda     mos8(__rc5)
        sta     mos8(X16_P4)            ; dst mid
        lda     mos8(__rc6)
        sta     mos8(X16_P5)            ; dst hi
        lda     mos8(__rc8)             ; A/X/Y = len lo/mid/hi
        ldx     mos8(__rc9)
        ldy     mos8(__rc10)
        jmp     gfx8h_copy

; void x16_gfx8h_copy_wait(void)
x16_gfx8h_copy_wait:
        jmp     gfx8h_copy_wait

; =====================================================================
; the x16_library bitmap8h module, verbatim
; =====================================================================
; =====================================================================
; x16lib :: gfx/bitmap8h.asm -- VERA_2 640x480x256 SDRAM bitmap drawing
; =====================================================================
; This file EMITS CODE. Source it exactly once (x16_code.asm does).
;
; Requires the MiSTer VERA_2 bitmap layer. The framebuffer is NOT VERA
; VRAM: it is the VERA_2 20-bit SDRAM byte address space behind $9F60-
; $9F6F. Feature-detect with gfx8h_has before relying on it.
;
; The framebuffer is 8bpp, one byte per pixel, rows of 640 bytes:
;   offset = y*640 + x, size = 307,200 bytes ($4B000).
;
; Calling convention follows the high-res engines:
;   X16_P0/P1 = x, X16_P2/P3 = y, colour in A.
; =====================================================================

; (zone: file scope in ca65)

GFX8H_WIDTH       = 640
GFX8H_HEIGHT      = 480
GFX8H_STRIDE      = 640
GFX8H_FRAME_PAGES = 1200       ; 307200 / 256

; ---------------------------------------------------------------------
; gfx8h_has -- feature-detect the VERA_2 bitmap layer
;   out: carry set if present, carry clear otherwise
; ---------------------------------------------------------------------
gfx8h_has:
    lda VERA2_ID
    cmp #VERA2_ID_MAGIC
    beq .Lgfx8h_has_yes
    clc
    rts
.Lgfx8h_has_yes:
    sec
    rts

; ---------------------------------------------------------------------
; gfx8h_init -- select 640x480@8bpp and load a grayscale palette
; gfx8h_off  -- disable the VERA_2 bitmap layer
; ---------------------------------------------------------------------
gfx8h_init:
    jsr gfx8h_pal_gray
    lda #(VERA2_CTRL_ENABLE | VERA2_CTRL_MODE_8BPP)
    sta VERA2_CTRL
    rts

gfx8h_off:
    stz VERA2_CTRL
    rts

gfx8h_passthru_on:
    lda VERA2_CTRL
    ora #VERA2_CTRL_PASSTHRU
    sta VERA2_CTRL
    rts

gfx8h_passthru_off:
    lda #$FF - VERA2_CTRL_PASSTHRU
    and VERA2_CTRL
    sta VERA2_CTRL
    rts

; ---------------------------------------------------------------------
; gfx8h_pal_set -- set one VERA_2 palette entry
;   in: X = index, A = low byte (G<<4 | B), Y = high byte (R)
; gfx8h_pal_load -- load entries from RAM
;   in: X16_PTR0 = source, A = first index, X = count (0 loads nothing)
; ---------------------------------------------------------------------
gfx8h_pal_set:
    sta g8h_t
    sty g8h_t2
    stx VERA2_PAL_IDX
    lda g8h_t
    sta VERA2_PAL_LO
    lda g8h_t2
    sta VERA2_PAL_HI
    rts

gfx8h_pal_load:
    cpx #0
    beq .Lgfx8h_pal_load_done
    sta VERA2_PAL_IDX
    stx g8h_n
    ldy #0
.Lgfx8h_pal_load_loop:
    lda (X16_PTR0),y
    sta VERA2_PAL_LO
    iny
    lda (X16_PTR0),y
    sta VERA2_PAL_HI
    iny
    dec g8h_n
    bne .Lgfx8h_pal_load_loop
.Lgfx8h_pal_load_done:
    rts

gfx8h_pal_gray:
    stz VERA2_PAL_IDX
    ldx #0
.Lgfx8h_pal_gray_loop:
    txa
    lsr
    lsr
    lsr
    lsr
    sta g8h_t                   ; v = index >> 4
    asl
    asl
    asl
    asl
    ora g8h_t
    sta VERA2_PAL_LO            ; G = B = v
    lda g8h_t
    sta VERA2_PAL_HI            ; R = v
    inx
    bne .Lgfx8h_pal_gray_loop
    rts

; ---------------------------------------------------------------------
; gfx8h_setptr -- point VERA_2 DATA at pixel (x,y)
;   in: A = VERA2_INC_* stride index, X16_P0/P1 = x, X16_P2/P3 = y
; ---------------------------------------------------------------------
gfx8h_setptr:
    asl
    asl
    asl
    asl
    sta g8h_inc
    jsr bitmap8h_addr_calc
    lda g8h_a0
    sta VERA2_ADDR_L
    lda g8h_a1
    sta VERA2_ADDR_M
    lda g8h_a2
    and #$0F
    ora g8h_inc
    sta VERA2_ADDR_H
    rts

; ---------------------------------------------------------------------
; gfx8h_clear -- fill the whole framebuffer with one colour
;   in: A = colour
; ---------------------------------------------------------------------
gfx8h_clear:
    sta g8h_c
    stz VERA2_ADDR_L
    stz VERA2_ADDR_M
    stz VERA2_ADDR_H            ; ptr 0, stride +1
    lda #<GFX8H_FRAME_PAGES
    sta g8h_n
    lda #>GFX8H_FRAME_PAGES
    sta g8h_n+1
    lda g8h_c
    jmp bitmap8h_fill_pages

; ---------------------------------------------------------------------
; gfx8h_pset / gfx8h_read -- clipped pixel access
;   pset in: A = colour, X16_P0/P1 = x, X16_P2/P3 = y
;   read out: carry clear, A = colour; carry set if off screen
; ---------------------------------------------------------------------
gfx8h_pset:
    sta g8h_c
    jsr bitmap8h_onscreen
    bcs .Lgfx8h_pset_off
    lda #VERA2_INC_1
    jsr gfx8h_setptr
    lda g8h_c
    sta VERA2_DATA
.Lgfx8h_pset_off:
    rts

gfx8h_read:
    jsr bitmap8h_onscreen
    bcs .Lgfx8h_read_off
    lda #VERA2_INC_0
    jsr gfx8h_setptr
    lda VERA2_DATA
    clc
.Lgfx8h_read_off:
    rts

; ---------------------------------------------------------------------
; gfx8h_hline / gfx8h_vline -- spans, no clipping
;   in: A = colour, X16_P0/P1 = x, X16_P2/P3 = y, X16_P4/P5 = length
; ---------------------------------------------------------------------
gfx8h_hline:
    sta g8h_c
    lda mos8(X16_P4)
    sta g8h_n
    lda mos8(X16_P5)
    sta g8h_n+1
    ora g8h_n
    beq .Lgfx8h_hline_done
    lda #VERA2_INC_1
    jsr gfx8h_setptr
    lda g8h_c
    jsr bitmap8h_fill_count
.Lgfx8h_hline_done:
    rts

gfx8h_vline:
    sta g8h_c
    lda mos8(X16_P4)
    sta g8h_n
    lda mos8(X16_P5)
    sta g8h_n+1
    ora g8h_n
    beq .Lgfx8h_vline_done
    lda #VERA2_INC_640
    jsr gfx8h_setptr
    lda g8h_c
    jsr bitmap8h_fill_count
.Lgfx8h_vline_done:
    rts

; ---------------------------------------------------------------------
; gfx8h_rect / gfx8h_frame -- rectangles, no clipping
;   in: A = colour, X16_P0/P1 = x, X16_P2/P3 = y,
;       X16_P4/P5 = width, X16_P6/P7 = height
; ---------------------------------------------------------------------
gfx8h_rect:
    sta g8h_rc
.Lgfx8h_rect_row:
    lda mos8(X16_P6)
    ora mos8(X16_P7)
    beq .Lgfx8h_rect_done
    lda g8h_rc
    jsr gfx8h_hline
    inc mos8(X16_P2)
    bne 1f
    inc mos8(X16_P3)
1:	lda X16_P6
    bne 1f
    dec mos8(X16_P7)
1:	dec X16_P6
    bra .Lgfx8h_rect_row
.Lgfx8h_rect_done:
    rts

gfx8h_frame:
    sta g8h_rc
    ldx #7
.Lgfx8h_frame_take:
    lda mos8(X16_P0),x
    sta g8h_fx,x
    dex
    bpl .Lgfx8h_frame_take

    jsr bitmap8h_frame_span
    lda g8h_rc
    jsr gfx8h_hline

    jsr bitmap8h_frame_span
    clc
    lda g8h_fy
    adc g8h_rh
    sta mos8(X16_P2)
    lda g8h_fy+1
    adc g8h_rh+1
    sta mos8(X16_P3)
    lda mos8(X16_P2)
    bne 1f
    dec mos8(X16_P3)
1:	dec X16_P2
    lda g8h_rc
    jsr gfx8h_hline

    jsr bitmap8h_frame_col
    lda g8h_rc
    jsr gfx8h_vline

    jsr bitmap8h_frame_col
    clc
    lda g8h_fx
    adc g8h_rw
    sta mos8(X16_P0)
    lda g8h_fx+1
    adc g8h_rw+1
    sta mos8(X16_P1)
    lda mos8(X16_P0)
    bne 1f
    dec mos8(X16_P1)
1:	dec X16_P0
    lda g8h_rc
    jmp gfx8h_vline

bitmap8h_frame_span:
    ldx #5
.Lbitmap8h_frame_span_s:
    lda g8h_fx,x
    sta mos8(X16_P0),x
    dex
    bpl .Lbitmap8h_frame_span_s
    rts

bitmap8h_frame_col:
    ldx #3
.Lbitmap8h_frame_col_c:
    lda g8h_fx,x
    sta mos8(X16_P0),x
    dex
    bpl .Lbitmap8h_frame_col_c
    lda g8h_rh
    sta mos8(X16_P4)
    lda g8h_rh+1
    sta mos8(X16_P5)
    rts

; ---------------------------------------------------------------------
; gfx8h_line -- Bresenham line, clipped by gfx8h_pset
;   in: A = colour, P0/P1=x0, P2/P3=y0, P4/P5=x1, P6/P7=y1
; ---------------------------------------------------------------------
gfx8h_line:
    sta g8h_lc
    ldx #7
.Lgfx8h_line_take:
    lda mos8(X16_P0),x
    sta g8h_lx0,x
    dex
    bpl .Lgfx8h_line_take

    sec
    lda g8h_lx1
    sbc g8h_lx0
    sta g8h_ldx
    lda g8h_lx1+1
    sbc g8h_lx0+1
    sta g8h_ldx+1
    bpl .Lgfx8h_line_dx_pos
    sec
    lda #0
    sbc g8h_ldx
    sta g8h_ldx
    lda #0
    sbc g8h_ldx+1
    sta g8h_ldx+1
    lda #$FF
    sta g8h_lsx
    sta g8h_lsx+1
    bra .Lgfx8h_line_dx_done
.Lgfx8h_line_dx_pos:
    lda #1
    sta g8h_lsx
    stz g8h_lsx+1
.Lgfx8h_line_dx_done:

    sec
    lda g8h_ly1
    sbc g8h_ly0
    sta g8h_ldy
    lda g8h_ly1+1
    sbc g8h_ly0+1
    sta g8h_ldy+1
    bpl .Lgfx8h_line_dy_pos
    sec
    lda #0
    sbc g8h_ldy
    sta g8h_ldy
    lda #0
    sbc g8h_ldy+1
    sta g8h_ldy+1
    lda #$FF
    sta g8h_lsy
    sta g8h_lsy+1
    bra .Lgfx8h_line_dy_done
.Lgfx8h_line_dy_pos:
    lda #1
    sta g8h_lsy
    stz g8h_lsy+1
.Lgfx8h_line_dy_done:
    sec
    lda #0
    sbc g8h_ldy
    sta g8h_ldy
    lda #0
    sbc g8h_ldy+1
    sta g8h_ldy+1

    clc
    lda g8h_ldx
    adc g8h_ldy
    sta g8h_lerr
    lda g8h_ldx+1
    adc g8h_ldy+1
    sta g8h_lerr+1

.Lgfx8h_line_loop:
    lda g8h_lc
    jsr bitmap8h_plot
    lda g8h_lx0
    cmp g8h_lx1
    bne .Lgfx8h_line_step
    lda g8h_lx0+1
    cmp g8h_lx1+1
    bne .Lgfx8h_line_step
    lda g8h_ly0
    cmp g8h_ly1
    bne .Lgfx8h_line_step
    lda g8h_ly0+1
    cmp g8h_ly1+1
    bne .Lgfx8h_line_step
    rts

.Lgfx8h_line_step:
    lda g8h_lerr
    asl
    sta g8h_le2
    lda g8h_lerr+1
    rol
    sta g8h_le2+1

    sec
    lda g8h_le2
    sbc g8h_ldy
    lda g8h_le2+1
    sbc g8h_ldy+1
    bvc .Lgfx8h_line_nv1
    eor #$80
.Lgfx8h_line_nv1:
    bmi .Lgfx8h_line_skip_x
    clc
    lda g8h_lerr
    adc g8h_ldy
    sta g8h_lerr
    lda g8h_lerr+1
    adc g8h_ldy+1
    sta g8h_lerr+1
    clc
    lda g8h_lx0
    adc g8h_lsx
    sta g8h_lx0
    lda g8h_lx0+1
    adc g8h_lsx+1
    sta g8h_lx0+1
.Lgfx8h_line_skip_x:
    sec
    lda g8h_ldx
    sbc g8h_le2
    lda g8h_ldx+1
    sbc g8h_le2+1
    bvc .Lgfx8h_line_nv2
    eor #$80
.Lgfx8h_line_nv2:
    bmi .Lgfx8h_line_skip_y
    clc
    lda g8h_lerr
    adc g8h_ldx
    sta g8h_lerr
    lda g8h_lerr+1
    adc g8h_ldx+1
    sta g8h_lerr+1
    clc
    lda g8h_ly0
    adc g8h_lsy
    sta g8h_ly0
    lda g8h_ly0+1
    adc g8h_lsy+1
    sta g8h_ly0+1
.Lgfx8h_line_skip_y:
    jmp .Lgfx8h_line_loop

bitmap8h_plot:
    sta g8h_c
    lda g8h_lx0
    sta mos8(X16_P0)
    lda g8h_lx0+1
    sta mos8(X16_P1)
    lda g8h_ly0
    sta mos8(X16_P2)
    lda g8h_ly0+1
    sta mos8(X16_P3)
    lda g8h_c
    jmp gfx8h_pset

; ---------------------------------------------------------------------
; gfx8h_pattern_set / gfx8h_pattern_rect
; ---------------------------------------------------------------------
gfx8h_pattern_set:
    sta mos8(X16_T0)
    stx mos8(X16_T0+1)
    ldy #7
.Lgfx8h_pattern_set_copy:
    lda (X16_T0),y
    sta gp8h_pat,y
    dey
    bpl .Lgfx8h_pattern_set_copy
    lda mos8(X16_P4)
    sta gp8h_bg
    lda mos8(X16_P5)
    sta gp8h_fg
    rts

gfx8h_pattern_rect:
    lda mos8(X16_P4)
    ora mos8(X16_P5)
    ora mos8(X16_P6)
    ora mos8(X16_P7)
    bne 1f
    jmp .Lgfx8h_pattern_rect_done
1:
    lda mos8(X16_P2)
    sta gp8h_by
    lda mos8(X16_P3)
    sta gp8h_by+1
    lda mos8(X16_P0)
    sta gp8h_bx
    lda mos8(X16_P1)
    sta gp8h_bx+1
.Lgfx8h_pattern_rect_row:
    lda mos8(X16_P6)
    ora mos8(X16_P7)
    bne 1f
    jmp .Lgfx8h_pattern_rect_done
1:
    lda gp8h_bx
    sta gp8h_x
    lda gp8h_bx+1
    sta gp8h_x+1
    lda mos8(X16_P4)
    sta gp8h_n
    lda mos8(X16_P5)
    sta gp8h_n+1
    lda mos8(X16_P2)
    and #7
    tay
    lda gp8h_pat,y
    sta gp8h_bits
.Lgfx8h_pattern_rect_col:
    lda gp8h_n
    ora gp8h_n+1
    beq .Lgfx8h_pattern_rect_next_row
    lda gp8h_bits
    bmi .Lgfx8h_pattern_rect_fg
    lda gp8h_bg
    bra .Lgfx8h_pattern_rect_plot
.Lgfx8h_pattern_rect_fg:
    lda gp8h_fg
.Lgfx8h_pattern_rect_plot:
    sta gp8h_c
    lda gp8h_x
    sta mos8(X16_P0)
    lda gp8h_x+1
    sta mos8(X16_P1)
    lda gp8h_by
    sta mos8(X16_P2)
    lda gp8h_by+1
    sta mos8(X16_P3)
    lda gp8h_c
    jsr gfx8h_pset
    lda gp8h_bits
    asl
    adc #0
    sta gp8h_bits
    inc gp8h_x
    bne 1f
    inc gp8h_x+1
1:	lda gp8h_n
    bne 1f
    dec gp8h_n+1
1:	dec gp8h_n
    jmp .Lgfx8h_pattern_rect_col
.Lgfx8h_pattern_rect_next_row:
    inc gp8h_by
    bne 1f
    inc gp8h_by+1
1:	lda gp8h_by
    sta mos8(X16_P2)
    lda gp8h_by+1
    sta mos8(X16_P3)
    lda mos8(X16_P6)
    bne 1f
    dec mos8(X16_P7)
1:	dec X16_P6
    jmp .Lgfx8h_pattern_rect_row
.Lgfx8h_pattern_rect_done:
    rts

; ---------------------------------------------------------------------
; gfx8h_blit / gfx8h_blitm -- RAM to framebuffer, row-major source
;   blit in: A = op (0 copy, 1 OR, 2 AND, 3 XOR)
;   common: P0/P1=x, P2/P3=y, P4=width (1-255), P5=height, P6/P7=source
; ---------------------------------------------------------------------
gfx8h_blit:
    and #3
    sta g8h_op
    bra bitmap8h_blit_common

gfx8h_blitm:
    lda #$80
    sta g8h_op
bitmap8h_blit_common:
    lda mos8(X16_P6)
    sta X16_PTR3
    lda mos8(X16_P7)
    sta X16_PTR3+1
.Lbitmap8h_blit_common_row:
    lda mos8(X16_P5)
    beq .Lbitmap8h_blit_common_done
    ldy #0
.Lbitmap8h_blit_common_col:
    cpy mos8(X16_P4)
    beq .Lbitmap8h_blit_common_next_row
    lda (X16_PTR3),y
    sta g8h_ink
    lda g8h_op
    bmi .Lbitmap8h_blit_common_masked
    beq .Lbitmap8h_blit_common_copy
    lda #VERA2_INC_0
    jsr gfx8h_setptr
    lda VERA2_DATA
    sta g8h_t
    lda g8h_op
    cmp #1
    beq .Lbitmap8h_blit_common_or
    cmp #2
    beq .Lbitmap8h_blit_common_and
    lda g8h_ink
    eor g8h_t
    bra .Lbitmap8h_blit_common_store
.Lbitmap8h_blit_common_and:
    lda g8h_ink
    and g8h_t
    bra .Lbitmap8h_blit_common_store
.Lbitmap8h_blit_common_or:
    lda g8h_ink
    ora g8h_t
    bra .Lbitmap8h_blit_common_store
.Lbitmap8h_blit_common_masked:
    lda g8h_ink
    beq .Lbitmap8h_blit_common_advance
.Lbitmap8h_blit_common_copy:
    lda g8h_ink
.Lbitmap8h_blit_common_store:
    jsr gfx8h_pset
.Lbitmap8h_blit_common_advance:
    inc mos8(X16_P0)
    bne 1f
    inc mos8(X16_P1)
1:	iny
    jmp .Lbitmap8h_blit_common_col
.Lbitmap8h_blit_common_next_row:
    sec
    lda mos8(X16_P0)
    sbc mos8(X16_P4)
    sta mos8(X16_P0)
    bcs 1f
    dec mos8(X16_P1)
1:	clc
    lda X16_PTR3
    adc mos8(X16_P4)
    sta X16_PTR3
    bcc 1f
    inc X16_PTR3+1
1:	inc X16_P2
    bne 1f
    inc mos8(X16_P3)
1:	dec X16_P5
    jmp .Lbitmap8h_blit_common_row
.Lbitmap8h_blit_common_done:
    rts

; ---------------------------------------------------------------------
; gfx8h_copy -- VERA_2 SDRAM-to-SDRAM hardware copy, then wait
;   in: P0/P1/P2 = source, P3/P4/P5 = destination, A/X/Y = length
; ---------------------------------------------------------------------
gfx8h_copy:
    sta VERA2_BLIT_LEN_L
    stx VERA2_BLIT_LEN_M
    sty VERA2_BLIT_LEN_H
    lda mos8(X16_P0)
    sta VERA2_ADDR_L
    lda mos8(X16_P1)
    sta VERA2_ADDR_M
    lda mos8(X16_P2)
    and #$0F
    sta VERA2_ADDR_H            ; source pointer, stride +1
    lda mos8(X16_P3)
    sta VERA2_BLIT_DST_L
    lda mos8(X16_P4)
    sta VERA2_BLIT_DST_M
    lda mos8(X16_P5)
    and #$0F
    sta VERA2_BLIT_DST_H
    lda #1
    sta VERA2_BLIT_CTRL
gfx8h_copy_wait:
    lda VERA2_BLIT_CTRL
    and #1
    bne gfx8h_copy_wait
    rts

; ---------------------------------------------------------------------
; private helpers
; ---------------------------------------------------------------------
bitmap8h_onscreen:
    lda mos8(X16_P1)
    cmp #>GFX8H_WIDTH
    bcc .Lbitmap8h_onscreen_xok
    bne .Lbitmap8h_onscreen_bad
    lda mos8(X16_P0)
    cmp #<GFX8H_WIDTH
    bcs .Lbitmap8h_onscreen_bad
.Lbitmap8h_onscreen_xok:
    lda mos8(X16_P3)
    cmp #>GFX8H_HEIGHT
    bcc .Lbitmap8h_onscreen_ok
    bne .Lbitmap8h_onscreen_bad
    lda mos8(X16_P2)
    cmp #<GFX8H_HEIGHT
    bcs .Lbitmap8h_onscreen_bad
.Lbitmap8h_onscreen_ok:
    clc
    rts
.Lbitmap8h_onscreen_bad:
    sec
    rts

bitmap8h_addr_calc:
    lda mos8(X16_P2)            ; y*640 = y*512 + y*128, in ~30 cycles:
    lsr                         ; lo = (y & 1) << 7
    tax                         ; md/hi = (y << 1) + (y >> 1)
    lda #0
    ror
    sta g8h_a0
    lda mos8(X16_P2)
    asl
    sta g8h_a1
    lda #0
    rol
    sta g8h_a2
    txa
    clc
    adc g8h_a1
    sta g8h_a1
    bcc 1f
    inc g8h_a2
1:
    lda mos8(X16_P3)            ; y >= 256: + 256*640 = $28000
    beq .Lbitmap8h_addr_calc_addx
    clc
    lda g8h_a1
    adc #$80
    sta g8h_a1
    lda g8h_a2
    adc #$02
    sta g8h_a2
.Lbitmap8h_addr_calc_addx:
    clc                         ; + x
    lda g8h_a0
    adc mos8(X16_P0)
    sta g8h_a0
    lda g8h_a1
    adc mos8(X16_P1)
    sta g8h_a1
    bcc 1f
    inc g8h_a2
1:	rts

bitmap8h_fill_count:
    ldy g8h_n+1                 ; high byte first, so beq tests the LOW byte:
    ldx g8h_n                  ; a partial low byte needs one extra dey pass,
    beq .Lbitmap8h_fill_count_full                  ; a zero low byte does not (testing the high
    iny                        ; byte made every width < 256 write 64K)
.Lbitmap8h_fill_count_full:
.Lbitmap8h_fill_count_loop:
    sta VERA2_DATA
    dex
    bne .Lbitmap8h_fill_count_loop
    dey
    bne .Lbitmap8h_fill_count_loop
    rts

bitmap8h_fill_pages:
    ldy g8h_n+1
.Lbitmap8h_fill_pages_outer:
    ldx #0
.Lbitmap8h_fill_pages_inner:
    sta VERA2_DATA
    dex
    bne .Lbitmap8h_fill_pages_inner
    lda g8h_n
    bne 1f
    dec g8h_n+1
1:	dec g8h_n
    lda g8h_n
    ora g8h_n+1
    beq .Lbitmap8h_fill_pages_done
    lda g8h_c
    bra .Lbitmap8h_fill_pages_outer
.Lbitmap8h_fill_pages_done:
    rts

; ---------------------------------------------------------------------
; data
; ---------------------------------------------------------------------
g8h_a0: .byte 0
g8h_a1: .byte 0
g8h_a2: .byte 0
g8h_inc:.byte 0
g8h_c:  .byte 0
g8h_t:  .byte 0
g8h_t2: .byte 0
g8h_n:  .word 0
g8h_op: .byte 0
g8h_ink:.byte 0

g8h_fx: .word 0
g8h_fy: .word 0
g8h_rw: .word 0
g8h_rh: .word 0
g8h_rc: .byte 0

gp8h_pat: .zero  8, 0
gp8h_bg:  .byte 0
gp8h_fg:  .byte 0
gp8h_bits:.byte 0
gp8h_bx:  .word 0
gp8h_x:   .word 0
gp8h_by:  .word 0
gp8h_n:   .word 0
gp8h_c:   .byte 0

g8h_lc:  .byte 0
g8h_lx0: .word 0
g8h_ly0: .word 0
g8h_lx1: .word 0
g8h_ly1: .word 0
g8h_ldx: .word 0
g8h_ldy: .word 0
g8h_lerr:.word 0
g8h_le2: .word 0
g8h_lsx: .word 0
g8h_lsy: .word 0

; (end zone)
