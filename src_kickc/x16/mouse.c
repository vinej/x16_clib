// =====================================================================
// x16clib :: x16/mouse.c -- the raw KERNAL mouse API
// =====================================================================
// The lower-level half of the mouse: x16_mse_config() turns the pointer
// on and sizes its bounding box, x16_mse_scan() polls the hardware, and
// x16_mse_get() reads position, buttons and the wheel in one call.
//
// The friendlier x16_mouse_* entries in x16/input.h sit on top of these.
//
// Unlike input.c, which pins its out-param pointers so inline asm can
// store through them, this module only pins the four bytes MOUSE_GET
// insists on and lets KickC do the pointer stores in C. Three out-params
// plus that scratch would not fit in the eight bytes at $78-$7F, and
// there is no reason for them to: the asm here does not need them.
// =====================================================================

#include <x16/mouse.h>

// MOUSE_GET writes the four position bytes to zero page starting at the
// address it takes in X -- a ONE-BYTE base, so this scratch must be zp.
// Same slots input.c uses for the same call; the two are never live at
// once (see x16/zpsafe.h).
//
// TOUCHED FROM ASM ONLY. KickC silently drops __address() from a
// variable that C code also reads -- it re-places it in ordinary memory
// and the asm then hands MOUSE_GET a low byte pointing at some
// unrelated zero-page cell, which corrupts the machine. So the four
// bytes are copied out in the asm block and C reads the copy.
__address(0x7c) volatile char x16__ms_pos[4];

__mem volatile unsigned char x16__ms_copy[4];

__mem volatile unsigned char x16__ms_btn;
__mem volatile unsigned char x16__ms_wheel;

// ---------------------------------------------------------------------
// Show or hide the pointer and set its bounding box, in units of 8
// pixels. show = 0 hides it, 1 shows it.
// ---------------------------------------------------------------------
void x16_mse_config(__mem unsigned char show,
                    __mem unsigned char width8,
                    __mem unsigned char height8) {
    asm {
        lda show
        ldx width8
        ldy height8
        jsr $ff68 /*MOUSE_CONFIG*/
    }
}

// ---------------------------------------------------------------------
// Poll the hardware. The KERNAL's IRQ already does this once a frame;
// call it yourself only if you have taken the interrupt over.
// ---------------------------------------------------------------------
void x16_mse_scan(void) {
    asm {
        jsr $ff71 /*MOUSE_SCAN*/
    }
}

// ---------------------------------------------------------------------
// Position, buttons and wheel in one call. Returns the wheel delta as a
// signed char; *buttons is bit 0 left, bit 1 right, bit 2 middle.
// ---------------------------------------------------------------------
signed char x16_mse_get(unsigned int *x,
                        unsigned int *y,
                        unsigned char *buttons) {
    asm {
        ldx #<x16__ms_pos
        jsr $ff6b /*MOUSE_GET*/         // A = buttons, X = wheel delta
        sta x16__ms_btn
        stx x16__ms_wheel
        lda x16__ms_pos+0               // out of zero page before C can
        sta x16__ms_copy+0              // see the array and unpin it
        lda x16__ms_pos+1
        sta x16__ms_copy+1
        lda x16__ms_pos+2
        sta x16__ms_copy+2
        lda x16__ms_pos+3
        sta x16__ms_copy+3
    }
    *x = (unsigned int)x16__ms_copy[0] | ((unsigned int)x16__ms_copy[1] << 8);
    *y = (unsigned int)x16__ms_copy[2] | ((unsigned int)x16__ms_copy[3] << 8);
    *buttons = x16__ms_btn;
    return (signed char)x16__ms_wheel;
}
