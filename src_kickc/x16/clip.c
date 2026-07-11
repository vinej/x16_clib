// =====================================================================
// x16clib :: x16/clip.c -- Cohen-Sutherland line clipping
// =====================================================================
// The core is the same hand-written 6502 as src_ca65/util/clip.s, in
// one asm body with the outcode, intersection and muldiv routines as
// jsr'd local labels.
//
// One deliberate difference: the ca65 core ended by preloading the
// X16_P0..P5 operand block "for gfx_line / fx_line", a contract for
// assembly callers. No such block exists in the KickC port -- a C
// caller passes seg's fields to the drawing routine itself.
// =====================================================================

#include <x16/clip.h>

// The rectangle has nonzero defaults: the full 320x240 bitmap.
__mem volatile unsigned int x16__clip_xmin = 0;
__mem volatile unsigned int x16__clip_ymin = 0;
__mem volatile unsigned int x16__clip_xmax = 319;
__mem volatile unsigned int x16__clip_ymax = 239;

// The in/out segment pointer, pinned in zero page: KickC ignores __zp
// on parameters and may spill one to main memory, where (seg),y cannot
// reach. The cc65 build used ptr1 the same way.
__address(0x78) char* volatile x16__cp_segp;

// The segment being clipped. x0(+0), y0(+2), x1(+4), y1(+6) must stay
// adjacent: x16_clip_line block-copies an x16_line straight onto them,
// and the outcode routine indexes the block by 0 and 4. Likewise
// x16__cw/x16__co hold an endpoint as a 4-byte x,y pair.
__mem volatile char x16__clipl[8];
__mem volatile char x16__cw[4];               // the endpoint being moved
__mem volatile char x16__co[4];               // ...and the fixed one opposite

__mem volatile char x16__cp_c0;
__mem volatile char x16__cp_c1;
__mem volatile char x16__cp_code;
__mem volatile char x16__cp_which;
__mem volatile char x16__cp_oc;
__mem volatile char x16__cp_vis;              // the core's verdict: 1 = visible
__mem volatile char x16__cp_b[2];
__mem volatile char x16__cp_m1[2];
__mem volatile char x16__cp_m2[2];
__mem volatile char x16__cp_m3[2];
__mem volatile char x16__cp_q[2];
__mem volatile char x16__cp_sgn;
__mem volatile char x16__cp_prod[4];
__mem volatile char x16__cp_rem[2];

// ---------------------------------------------------------------------
// Internal: clip x16__clipl against the rectangle. x16__cp_vis = 0 if
// the segment lies entirely outside, else 1 with x16__clipl holding the
// visible sub-segment.
//
// Outcode bits: LEFT $01, RIGHT $02, TOP $04, BOTTOM $08.
// ---------------------------------------------------------------------
void x16__clip_core(void) {
    asm {
    cc_loop:
        jsr cc_oc0
        sta x16__cp_c0
        jsr cc_oc1
        sta x16__cp_c1
        ora x16__cp_c0
        bne cc_outside
        jmp cc_accept           // both inside (out of branch range)
    cc_outside:
        lda x16__cp_c0
        and x16__cp_c1
        beq cc_clip_one
        stz x16__cp_vis         // share an outside half-plane: reject
        jmp cc_end

    cc_clip_one:
        // pull the endpoint with a nonzero code into the work slot
        lda x16__cp_c0
        bne cc_use0
        lda #1
        sta x16__cp_which
        lda x16__cp_c1
        sta x16__cp_code
        ldx #3
    cc_cp1:
        lda x16__clipl+4,x
        sta x16__cw,x
        lda x16__clipl,x
        sta x16__co,x
        dex
        bpl cc_cp1
        jmp cc_intersect        // out of branch range
    cc_use0:
        stz x16__cp_which
        sta x16__cp_code
        ldx #3
    cc_cp0:
        lda x16__clipl,x
        sta x16__cw,x
        lda x16__clipl+4,x
        sta x16__co,x
        dex
        bpl cc_cp0

    cc_intersect:
        lda x16__cp_code
        and #$08                // CLIP_BOTTOM
        beq cc_not_bottom
        lda x16__clip_ymax
        sta x16__cp_b
        lda x16__clip_ymax+1
        sta x16__cp_b+1
        jsr cc_cross_y
        bra cc_store
    cc_not_bottom:
        lda x16__cp_code
        and #$04                // CLIP_TOP
        beq cc_not_top
        lda x16__clip_ymin
        sta x16__cp_b
        lda x16__clip_ymin+1
        sta x16__cp_b+1
        jsr cc_cross_y
        bra cc_store
    cc_not_top:
        lda x16__cp_code
        and #$02                // CLIP_RIGHT
        beq cc_not_right
        lda x16__clip_xmax
        sta x16__cp_b
        lda x16__clip_xmax+1
        sta x16__cp_b+1
        jsr cc_cross_x
        bra cc_store
    cc_not_right:
        lda x16__clip_xmin      // CLIP_LEFT is all that remains
        sta x16__cp_b
        lda x16__clip_xmin+1
        sta x16__cp_b+1
        jsr cc_cross_x

    cc_store:
        // write the moved endpoint back and go around again
        lda x16__cp_which
        bne cc_st1
        ldx #3
    cc_sb0:
        lda x16__cw,x
        sta x16__clipl,x
        dex
        bpl cc_sb0
        jmp cc_loop
    cc_st1:
        ldx #3
    cc_sb1:
        lda x16__cw,x
        sta x16__clipl+4,x
        dex
        bpl cc_sb1
        jmp cc_loop

    cc_accept:
        lda #1
        sta x16__cp_vis
        jmp cc_end

    // --- outcodes ---------------------------------------------------
    // A = outcode of endpoint 0 (X=0) or endpoint 1 (X=4)
    cc_oc0:
        ldx #0
        bra cc_outcode
    cc_oc1:
        ldx #4
    cc_outcode:
        stz x16__cp_oc
        // x < xmin?
        lda x16__clipl,x
        cmp x16__clip_xmin
        lda x16__clipl+1,x
        sbc x16__clip_xmin+1
        bvc oc_x1
        eor #$80
    oc_x1:
        bpl oc_x2               // x >= xmin
        lda #$01                // CLIP_LEFT
        tsb x16__cp_oc
        bra oc_y                // can't also be right of xmax
    oc_x2:
        // xmax < x?
        lda x16__clip_xmax
        cmp x16__clipl,x
        lda x16__clip_xmax+1
        sbc x16__clipl+1,x
        bvc oc_x3
        eor #$80
    oc_x3:
        bpl oc_y
        lda #$02                // CLIP_RIGHT
        tsb x16__cp_oc
    oc_y:
        // y < ymin?
        lda x16__clipl+2,x
        cmp x16__clip_ymin
        lda x16__clipl+3,x
        sbc x16__clip_ymin+1
        bvc oc_y1
        eor #$80
    oc_y1:
        bpl oc_y2
        lda #$04                // CLIP_TOP
        tsb x16__cp_oc
        bra oc_done
    oc_y2:
        // ymax < y?
        lda x16__clip_ymax
        cmp x16__clipl+2,x
        lda x16__clip_ymax+1
        sbc x16__clipl+3,x
        bvc oc_y3
        eor #$80
    oc_y3:
        bpl oc_done
        lda #$08                // CLIP_BOTTOM
        tsb x16__cp_oc
    oc_done:
        lda x16__cp_oc
        rts

    // --- intersections ----------------------------------------------
    // Move the work endpoint onto the horizontal boundary cp_b:
    //   cw_x += (co_x - cw_x) * (cp_b - cw_y) / (co_y - cw_y); cw_y = cp_b
    cc_cross_y:
        sec                     // numerator 1: dx = co_x - cw_x
        lda x16__co
        sbc x16__cw
        sta x16__cp_m1
        lda x16__co+1
        sbc x16__cw+1
        sta x16__cp_m1+1
        sec                     // numerator 2: cp_b - cw_y
        lda x16__cp_b
        sbc x16__cw+2
        sta x16__cp_m2
        lda x16__cp_b+1
        sbc x16__cw+3
        sta x16__cp_m2+1
        sec                     // denominator: dy = co_y - cw_y
        lda x16__co+2
        sbc x16__cw+2
        sta x16__cp_m3
        lda x16__co+3
        sbc x16__cw+3
        sta x16__cp_m3+1
        jsr cc_muldiv           // cp_q = m1 * m2 / m3, signed
        clc
        lda x16__cw
        adc x16__cp_q
        sta x16__cw
        lda x16__cw+1
        adc x16__cp_q+1
        sta x16__cw+1
        lda x16__cp_b
        sta x16__cw+2
        lda x16__cp_b+1
        sta x16__cw+3
        rts

    // Move the work endpoint onto the vertical boundary cp_b:
    //   cw_y += (co_y - cw_y) * (cp_b - cw_x) / (co_x - cw_x); cw_x = cp_b
    cc_cross_x:
        sec
        lda x16__co+2
        sbc x16__cw+2
        sta x16__cp_m1
        lda x16__co+3
        sbc x16__cw+3
        sta x16__cp_m1+1
        sec
        lda x16__cp_b
        sbc x16__cw
        sta x16__cp_m2
        lda x16__cp_b+1
        sbc x16__cw+1
        sta x16__cp_m2+1
        sec
        lda x16__co
        sbc x16__cw
        sta x16__cp_m3
        lda x16__co+1
        sbc x16__cw+1
        sta x16__cp_m3+1
        jsr cc_muldiv
        clc
        lda x16__cw+2
        adc x16__cp_q
        sta x16__cw+2
        lda x16__cw+3
        adc x16__cp_q+1
        sta x16__cw+3
        lda x16__cp_b
        sta x16__cw
        lda x16__cp_b+1
        sta x16__cw+1
        rts

    // cp_q = (cp_m1 * cp_m2) / cp_m3, all signed 16-bit. With inputs
    // within +/-4095 the product fits 24 bits and the quotient 16.
    cc_muldiv:
        stz x16__cp_sgn
        lda x16__cp_m1+1        // strip the three signs
        bpl md_m1p
        inc x16__cp_sgn
        jsr md_neg1
    md_m1p:
        lda x16__cp_m2+1
        bpl md_m2p
        inc x16__cp_sgn
        sec
        lda #0
        sbc x16__cp_m2
        sta x16__cp_m2
        lda #0
        sbc x16__cp_m2+1
        sta x16__cp_m2+1
    md_m2p:
        lda x16__cp_m3+1
        bpl md_m3p
        inc x16__cp_sgn
        sec
        lda #0
        sbc x16__cp_m3
        sta x16__cp_m3
        lda #0
        sbc x16__cp_m3+1
        sta x16__cp_m3+1
    md_m3p:
        // 16x16 -> 32 shift-add multiply: prod = m1 * m2 (umul16's
        // shape, with the adc carry rolling down through the rotate)
        stz x16__cp_prod+2
        stz x16__cp_prod+3
        ldx #16
    md_mul:
        lsr x16__cp_m2+1
        ror x16__cp_m2
        bcc md_noadd
        lda x16__cp_prod+2
        clc
        adc x16__cp_m1
        sta x16__cp_prod+2
        lda x16__cp_prod+3
        adc x16__cp_m1+1
        bra md_rot
    md_noadd:
        lda x16__cp_prod+3      // carry is already clear
    md_rot:
        ror
        sta x16__cp_prod+3
        ror x16__cp_prod+2
        ror x16__cp_prod+1
        ror x16__cp_prod
        dex
        bne md_mul

        // 32 / 16 restoring divide: quotient into cp_prod
        stz x16__cp_rem
        stz x16__cp_rem+1
        ldx #32
    md_div:
        asl x16__cp_prod
        rol x16__cp_prod+1
        rol x16__cp_prod+2
        rol x16__cp_prod+3
        rol x16__cp_rem
        rol x16__cp_rem+1
        sec
        lda x16__cp_rem
        sbc x16__cp_m3
        tay
        lda x16__cp_rem+1
        sbc x16__cp_m3+1
        bcc md_nofit
        sta x16__cp_rem+1
        sty x16__cp_rem
        inc x16__cp_prod
    md_nofit:
        dex
        bne md_div

        lda x16__cp_prod
        sta x16__cp_q
        lda x16__cp_prod+1
        sta x16__cp_q+1
        lda x16__cp_sgn         // odd number of negatives: negate
        lsr
        bcc md_posq
        sec
        lda #0
        sbc x16__cp_q
        sta x16__cp_q
        lda #0
        sbc x16__cp_q+1
        sta x16__cp_q+1
    md_posq:
        rts

    md_neg1:
        sec
        lda #0
        sbc x16__cp_m1
        sta x16__cp_m1
        lda #0
        sbc x16__cp_m1+1
        sta x16__cp_m1+1
        rts

    cc_end:
    }
}

void x16_clip_set(__mem unsigned int xmin, __mem unsigned int ymin,
                  __mem unsigned int xmax, __mem unsigned int ymax) {
    x16__clip_xmin = xmin;
    x16__clip_ymin = ymin;
    x16__clip_xmax = xmax;
    x16__clip_ymax = ymax;
}

unsigned char x16_clip_line(x16_line *seg) {
    x16__cp_segp = (char*)seg;
    asm {
        ldy #7
    clw_in:
        lda (x16__cp_segp),y
        sta x16__clipl,y
        dey
        bpl clw_in
    }
    x16__clip_core();
    if (!x16__cp_vis) {
        return 0;
    }
    asm {
        ldy #7
    clw_out:
        lda x16__clipl,y
        sta (x16__cp_segp),y
        dey
        bpl clw_out
    }
    return 1;
}
