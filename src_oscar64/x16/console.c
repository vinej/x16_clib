// =====================================================================
// x16clib :: x16/console.c -- the KERNAL console
// =====================================================================
// A word-wrapping, paging text window layered over the screen editor.
// CALL x16_con_init() FIRST: the other entries dispatch through vectors
// it installs.
//
// The console writes through CHROUT, so it obeys the current text
// colour and charset, and it does NOT understand the bitmap modes.
// =====================================================================

#include <x16/console.h>

// Oscar64 loses an asm write to a C local, even a volatile one, so
// anything the assembly stores to lives at module scope.
volatile unsigned char c;

// ---------------------------------------------------------------------
// Set the window and start the console. All four are pixel-free text
// cells. A width or height of 0 means "to the edge of the screen".
// ---------------------------------------------------------------------
void x16_con_init(unsigned int px,
                  unsigned int py,
                  unsigned int width,
                  unsigned int height) {
    __asm {
        lda px
        sta 0x02                        // r0L
        lda px+1
        sta 0x03                        // r0H
        lda py
        sta 0x04                        // r1L
        lda py+1
        sta 0x05                        // r1H
        lda width
        sta 0x06                        // r2L
        lda width+1
        sta 0x07                        // r2H
        lda height
        sta 0x08                        // r3L
        lda height+1
        sta 0x09                        // r3H
        jsr 0xfedb                      // CONSOLE_INIT
    }
}

// ---------------------------------------------------------------------
// Put one character. wrap != 0 lets it break the line at a word
// boundary; 0 writes it literally.
// ---------------------------------------------------------------------
void x16_con_put_char(unsigned char c,
                      unsigned char wrap) {
    __asm {
        lda wrap
        cmp #1                          // carry set iff wrap != 0
        lda c                           // lda leaves the carry alone
        jsr 0xfede                      // CONSOLE_PUT_CHAR
    }
}

// ---------------------------------------------------------------------
// Read one line of input, blocking. Returns the PETSCII character.
// ---------------------------------------------------------------------
unsigned char x16_con_get_char(void) {    __asm {
        jsr 0xfee1                      // CONSOLE_GET_CHAR
        sta c
    }
    return c;
}

// ---------------------------------------------------------------------
// The message shown at the foot of a full page. NULL restores the
// KERNAL's own.
// ---------------------------------------------------------------------
void x16_con_set_paging_message(const char *msg) {
    __asm {
        lda msg
        sta 0x02                        // r0L
        lda msg+1
        sta 0x03                        // r0H
        jsr 0xfed5                      // CONSOLE_SET_PAGING_MESSAGE
    }
}

// ---------------------------------------------------------------------
// Turn paging off: a null message pointer means "never pause".
// ---------------------------------------------------------------------
void x16_con_disable_paging(void) {
    __asm {
        lda #0
        sta 0x02                        // r0L
        sta 0x03                        // r0H
        jsr 0xfed5                      // CONSOLE_SET_PAGING_MESSAGE
    }
}

// ---------------------------------------------------------------------
// Inline a GRAPH_draw_image-format bitmap at the cursor, like an
// oversized character. Fails the line if it cannot fit.
// ---------------------------------------------------------------------
void x16_con_put_image(const unsigned char *image,
                       unsigned int width,
                       unsigned int height) {
    __asm {
        lda image
        sta 0x02                        // r0L
        lda image+1
        sta 0x03                        // r0H
        lda width
        sta 0x04                        // r1L
        lda width+1
        sta 0x05                        // r1H
        lda height
        sta 0x06                        // r2L
        lda height+1
        sta 0x07                        // r2H
        jsr 0xfed8                      // CONSOLE_PUT_IMAGE
    }
}
