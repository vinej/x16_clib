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

// Pointer scratch, pinned in zero page (KickC ignores __zp on
// parameters; see x16/zpsafe.h).
__address(0x78) unsigned char* volatile x16__gr_p0;
__address(0x7a) unsigned int* volatile x16__gr_p1;
__address(0x7c) unsigned int* volatile x16__gr_p2;

__mem volatile unsigned char x16__gr_v;
__mem volatile unsigned char x16__gr_w;
__mem volatile unsigned char x16__gr_h;

// ---------------------------------------------------------------------
// Install a driver, or the ROM's 320x240@8bpp one for NULL.
// ---------------------------------------------------------------------
void x16_graph_init(const void *driver) {
    asm {
        lda driver
        sta $02 /*r0L*/
        lda driver+1
        sta $03 /*r0H*/
        jsr $ff20 /*GRAPH_INIT*/
    }
}

// ---------------------------------------------------------------------
// Clear the whole window to the background colour.
// ---------------------------------------------------------------------
void x16_graph_clear(void) {
    asm {
        jsr $ff23 /*GRAPH_CLEAR*/
    }
}

// ---------------------------------------------------------------------
// Clip everything after this to the given rectangle. A width or height
// of 0 restores the full screen.
// ---------------------------------------------------------------------
void x16_graph_set_window(__mem unsigned int px,
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
        jsr $ff26 /*GRAPH_SET_WINDOW*/
    }
}

// ---------------------------------------------------------------------
// Stroke, fill and background colours, as palette indices.
// ---------------------------------------------------------------------
void x16_graph_set_colors(__mem unsigned char stroke,
                          __mem unsigned char fill,
                          __mem unsigned char background) {
    asm {
        lda stroke
        ldx fill
        ldy background
        jsr $ff29 /*GRAPH_SET_COLORS*/
    }
}

// ---------------------------------------------------------------------
// A line in the stroke colour.
// ---------------------------------------------------------------------
void x16_graph_draw_line(__mem unsigned int x1,
                         __mem unsigned int y1,
                         __mem unsigned int x2,
                         __mem unsigned int y2) {
    asm {
        lda x1
        sta $02 /*r0L*/
        lda x1+1
        sta $03 /*r0H*/
        lda y1
        sta $04 /*r1L*/
        lda y1+1
        sta $05 /*r1H*/
        lda x2
        sta $06 /*r2L*/
        lda x2+1
        sta $07 /*r2H*/
        lda y2
        sta $08 /*r3L*/
        lda y2+1
        sta $09 /*r3H*/
        jsr $ff2c /*GRAPH_DRAW_LINE*/
    }
}

// ---------------------------------------------------------------------
// A rectangle, optionally with rounded corners and optionally filled.
// ---------------------------------------------------------------------
void x16_graph_draw_rect(__mem unsigned int px,
                         __mem unsigned int py,
                         __mem unsigned int width,
                         __mem unsigned int height,
                         __mem unsigned int radius,
                         __mem unsigned char fill) {
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
        lda radius
        sta $0a /*r4L*/
        lda radius+1
        sta $0b /*r4H*/
        lda fill
        cmp #1                          // carry set iff fill != 0
        jsr $ff2f /*GRAPH_DRAW_RECT*/
    }
}

// ---------------------------------------------------------------------
// Copy a rectangle. Moving DOWN copies height+1 rows -- a ROM quirk,
// not a rounding here.
// ---------------------------------------------------------------------
void x16_graph_move_rect(__mem unsigned int sx,
                         __mem unsigned int sy,
                         __mem unsigned int tx,
                         __mem unsigned int ty,
                         __mem unsigned int width,
                         __mem unsigned int height) {
    asm {
        lda sx
        sta $02 /*r0L*/
        lda sx+1
        sta $03 /*r0H*/
        lda sy
        sta $04 /*r1L*/
        lda sy+1
        sta $05 /*r1H*/
        lda tx
        sta $06 /*r2L*/
        lda tx+1
        sta $07 /*r2H*/
        lda ty
        sta $08 /*r3L*/
        lda ty+1
        sta $09 /*r3H*/
        lda width
        sta $0a /*r4L*/
        lda width+1
        sta $0b /*r4H*/
        lda height
        sta $0c /*r5L*/
        lda height+1
        sta $0d /*r5H*/
        jsr $ff32 /*GRAPH_MOVE_RECT*/
    }
}

// ---------------------------------------------------------------------
// An ellipse inscribed in the rectangle, optionally filled.
// ---------------------------------------------------------------------
void x16_graph_draw_oval(__mem unsigned int px,
                         __mem unsigned int py,
                         __mem unsigned int width,
                         __mem unsigned int height,
                         __mem unsigned char fill) {
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
        lda fill
        cmp #1                          // carry set iff fill != 0
        jsr $ff35 /*GRAPH_DRAW_OVAL*/
    }
}

// ---------------------------------------------------------------------
// Blit an image: one byte per pixel, row-major, no header.
// ---------------------------------------------------------------------
void x16_graph_draw_image(__mem unsigned int px,
                          __mem unsigned int py,
                          const unsigned char *image,
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
        lda image
        sta $06 /*r2L*/
        lda image+1
        sta $07 /*r2H*/
        lda width
        sta $08 /*r3L*/
        lda width+1
        sta $09 /*r3H*/
        lda height
        sta $0a /*r4L*/
        lda height+1
        sta $0b /*r4H*/
        jsr $ff38 /*GRAPH_DRAW_IMAGE*/
    }
}

// ---------------------------------------------------------------------
// Install a font, or the ROM's own for NULL.
// ---------------------------------------------------------------------
void x16_graph_set_font(const void *font) {
    asm {
        lda font
        sta $02 /*r0L*/
        lda font+1
        sta $03 /*r0H*/
        jsr $ff3b /*GRAPH_SET_FONT*/
    }
}

// ---------------------------------------------------------------------
// Measure one character in the active font. Returns 1 if it is
// printable, 0 for a control code (in which case *out is untouched).
// ---------------------------------------------------------------------
unsigned char x16_graph_get_char_size(__mem unsigned char c,
                                      __mem unsigned char style,
                                      x16_char_size *out) {
    x16__gr_p0 = (unsigned char*)out;
    asm {
        lda c
        ldx style
        jsr $ff3e /*GRAPH_GET_CHAR_SIZE*/   // A/X/Y = baseline/width/height,
        bcs gr_size_control                 // carry set = a control code
        stx x16__gr_w                   // Y is about to become the store
        sty x16__gr_h                   // index, so park both first
        ldy #0
        sta (x16__gr_p0),y              // baseline
        iny
        lda x16__gr_w
        sta (x16__gr_p0),y              // width
        iny
        lda x16__gr_h
        sta (x16__gr_p0),y              // height
        lda #1
        sta x16__gr_v
        jmp gr_size_done
    gr_size_control:
        txa                             // the style the code selects
        ldy #3
        sta (x16__gr_p0),y
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
                                 __mem unsigned char c) {
    x16__gr_p1 = px;
    x16__gr_p2 = py;
    asm {
        ldy #0                          // the KERNAL takes the CURRENT pen
        lda (x16__gr_p1),y              // position in r0/r1 and advances it
        sta $02 /*r0L*/
        lda (x16__gr_p2),y
        sta $04 /*r1L*/
        iny
        lda (x16__gr_p1),y
        sta $03 /*r0H*/
        lda (x16__gr_p2),y
        sta $05 /*r1H*/

        lda c
        jsr $ff41 /*GRAPH_PUT_CHAR*/    // r0/r1 advance, carry = clipped

        lda #0                          // read the carry out before the
        rol                             // stores below can disturb it
        eor #1                          // ...inverted: 1 = drawn
        sta x16__gr_v

        ldy #0
        lda $02 /*r0L*/
        sta (x16__gr_p1),y
        lda $04 /*r1L*/
        sta (x16__gr_p2),y
        iny
        lda $03 /*r0H*/
        sta (x16__gr_p1),y
        lda $05 /*r1H*/
        sta (x16__gr_p2),y
    }
    return x16__gr_v;
}
