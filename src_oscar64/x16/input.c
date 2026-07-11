// =====================================================================
// x16clib :: x16/input.c -- joystick, mouse, keyboard
// =====================================================================
// Same code as src_ca65/input/input.s: thin wrappers over the KERNAL.
// Nothing here touches VERA directly except mouse_show, which the
// KERNAL handles for us.
// =====================================================================

#include <x16/input.h>

// MOUSE_GET writes the four position bytes to zero page starting at
// the address in X -- a one-byte base, so this scratch MUST be zp.
__zeropage volatile char x16__in_mpos[4];

// The out-parameters are indirected directly -- Oscar64 keeps pointer
// parameters in zero page.

// Sample every joystick.
void x16_joy_scan(void) {
    __asm {
        jsr 0xff53                      /* JOYSTICK_SCAN */
    }
}

// ---------------------------------------------------------------------
// JOYSTICK_GET returns byte0 in A, byte1 in X, and presence in Y:
// $00 = present, $FF = absent. The wrapper turns that Y into a clean
// 1/0 through the pointer. Bits are ACTIVE LOW: pressed reads 0.
// ---------------------------------------------------------------------
unsigned int x16_joy_get(unsigned char joy, unsigned char *present) {
    return __asm {
        lda joy
        jsr 0xff56      /* A = byte0, X = byte1, Y = 0x00/0xFF (JOYSTICK_GET) */
        sta accu
        stx accu + 1
        lda #1
        cpy #0x00                        /* JOY_PRESENT */
        beq jg_store
        lda #0
    jg_store:
        ldy #0
        sta (present),y
    };
}

// ---------------------------------------------------------------------
// $FF shows without changing the cursor; n selects cursor sprite n.
// The screen size is left unchanged (MOUSE_CONFIG gets X = Y = 0).
// Call MOUSE_CONFIG yourself to resize the mouse field.
// ---------------------------------------------------------------------
void x16_mouse_show(unsigned char cursor) {
    __asm {
        lda cursor
        ldx #0
        ldy #0
        jsr 0xff68                      /* MOUSE_CONFIG */
    }
}

void x16_mouse_hide(void) {
    __asm {
        lda #0
        ldx #0
        ldy #0
        jsr 0xff68                      /* MOUSE_CONFIG */
    }
}

// Returns the button mask: bit 0 left, bit 1 right, bit 2 middle.
unsigned char x16_mouse_get(unsigned int *xp, unsigned int *yp) {
    return __asm {
        ldx #<x16__in_mpos
        jsr 0xff6b         /* A = buttons (MOUSE_GET) */
        sta accu
        ldy #0
        lda x16__in_mpos
        sta (xp),y
        lda x16__in_mpos+2
        sta (yp),y
        iny
        lda x16__in_mpos+1
        sta (xp),y
        lda x16__in_mpos+3
        sta (yp),y
    };
}

// PETSCII code, or 0 if nothing is waiting. Non-blocking.
unsigned char x16_key_get(void) {
    return __asm {
        jsr 0xffe4                      /* GETIN */
        sta accu
    };
}

// Block until a key is pressed.
unsigned char x16_key_wait(void) {
    return __asm {
    kw_loop:
        jsr 0xffe4                      /* GETIN */
        beq kw_loop
        sta accu
    };
}

// key | queued<<8, without consuming the key. KBDBUF_PEEK returns the
// key in A and the queue depth in X.
unsigned int x16_key_peek(void) {
    return __asm {
        jsr 0xfebd                      /* KBDBUF_PEEK */
        sta accu
        stx accu + 1
    };
}
