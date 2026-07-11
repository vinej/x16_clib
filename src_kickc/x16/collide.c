// =====================================================================
// x16clib :: x16/collide.c -- axis-aligned bounding-box overlap
// =====================================================================
// The internal routines are the same hand-written 6502 as
// src_ca65/util/collide.s. What is gone is the marshalling: KickC
// places each parameter in zero page under its own name, so the cc65
// popa-chain shims have no equivalent here.
// =====================================================================

#include <x16/collide.h>

// Box A, box B, scratch. The adjacency of the eight bytes within each
// array is load-bearing: x16_collide16 block-copies an x16_box16 onto
// them, so the field order is x(+0), y(+2), w(+4), h(+6), low byte
// first, exactly the struct's own layout.
__mem volatile char x16__cl_a[8];
__mem volatile char x16__cl_b[8];
// The block-copy pointers. Inline asm can only indirect through zero
// page, KickC ignores __zp on parameters, and under zero-page pressure
// it silently spills a pointer parameter to main memory -- so the
// parameters are copied into these pinned slots first, exactly the
// cc65 build's ptr1/ptr2.
__address(0x78) const char* volatile x16__cl_pa;
__address(0x7a) const char* volatile x16__cl_pb;

__mem volatile char x16__cl_t0;
__mem volatile char x16__cl_t1;

// ---------------------------------------------------------------------
// Coordinates and sizes are unsigned bytes; the edge sums are computed
// in 9 bits so a box may legitimately run past x=255.
//
// Edges that merely touch do NOT overlap: a box at x=0 width 10 and one
// at x=10 are adjacent, not colliding. Overlap on an axis is
//      ax < bx+bw  AND  bx < ax+aw
// and both must hold on x and on y.
// ---------------------------------------------------------------------
unsigned char x16_collide8(__mem unsigned char ax, __mem unsigned char ay,
                           __mem unsigned char aw, __mem unsigned char ah,
                           __mem unsigned char bx, __mem unsigned char by,
                           __mem unsigned char bw, __mem unsigned char bh) {
    __mem char r;
    asm {
        // --- x axis ---
        lda bx
        clc
        adc bw              // bx + bw
        bcs c8_ax_lt        // past 255, so ax is certainly less
        cmp ax              // carry set if (bx+bw) >= ax
        bcc c8_apart
        beq c8_apart        // equal means touching, not overlapping
    c8_ax_lt:
        lda ax
        clc
        adc aw              // ax + aw
        bcs c8_bx_lt
        cmp bx
        bcc c8_apart
        beq c8_apart
    c8_bx_lt:

        // --- y axis ---
        lda by
        clc
        adc bh              // by + bh
        bcs c8_ay_lt
        cmp ay
        bcc c8_apart
        beq c8_apart
    c8_ay_lt:
        lda ay
        clc
        adc ah              // ay + ah
        bcs c8_by_lt
        cmp by
        bcc c8_apart
        beq c8_apart
    c8_by_lt:
        lda #1
        sta r
        jmp c8_done
    c8_apart:
        lda #0
        sta r
    c8_done:
    }
    return r;
}

// ---------------------------------------------------------------------
// The same test with 16-bit unsigned coordinates and sizes. The edge
// sums are 17-bit, so a box may legitimately run past x=65535. Touching
// edges do not overlap, exactly as in x16_collide8.
// ---------------------------------------------------------------------
unsigned char x16_collide16(const x16_box16 *a, const x16_box16 *b) {
    __mem char r;
    x16__cl_pa = (char*)a;
    x16__cl_pb = (char*)b;
    asm {
        ldy #7
    c16_copy:
        lda (x16__cl_pa),y
        sta x16__cl_a,y
        lda (x16__cl_pb),y
        sta x16__cl_b,y
        dey
        bpl c16_copy

        // ax < bx + bw ?
        clc
        lda x16__cl_b       // bx
        adc x16__cl_b+4     // + bw
        sta x16__cl_t0
        lda x16__cl_b+1
        adc x16__cl_b+5
        sta x16__cl_t1
        bcs c16_ax_lt       // sum overflowed 16 bits: ax is less
        lda x16__cl_a       // ax
        cmp x16__cl_t0
        lda x16__cl_a+1
        sbc x16__cl_t1
        bcs c16_apart       // ax >= sum, so touching or clear
    c16_ax_lt:

        // bx < ax + aw ?
        clc
        lda x16__cl_a       // ax
        adc x16__cl_a+4     // + aw
        sta x16__cl_t0
        lda x16__cl_a+1
        adc x16__cl_a+5
        sta x16__cl_t1
        bcs c16_bx_lt
        lda x16__cl_b       // bx
        cmp x16__cl_t0
        lda x16__cl_b+1
        sbc x16__cl_t1
        bcs c16_apart
    c16_bx_lt:

        // ay < by + bh ?
        clc
        lda x16__cl_b+2     // by
        adc x16__cl_b+6     // + bh
        sta x16__cl_t0
        lda x16__cl_b+3
        adc x16__cl_b+7
        sta x16__cl_t1
        bcs c16_ay_lt
        lda x16__cl_a+2     // ay
        cmp x16__cl_t0
        lda x16__cl_a+3
        sbc x16__cl_t1
        bcs c16_apart
    c16_ay_lt:

        // by < ay + ah ?
        clc
        lda x16__cl_a+2     // ay
        adc x16__cl_a+6     // + ah
        sta x16__cl_t0
        lda x16__cl_a+3
        adc x16__cl_a+7
        sta x16__cl_t1
        bcs c16_by_lt
        lda x16__cl_b+2     // by
        cmp x16__cl_t0
        lda x16__cl_b+3
        sbc x16__cl_t1
        bcs c16_apart
    c16_by_lt:
        lda #1
        sta r
        jmp c16_done
    c16_apart:
        lda #0
        sta r
    c16_done:
    }
    return r;
}
