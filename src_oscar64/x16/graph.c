// =====================================================================
// x16clib :: x16/graph.c -- the KERNAL GRAPH API
// =====================================================================
// Lines, rectangles, ovals, images and text, in whatever mode the
// active framebuffer driver provides. CALL x16_graph_init() FIRST: it
// installs the driver the whole GRAPH and FB API dispatches through.
//
// x16_graph_init(NULL) switches the display to the ROM's 320x240@8bpp
// bitmap at VRAM $00000 and points GRAPH at it.
//
// Coordinates are signed 16-bit and clipped to the window.
// =====================================================================

#include <x16/graph.h>

// No pointer relay is needed here: Oscar64's inline asm
// indirects a pointer PARAMETER directly and places it where (zp),y can
// reach it, the way screen.c does.

volatile unsigned char x16__gr_v;
volatile unsigned char x16__gr_w;
volatile unsigned char x16__gr_h;

// ---------------------------------------------------------------------
// Install a driver, or the ROM's 320x240@8bpp one for NULL.
// ---------------------------------------------------------------------
void x16_graph_init(const void *driver) {
    __asm {
        lda driver
        sta 0x02                        // r0L
        lda driver+1
        sta 0x03                        // r0H
        jsr 0xff20                      // GRAPH_INIT
    }
}

// ---------------------------------------------------------------------
// Clear the whole window to the background colour.
// ---------------------------------------------------------------------
void x16_graph_clear(void) {
    __asm {
        jsr 0xff23                      // GRAPH_CLEAR
    }
}

// ---------------------------------------------------------------------
// Clip everything after this to the given rectangle. A width or height
// of 0 restores the full screen.
// ---------------------------------------------------------------------
void x16_graph_set_window(unsigned int px,
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
        jsr 0xff26                      // GRAPH_SET_WINDOW
    }
}

// ---------------------------------------------------------------------
// Stroke, fill and background colours, as palette indices.
// ---------------------------------------------------------------------
void x16_graph_set_colors(unsigned char stroke,
                          unsigned char fill,
                          unsigned char background) {
    __asm {
        lda stroke
        ldx fill
        ldy background
        jsr 0xff29                      // GRAPH_SET_COLORS
    }
}

// ---------------------------------------------------------------------
// A line in the stroke colour.
// ---------------------------------------------------------------------
void x16_graph_draw_line(unsigned int x1,
                         unsigned int y1,
                         unsigned int x2,
                         unsigned int y2) {
    __asm {
        lda x1
        sta 0x02                        // r0L
        lda x1+1
        sta 0x03                        // r0H
        lda y1
        sta 0x04                        // r1L
        lda y1+1
        sta 0x05                        // r1H
        lda x2
        sta 0x06                        // r2L
        lda x2+1
        sta 0x07                        // r2H
        lda y2
        sta 0x08                        // r3L
        lda y2+1
        sta 0x09                        // r3H
        jsr 0xff2c                      // GRAPH_DRAW_LINE
    }
}

// ---------------------------------------------------------------------
// A rectangle, optionally with rounded corners and optionally filled.
// ---------------------------------------------------------------------
void x16_graph_draw_rect(unsigned int px,
                         unsigned int py,
                         unsigned int width,
                         unsigned int height,
                         unsigned int radius,
                         unsigned char fill) {
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
        lda radius
        sta 0x0a                        // r4L
        lda radius+1
        sta 0x0b                        // r4H
        lda fill
        cmp #1                          // carry set iff fill != 0
        jsr 0xff2f                      // GRAPH_DRAW_RECT
    }
}

// ---------------------------------------------------------------------
// Copy a rectangle. Moving DOWN copies height+1 rows -- a ROM quirk,
// not a rounding here.
// ---------------------------------------------------------------------
void x16_graph_move_rect(unsigned int sx,
                         unsigned int sy,
                         unsigned int tx,
                         unsigned int ty,
                         unsigned int width,
                         unsigned int height) {
    __asm {
        lda sx
        sta 0x02                        // r0L
        lda sx+1
        sta 0x03                        // r0H
        lda sy
        sta 0x04                        // r1L
        lda sy+1
        sta 0x05                        // r1H
        lda tx
        sta 0x06                        // r2L
        lda tx+1
        sta 0x07                        // r2H
        lda ty
        sta 0x08                        // r3L
        lda ty+1
        sta 0x09                        // r3H
        lda width
        sta 0x0a                        // r4L
        lda width+1
        sta 0x0b                        // r4H
        lda height
        sta 0x0c                        // r5L
        lda height+1
        sta 0x0d                        // r5H
        jsr 0xff32                      // GRAPH_MOVE_RECT
    }
}

// ---------------------------------------------------------------------
// An ellipse inscribed in the rectangle, optionally filled.
// ---------------------------------------------------------------------
void x16_graph_draw_oval(unsigned int px,
                         unsigned int py,
                         unsigned int width,
                         unsigned int height,
                         unsigned char fill) {
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
        lda fill
        cmp #1                          // carry set iff fill != 0
        jsr 0xff35                      // GRAPH_DRAW_OVAL
    }
}

// ---------------------------------------------------------------------
// Blit an image: one byte per pixel, row-major, no header.
// ---------------------------------------------------------------------
void x16_graph_draw_image(unsigned int px,
                          unsigned int py,
                          const unsigned char *image,
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
        lda image
        sta 0x06                        // r2L
        lda image+1
        sta 0x07                        // r2H
        lda width
        sta 0x08                        // r3L
        lda width+1
        sta 0x09                        // r3H
        lda height
        sta 0x0a                        // r4L
        lda height+1
        sta 0x0b                        // r4H
        jsr 0xff38                      // GRAPH_DRAW_IMAGE
    }
}

// ---------------------------------------------------------------------
// Install a font, or the ROM's own for NULL.
// ---------------------------------------------------------------------
void x16_graph_set_font(const void *font) {
    __asm {
        lda font
        sta 0x02                        // r0L
        lda font+1
        sta 0x03                        // r0H
        jsr 0xff3b                      // GRAPH_SET_FONT
    }
}

// ---------------------------------------------------------------------
// Measure one character in the active font. Returns 1 if it is
// printable, 0 for a control code (in which case *out is untouched).
// ---------------------------------------------------------------------
unsigned char x16_graph_get_char_size(unsigned char c,
                                      unsigned char style,
                                      x16_char_size *out) {
    __asm {
        lda c
        ldx style
        jsr 0xff3e   // A/X/Y = baseline/width/height, (GRAPH_GET_CHAR_SIZE)
        bcs gr_size_control                 // carry set = a control code
        stx x16__gr_w                   // Y is about to become the store
        sty x16__gr_h                   // index, so park both first
        ldy #0
        sta (out),y              // baseline
        iny
        lda x16__gr_w
        sta (out),y              // width
        iny
        lda x16__gr_h
        sta (out),y              // height
        lda #1
        sta x16__gr_v
        jmp gr_size_done
    gr_size_control:
        txa                             // the style the code selects
        ldy #3
        sta (out),y
        lda #0
        sta x16__gr_v
    gr_size_done:
    }
    return x16__gr_v;
}

// ---------------------------------------------------------------------
// Draw one character at (*px, *py) and advance them past it. Returns 1 if
// it was drawn, 0 if it was clipped away.
// ---------------------------------------------------------------------
unsigned char x16_graph_put_char(unsigned int *px,
                                 unsigned int *py,
                                 unsigned char c) {
    __asm {
        ldy #0                          // the KERNAL takes the CURRENT pen
        lda (px),y              // position in r0/r1 and advances it
        sta 0x02                        // r0L
        lda (py),y
        sta 0x04                        // r1L
        iny
        lda (px),y
        sta 0x03                        // r0H
        lda (py),y
        sta 0x05                        // r1H

        lda c
        jsr 0xff41    // r0/r1 advance, carry = clipped (GRAPH_PUT_CHAR)

        lda #0                          // read the carry out before the
        rol                             // stores below can disturb it
        eor #1                          // ...inverted: 1 = drawn
        sta x16__gr_v

        ldy #0
        lda 0x02                        // r0L
        sta (px),y
        lda 0x04                        // r1L
        sta (py),y
        iny
        lda 0x03                        // r0H
        sta (px),y
        lda 0x05                        // r1H
        sta (py),y
    }
    return x16__gr_v;
}
