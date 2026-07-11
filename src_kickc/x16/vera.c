// =====================================================================
// x16clib :: x16/vera.c -- VRAM data-port access
// =====================================================================
// The same hand-written loops as src_ca65/video/vera.s. Symbolic
// constants are inlined as literals because KickC's preprocessor does
// not reach inside asm {} blocks; each carries its name in a comment.
//
// Cross-module note: the ca65 build exported the internal vera_fill for
// gfx/bitmap.s and sprite/sprite.s to jsr. Here there is no shim for an
// internal routine to skip, so other modules simply call
// x16_vera_fill() itself.
// =====================================================================

#include <x16/vera.h>

__mem volatile char x16__vr_t;                // ADDR_H under construction

// ---------------------------------------------------------------------
// Point a data port at a 17-bit VRAM address. Only bit 16 of the high
// half exists in hardware. `incr` is a raw index 0-15, optionally OR'd
// with X16_DECR ($10).
//
// ADDRSEL is bit 0 of CTRL and DCSEL is bits 6:1: a plain `lda #0 / sta
// CTRL` would silently reset DCSEL too, so the port select is a
// read-modify-write via the 65C02's trb/tsb.
// ---------------------------------------------------------------------
void x16_vera_addr0(__mem unsigned char incr, __mem unsigned long addr) {
    asm {
        lda addr+2
        and #$01 /*VERA_ADDR_H_BANK*/   // ...only bit 16 exists
        sta x16__vr_t                   // ADDR_H under construction
        lda incr
        and #$0f                        // the increment index
        asl
        asl
        asl
        asl                             // into ADDR_H bits 7:4
        ora x16__vr_t
        sta x16__vr_t
        lda incr
        and #$10 /*X16_DECR*/
        beq va0_ascending
        lda #$08 /*VERA_ADDR_H_DECR*/
        ora x16__vr_t
        sta x16__vr_t
    va0_ascending:
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/         // ADDRSEL = 0
        lda addr
        sta $9f20 /*VERA_ADDR_L*/
        lda addr+1
        sta $9f21 /*VERA_ADDR_M*/
        lda x16__vr_t
        sta $9f22 /*VERA_ADDR_H*/
    }
}

void x16_vera_addr1(__mem unsigned char incr, __mem unsigned long addr) {
    asm {
        lda addr+2
        and #$01 /*VERA_ADDR_H_BANK*/
        sta x16__vr_t
        lda incr
        and #$0f
        asl
        asl
        asl
        asl
        ora x16__vr_t
        sta x16__vr_t
        lda incr
        and #$10 /*X16_DECR*/
        beq va1_ascending
        lda #$08 /*VERA_ADDR_H_DECR*/
        ora x16__vr_t
        sta x16__vr_t
    va1_ascending:
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        tsb $9f25 /*VERA_CTRL*/         // ADDRSEL = 1
        lda addr
        sta $9f20 /*VERA_ADDR_L*/
        lda addr+1
        sta $9f21 /*VERA_ADDR_M*/
        lda x16__vr_t
        sta $9f22 /*VERA_ADDR_H*/
    }
}

// ---------------------------------------------------------------------
// The tight `sta VERA_DATA0` loop -- far faster than a per-byte address
// reload. Port 0 must already point at the destination, with the
// increment the caller wants.
// ---------------------------------------------------------------------
void x16_vera_fill(__mem unsigned char value, __mem unsigned int count) {
    asm {
        lda count
        ora count+1
        beq vf_done                     // count == 0
        ldx count                       // X = count lo, Y = count hi
        ldy count+1
        lda count
        beq vf_full                     // low byte 0 -> exactly hi*256
        iny                             // otherwise one extra partial page
    vf_full:
        lda value
    vf_loop:
        sta $9f23 /*VERA_DATA0*/
        dex
        bne vf_loop
        dey
        bne vf_loop
    vf_done:
    }
}

// ---------------------------------------------------------------------
// DATA0 always reads port 0 and DATA1 always writes port 1, whatever
// ADDRSEL says -- so the inner loop never touches CTRL and never
// reloads an address. Two bytes per iteration, both auto-incrementing.
// ---------------------------------------------------------------------
void x16_vera_copy(__mem unsigned int count) {
    asm {
        lda count
        ora count+1
        beq vc_done
        ldx count
        ldy count+1
        lda count
        beq vc_full
        iny
    vc_full:
    vc_loop:
        lda $9f23 /*VERA_DATA0*/
        sta $9f24 /*VERA_DATA1*/
        dex
        bne vc_loop
        dey
        bne vc_loop
    vc_done:
    }
}

// ---------------------------------------------------------------------
// Probes DCSEL=63, where DC_VER0 reads back ASCII 'V' on FX-capable
// VERA (v0.3.1 and later). Restores DCSEL to 0 on the way out. The
// major version the hardware also reports is useless as a return
// value: FX first shipped with major version zero, so it could not be
// told apart from "absent".
// ---------------------------------------------------------------------
unsigned char x16_vera_has_fx(void) {
    __mem char r;
    asm {
        lda $9f25 /*VERA_CTRL*/         // vera_dcsel 63: keep ADDRSEL,
        and #$01 /*VERA_CTRL_ADDRSEL*/  // never write bit 7 (VERA reset)
        ora #$7e                        // 63 << 1
        sta $9f25 /*VERA_CTRL*/
        lda $9f29 /*VERA_DC_VER0*/
        cmp #$56 /*VERA_VERSION_MAGIC 'V'*/
        bne hfx_no
        lda $9f25 /*VERA_CTRL*/         // vera_dcsel 0
        and #$01
        sta $9f25 /*VERA_CTRL*/
        lda #1
        sta r
        bra hfx_done
    hfx_no:
        lda $9f25 /*VERA_CTRL*/         // vera_dcsel 0
        and #$01
        sta $9f25 /*VERA_CTRL*/
        stz r
    hfx_done:
    }
    return r;
}
