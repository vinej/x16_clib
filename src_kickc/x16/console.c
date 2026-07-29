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

// ---------------------------------------------------------------------
// Set the window and start the console. All four are pixel-free text
// cells. A width or height of 0 means "to the edge of the screen".
// ---------------------------------------------------------------------
void x16_con_init(__mem unsigned int px,
                  __mem unsigned int py,
                  __mem unsigned int width,
                  __mem unsigned int height) {
    asm {
        lda px
        sta $02 /*r0L*/
        lda px+1
        sta $03 /*r0H*/
        lda py
        sta $04 /*r1L*/
        lda py+1
        sta $05 /*r1H*/
        lda width
        sta $06 /*r2L*/
        lda width+1
        sta $07 /*r2H*/
        lda height
        sta $08 /*r3L*/
        lda height+1
        sta $09 /*r3H*/
        jsr $fedb /*CONSOLE_INIT*/
    }
}

// ---------------------------------------------------------------------
// Put one character. wrap != 0 lets it break the line at a word
// boundary; 0 writes it literally.
// ---------------------------------------------------------------------
void x16_con_put_char(__mem unsigned char c,
                      __mem unsigned char wrap) {
    asm {
        lda wrap
        cmp #1                          // carry set iff wrap != 0
        lda c                           // lda leaves the carry alone
        jsr $fede /*CONSOLE_PUT_CHAR*/
    }
}

// ---------------------------------------------------------------------
// Read one line of input, blocking. Returns the PETSCII character.
// ---------------------------------------------------------------------
unsigned char x16_con_get_char(void) {
    __mem unsigned char c;
    asm {
        jsr $fee1 /*CONSOLE_GET_CHAR*/
        sta c
    }
    return c;
}

// ---------------------------------------------------------------------
// The message shown at the foot of a full page. NULL restores the
// KERNAL's own.
// ---------------------------------------------------------------------
void x16_con_set_paging_message(const char *msg) {
    asm {
        lda msg
        sta $02 /*r0L*/
        lda msg+1
        sta $03 /*r0H*/
        jsr $fed5 /*CONSOLE_SET_PAGING_MESSAGE*/
    }
}

// ---------------------------------------------------------------------
// Turn paging off: a null message pointer means "never pause".
// ---------------------------------------------------------------------
void x16_con_disable_paging(void) {
    asm {
        lda #0
        sta $02 /*r0L*/
        sta $03 /*r0H*/
        jsr $fed5 /*CONSOLE_SET_PAGING_MESSAGE*/
    }
}

// ---------------------------------------------------------------------
// Inline a GRAPH_draw_image-format bitmap at the cursor, like an
// oversized character. Fails the line if it cannot fit.
// ---------------------------------------------------------------------
void x16_con_put_image(const unsigned char *image,
                       __mem unsigned int width,
                       __mem unsigned int height) {
    asm {
        lda image
        sta $02 /*r0L*/
        lda image+1
        sta $03 /*r0H*/
        lda width
        sta $04 /*r1L*/
        lda width+1
        sta $05 /*r1H*/
        lda height
        sta $06 /*r2L*/
        lda height+1
        sta $07 /*r2H*/
        jsr $fed8 /*CONSOLE_PUT_IMAGE*/
    }
}
