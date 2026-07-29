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


volatile unsigned char x16__kb_v;
volatile unsigned char x16__kb_lo;
volatile unsigned char x16__kb_hi;
volatile unsigned char x16__kb_bank;

// ---------------------------------------------------------------------
// Scan the keyboard matrix once. The KERNAL's IRQ already does this.
// ---------------------------------------------------------------------
void x16_kbd_scan(void) {
    __asm {
        jsr 0xff9f                      // SCNKEY
    }
}

// ---------------------------------------------------------------------
// Append one PETSCII key to the KERNAL's buffer. Silently dropped if
// the buffer is full (10 keys).
// ---------------------------------------------------------------------
void x16_kbd_put(unsigned char key) {
    __asm {
        lda key
        jsr 0xfec3                      // KBDBUF_PUT
    }
}

// ---------------------------------------------------------------------
// The live modifier mask: bit 0 shift, 1 Commodore, 2 control,
// 3 Alt, 4 Caps.
// ---------------------------------------------------------------------
unsigned char x16_kbd_get_modifiers(void) {
    __asm {
        jsr 0xfec0                      // KBDBUF_GET_MODIFIERS
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
    const char *src;
    unsigned char i;
    unsigned char bank;

    __asm {
        sec
        jsr 0xfed2                      /* KEYMAP: A = index, X/Y = name */
        stx x16__kb_lo
        sty x16__kb_hi
        sta x16__kb_v
        lda 0x00                        /* RAM_BANK */
        sta x16__kb_bank
        lda #0
        sta 0x00                        /* kbdnam lives in bank 0 */
    }

    /* The copy is C here, not asm. Oscar64 needs a pointer that inline
    ** asm indirects to be __zeropage, and its user region is only
    ** $F7-$FF -- input.c, load.c and pcm.c already hold eight of those
    ** nine bytes. Rebuilding the pointer in C costs nothing and asks
    ** for none. */
    src = (const char *)((unsigned int)x16__kb_lo
                         | ((unsigned int)x16__kb_hi << 8));
    for (i = 0; i < 15; ++i) {
        name[i] = src[i];
        if (src[i] == 0) {
            break;
        }
    }
    name[15] = 0;                       /* the ROM guarantees a NUL, but
                                        ** never trust it past the end */
    bank = x16__kb_bank;
    __asm {
        lda bank
        sta 0x00                        /* RAM_BANK back as it was */
    }
    return x16__kb_v;
}

// ---------------------------------------------------------------------
// Switch to the named keymap ("en-us", "de-de", ...). Returns 1 on
// success, 0 if the ROM does not carry that layout.
// ---------------------------------------------------------------------
unsigned char x16_kbd_set_keymap(const char *name) {
    __asm {
        lda name
        tax                             // KEYMAP wants X = lo, Y = hi
        lda name+1
        tay
        clc
        jsr 0xfed2            // carry set on unknown layout (KEYMAP)
        lda #0
        rol                             // carry -> bit 0
        eor #1                          // ...inverted: 1 = success
        sta x16__kb_v
    }
    return x16__kb_v;
}
