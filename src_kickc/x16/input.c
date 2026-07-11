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
__address(0x7c) volatile char x16__in_mpos[4];

// Out-param pointers, pinned in zero page (KickC ignores __zp on
// parameters; see x16/zpsafe.h).
__address(0x78) char* volatile x16__in_p0;
__address(0x7a) char* volatile x16__in_p1;

// Sample every joystick.
void x16_joy_scan(void) {
    asm {
        jsr $ff53 /*JOYSTICK_SCAN*/
    }
}

// ---------------------------------------------------------------------
// JOYSTICK_GET returns byte0 in A, byte1 in X, and presence in Y:
// $00 = present, $FF = absent. The wrapper turns that Y into a clean
// 1/0 through the pointer. Bits are ACTIVE LOW: pressed reads 0.
// ---------------------------------------------------------------------
unsigned int x16_joy_get(__mem unsigned char joy, unsigned char *present) {
    __mem unsigned int r;
    x16__in_p0 = (char*)present;
    asm {
        lda joy
        jsr $ff56 /*JOYSTICK_GET*/      // A = byte0, X = byte1, Y = $00/$FF
        sta r
        stx r+1
        lda #1
        cpy #$00                        // JOY_PRESENT
        beq jg_store
        lda #0
    jg_store:
        ldy #0
        sta (x16__in_p0),y
    }
    return r;
}

// ---------------------------------------------------------------------
// $FF shows without changing the cursor; n selects cursor sprite n.
// The screen size is left unchanged (MOUSE_CONFIG gets X = Y = 0).
// Call MOUSE_CONFIG yourself to resize the mouse field.
// ---------------------------------------------------------------------
void x16_mouse_show(__mem unsigned char cursor) {
    asm {
        lda cursor
        ldx #0
        ldy #0
        jsr $ff68 /*MOUSE_CONFIG*/
    }
}

void x16_mouse_hide(void) {
    asm {
        lda #0
        ldx #0
        ldy #0
        jsr $ff68 /*MOUSE_CONFIG*/
    }
}

// Returns the button mask: bit 0 left, bit 1 right, bit 2 middle.
unsigned char x16_mouse_get(unsigned int *xp, unsigned int *yp) {
    __mem char r;
    x16__in_p0 = (char*)xp;
    x16__in_p1 = (char*)yp;
    asm {
        ldx #<x16__in_mpos
        jsr $ff6b /*MOUSE_GET*/         // A = buttons
        sta r
        ldy #0
        lda x16__in_mpos
        sta (x16__in_p0),y
        lda x16__in_mpos+2
        sta (x16__in_p1),y
        iny
        lda x16__in_mpos+1
        sta (x16__in_p0),y
        lda x16__in_mpos+3
        sta (x16__in_p1),y
    }
    return r;
}

// PETSCII code, or 0 if nothing is waiting. Non-blocking.
unsigned char x16_key_get(void) {
    __mem char r;
    asm {
        jsr $ffe4 /*GETIN*/
        sta r
    }
    return r;
}

// Block until a key is pressed.
unsigned char x16_key_wait(void) {
    __mem char r;
    asm {
    kw_loop:
        jsr $ffe4 /*GETIN*/
        beq kw_loop
        sta r
    }
    return r;
}

// key | queued<<8, without consuming the key. KBDBUF_PEEK returns the
// key in A and the queue depth in X.
unsigned int x16_key_peek(void) {
    __mem unsigned int r;
    asm {
        jsr $febd /*KBDBUF_PEEK*/
        sta r
        stx r+1
    }
    return r;
}
