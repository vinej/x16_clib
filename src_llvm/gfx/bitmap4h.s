; =====================================================================
; x16clib :: gfx/bitmap4h.s -- VERA_2 640x480x16 SDRAM bitmap drawing
; =====================================================================
; Requires the MiSTer VERA_2 bitmap layer: the framebuffer is NOT VERA
; VRAM but the VERA_2 20-bit SDRAM byte address space behind $9F60-
; $9F6F. Feature-detect with x16_gfx4h_has() before relying on it --
; on stock hardware (and the emulator) every routine here writes into
; open bus.
;
; The framebuffer is 4bpp, two pixels per byte, rows of 320 bytes:
; offset = y*320 + (x>>1), 153,600 bytes in all.
;
; x16_gfx4h_pset()/read() clip; the span/rect/line/blit primitives do
; NOT (the 8bpp module's policy).
;
; The routine bodies are the x16_library bitmap4h module, byte for
; byte; only the C shims in front are new.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; (import dropped: popa, popax, popeax)
; (import dropped: sreg)

        .globl  x16_gfx4h_has
        .globl  x16_gfx4h_init
        .globl  x16_gfx4h_off
        .globl  x16_gfx4h_passthru_on
        .globl  x16_gfx4h_passthru_off
        .globl  x16_gfx4h_pal_gray
        .globl  x16_gfx4h_pal_set
        .globl  x16_gfx4h_pal_load
        .globl  x16_gfx4h_setptr
        .globl  x16_gfx4h_clear
        .globl  x16_gfx4h_pset
        .globl  x16_gfx4h_read
        .globl  x16_gfx4h_hline
        .globl  x16_gfx4h_vline
        .globl  x16_gfx4h_rect
        .globl  x16_gfx4h_frame
        .globl  x16_gfx4h_line
        .globl  x16_gfx4h_pattern_set
        .globl  x16_gfx4h_pattern_rect
        .globl  x16_gfx4h_blit
        .globl  x16_gfx4h_blitm
        .globl  x16_gfx4h_copy
        .globl  x16_gfx4h_copy_wait

        .section .text,"ax",@progbits

; llvm-mos argument placement, measured on the machine (see gfx/bitmap8l.s):
;   INTEGER bytes take A, then X, then single __rc bytes from __rc2 up;
;   a 16-bit int simply takes the next two byte slots; a LONG takes the
;   next four (measured for x16_gfx4h_copy with clang -Os -S). POINTERS
;   take aligned __rc PAIRS, skipping any pair already partly used.
; Returns: char in A; int in A/X.
; The x/y/len/w/h words land exactly as in gfx/bitmap2h.s.

; =====================================================================
; C entry points
; =====================================================================

; unsigned char x16_gfx4h_has(void)
;   1 if the VERA_2 bitmap layer answers, 0 otherwise
x16_gfx4h_has:
        jsr     gfx4h_has               ; carry set = present
        lda     #0
        rol     a
        rts

; void x16_gfx4h_init(void)
x16_gfx4h_init:
        jmp     gfx4h_init

; void x16_gfx4h_off(void)
x16_gfx4h_off:
        jmp     gfx4h_off

; void x16_gfx4h_passthru_on(void)
x16_gfx4h_passthru_on:
        jmp     gfx4h_passthru_on

; void x16_gfx4h_passthru_off(void)
x16_gfx4h_passthru_off:
        jmp     gfx4h_passthru_off

; void x16_gfx4h_pal_gray(void)
x16_gfx4h_pal_gray:
        jmp     gfx4h_pal_gray

; void x16_gfx4h_pal_set(unsigned char index,
;                       unsigned char lo, unsigned char hi)
;   lo = (G << 4) | B, hi = R
; index -> A, lo -> X, hi -> __rc2. The body wants X = index, A = lo,
; Y = hi.
x16_gfx4h_pal_set:
        ldy     __rc2                   ; hi
        pha                             ; park the index...
        txa                             ; lo -> A
        plx                             ; ...and pull it into X
        jmp     gfx4h_pal_set

; void x16_gfx4h_pal_load(const unsigned char *src,
;                        unsigned char first, unsigned char count)
; src (pointer) -> __rc2/__rc3; first -> A, count -> X -- exactly what
; the body wants, so only the pointer needs a store.
x16_gfx4h_pal_load:
        ldy     __rc2
        sty     X16_P0                  ; src -- P0/P1 is X16_PTR0
        ldy     __rc3
        sty     X16_P1
        jmp     gfx4h_pal_load

; void x16_gfx4h_setptr(unsigned char inc, unsigned int x, unsigned int y)
;   inc is a VERA2_INC_* stride index
; inc -> A, x -> X/__rc2, y -> __rc3/__rc4.
x16_gfx4h_setptr:
        stx     X16_P0                  ; x lo
        ldx     __rc2
        stx     X16_P1                  ; x hi
        ldx     __rc3
        stx     X16_P2                  ; y lo
        ldx     __rc4
        stx     X16_P3                  ; y hi
        jmp     gfx4h_setptr            ; A = stride index

; void x16_gfx4h_clear(unsigned char color)
;   color is already in A: no shim.
x16_gfx4h_clear:
        jmp     gfx4h_clear

; void x16_gfx4h_pset(unsigned int x, unsigned int y, unsigned char color)
; x -> A/X, y -> __rc2/__rc3, color -> __rc4.
x16_gfx4h_pset:
        sta     X16_P0                  ; x lo
        stx     X16_P1                  ; x hi
        lda     __rc2
        sta     X16_P2                  ; y lo
        lda     __rc3
        sta     X16_P3                  ; y hi
        lda     __rc4                   ; color
        jmp     gfx4h_pset

; unsigned char x16_gfx4h_read(unsigned int x, unsigned int y)
;   0-15, or $FF off screen (the body answers with carry set)
; x -> A/X, y -> __rc2/__rc3.
x16_gfx4h_read:
        sta     X16_P0
        stx     X16_P1
        lda     __rc2
        sta     X16_P2
        lda     __rc3
        sta     X16_P3
        jsr     gfx4h_read
        bcc     .Lg4hread_on
        lda     #$FF
.Lg4hread_on:
        rts

; void x16_gfx4h_hline(unsigned int x, unsigned int y,
;                     unsigned int len, unsigned char color)
; x -> A/X, y -> __rc2/__rc3, len -> __rc4/__rc5, color -> __rc6.
x16_gfx4h_hline:
        jsr     span_marshal
        lda     __rc6                   ; color
        jmp     gfx4h_hline

; void x16_gfx4h_vline(unsigned int x, unsigned int y,
;                     unsigned int len, unsigned char color)
x16_gfx4h_vline:
        jsr     span_marshal
        lda     __rc6                   ; color
        jmp     gfx4h_vline

; x, y and one more 16-bit argument into P0..P5
span_marshal:
        sta     X16_P0                  ; x lo
        stx     X16_P1                  ; x hi
        lda     __rc2
        sta     X16_P2                  ; y lo
        lda     __rc3
        sta     X16_P3                  ; y hi
        lda     __rc4
        sta     X16_P4                  ; len/w lo
        lda     __rc5
        sta     X16_P5                  ; len/w hi
        rts

; void x16_gfx4h_rect(unsigned int x, unsigned int y,
;                    unsigned int w, unsigned int h, unsigned char color)
; x -> A/X, y -> __rc2/__rc3, w -> __rc4/__rc5, h -> __rc6/__rc7,
; color -> __rc8.
x16_gfx4h_rect:
        jsr     quad_marshal
        lda     __rc8                   ; color
        jmp     gfx4h_rect

; void x16_gfx4h_frame(... same arguments ...)
x16_gfx4h_frame:
        jsr     quad_marshal
        lda     __rc8                   ; color
        jmp     gfx4h_frame

; void x16_gfx4h_line(unsigned int x0, unsigned int y0,
;                    unsigned int x1, unsigned int y1, unsigned char color)
;   the same four words land in the same four parameter slots
x16_gfx4h_line:
        jsr     quad_marshal
        lda     __rc8                   ; color
        jmp     gfx4h_line

; four 16-bit arguments into P0..P7
quad_marshal:
        jsr     span_marshal
        lda     __rc6
        sta     X16_P6                  ; h/y1 lo
        lda     __rc7
        sta     X16_P7                  ; h/y1 hi
        rts

; void x16_gfx4h_pattern_set(const unsigned char *pattern,
;                           unsigned char bg, unsigned char fg)
;   pattern is a pointer -> __rc2/__rc3; bg and fg take A and X. The
;   body wants A/X = pattern, P4 = bg, P5 = fg.
x16_gfx4h_pattern_set:
        sta     X16_P4                  ; bg
        stx     X16_P5                  ; fg
        lda     __rc2
        ldx     __rc3                   ; A/X = pattern
        jmp     gfx4h_pattern_set

; void x16_gfx4h_pattern_rect(unsigned int x, unsigned int y,
;                            unsigned int w, unsigned int h)
x16_gfx4h_pattern_rect:
        jsr     quad_marshal
        jmp     gfx4h_pattern_rect

; void x16_gfx4h_blit(unsigned int x, unsigned int y,
;                    unsigned char w, unsigned char h,
;                    const unsigned char *src, unsigned char op)
; x -> A/X, y -> __rc2/__rc3, w -> __rc4, h -> __rc5,
; src -> the __rc6/__rc7 pair, op -> __rc8.
x16_gfx4h_blit:
        jsr     blit_marshal
        lda     __rc8                   ; op
        jmp     gfx4h_blit

; void x16_gfx4h_blitm(unsigned int x, unsigned int y,
;                     unsigned char w, unsigned char h,
;                     const unsigned char *src)
;   The same placement, without the trailing op.
x16_gfx4h_blitm:
        jsr     blit_marshal
        jmp     gfx4h_blitm

; x, y, two bytes, and a pointer into P0..P7
blit_marshal:
        sta     X16_P0                  ; x lo
        stx     X16_P1                  ; x hi
        lda     __rc2
        sta     X16_P2                  ; y lo
        lda     __rc3
        sta     X16_P3                  ; y hi
        lda     __rc4
        sta     X16_P4                  ; w
        lda     __rc5
        sta     X16_P5                  ; h
        lda     __rc6
        sta     X16_P6                  ; src lo
        lda     __rc7
        sta     X16_P7                  ; src hi
        rts

; void x16_gfx4h_copy(unsigned long src, unsigned long dst,
;                    unsigned long len)
;   VERA_2 hardware SDRAM-to-SDRAM copy; waits for completion
; A long is four byte slots in order (measured): src -> A/X/__rc2/__rc3,
; dst -> __rc4..__rc7, len -> __rc8..__rc11. The addresses are 20-bit,
; so each long's top byte is ignored. The body wants P0/P1/P2 = source,
; P3/P4/P5 = destination, A/X/Y = length.
x16_gfx4h_copy:
        sta     X16_P0                  ; src lo
        stx     X16_P1                  ; src mid
        lda     __rc2
        sta     X16_P2                  ; src hi (bits 16-19)
        lda     __rc4
        sta     X16_P3                  ; dst lo
        lda     __rc5
        sta     X16_P4                  ; dst mid
        lda     __rc6
        sta     X16_P5                  ; dst hi
        lda     __rc8                   ; A/X/Y = len lo/mid/hi
        ldx     __rc9
        ldy     __rc10
        jmp     gfx4h_copy

; void x16_gfx4h_copy_wait(void)
x16_gfx4h_copy_wait:
        jmp     gfx4h_copy_wait

; =====================================================================
; the x16_library bitmap4h module, verbatim
; =====================================================================
; =====================================================================
; x16lib :: gfx/bitmap4h.asm -- VERA_2 640x480x16 SDRAM bitmap drawing
; =====================================================================
; This file EMITS CODE. Source it exactly once (x16_code.asm does).
;
; Requires the MiSTer VERA_2 bitmap layer. The framebuffer is NOT VERA
; VRAM: it is the VERA_2 20-bit SDRAM byte address space behind $9F60-
; $9F6F. Feature-detect with gfx4h_has before relying on it.
;
; The framebuffer is 4bpp, two pixels per byte, rows of 320 bytes:
;   offset = y*320 + (x>>1), size = 153,600 bytes ($25800).
; High nibble is the left/even pixel, low nibble is the right/odd pixel.
;
; Calling convention follows the high-res engines:
;   X16_P0/P1 = x, X16_P2/P3 = y, colour in A.
; =====================================================================

; (zone: file scope in ca65)

GFX4H_WIDTH       = 640
GFX4H_HEIGHT      = 480
GFX4H_STRIDE      = 320
GFX4H_FRAME_PAGES = 600        ; 153600 / 256

; ---------------------------------------------------------------------
; gfx4h_has -- feature-detect the VERA_2 bitmap layer
;   out: carry set if present, carry clear otherwise
; ---------------------------------------------------------------------
gfx4h_has:
    lda VERA2_ID
    cmp #VERA2_ID_MAGIC
    beq .Lgfx4h_has_yes
    clc
    rts
.Lgfx4h_has_yes:
    sec
    rts

; ---------------------------------------------------------------------
; gfx4h_init -- select 640x480@4bpp and load a 16-colour gray palette
; gfx4h_off  -- disable the VERA_2 bitmap layer
; ---------------------------------------------------------------------
gfx4h_init:
    jsr gfx4h_pal_gray
    lda #(VERA2_CTRL_ENABLE | VERA2_CTRL_MODE_4BPP)
    sta VERA2_CTRL
    rts

gfx4h_off:
    stz VERA2_CTRL
    rts

gfx4h_passthru_on:
    lda VERA2_CTRL
    ora #VERA2_CTRL_PASSTHRU
    sta VERA2_CTRL
    rts

gfx4h_passthru_off:
    lda #$FF - VERA2_CTRL_PASSTHRU
    and VERA2_CTRL
    sta VERA2_CTRL
    rts

; ---------------------------------------------------------------------
; gfx4h_pal_set -- set one VERA_2 palette entry
;   in: X = index, A = low byte (G<<4 | B), Y = high byte (R)
; gfx4h_pal_load -- load entries from RAM
;   in: X16_PTR0 = source, A = first index, X = count (0 loads nothing)
; ---------------------------------------------------------------------
gfx4h_pal_set:
    sta g4h_t
    sty g4h_t2
    stx VERA2_PAL_IDX
    lda g4h_t
    sta VERA2_PAL_LO
    lda g4h_t2
    sta VERA2_PAL_HI
    rts

gfx4h_pal_load:
    cpx #0
    beq .Lgfx4h_pal_load_done
    sta VERA2_PAL_IDX
    stx g4h_n
    ldy #0
.Lgfx4h_pal_load_loop:
    lda (X16_PTR0),y
    sta VERA2_PAL_LO
    iny
    lda (X16_PTR0),y
    sta VERA2_PAL_HI
    iny
    dec g4h_n
    bne .Lgfx4h_pal_load_loop
.Lgfx4h_pal_load_done:
    rts

gfx4h_pal_gray:
    stz VERA2_PAL_IDX
    ldx #0
.Lgfx4h_pal_gray_loop:
    txa
    asl
    asl
    asl
    asl
    stx g4h_t
    ora g4h_t
    sta VERA2_PAL_LO
    stx VERA2_PAL_HI
    inx
    cpx #16
    bne .Lgfx4h_pal_gray_loop
    rts

; ---------------------------------------------------------------------
; gfx4h_setptr -- point VERA_2 DATA at byte holding pixel (x,y)
;   in: A = VERA2_INC_* stride index, X16_P0/P1 = x, X16_P2/P3 = y
; ---------------------------------------------------------------------
gfx4h_setptr:
    asl
    asl
    asl
    asl
    sta g4h_inc
    jsr bitmap4h_addr_calc
    lda g4h_a0
    sta VERA2_ADDR_L
    lda g4h_a1
    sta VERA2_ADDR_M
    lda g4h_a2
    and #$0F
    ora g4h_inc
    sta VERA2_ADDR_H
    rts

; ---------------------------------------------------------------------
; gfx4h_clear -- fill the whole framebuffer with one colour
;   in: A = colour (0-15)
; ---------------------------------------------------------------------
gfx4h_clear:
    and #$0F
    tax
    lda bitmap4h_colbyte,x
    sta g4h_c
    stz VERA2_ADDR_L
    stz VERA2_ADDR_M
    stz VERA2_ADDR_H            ; ptr 0, stride +1
    lda #<GFX4H_FRAME_PAGES
    sta g4h_n
    lda #>GFX4H_FRAME_PAGES
    sta g4h_n+1
    lda g4h_c
    jmp bitmap4h_fill_pages

; ---------------------------------------------------------------------
; gfx4h_pset / gfx4h_read -- clipped pixel access
;   pset in: A = colour, X16_P0/P1 = x, X16_P2/P3 = y
;   read out: carry clear, A = colour; carry set if off screen
; ---------------------------------------------------------------------
gfx4h_pset:
    and #$0F
    sta g4h_c
    jsr bitmap4h_onscreen
    bcs .Lgfx4h_pset_off
    lda #VERA2_INC_0            ; hold: read and write the same byte
    jsr gfx4h_setptr
    lda VERA2_DATA
    sta g4h_t
    lda X16_P0
    and #1
    bne .Lgfx4h_pset_odd
    lda g4h_c
    asl
    asl
    asl
    asl
    sta g4h_t2
    lda g4h_t
    and #$0F
    ora g4h_t2
    sta VERA2_DATA
    rts
.Lgfx4h_pset_odd:
    lda g4h_t
    and #$F0
    ora g4h_c
    sta VERA2_DATA
.Lgfx4h_pset_off:
    rts

gfx4h_read:
    jsr bitmap4h_onscreen
    bcs .Lgfx4h_read_off
    lda #VERA2_INC_0
    jsr gfx4h_setptr
    lda VERA2_DATA
    sta g4h_t
    lda X16_P0
    and #1
    beq .Lgfx4h_read_even
    lda g4h_t
    and #$0F
    clc
    rts
.Lgfx4h_read_even:
    lda g4h_t
    and #$F0
    lsr
    lsr
    lsr
    lsr
    clc
    rts
.Lgfx4h_read_off:
    rts

; ---------------------------------------------------------------------
; gfx4h_hline / gfx4h_vline -- spans, no clipping
;   in: A = colour, X16_P0/P1 = x, X16_P2/P3 = y, X16_P4/P5 = length
; ---------------------------------------------------------------------
; hline: RMW the odd leading/trailing nibbles, STREAM the interior as
; whole two-pixel bytes through DATA at stride +1 -- one sta per two
; pixels instead of a full pset (address calc + RMW) per pixel.
gfx4h_hline:
    and #$0F
    sta g4h_c
    tax
    lda bitmap4h_colbyte,x
    sta g4h_t2                  ; the both-nibbles fill byte
    lda X16_P4
    sta g4h_n
    lda X16_P5
    sta g4h_n+1
    ora g4h_n
    bne 1f
    rts
1:	lda X16_P0
    and #1
    beq .Lgfx4h_hline_aligned
    lda #VERA2_INC_0            ; leading odd pixel: RMW the low nibble
    jsr gfx4h_setptr
    lda VERA2_DATA
    and #$F0
    ora g4h_c
    sta VERA2_DATA
    inc X16_P0
    bne 1f
    inc X16_P1
1:	lda g4h_n
    bne 1f
    dec g4h_n+1
1:	dec g4h_n
    lda g4h_n
    ora g4h_n+1
    bne .Lgfx4h_hline_aligned
    rts
.Lgfx4h_hline_aligned:
    lsr g4h_n+1                 ; n -> full bytes, carry = trailing pixel
    ror g4h_n
    bcc 1f
    lda #1
    sta g4h_phase               ; remember the trailing odd-width pixel
    bra 2f
1:	stz g4h_phase
2:	lda g4h_n
    ora g4h_n+1
    beq .Lgfx4h_hline_nofull
    lda #VERA2_INC_1
    jsr gfx4h_setptr
    lda g4h_t2
    jsr bitmap4h_fill_count
    lda g4h_phase
    beq .Lgfx4h_hline_done
    lda VERA2_ADDR_H            ; the +1 stride left the pointer ON the
    and #$0F                    ; trailing byte: just switch it to hold
    ora #(VERA2_INC_0 << 4)
    sta VERA2_ADDR_H
    bra .Lgfx4h_hline_rmwhi
.Lgfx4h_hline_nofull:
    lda g4h_phase
    beq .Lgfx4h_hline_done
    lda #VERA2_INC_0
    jsr gfx4h_setptr
.Lgfx4h_hline_rmwhi:
    lda VERA2_DATA              ; trailing even pixel: RMW the high nibble
    and #$0F
    sta g4h_t
    lda g4h_t2
    and #$F0
    ora g4h_t
    sta VERA2_DATA
.Lgfx4h_hline_done:
    rts

; vline: one address calc, then per row an RMW at hold stride and a
; 24-bit +320 on the cached address (three pointer stores) -- the same
; nibble mask the whole way down, no per-pixel pset.
gfx4h_vline:
    and #$0F
    sta g4h_c
    lda X16_P4
    sta g4h_n
    lda X16_P5
    sta g4h_n+1
    ora g4h_n
    beq .Lgfx4h_vline_done
    jsr bitmap4h_addr_calc              ; g4h_a0..a2 = the column's first byte
    lda X16_P0
    and #1
    bne .Lgfx4h_vline_odd
    lda #$0F                    ; even x: keep low nibble, or in col<<4
    sta g4h_t2
    lda g4h_c
    asl
    asl
    asl
    asl
    sta g4h_t
    bra .Lgfx4h_vline_row
.Lgfx4h_vline_odd:
    lda #$F0                    ; odd x: keep high nibble, or in col
    sta g4h_t2
    lda g4h_c
    sta g4h_t
.Lgfx4h_vline_row:
    lda g4h_a0
    sta VERA2_ADDR_L
    lda g4h_a1
    sta VERA2_ADDR_M
    lda g4h_a2
    and #$0F
    ora #(VERA2_INC_0 << 4)     ; hold: read and write the same byte
    sta VERA2_ADDR_H
    lda VERA2_DATA
    and g4h_t2
    ora g4h_t
    sta VERA2_DATA
    clc                         ; address += 320, one row down
    lda g4h_a0
    adc #$40
    sta g4h_a0
    lda g4h_a1
    adc #$01
    sta g4h_a1
    bcc 1f
    inc g4h_a2
1:	lda g4h_n
    bne 1f
    dec g4h_n+1
1:	dec g4h_n
    lda g4h_n
    ora g4h_n+1
    bne .Lgfx4h_vline_row
.Lgfx4h_vline_done:
    rts

; ---------------------------------------------------------------------
; gfx4h_rect / gfx4h_frame -- rectangles, no clipping
;   in: A = colour, X16_P0/P1 = x, X16_P2/P3 = y,
;       X16_P4/P5 = width, X16_P6/P7 = height
; ---------------------------------------------------------------------
gfx4h_rect:
    and #$0F
    sta g4h_rc
    lda X16_P0
    sta g4h_rx
    lda X16_P1
    sta g4h_rx+1
.Lgfx4h_rect_row:
    lda X16_P6
    ora X16_P7
    beq .Lgfx4h_rect_done
    lda g4h_rc
    jsr gfx4h_hline
    lda g4h_rx                  ; hline may nudge x for alignment: restore
    sta X16_P0
    lda g4h_rx+1
    sta X16_P1
    inc X16_P2
    bne 1f
    inc X16_P3
1:	lda X16_P6
    bne 1f
    dec X16_P7
1:	dec X16_P6
    bra .Lgfx4h_rect_row
.Lgfx4h_rect_done:
    rts

gfx4h_frame:
    and #$0F
    sta g4h_rc
    ldx #7
.Lgfx4h_frame_take:
    lda X16_P0,x
    sta g4h_fx,x
    dex
    bpl .Lgfx4h_frame_take

    jsr bitmap4h_frame_span
    lda g4h_rc
    jsr gfx4h_hline

    jsr bitmap4h_frame_span
    clc
    lda g4h_fy
    adc g4h_rh
    sta X16_P2
    lda g4h_fy+1
    adc g4h_rh+1
    sta X16_P3
    lda X16_P2
    bne 1f
    dec X16_P3
1:	dec X16_P2
    lda g4h_rc
    jsr gfx4h_hline

    jsr bitmap4h_frame_col
    lda g4h_rc
    jsr gfx4h_vline

    jsr bitmap4h_frame_col
    clc
    lda g4h_fx
    adc g4h_rw
    sta X16_P0
    lda g4h_fx+1
    adc g4h_rw+1
    sta X16_P1
    lda X16_P0
    bne 1f
    dec X16_P1
1:	dec X16_P0
    lda g4h_rc
    jmp gfx4h_vline

bitmap4h_frame_span:
    ldx #5
.Lbitmap4h_frame_span_s:
    lda g4h_fx,x
    sta X16_P0,x
    dex
    bpl .Lbitmap4h_frame_span_s
    rts

bitmap4h_frame_col:
    ldx #3
.Lbitmap4h_frame_col_c:
    lda g4h_fx,x
    sta X16_P0,x
    dex
    bpl .Lbitmap4h_frame_col_c
    lda g4h_rh
    sta X16_P4
    lda g4h_rh+1
    sta X16_P5
    rts

; ---------------------------------------------------------------------
; gfx4h_line -- Bresenham line, clipped by gfx4h_pset
;   in: A = colour, P0/P1=x0, P2/P3=y0, P4/P5=x1, P6/P7=y1
; ---------------------------------------------------------------------
gfx4h_line:
    and #$0F
    sta g4h_lc
    ldx #7
.Lgfx4h_line_take:
    lda X16_P0,x
    sta g4h_lx0,x
    dex
    bpl .Lgfx4h_line_take

    sec
    lda g4h_lx1
    sbc g4h_lx0
    sta g4h_ldx
    lda g4h_lx1+1
    sbc g4h_lx0+1
    sta g4h_ldx+1
    bpl .Lgfx4h_line_dx_pos
    sec
    lda #0
    sbc g4h_ldx
    sta g4h_ldx
    lda #0
    sbc g4h_ldx+1
    sta g4h_ldx+1
    lda #$FF
    sta g4h_lsx
    sta g4h_lsx+1
    bra .Lgfx4h_line_dx_done
.Lgfx4h_line_dx_pos:
    lda #1
    sta g4h_lsx
    stz g4h_lsx+1
.Lgfx4h_line_dx_done:

    sec
    lda g4h_ly1
    sbc g4h_ly0
    sta g4h_ldy
    lda g4h_ly1+1
    sbc g4h_ly0+1
    sta g4h_ldy+1
    bpl .Lgfx4h_line_dy_pos
    sec
    lda #0
    sbc g4h_ldy
    sta g4h_ldy
    lda #0
    sbc g4h_ldy+1
    sta g4h_ldy+1
    lda #$FF
    sta g4h_lsy
    sta g4h_lsy+1
    bra .Lgfx4h_line_dy_done
.Lgfx4h_line_dy_pos:
    lda #1
    sta g4h_lsy
    stz g4h_lsy+1
.Lgfx4h_line_dy_done:
    sec
    lda #0
    sbc g4h_ldy
    sta g4h_ldy
    lda #0
    sbc g4h_ldy+1
    sta g4h_ldy+1

    clc
    lda g4h_ldx
    adc g4h_ldy
    sta g4h_lerr
    lda g4h_ldx+1
    adc g4h_ldy+1
    sta g4h_lerr+1

.Lgfx4h_line_loop:
    lda g4h_lc
    jsr bitmap4h_plot
    lda g4h_lx0
    cmp g4h_lx1
    bne .Lgfx4h_line_step
    lda g4h_lx0+1
    cmp g4h_lx1+1
    bne .Lgfx4h_line_step
    lda g4h_ly0
    cmp g4h_ly1
    bne .Lgfx4h_line_step
    lda g4h_ly0+1
    cmp g4h_ly1+1
    bne .Lgfx4h_line_step
    rts

.Lgfx4h_line_step:
    lda g4h_lerr
    asl
    sta g4h_le2
    lda g4h_lerr+1
    rol
    sta g4h_le2+1

    sec
    lda g4h_le2
    sbc g4h_ldy
    lda g4h_le2+1
    sbc g4h_ldy+1
    bvc .Lgfx4h_line_nv1
    eor #$80
.Lgfx4h_line_nv1:
    bmi .Lgfx4h_line_skip_x
    clc
    lda g4h_lerr
    adc g4h_ldy
    sta g4h_lerr
    lda g4h_lerr+1
    adc g4h_ldy+1
    sta g4h_lerr+1
    clc
    lda g4h_lx0
    adc g4h_lsx
    sta g4h_lx0
    lda g4h_lx0+1
    adc g4h_lsx+1
    sta g4h_lx0+1
.Lgfx4h_line_skip_x:
    sec
    lda g4h_ldx
    sbc g4h_le2
    lda g4h_ldx+1
    sbc g4h_le2+1
    bvc .Lgfx4h_line_nv2
    eor #$80
.Lgfx4h_line_nv2:
    bmi .Lgfx4h_line_skip_y
    clc
    lda g4h_lerr
    adc g4h_ldx
    sta g4h_lerr
    lda g4h_lerr+1
    adc g4h_ldx+1
    sta g4h_lerr+1
    clc
    lda g4h_ly0
    adc g4h_lsy
    sta g4h_ly0
    lda g4h_ly0+1
    adc g4h_lsy+1
    sta g4h_ly0+1
.Lgfx4h_line_skip_y:
    jmp .Lgfx4h_line_loop

bitmap4h_plot:
    sta g4h_c
    lda g4h_lx0
    sta X16_P0
    lda g4h_lx0+1
    sta X16_P1
    lda g4h_ly0
    sta X16_P2
    lda g4h_ly0+1
    sta X16_P3
    lda g4h_c
    jmp gfx4h_pset

; ---------------------------------------------------------------------
; gfx4h_pattern_set / gfx4h_pattern_rect
; ---------------------------------------------------------------------
gfx4h_pattern_set:
    sta X16_T0
    stx X16_T0+1
    ldy #7
.Lgfx4h_pattern_set_copy:
    lda (X16_T0),y
    sta gp4h_pat,y
    dey
    bpl .Lgfx4h_pattern_set_copy
    lda X16_P4
    and #$0F
    sta gp4h_bg
    lda X16_P5
    and #$0F
    sta gp4h_fg
    rts

gfx4h_pattern_rect:
    lda X16_P4
    ora X16_P5
    ora X16_P6
    ora X16_P7
    bne 1f
    jmp .Lgfx4h_pattern_rect_done
1:
    lda X16_P2
    sta gp4h_by
    lda X16_P3
    sta gp4h_by+1
    lda X16_P0
    sta gp4h_bx
    lda X16_P1
    sta gp4h_bx+1
.Lgfx4h_pattern_rect_row:
    lda X16_P6
    ora X16_P7
    bne 1f
    jmp .Lgfx4h_pattern_rect_done
1:
    lda gp4h_bx
    sta gp4h_x
    lda gp4h_bx+1
    sta gp4h_x+1
    lda X16_P4
    sta gp4h_n
    lda X16_P5
    sta gp4h_n+1
    lda X16_P2
    and #7
    tay
    lda gp4h_pat,y
    sta gp4h_bits
.Lgfx4h_pattern_rect_col:
    lda gp4h_n
    ora gp4h_n+1
    beq .Lgfx4h_pattern_rect_next_row
    lda gp4h_bits
    bmi .Lgfx4h_pattern_rect_fg
    lda gp4h_bg
    bra .Lgfx4h_pattern_rect_plot
.Lgfx4h_pattern_rect_fg:
    lda gp4h_fg
.Lgfx4h_pattern_rect_plot:
    sta gp4h_c
    lda gp4h_x
    sta X16_P0
    lda gp4h_x+1
    sta X16_P1
    lda gp4h_by
    sta X16_P2
    lda gp4h_by+1
    sta X16_P3
    lda gp4h_c
    jsr gfx4h_pset
    lda gp4h_bits
    asl
    adc #0
    sta gp4h_bits
    inc gp4h_x
    bne 1f
    inc gp4h_x+1
1:	lda gp4h_n
    bne 1f
    dec gp4h_n+1
1:	dec gp4h_n
    jmp .Lgfx4h_pattern_rect_col
.Lgfx4h_pattern_rect_next_row:
    inc gp4h_by
    bne 1f
    inc gp4h_by+1
1:	lda gp4h_by
    sta X16_P2
    lda gp4h_by+1
    sta X16_P3
    lda X16_P6
    bne 1f
    dec X16_P7
1:	dec X16_P6
    jmp .Lgfx4h_pattern_rect_row
.Lgfx4h_pattern_rect_done:
    rts

; ---------------------------------------------------------------------
; gfx4h_blit / gfx4h_blitm -- packed RAM pixels to framebuffer
;   blit in: A = op (0 copy, 1 OR, 2 AND, 3 XOR)
;   common: P0/P1=x, P2/P3=y, P4=width (1-255), P5=height, P6/P7=source
; ---------------------------------------------------------------------
gfx4h_blit:
    and #3
    sta g4h_op
    bra bitmap4h_blit_common

gfx4h_blitm:
    lda #$80
    sta g4h_op
bitmap4h_blit_common:
    lda X16_P6
    sta g4h_src
    lda X16_P7
    sta g4h_src+1
    lda X16_P4
    clc
    adc #1
    lsr
    sta g4h_rowbytes
.Lbitmap4h_blit_common_row:
    lda X16_P5
    bne 1f
    jmp .Lbitmap4h_blit_common_done
1:
    lda g4h_src
    sta X16_PTR3
    lda g4h_src+1
    sta X16_PTR3+1
    stz g4h_phase
    lda X16_P4
    sta g4h_w
.Lbitmap4h_blit_common_col:
    lda g4h_w
    beq .Lbitmap4h_blit_common_next_row
    ldy #0
    lda (X16_PTR3),y
    ldy g4h_phase
    bne .Lbitmap4h_blit_common_low
    and #$F0
    lsr
    lsr
    lsr
    lsr
    bra .Lbitmap4h_blit_common_got
.Lbitmap4h_blit_common_low:
    and #$0F
.Lbitmap4h_blit_common_got:
    sta g4h_ink
    lda g4h_op
    bmi .Lbitmap4h_blit_common_masked
    beq .Lbitmap4h_blit_common_copy
    jsr gfx4h_read
    sta g4h_t
    lda g4h_op
    cmp #1
    beq .Lbitmap4h_blit_common_or
    cmp #2
    beq .Lbitmap4h_blit_common_and
    lda g4h_ink
    eor g4h_t
    bra .Lbitmap4h_blit_common_store
.Lbitmap4h_blit_common_and:
    lda g4h_ink
    and g4h_t
    bra .Lbitmap4h_blit_common_store
.Lbitmap4h_blit_common_or:
    lda g4h_ink
    ora g4h_t
    bra .Lbitmap4h_blit_common_store
.Lbitmap4h_blit_common_masked:
    lda g4h_ink
    beq .Lbitmap4h_blit_common_advance
.Lbitmap4h_blit_common_copy:
    lda g4h_ink
.Lbitmap4h_blit_common_store:
    jsr gfx4h_pset
.Lbitmap4h_blit_common_advance:
    inc X16_P0
    bne 1f
    inc X16_P1
1:	lda g4h_phase
    eor #1
    sta g4h_phase
    bne 1f
    inc X16_PTR3
    bne 1f
    inc X16_PTR3+1
1:	dec g4h_w
    jmp .Lbitmap4h_blit_common_col
.Lbitmap4h_blit_common_next_row:
    sec
    lda X16_P0
    sbc X16_P4
    sta X16_P0
    bcs 1f
    dec X16_P1
1:	clc
    lda g4h_src
    adc g4h_rowbytes
    sta g4h_src
    lda g4h_src+1
    adc #0
    sta g4h_src+1
    inc X16_P2
    bne 1f
    inc X16_P3
1:	dec X16_P5
    jmp .Lbitmap4h_blit_common_row
.Lbitmap4h_blit_common_done:
    rts

; ---------------------------------------------------------------------
; gfx4h_copy -- VERA_2 SDRAM-to-SDRAM hardware copy, then wait
;   in: P0/P1/P2 = source, P3/P4/P5 = destination, A/X/Y = length
; ---------------------------------------------------------------------
gfx4h_copy:
    sta VERA2_BLIT_LEN_L
    stx VERA2_BLIT_LEN_M
    sty VERA2_BLIT_LEN_H
    lda X16_P0
    sta VERA2_ADDR_L
    lda X16_P1
    sta VERA2_ADDR_M
    lda X16_P2
    and #$0F
    sta VERA2_ADDR_H            ; source pointer, stride +1
    lda X16_P3
    sta VERA2_BLIT_DST_L
    lda X16_P4
    sta VERA2_BLIT_DST_M
    lda X16_P5
    and #$0F
    sta VERA2_BLIT_DST_H
    lda #1
    sta VERA2_BLIT_CTRL
gfx4h_copy_wait:
    lda VERA2_BLIT_CTRL
    and #1
    bne gfx4h_copy_wait
    rts

; ---------------------------------------------------------------------
; private helpers
; ---------------------------------------------------------------------
bitmap4h_onscreen:
    lda X16_P1
    cmp #>GFX4H_WIDTH
    bcc .Lbitmap4h_onscreen_xok
    bne .Lbitmap4h_onscreen_bad
    lda X16_P0
    cmp #<GFX4H_WIDTH
    bcs .Lbitmap4h_onscreen_bad
.Lbitmap4h_onscreen_xok:
    lda X16_P3
    cmp #>GFX4H_HEIGHT
    bcc .Lbitmap4h_onscreen_ok
    bne .Lbitmap4h_onscreen_bad
    lda X16_P2
    cmp #<GFX4H_HEIGHT
    bcs .Lbitmap4h_onscreen_bad
.Lbitmap4h_onscreen_ok:
    clc
    rts
.Lbitmap4h_onscreen_bad:
    sec
    rts

bitmap4h_addr_calc:
    lda X16_P2                  ; y*320 = y*256 + y*64, in ~25 cycles:
    ror                         ; lo = (y & 3) << 6
    ror                         ; md = y + (y >> 2)
    ror                         ; hi = carry out of the md add
    and #$C0
    sta g4h_a0
    lda X16_P2
    lsr
    lsr
    clc
    adc X16_P2
    sta g4h_a1
    lda #0
    rol
    sta g4h_a2
    lda X16_P3                  ; y >= 256: + 256*320 = $14000
    beq .Lbitmap4h_addr_calc_addx
    clc
    lda g4h_a1
    adc #$40
    sta g4h_a1
    bcc 1f
    inc g4h_a2
1:	inc g4h_a2
.Lbitmap4h_addr_calc_addx:
    lda X16_P1                  ; + x >> 1
    lsr
    sta X16_T1
    lda X16_P0
    ror
    clc
    adc g4h_a0
    sta g4h_a0
    lda g4h_a1
    adc X16_T1
    sta g4h_a1
    bcc 1f
    inc g4h_a2
1:	rts

bitmap4h_fill_count:
    ldy g4h_n+1                 ; high byte first, so beq tests the LOW
    ldx g4h_n                   ; byte (same shape as bitmap8h)
    beq .Lbitmap4h_fill_count_full
    iny
.Lbitmap4h_fill_count_full:
.Lbitmap4h_fill_count_loop:
    sta VERA2_DATA
    dex
    bne .Lbitmap4h_fill_count_loop
    dey
    bne .Lbitmap4h_fill_count_loop
    rts

bitmap4h_fill_pages:
.Lbitmap4h_fill_pages_outer:
    ldx #0
.Lbitmap4h_fill_pages_inner:
    sta VERA2_DATA
    dex
    bne .Lbitmap4h_fill_pages_inner
    lda g4h_n
    bne 1f
    dec g4h_n+1
1:	dec g4h_n
    lda g4h_n
    ora g4h_n+1
    beq .Lbitmap4h_fill_pages_done
    lda g4h_c
    bra .Lbitmap4h_fill_pages_outer
.Lbitmap4h_fill_pages_done:
    rts

; ---------------------------------------------------------------------
; data
; ---------------------------------------------------------------------
g4h_a0: .byte 0
g4h_a1: .byte 0
g4h_a2: .byte 0
g4h_inc:.byte 0
g4h_c:  .byte 0
g4h_t:  .byte 0
g4h_t2: .byte 0
g4h_n:  .word 0
g4h_w:  .byte 0
g4h_op: .byte 0
g4h_ink:.byte 0
g4h_src:.word 0
g4h_rowbytes:.byte 0
g4h_phase:.byte 0

g4h_rx: .word 0
g4h_fx: .word 0
g4h_fy: .word 0
g4h_rw: .word 0
g4h_rh: .word 0
g4h_rc: .byte 0

gp4h_pat: .zero  8, 0
gp4h_bg:  .byte 0
gp4h_fg:  .byte 0
gp4h_bits:.byte 0
gp4h_bx:  .word 0
gp4h_x:   .word 0
gp4h_by:  .word 0
gp4h_n:   .word 0
gp4h_c:   .byte 0

g4h_lc:  .byte 0
g4h_lx0: .word 0
g4h_ly0: .word 0
g4h_lx1: .word 0
g4h_ly1: .word 0
g4h_ldx: .word 0
g4h_ldy: .word 0
g4h_lerr:.word 0
g4h_le2: .word 0
g4h_lsx: .word 0
g4h_lsy: .word 0

bitmap4h_colbyte:
    .byte $00, $11, $22, $33, $44, $55, $66, $77
         .byte $88, $99, $AA, $BB, $CC, $DD, $EE, $FF


; (end zone)
