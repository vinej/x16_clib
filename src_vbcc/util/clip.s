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

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers: the four ints of clip_set ride in the pairs
; r0/r1, r2/r3, r4/r5, r6/r7. clip_line's pointer arrives in the a/x pair.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3
        zpage	r4
        zpage	r5
        zpage	r6
        zpage	r7

        global	_x16_clip_set
        global	_x16_clip_line

; outcode bits
CLIP_LEFT   = %0001
CLIP_RIGHT  = %0010
CLIP_TOP    = %0100
CLIP_BOTTOM = %1000

        section text

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void x16_clip_set(__reg("r0/r1") unsigned int xmin,
;                   __reg("r2/r3") unsigned int ymin,
;                   __reg("r4/r5") unsigned int xmax,
;                   __reg("r6/r7") unsigned int ymax)
;   The rectangle is inclusive. Four ints, one per register pair.
; ---------------------------------------------------------------------
_x16_clip_set:
        lda     r0
        sta     clip_xmin
        lda     r1
        sta     clip_xmin+1
        lda     r2
        sta     clip_ymin
        lda     r3
        sta     clip_ymin+1
        lda     r4
        sta     clip_xmax
        lda     r5
        sta     clip_xmax+1
        lda     r6
        sta     clip_ymax
        lda     r7
        sta     clip_ymax+1
        rts

; ---------------------------------------------------------------------
; unsigned char x16_clip_line(__reg("a/x") x16_line *seg)
;   1 if any of it is visible, and *seg now holds the visible part;
;   0 if the whole segment lies outside the rectangle.
;
; x16_line is { int x0, y0, x1, y1; } -- eight bytes, in the same order
; as clipl_x0..clipl_y1, so this is a straight block copy in and out. The
; pointer arrives in a/x; stage it in the library's own X16_PTR3 scratch.
; ---------------------------------------------------------------------
_x16_clip_line:
        sta     X16_PTR3
        stx     X16_PTR3+1
        ldy     #7
.in:
        lda     (X16_PTR3),y
        sta     clipl_x0,y
        dey
        bpl     .in

        jsr     clip_line               ; carry set = entirely outside
        bcc     .visible
        lda     #0
        ldx     #0                      ; high byte, for int-promoting callers
        rts

.visible:
        ldy     #7
.out:
        lda     clipl_x0,y
        sta     (X16_PTR3),y
        dey
        bpl     .out
        lda     #1
        ldx     #0
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
.loop:
        jsr     oc0
        sta     cp_c0
        jsr     oc1
        sta     cp_c1
        ora     cp_c0
        bne     .outside
        jmp     .accept                 ; both inside (out of branch range)
.outside:
        lda     cp_c0
        and     cp_c1
        beq     .clip_one
        sec                             ; share an outside half-plane: reject
        rts

.clip_one:
        ; pull the endpoint with a nonzero code into the work slot
        lda     cp_c0
        bne     .use0
        lda     #1
        sta     cp_which
        lda     cp_c1
        sta     cp_code
        ldx     #3
.cp1:
        lda     clipl_x1,x
        sta     cw_x,x
        lda     clipl_x0,x
        sta     co_x,x
        dex
        bpl     .cp1
        jmp     .intersect              ; out of branch range
.use0:
        stz     cp_which
        sta     cp_code
        ldx     #3
.cp0:
        lda     clipl_x0,x
        sta     cw_x,x
        lda     clipl_x1,x
        sta     co_x,x
        dex
        bpl     .cp0

.intersect:
        lda     cp_code
        and     #CLIP_BOTTOM
        beq     .not_bottom
        lda     clip_ymax
        sta     cp_b
        lda     clip_ymax+1
        sta     cp_b+1
        jsr     cross_y
        bra     .store
.not_bottom:
        lda     cp_code
        and     #CLIP_TOP
        beq     .not_top
        lda     clip_ymin
        sta     cp_b
        lda     clip_ymin+1
        sta     cp_b+1
        jsr     cross_y
        bra     .store
.not_top:
        lda     cp_code
        and     #CLIP_RIGHT
        beq     .not_right
        lda     clip_xmax
        sta     cp_b
        lda     clip_xmax+1
        sta     cp_b+1
        jsr     cross_x
        bra     .store
.not_right:
        lda     clip_xmin
        sta     cp_b
        lda     clip_xmin+1
        sta     cp_b+1
        jsr     cross_x

.store:
        ; write the moved endpoint back and go around again
        lda     cp_which
        bne     .st1
        ldx     #3
.sb0:
        lda     cw_x,x
        sta     clipl_x0,x
        dex
        bpl     .sb0
        jmp     .loop
.st1:
        ldx     #3
.sb1:
        lda     cw_x,x
        sta     clipl_x1,x
        dex
        bpl     .sb1
        jmp     .loop

.accept:
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
        bvc     .ocx1
        eor     #$80
.ocx1:
        bpl     .ocx2                   ; x >= xmin
        lda     #CLIP_LEFT
        tsb     cp_oc
        bra     .ocy                    ; can't also be right of xmax
.ocx2:
        ; xmax < x?
        lda     clip_xmax
        cmp     clipl_x0,x
        lda     clip_xmax+1
        sbc     clipl_x0+1,x
        bvc     .ocx3
        eor     #$80
.ocx3:
        bpl     .ocy
        lda     #CLIP_RIGHT
        tsb     cp_oc
.ocy:
        ; y < ymin?
        lda     clipl_y0,x
        cmp     clip_ymin
        lda     clipl_y0+1,x
        sbc     clip_ymin+1
        bvc     .ocy1
        eor     #$80
.ocy1:
        bpl     .ocy2
        lda     #CLIP_TOP
        tsb     cp_oc
        bra     .ocdone
.ocy2:
        ; ymax < y?
        lda     clip_ymax
        cmp     clipl_y0,x
        lda     clip_ymax+1
        sbc     clipl_y0+1,x
        bvc     .ocy3
        eor     #$80
.ocy3:
        bpl     .ocdone
        lda     #CLIP_BOTTOM
        tsb     cp_oc
.ocdone:
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
        bpl     .m1p
        inc     cp_sgn
        jsr     neg1
.m1p:
        lda     cp_m2+1
        bpl     .m2p
        inc     cp_sgn
        sec
        lda     #0
        sbc     cp_m2
        sta     cp_m2
        lda     #0
        sbc     cp_m2+1
        sta     cp_m2+1
.m2p:
        lda     cp_m3+1
        bpl     .m3p
        inc     cp_sgn
        sec
        lda     #0
        sbc     cp_m3
        sta     cp_m3
        lda     #0
        sbc     cp_m3+1
        sta     cp_m3+1
.m3p:
        ; 16x16 -> 32 shift-add multiply: prod = m1 * m2 (umul16's shape,
        ; with the adc carry rolling down through the rotate)
        stz     cp_prod+2
        stz     cp_prod+3
        ldx     #16
.mul:
        lsr     cp_m2+1
        ror     cp_m2
        bcc     .noadd
        lda     cp_prod+2
        clc
        adc     cp_m1
        sta     cp_prod+2
        lda     cp_prod+3
        adc     cp_m1+1
        bra     .rot
.noadd:
        lda     cp_prod+3               ; carry is already clear
.rot:
        ror     a
        sta     cp_prod+3
        ror     cp_prod+2
        ror     cp_prod+1
        ror     cp_prod
        dex
        bne     .mul

        ; 32 / 16 restoring divide: quotient into cp_prod
        stz     cp_rem
        stz     cp_rem+1
        ldx     #32
.div:
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
        bcc     .nofit
        sta     cp_rem+1
        sty     cp_rem
        inc     cp_prod
.nofit:
        dex
        bne     .div

        lda     cp_prod
        sta     cp_q
        lda     cp_prod+1
        sta     cp_q+1
        lda     cp_sgn                  ; odd number of negatives: negate
        lsr     a
        bcc     .posq
        sec
        lda     #0
        sbc     cp_q
        sta     cp_q
        lda     #0
        sbc     cp_q+1
        sta     cp_q+1
.posq:
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
        section data

clip_xmin: word 0
clip_ymin: word 0
clip_xmax: word 319
clip_ymax: word 239

; ---------------------------------------------------------------------
; The segment being clipped. x0,y0,x1,y1 must stay adjacent as four
; words: _x16_clip_line block-copies an x16_line straight onto them, and
; the outcode routine indexes clipl_x0 by 0 and 4. Do not reorder.
;
; cw_x/cw_y and co_x/co_y are likewise indexed as 4-byte pairs.
; ---------------------------------------------------------------------
        section bss

clipl_x0: reserve 2
clipl_y0: reserve 2
clipl_x1: reserve 2
clipl_y1: reserve 2

cw_x:     reserve 2                        ; the endpoint being moved
cw_y:     reserve 2
co_x:     reserve 2                        ; ...and the fixed one opposite
co_y:     reserve 2

cp_c0:    reserve 1
cp_c1:    reserve 1
cp_code:  reserve 1
cp_which: reserve 1
cp_oc:    reserve 1
cp_b:     reserve 2
cp_m1:    reserve 2
cp_m2:    reserve 2
cp_m3:    reserve 2
cp_q:     reserve 2
cp_sgn:   reserve 1
cp_prod:  reserve 4
cp_rem:   reserve 2
