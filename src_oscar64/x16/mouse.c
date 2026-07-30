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
// insists on and lets C do the pointer stores. Three out-params
// plus that scratch would not fit in the eight bytes at $78-$7F, and
// there is no reason for them to: the asm here does not need them.
// =====================================================================

#include <x16/mouse.h>

// MOUSE_GET fills four bytes of zero page starting at the address it
// takes in X -- a ONE-BYTE base, so the area must be in zero page.
//
// $7C-$7F is the library's, reserved in x16/zpsafe.h, and input.c pins
// the same four for the same call; the two are never live at once. This
// module names the address NUMERICALLY rather than declaring a second
// pinned variable, because there is no linker: two blocks at one
// address are rejected outright, and two at different addresses make
// KickAssembler emit a PRG that overwrites zero page as it loads --
// which crashes before main() is even reached.
// The four bytes copied out of that zero-page area, so C can read them
// without the compiler deciding to re-place anything.
volatile unsigned char x16__ms_copy[4];

volatile unsigned char x16__ms_btn;
volatile unsigned char x16__ms_wheel;

// ---------------------------------------------------------------------
// Show or hide the pointer and set its bounding box, in units of 8
// pixels. show = 0 hides it, 1 shows it.
// ---------------------------------------------------------------------
void x16_mse_config(unsigned char show,
                    unsigned char width8,
                    unsigned char height8) {
    __asm {
        lda show
        ldx width8
        ldy height8
        jsr 0xff68                      // MOUSE_CONFIG
    }
}

// ---------------------------------------------------------------------
// Poll the hardware. The KERNAL's IRQ already does this once a frame;
// call it yourself only if you have taken the interrupt over.
// ---------------------------------------------------------------------
void x16_mse_scan(void) {
    __asm {
        jsr 0xff71                      // MOUSE_SCAN
    }
}

// ---------------------------------------------------------------------
// Position, buttons and wheel in one call. Returns the wheel delta as a
// signed char; *buttons is bit 0 left, bit 1 right, bit 2 middle.
// ---------------------------------------------------------------------
signed char x16_mse_get(unsigned int *x,
                        unsigned int *y,
                        unsigned char *buttons) {
    unsigned char *xb;
    __asm {
        ldx #0x7c                        // the shared scratch; see above
        jsr 0xff6b         // A = buttons, X = wheel delta (MOUSE_GET)
        sta x16__ms_btn
        stx x16__ms_wheel
        lda 0x7c
        sta x16__ms_copy+0
        lda 0x7d
        sta x16__ms_copy+1
        lda 0x7e
        sta x16__ms_copy+2
        lda 0x7f
        sta x16__ms_copy+3
    }
    /* Byte stores, not `*x = lo | hi << 8`: this avoids a fragment for
    ** storing a computed 16-bit value through a pointer parameter. */
    xb = (unsigned char *)x;
    xb[0] = x16__ms_copy[0];
    xb[1] = x16__ms_copy[1];
    xb = (unsigned char *)y;
    xb[0] = x16__ms_copy[2];
    xb[1] = x16__ms_copy[3];
    *buttons = x16__ms_btn;
    return (signed char)x16__ms_wheel;
}
