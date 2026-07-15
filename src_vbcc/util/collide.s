; =====================================================================
; x16clib :: util/collide.s -- axis-aligned bounding-box overlap
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

; (import: popa, popax)
        zpage	ptr1
        zpage	ptr2

; vbcc argument registers, plus the C soft-stack pointer for collide8's
; five-plus-th arguments. Chars are placed one per EVEN register
; (r0,r2,r4,r6), then spill to the stack at (sp),0..3.
        zpage	r0
        zpage	r2
        zpage	r4
        zpage	r6
        zpage	r1
        zpage	r3
        zpage	sp

        global	_x16_collide8
        global	_x16_collide16

        section text

; ---------------------------------------------------------------------
; unsigned char x16_collide8(
;       unsigned char ax, unsigned char ay, unsigned char aw, unsigned char ah,
;       unsigned char bx, unsigned char by, unsigned char bw, unsigned char bh)
;
; Eight char arguments. vbcc passes the first four in r0/r2/r4/r6 (one per
; even register) and spills the last four to the C soft stack: with no
; frame of our own, bx,by,bw,bh sit at (sp)+0..3. We copy all eight into
; the P block the internal routine reads. sp is preserved (we only read
; through it); the caller restores it after we return.
; ---------------------------------------------------------------------
_x16_collide8:
        lda     r0
        sta     X16_P0                  ; ax
        lda     r2
        sta     X16_P1                  ; ay
        lda     r4
        sta     X16_P2                  ; aw
        lda     r6
        sta     X16_P3                  ; ah
        ldy     #0
        lda     (sp),y
        sta     X16_P4                  ; bx
        iny
        lda     (sp),y
        sta     X16_P5                  ; by
        iny
        lda     (sp),y
        sta     X16_P6                  ; bw
        iny
        lda     (sp),y
        sta     X16_P7                  ; bh

        jsr     collide8                ; carry = overlap
        lda     #0
        ldx     #0                      ; high byte, for int-promoting callers
        rol     a                       ; carry -> bit 0
        rts

; ---------------------------------------------------------------------
; unsigned char x16_collide16(__reg("r0/r1") const x16_box16 *a,
;                             __reg("r2/r3") const x16_box16 *b)
;
; struct x16_box16 is { unsigned int x, y, w, h; } -- eight bytes, in the
; same order and endianness as cl_ax..cl_ah and cl_bx..cl_bh, so the
; marshalling is a straight eight-byte copy per box. The two pointers
; arrive in r0/r1 and r2/r3; move them into the library's own pointer
; scratch (X16_PTR2, X16_PTR3) for the (ind),y copy.
; ---------------------------------------------------------------------
_x16_collide16:
        lda     r0
        sta     X16_PTR2                ; a*
        lda     r1
        sta     X16_PTR2+1
        lda     r2
        sta     X16_PTR3                ; b*
        lda     r3
        sta     X16_PTR3+1

        ldy     #7
.copy:
        lda     (X16_PTR2),y
        sta     cl_ax,y
        lda     (X16_PTR3),y
        sta     cl_bx,y
        dey
        bpl     .copy

        jsr     collide16               ; carry = overlap
        lda     #0
        ldx     #0
        rol     a
        rts

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; collide8 -- do two boxes overlap?
;   in:  X16_P0 = ax, X16_P1 = ay, X16_P2 = aw, X16_P3 = ah
;        X16_P4 = bx, X16_P5 = by, X16_P6 = bw, X16_P7 = bh
;   out: carry set if the boxes overlap, clear otherwise
;
; Coordinates and sizes are unsigned bytes; the edge sums are computed
; in 9 bits so a box may legitimately run past x=255.
;
; Edges that merely touch do NOT overlap: a box at x=0 width 10 and one
; at x=10 are adjacent, not colliding. Overlap on an axis is
;       ax < bx+bw  AND  bx < ax+aw
; and both must hold on x and on y.
;
; Coordinates fit in a byte, so this cannot describe the right-hand half
; of a 640-wide display. Use collide16 there.
; ---------------------------------------------------------------------
collide8:
        ; --- x axis ---
        lda     X16_P4
        clc
        adc     X16_P6                  ; bx + bw
        bcs     .ax_lt                  ; past 255, so ax is certainly less
        cmp     X16_P0                  ; carry set if (bx+bw) >= ax
        bcc     .apart
        beq     .apart                  ; equal means touching, not overlapping
.ax_lt:
        lda     X16_P0
        clc
        adc     X16_P2                  ; ax + aw
        bcs     .bx_lt
        cmp     X16_P4
        bcc     .apart
        beq     .apart
.bx_lt:

        ; --- y axis ---
        lda     X16_P5
        clc
        adc     X16_P7                  ; by + bh
        bcs     .ay_lt
        cmp     X16_P1
        bcc     .apart
        beq     .apart
.ay_lt:
        lda     X16_P1
        clc
        adc     X16_P3                  ; ay + ah
        bcs     .by_lt
        cmp     X16_P5
        bcc     .apart
        beq     .apart
.by_lt:

        sec
        rts
.apart:
        clc
        rts

; ---------------------------------------------------------------------
; collide16 -- the same test with 16-bit unsigned coordinates and sizes.
;
; Needed for anything positioned in display space: in the default 80x60
; text mode the X16's screen is 640x480, and sprite coordinates are in
; those units. Only screen modes 2, 3 and $80 halve it to 320x240.
;
;   in:  cl_ax .. cl_bh, written by the caller
;   out: carry set if the boxes overlap
;
; The edge sums are 17-bit, so a box may legitimately run past x=65535.
; Touching edges do not overlap, exactly as in collide8.
; ---------------------------------------------------------------------
collide16:
        ; ax < bx + bw ?
        clc
        lda     cl_bx
        adc     cl_bw
        sta     cl_t0
        lda     cl_bx+1
        adc     cl_bw+1
        sta     cl_t1
        bcs     .ax_lt                  ; sum overflowed 16 bits: ax is less
        lda     cl_ax
        cmp     cl_t0
        lda     cl_ax+1
        sbc     cl_t1
        bcs     .apart16                ; ax >= sum, so touching or clear
.ax_lt:

        ; bx < ax + aw ?
        clc
        lda     cl_ax
        adc     cl_aw
        sta     cl_t0
        lda     cl_ax+1
        adc     cl_aw+1
        sta     cl_t1
        bcs     .bx_lt
        lda     cl_bx
        cmp     cl_t0
        lda     cl_bx+1
        sbc     cl_t1
        bcs     .apart16
.bx_lt:

        ; ay < by + bh ?
        clc
        lda     cl_by
        adc     cl_bh
        sta     cl_t0
        lda     cl_by+1
        adc     cl_bh+1
        sta     cl_t1
        bcs     .ay_lt
        lda     cl_ay
        cmp     cl_t0
        lda     cl_ay+1
        sbc     cl_t1
        bcs     .apart16
.ay_lt:

        ; by < ay + ah ?
        clc
        lda     cl_ay
        adc     cl_ah
        sta     cl_t0
        lda     cl_ay+1
        adc     cl_ah+1
        sta     cl_t1
        bcs     .by_lt
        lda     cl_by
        cmp     cl_t0
        lda     cl_by+1
        sbc     cl_t1
        bcs     .apart16
.by_lt:

        sec
        rts
.apart16:
        clc
        rts

; ---------------------------------------------------------------------
; Box A, then box B, then scratch. The order and adjacency of these
; sixteen bytes is load-bearing: _x16_collide16 block-copies into them.
; ---------------------------------------------------------------------
        section bss

cl_ax:  reserve    2
cl_ay:  reserve    2
cl_aw:  reserve    2
cl_ah:  reserve    2
cl_bx:  reserve    2
cl_by:  reserve    2
cl_bw:  reserve    2
cl_bh:  reserve    2
cl_t0:  reserve    1
cl_t1:  reserve    1
