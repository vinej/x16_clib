; =====================================================================
; x16clib :: util/clip.s -- Cohen-Sutherland line clipping
; =====================================================================
; gfx_line and fx_line are documented as non-clipping. This removes that
; sharp edge: give clip_line a segment in 16-bit SIGNED coordinates
; (anywhere within +/-4095) and it either rejects it or hands back the
; visible part.
;
; The rectangle is inclusive and defaults to the full 320x240 bitmap.
;
; The module is deliberately standalone: it never calls a drawing
; routine, so linking it does not drag gfx/bitmap.o in behind it.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X, then whatever __rc bytes the pointers
;   left free. So f(ptr, int, char) is ptr in __rc2/3, int in A/X, char in
;   __rc4.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_clip_set
        .globl  x16_clip_line

; outcode bits
CLIP_LEFT   = %0001
CLIP_RIGHT  = %0010
CLIP_TOP    = %0100
CLIP_BOTTOM = %1000

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_clip_set(unsigned int xmin, unsigned int ymin,
;                                unsigned int xmax, unsigned int ymax)
;   The rectangle is inclusive.
; ---------------------------------------------------------------------
; Four 16-bit arguments, all integers, so their eight bytes fill A, X and
; __rc2 through __rc7 in declaration order.
x16_clip_set:
        sta     clip_xmin               ; xmin lo
        stx     clip_xmin+1             ; xmin hi
        lda     __rc2
        sta     clip_ymin
        lda     __rc3
        sta     clip_ymin+1
        lda     __rc4
        sta     clip_xmax
        lda     __rc5
        sta     clip_xmax+1
        lda     __rc6
        sta     clip_ymax
        lda     __rc7
        sta     clip_ymax+1
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_clip_line(x16_line *seg)
;   1 if any of it is visible, and *seg now holds the visible part;
;   0 if the whole segment lies outside the rectangle.
;
; x16_line is { int x0, y0, x1, y1; } -- eight bytes, in the same order
; as clipl_x0..clipl_y1, so this is a straight block copy in and out.
; Keep the struct and those .res directives in step.
; ---------------------------------------------------------------------
; `seg` is a pointer, so it arrives in __rc2/__rc3, not A/X. Indirecting
; needs a zero-page pair of our own: clip.s touches no T bytes at all, so
; T0/T1 is free.
x16_clip_line:
        lda     __rc2
        sta     X16_T0
        lda     __rc3
        sta     X16_T1

        ldy     #7
.Lx16_clip_line_in:
        lda     (X16_T0),y
        sta     clipl_x0,y
        dey
        bpl     .Lx16_clip_line_in

        jsr     clip_line               ; carry set = entirely outside
        bcc     .Lx16_clip_line_visible
        lda     #0
        rts

.Lx16_clip_line_visible:
        ldy     #7
.Lx16_clip_line_out:
        lda     clipl_x0,y
        sta     (X16_T0),y
        dey
        bpl     .Lx16_clip_line_out
        lda     #1
        rts

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; clip_line -- clip clipl_* against the rectangle
;   out: carry set   = entirely outside, draw nothing
;        carry clear = clipl_* now hold the visible sub-segment, and
;                      X16_P0..P5 are loaded for gfx_line / fx_line
; ---------------------------------------------------------------------
clip_line:
.Lclip_line_loop:
        jsr     oc0
        sta     cp_c0
        jsr     oc1
        sta     cp_c1
        ora     cp_c0
        bne     .Lclip_line_outside
        jmp     .Lclip_line_accept                 ; both inside (out of branch range)
.Lclip_line_outside:
        lda     cp_c0
        and     cp_c1
        beq     .Lclip_line_clip_one
        sec                             ; share an outside half-plane: reject
        rts

.Lclip_line_clip_one:
        ; pull the endpoint with a nonzero code into the work slot
        lda     cp_c0
        bne     .Lclip_line_use0
        lda     #1
        sta     cp_which
        lda     cp_c1
        sta     cp_code
        ldx     #3
.Lclip_line_cp1:
        lda     clipl_x1,x
        sta     cw_x,x
        lda     clipl_x0,x
        sta     co_x,x
        dex
        bpl     .Lclip_line_cp1
        jmp     .Lclip_line_intersect              ; out of branch range
.Lclip_line_use0:
        stz     cp_which
        sta     cp_code
        ldx     #3
.Lclip_line_cp0:
        lda     clipl_x0,x
        sta     cw_x,x
        lda     clipl_x1,x
        sta     co_x,x
        dex
        bpl     .Lclip_line_cp0

.Lclip_line_intersect:
        lda     cp_code
        and     #CLIP_BOTTOM
        beq     .Lclip_line_not_bottom
        lda     clip_ymax
        sta     cp_b
        lda     clip_ymax+1
        sta     cp_b+1
        jsr     cross_y
        bra     .Lclip_line_store
.Lclip_line_not_bottom:
        lda     cp_code
        and     #CLIP_TOP
        beq     .Lclip_line_not_top
        lda     clip_ymin
        sta     cp_b
        lda     clip_ymin+1
        sta     cp_b+1
        jsr     cross_y
        bra     .Lclip_line_store
.Lclip_line_not_top:
        lda     cp_code
        and     #CLIP_RIGHT
        beq     .Lclip_line_not_right
        lda     clip_xmax
        sta     cp_b
        lda     clip_xmax+1
        sta     cp_b+1
        jsr     cross_x
        bra     .Lclip_line_store
.Lclip_line_not_right:
        lda     clip_xmin
        sta     cp_b
        lda     clip_xmin+1
        sta     cp_b+1
        jsr     cross_x

.Lclip_line_store:
        ; write the moved endpoint back and go around again
        lda     cp_which
        bne     .Lclip_line_st1
        ldx     #3
.Lclip_line_sb0:
        lda     cw_x,x
        sta     clipl_x0,x
        dex
        bpl     .Lclip_line_sb0
        jmp     .Lclip_line_loop
.Lclip_line_st1:
        ldx     #3
.Lclip_line_sb1:
        lda     cw_x,x
        sta     clipl_x1,x
        dex
        bpl     .Lclip_line_sb1
        jmp     .Lclip_line_loop

.Lclip_line_accept:
        lda     clipl_x0                ; load the drawers' parameter block
        sta     X16_P0
        lda     clipl_x0+1
        sta     X16_P1
        lda     clipl_y0
        sta     X16_P2
        lda     clipl_x1
        sta     X16_P3
        lda     clipl_x1+1
        sta     X16_P4
        lda     clipl_y1
        sta     X16_P5
        clc
        rts

; --- outcodes ---------------------------------------------------------
; A = outcode of (clipl_x0, clipl_y0) / (clipl_x1, clipl_y1)
oc0:
        ldx     #0                      ; offset of endpoint 0's fields
        bra     outcode
oc1:
        ldx     #4
outcode:
        stz     cp_oc
        ; x < xmin?
        lda     clipl_x0,x
        cmp     clip_xmin
        lda     clipl_x0+1,x
        sbc     clip_xmin+1
        bvc     .Loutcode_ocx1
        eor     #$80
.Loutcode_ocx1:
        bpl     .Loutcode_ocx2                   ; x >= xmin
        lda     #CLIP_LEFT
        tsb     cp_oc
        bra     .Loutcode_ocy                    ; can't also be right of xmax
.Loutcode_ocx2:
        ; xmax < x?
        lda     clip_xmax
        cmp     clipl_x0,x
        lda     clip_xmax+1
        sbc     clipl_x0+1,x
        bvc     .Loutcode_ocx3
        eor     #$80
.Loutcode_ocx3:
        bpl     .Loutcode_ocy
        lda     #CLIP_RIGHT
        tsb     cp_oc
.Loutcode_ocy:
        ; y < ymin?
        lda     clipl_y0,x
        cmp     clip_ymin
        lda     clipl_y0+1,x
        sbc     clip_ymin+1
        bvc     .Loutcode_ocy1
        eor     #$80
.Loutcode_ocy1:
        bpl     .Loutcode_ocy2
        lda     #CLIP_TOP
        tsb     cp_oc
        bra     .Loutcode_ocdone
.Loutcode_ocy2:
        ; ymax < y?
        lda     clip_ymax
        cmp     clipl_y0,x
        lda     clip_ymax+1
        sbc     clipl_y0+1,x
        bvc     .Loutcode_ocy3
        eor     #$80
.Loutcode_ocy3:
        bpl     .Loutcode_ocdone
        lda     #CLIP_BOTTOM
        tsb     cp_oc
.Loutcode_ocdone:
        lda     cp_oc
        rts

; --- intersections ----------------------------------------------------
; Move the work endpoint onto the horizontal boundary cp_b:
;   cw_x += (co_x - cw_x) * (cp_b - cw_y) / (co_y - cw_y);  cw_y = cp_b
cross_y:
        sec                             ; numerator 1: dx = co_x - cw_x
        lda     co_x
        sbc     cw_x
        sta     cp_m1
        lda     co_x+1
        sbc     cw_x+1
        sta     cp_m1+1
        sec                             ; numerator 2: cp_b - cw_y
        lda     cp_b
        sbc     cw_y
        sta     cp_m2
        lda     cp_b+1
        sbc     cw_y+1
        sta     cp_m2+1
        sec                             ; denominator: dy = co_y - cw_y
        lda     co_y
        sbc     cw_y
        sta     cp_m3
        lda     co_y+1
        sbc     cw_y+1
        sta     cp_m3+1
        jsr     muldiv                  ; cp_q = m1 * m2 / m3, signed
        clc
        lda     cw_x
        adc     cp_q
        sta     cw_x
        lda     cw_x+1
        adc     cp_q+1
        sta     cw_x+1
        lda     cp_b
        sta     cw_y
        lda     cp_b+1
        sta     cw_y+1
        rts

; Move the work endpoint onto the vertical boundary cp_b:
;   cw_y += (co_y - cw_y) * (cp_b - cw_x) / (co_x - cw_x);  cw_x = cp_b
cross_x:
        sec
        lda     co_y
        sbc     cw_y
        sta     cp_m1
        lda     co_y+1
        sbc     cw_y+1
        sta     cp_m1+1
        sec
        lda     cp_b
        sbc     cw_x
        sta     cp_m2
        lda     cp_b+1
        sbc     cw_x+1
        sta     cp_m2+1
        sec
        lda     co_x
        sbc     cw_x
        sta     cp_m3
        lda     co_x+1
        sbc     cw_x+1
        sta     cp_m3+1
        jsr     muldiv
        clc
        lda     cw_y
        adc     cp_q
        sta     cw_y
        lda     cw_y+1
        adc     cp_q+1
        sta     cw_y+1
        lda     cp_b
        sta     cw_x
        lda     cp_b+1
        sta     cw_x+1
        rts

; cp_q = (cp_m1 * cp_m2) / cp_m3, all signed 16-bit. With inputs within
; +/-4095 the product fits 24 bits and the quotient 16.
muldiv:
        stz     cp_sgn
        lda     cp_m1+1                 ; strip the three signs
        bpl     .Lmuldiv_m1p
        inc     cp_sgn
        jsr     neg1
.Lmuldiv_m1p:
        lda     cp_m2+1
        bpl     .Lmuldiv_m2p
        inc     cp_sgn
        sec
        lda     #0
        sbc     cp_m2
        sta     cp_m2
        lda     #0
        sbc     cp_m2+1
        sta     cp_m2+1
.Lmuldiv_m2p:
        lda     cp_m3+1
        bpl     .Lmuldiv_m3p
        inc     cp_sgn
        sec
        lda     #0
        sbc     cp_m3
        sta     cp_m3
        lda     #0
        sbc     cp_m3+1
        sta     cp_m3+1
.Lmuldiv_m3p:
        ; 16x16 -> 32 shift-add multiply: prod = m1 * m2 (umul16's shape,
        ; with the adc carry rolling down through the rotate)
        stz     cp_prod+2
        stz     cp_prod+3
        ldx     #16
.Lmuldiv_mul:
        lsr     cp_m2+1
        ror     cp_m2
        bcc     .Lmuldiv_noadd
        lda     cp_prod+2
        clc
        adc     cp_m1
        sta     cp_prod+2
        lda     cp_prod+3
        adc     cp_m1+1
        bra     .Lmuldiv_rot
.Lmuldiv_noadd:
        lda     cp_prod+3               ; carry is already clear
.Lmuldiv_rot:
        ror     a
        sta     cp_prod+3
        ror     cp_prod+2
        ror     cp_prod+1
        ror     cp_prod
        dex
        bne     .Lmuldiv_mul

        ; 32 / 16 restoring divide: quotient into cp_prod
        stz     cp_rem
        stz     cp_rem+1
        ldx     #32
.Lmuldiv_div:
        asl     cp_prod
        rol     cp_prod+1
        rol     cp_prod+2
        rol     cp_prod+3
        rol     cp_rem
        rol     cp_rem+1
        sec
        lda     cp_rem
        sbc     cp_m3
        tay
        lda     cp_rem+1
        sbc     cp_m3+1
        bcc     .Lmuldiv_nofit
        sta     cp_rem+1
        sty     cp_rem
        inc     cp_prod
.Lmuldiv_nofit:
        dex
        bne     .Lmuldiv_div

        lda     cp_prod
        sta     cp_q
        lda     cp_prod+1
        sta     cp_q+1
        lda     cp_sgn                  ; odd number of negatives: negate
        lsr     a
        bcc     .Lmuldiv_posq
        sec
        lda     #0
        sbc     cp_q
        sta     cp_q
        lda     #0
        sbc     cp_q+1
        sta     cp_q+1
.Lmuldiv_posq:
        rts

neg1:
        sec
        lda     #0
        sbc     cp_m1
        sta     cp_m1
        lda     #0
        sbc     cp_m1+1
        sta     cp_m1+1
        rts

; ---------------------------------------------------------------------
; The clip rectangle has nonzero defaults, so it lives in DATA (loaded
; from the PRG) rather than BSS (zeroed by crt0).
; ---------------------------------------------------------------------
        .section .data,"aw",@progbits

clip_xmin: .word 0
clip_ymin: .word 0
clip_xmax: .word 319
clip_ymax: .word 239

; ---------------------------------------------------------------------
; The segment being clipped. x0,y0,x1,y1 must stay adjacent as four
; words: _x16_clip_line block-copies an x16_line straight onto them, and
; the outcode routine indexes clipl_x0 by 0 and 4. Do not reorder.
;
; cw_x/cw_y and co_x/co_y are likewise indexed as 4-byte pairs.
; ---------------------------------------------------------------------
        .section .bss,"aw",@nobits

clipl_x0: .zero  2
clipl_y0: .zero  2
clipl_x1: .zero  2
clipl_y1: .zero  2

cw_x:     .zero  2                        ; the endpoint being moved
cw_y:     .zero  2
co_x:     .zero  2                        ; ...and the fixed one opposite
co_y:     .zero  2

cp_c0:    .zero  1
cp_c1:    .zero  1
cp_code:  .zero  1
cp_which: .zero  1
cp_oc:    .zero  1
cp_b:     .zero  2
cp_m1:    .zero  2
cp_m2:    .zero  2
cp_m3:    .zero  2
cp_q:     .zero  2
cp_sgn:   .zero  1
cp_prod:  .zero  4
cp_rem:   .zero  2
