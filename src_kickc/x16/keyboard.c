// =====================================================================
// x16clib :: x16/keyboard.c -- keyboard buffer, modifiers, keymap
// =====================================================================
// x16_kbd_put() appends to the KERNAL's own 10-key buffer, which is the
// same queue GETIN drains -- so a program can feed itself keystrokes,
// which is how the headless tests drive anything modal.
//
// The keymap name lives in RAM bank 0, so the getter banks in before
// copying and puts the caller's bank back afterwards.
// =====================================================================

#include <x16/keyboard.h>

// Pointer scratch, pinned in zero page (KickC ignores __zp on
// parameters; see x16/zpsafe.h). The cc65 build used ptr1/ptr2.
__address(0x78) char* volatile x16__kb_p0;
__address(0x7a) char* volatile x16__kb_p1;

__mem volatile unsigned char x16__kb_v;

// ---------------------------------------------------------------------
// Scan the keyboard matrix once. The KERNAL's IRQ already does this.
// ---------------------------------------------------------------------
void x16_kbd_scan(void) {
    asm {
        jsr $ff9f /*SCNKEY*/
    }
}

// ---------------------------------------------------------------------
// Append one PETSCII key to the KERNAL's buffer. Silently dropped if
// the buffer is full (10 keys).
// ---------------------------------------------------------------------
void x16_kbd_put(__mem unsigned char key) {
    asm {
        lda key
        jsr $fec3 /*KBDBUF_PUT*/
    }
}

// ---------------------------------------------------------------------
// The live modifier mask: bit 0 shift, 1 Commodore, 2 control,
// 3 Alt, 4 Caps.
// ---------------------------------------------------------------------
unsigned char x16_kbd_get_modifiers(void) {
    asm {
        jsr $fec0 /*KBDBUF_GET_MODIFIERS*/
        sta x16__kb_v
    }
    return x16__kb_v;
}

// ---------------------------------------------------------------------
// Copy the active keymap's name into `name`, which must hold at least
// X16_KEYMAP_LEN bytes. Returns the keymap index.
//
// The name lives in bank 0, so RAM_BANK is switched around the copy and
// put back -- the caller's bank is preserved.
// ---------------------------------------------------------------------
unsigned char x16_kbd_get_keymap(char *name) {
    x16__kb_p0 = name;
    asm {
        sec
        jsr $fed2 /*KEYMAP*/            // A = index, X = name lo, Y = hi
        stx x16__kb_p1
        sty x16__kb_p1+1
        sta x16__kb_v                   // park the index

        lda $00 /*RAM_BANK*/
        pha
        lda #0
        sta $00 /*RAM_BANK*/            // kbdnam lives in bank 0

        ldy #0
    kb_getmap_copy:
        lda (x16__kb_p1),y
        sta (x16__kb_p0),y
        beq kb_getmap_done              // NUL copied
        iny
        cpy #15                         // X16_KEYMAP_LEN - 1
        bne kb_getmap_copy
        lda #0                          // the ROM guarantees a NUL, but
        sta (x16__kb_p0),y              // never trust it past the buffer
    kb_getmap_done:
        pla
        sta $00 /*RAM_BANK*/
    }
    return x16__kb_v;
}

// ---------------------------------------------------------------------
// Switch to the named keymap ("en-us", "de-de", ...). Returns 1 on
// success, 0 if the ROM does not carry that layout.
// ---------------------------------------------------------------------
unsigned char x16_kbd_set_keymap(const char *name) {
    asm {
        lda name
        tax                             // KEYMAP wants X = lo, Y = hi
        lda name+1
        tay
        clc
        jsr $fed2 /*KEYMAP*/            // carry set on unknown layout
        lda #0
        rol                             // carry -> bit 0
        eor #1                          // ...inverted: 1 = success
        sta x16__kb_v
    }
    return x16__kb_v;
}
