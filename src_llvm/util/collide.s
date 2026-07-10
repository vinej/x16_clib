; =====================================================================
; x16clib :: util/collide.s -- axis-aligned bounding-box overlap
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   INTEGER bytes fill A, then X, then __rc2, __rc3, ... left to right.
;   A POINTER takes a whole __rc pair (__rc2/__rc3, then __rc4/__rc5) and
;   consumes no A/X -- only zero page can be indirected through.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_collide8
        .globl  x16_collide16

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_collide8(
;       unsigned char ax, unsigned char ay, unsigned char aw, unsigned char ah,
;       unsigned char bx, unsigned char by, unsigned char bw, unsigned char bh)
;
; Eight one-byte arguments: the rightmost (bh) arrives in A, the other
; seven come off the C stack in reverse order. Pops go straight into the
; parameter block -- popa touches only A and Y.
; ---------------------------------------------------------------------
; Eight byte arguments, so they fill A, X, then __rc2 through __rc7, in
; declaration order. The cc65 build had to pop seven of them.
x16_collide8:
        sta     X16_P0                  ; ax
        stx     X16_P1                  ; ay
        lda     __rc2
        sta     X16_P2                  ; aw
        lda     __rc3
        sta     X16_P3                  ; ah
        lda     __rc4
        sta     X16_P4                  ; bx
        lda     __rc5
        sta     X16_P5                  ; by
        lda     __rc6
        sta     X16_P6                  ; bw
        lda     __rc7
        sta     X16_P7                  ; bh

        jsr     collide8                ; carry = overlap
        lda     #0
        rol     a                       ; carry -> bit 0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_collide16(const x16_box16 *a,
;                                          const x16_box16 *b)
;
; struct x16_box16 is { unsigned int x, y, w, h; } -- eight bytes, in the
; same order and endianness as cl_ax..cl_ah and cl_bx..cl_bh. So the
; marshalling is a straight eight-byte copy per box, not a field-by-field
; unpack. Keep the struct and those .res directives in step.
;
; The internal routine takes its operands from those variables rather
; than the parameter block: eight 16-bit fields is sixteen bytes, twice
; what the block holds.
; ---------------------------------------------------------------------
; Two pointers: a in __rc2/__rc3, b in __rc4/__rc5. cc65 borrowed its
; runtime ptr1/ptr2 to indirect through; here T0/T1 and T2/T3 serve, and
; collide16 reads cl_* rather than the P block, so nothing collides.
x16_collide16:
        lda     __rc2
        sta     X16_T0                  ; a
        lda     __rc3
        sta     X16_T1
        lda     __rc4
        sta     X16_T2                  ; b
        lda     __rc5
        sta     X16_T3

        ldy     #7
.Lx16_collide16_copy:
        lda     (X16_T0),y
        sta     cl_ax,y
        lda     (X16_T2),y
        sta     cl_bx,y
        dey
        bpl     .Lx16_collide16_copy

        jsr     collide16               ; carry = overlap
        lda     #0
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
        bcs     .Lcollide8_ax_lt                  ; past 255, so ax is certainly less
        cmp     X16_P0                  ; carry set if (bx+bw) >= ax
        bcc     .Lcollide8_apart
        beq     .Lcollide8_apart                  ; equal means touching, not overlapping
.Lcollide8_ax_lt:
        lda     X16_P0
        clc
        adc     X16_P2                  ; ax + aw
        bcs     .Lcollide8_bx_lt
        cmp     X16_P4
        bcc     .Lcollide8_apart
        beq     .Lcollide8_apart
.Lcollide8_bx_lt:

        ; --- y axis ---
        lda     X16_P5
        clc
        adc     X16_P7                  ; by + bh
        bcs     .Lcollide8_ay_lt
        cmp     X16_P1
        bcc     .Lcollide8_apart
        beq     .Lcollide8_apart
.Lcollide8_ay_lt:
        lda     X16_P1
        clc
        adc     X16_P3                  ; ay + ah
        bcs     .Lcollide8_by_lt
        cmp     X16_P5
        bcc     .Lcollide8_apart
        beq     .Lcollide8_apart
.Lcollide8_by_lt:

        sec
        rts
.Lcollide8_apart:
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
        bcs     .Lcollide16_ax_lt                  ; sum overflowed 16 bits: ax is less
        lda     cl_ax
        cmp     cl_t0
        lda     cl_ax+1
        sbc     cl_t1
        bcs     .Lcollide16_apart16                ; ax >= sum, so touching or clear
.Lcollide16_ax_lt:

        ; bx < ax + aw ?
        clc
        lda     cl_ax
        adc     cl_aw
        sta     cl_t0
        lda     cl_ax+1
        adc     cl_aw+1
        sta     cl_t1
        bcs     .Lcollide16_bx_lt
        lda     cl_bx
        cmp     cl_t0
        lda     cl_bx+1
        sbc     cl_t1
        bcs     .Lcollide16_apart16
.Lcollide16_bx_lt:

        ; ay < by + bh ?
        clc
        lda     cl_by
        adc     cl_bh
        sta     cl_t0
        lda     cl_by+1
        adc     cl_bh+1
        sta     cl_t1
        bcs     .Lcollide16_ay_lt
        lda     cl_ay
        cmp     cl_t0
        lda     cl_ay+1
        sbc     cl_t1
        bcs     .Lcollide16_apart16
.Lcollide16_ay_lt:

        ; by < ay + ah ?
        clc
        lda     cl_ay
        adc     cl_ah
        sta     cl_t0
        lda     cl_ay+1
        adc     cl_ah+1
        sta     cl_t1
        bcs     .Lcollide16_by_lt
        lda     cl_by
        cmp     cl_t0
        lda     cl_by+1
        sbc     cl_t1
        bcs     .Lcollide16_apart16
.Lcollide16_by_lt:

        sec
        rts
.Lcollide16_apart16:
        clc
        rts

; ---------------------------------------------------------------------
; Box A, then box B, then scratch. The order and adjacency of these
; sixteen bytes is load-bearing: _x16_collide16 block-copies into them.
; ---------------------------------------------------------------------
        .section .bss,"aw",@nobits

cl_ax:  .zero  2
cl_ay:  .zero  2
cl_aw:  .zero  2
cl_ah:  .zero  2
cl_bx:  .zero  2
cl_by:  .zero  2
cl_bw:  .zero  2
cl_bh:  .zero  2
cl_t0:  .zero  1
cl_t1:  .zero  1
