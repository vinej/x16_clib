// =====================================================================
// x16clib :: x16/bitmap.c -- 320x240x256 bitmap drawing
// =====================================================================
// The framebuffer is 8bpp at VRAM $00000, one byte per pixel, rows of
// 320. A pixel is at y*320 + x.
//
// x16_gfx_pset clips. The line/rect primitives do NOT: they assume
// their arguments are on screen.
//
// HOW THIS PORT IS SPLIT. The hot per-pixel paths -- the y*320+x port
// pointer (setptr), the clipped pixel store, the flood fill's VRAM
// scans -- are the same hand-written 6502 as src_ca65/gfx/bitmap.s,
// operating on a module operand block. The ORCHESTRATION around them
// (Bresenham's error steps, the circle's octant walk, the glyph loops,
// the flood's span stack) is C here, mirroring the ca65 control flow
// statement for statement: inline asm cannot jsr another function, and
// those layers spend their time inside the asm they drive.
// =====================================================================

#include <x16/bitmap.h>
#include <x16/vera.h>
#include <x16/screen.h>

// The operand block (the ca65 build's X16_P0..P3).
volatile unsigned int x16__gp_x;
volatile char x16__gp_y;
volatile char x16__gp_c;
volatile char x16__gp_off;        // pset's clip verdict

volatile char x16__gp_t0;         // setptr's 17-bit accumulator
volatile char x16__gp_t1;
volatile char x16__gp_t2;

// ---------------------------------------------------------------------
// Internal: point data port 0 at pixel (gp_x, gp_y) with increment
// index `incr`. y*320 = (y<<8) + (y<<6), so no multiply is needed;
// the result is 17-bit. Stepping by X16_INC_320 (14) then walks
// straight down a column.
// ---------------------------------------------------------------------
void x16__gfx_setptr(unsigned char incr) {
    __asm {
        lda incr
        asl
        asl
        asl
        asl
        sta x16__gp_t2                  /* increment field, pre-shifted */
                                        /* (parked here until ADDR_H) */
        lda x16__gp_y                   /* y << 6 */
        ldx #0
        stx x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        sta x16__gp_t0                  /* t1:t0 = y*64 */

        clc                             /* + y<<8, whose low byte is zero */
        lda x16__gp_y
        adc x16__gp_t1
        sta x16__gp_t1
        lda #0
        adc #0
        pha                             /* bit 16 of y*320 */

        clc                             /* + x */
        lda x16__gp_t0
        adc x16__gp_x
        sta x16__gp_t0
        lda x16__gp_t1
        adc x16__gp_x+1
        sta x16__gp_t1
        pla
        adc #0

        ldx x16__gp_t2                  /* recover the increment field */
        and #0x01                       /* VERA_ADDR_H_BANK */
        sta x16__gp_t2
        txa
        ora x16__gp_t2
        tax

        lda 0x9f25  /*VERA_CTRL*/
        and #0xfe
        sta 0x9f25
        lda x16__gp_t0
        sta 0x9f20                      /* VERA_ADDR_L */
        lda x16__gp_t1
        sta 0x9f21                      /* VERA_ADDR_M */
        stx 0x9f22                      /* VERA_ADDR_H */
    }
}

// Internal: set pixel (gp_x, gp_y) to gp_c, clipped to 320x240.
void x16__gfx_pset_i(void) {
    __asm {
        lda x16__gp_y
        cmp #240                        /* GFX_HEIGHT */
        bcs gpp_off                     /* y >= 240 */

        lda x16__gp_x+1                 /* x high byte */
        beq gpp_on                      /* x < 256, always on screen */
        cmp #1
        bne gpp_off                     /* x >= 512 */
        lda x16__gp_x
        cmp #0x40                        /* 320 = 0x140: low byte must be < 0x40 */
        bcs gpp_off
    gpp_on:
        lda #0
        sta x16__gp_off
        jmp gpp_done
    gpp_off:
        lda #1
        sta x16__gp_off
    gpp_done:
    }
    if (x16__gp_off) {
        return;
    }
    x16__gfx_setptr(0);                 // X16_INC_0
    __asm {
        lda x16__gp_c
        sta 0x9f23                      /* VERA_DATA0 */
    }
}

// Internal: hline from the operand block, `len` pixels.
void x16__gfx_hline_i(unsigned int len) {
    x16__gfx_setptr(1);                 // X16_INC_1
    x16_vera_fill(x16__gp_c, len);
}

// Internal: read one pixel from the 8bpp plane (no clip). The shared
// shape module (shapes.c) reads through this for its flood fill.
unsigned char x16__gfx_read8(unsigned int x, unsigned char y) {
    x16__gp_x = x;
    x16__gp_y = y;
    x16__gfx_setptr(0);                 // X16_INC_0
    return __asm {
        lda 0x9f23                      /* VERA_DATA0 */
        sta accu
    };
}

unsigned char x16_gfx_init(void) {
    return x16_screen_set_mode(0x80);   // X16_MODE_320x240
}

// ---------------------------------------------------------------------
// 320*240 = 76800 = $12C00 bytes does not fit vera_fill's 16-bit count:
// pass it naively and it truncates to $2C00, the top 35 rows. Hence two
// halves; port 0 keeps auto-incrementing between the calls. GFX_CLEAR
// regression-tests the LAST pixel rather than the first.
// ---------------------------------------------------------------------
void x16_gfx_clear(unsigned char color) {
    x16_vera_addr0(X16_INC_1, 0x00000); // X16_VRAM_BITMAP
    x16_vera_fill(color, 38400);
    x16_vera_fill(color, 38400);
}

void x16_gfx_pset(unsigned int x, unsigned char y, unsigned char color) {
    x16__gp_x = x;
    x16__gp_y = y;
    x16__gp_c = color;
    x16__gfx_pset_i();
}

void x16_gfx_hline(unsigned int x, unsigned char y, unsigned int len,
                   unsigned char color) {
    x16__gp_x = x;
    x16__gp_y = y;
    x16__gp_c = color;
    x16__gfx_hline_i(len);
}

// len is 1-255: a column of a 240-row screen never needs more. The
// hardware's odd X16_INC_320 makes a vertical line the same tight fill
// loop as a horizontal one.
void x16_gfx_vline(unsigned int x, unsigned char y, unsigned char len,
                   unsigned char color) {
    x16__gp_x = x;
    x16__gp_y = y;
    x16__gp_c = color;
    x16__gfx_setptr(14);                // X16_INC_320
    x16_vera_fill(color, len);
}

// Filled rectangle: a row of hlines.
void x16_gfx_rect(unsigned int x, unsigned char y, unsigned int w,
                  unsigned char h, unsigned char color) {
    unsigned char i;

    x16__gp_x = x;
    x16__gp_c = color;
    for (i = 0; i < h; i++) {
        x16__gp_y = y + i;
        x16__gfx_hline_i(w);
    }
}

// Rectangle outline.
void x16_gfx_frame(unsigned int x, unsigned char y, unsigned int w,
                   unsigned char h, unsigned char color) {
    x16_gfx_hline(x, y, w, color);
    x16_gfx_hline(x, y + h - 1, w, color);
    x16_gfx_vline(x, y, h, color);
    x16_gfx_vline(x + w - 1, y, h, color);
}

// ---------------------------------------------------------------------
// Bresenham, any direction -- the same signed-16-bit error walk as the
// ca65 build (dx = |x1-x0|, dy = -|y1-y0|, err = dx+dy), with each
// pixel going through the clipped pset.
// ---------------------------------------------------------------------
void x16_gfx_line(unsigned int x0, unsigned char y0, unsigned int x1,
                  unsigned char y1, unsigned char color) {
    int lx0;
    int ly0;
    int lx1;
    int ly1;
    int dx;
    int dy;
    int sx;
    int sy;
    int err;
    int e2;

    lx0 = (int)x0;
    ly0 = (int)y0;
    lx1 = (int)x1;
    ly1 = (int)y1;

    dx = lx1 - lx0;
    if (dx < 0) {
        dx = 0 - dx;
        sx = -1;
    } else {
        sx = 1;
    }
    dy = ly1 - ly0;
    if (dy < 0) {
        sy = -1;
    } else {
        dy = 0 - dy;                    // dy = -|dy|
        sy = 1;
    }
    err = dx + dy;

    x16__gp_c = color;
    for (;;) {
        x16__gp_x = (unsigned int)lx0;
        x16__gp_y = (unsigned char)ly0;
        x16__gfx_pset_i();

        if (lx0 == lx1 && ly0 == ly1) {
            break;
        }
        e2 = err << 1;
        if (e2 >= dy) {
            err = err + dy;
            lx0 = lx0 + sx;
        }
        if (e2 <= dx) {
            err = err + dx;
            ly0 = ly0 + sy;
        }
    }
}

// circle / disc / flood now live in the shared shapes.c

/* --- text ----------------------------------------------------------- */

volatile char x16__gt_glyph[8];

// Internal: fetch screen code `code`'s 8-byte 1bpp glyph from the
// charset the KERNAL keeps at VRAM $1F000, through port 1 so port 0's
// drawing address survives.
void x16__gfx_glyph(unsigned char code) {
    __asm {
        lda code                        /* glyph address = 0x1F000 + code*8 */
        ldx #0
        stx x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1                  /* t1:A = code * 8 */

        tax
        lda 0x9f25  /*VERA_CTRL*/         /* port 1 */
        ora #0x01
        sta 0x9f25
        stx 0x9f20                      /* VERA_ADDR_L */
        lda x16__gp_t1
        clc
        adc #0xf0                        /* >(0x1F000 & 0xFFFF) */
        sta 0x9f21                      /* VERA_ADDR_M */
        lda #0x11                        /* BANK | (INC_1 << 4): bank 1 */
        sta 0x9f22                      /* VERA_ADDR_H */
        ldx #0
    gg_fetch:
        lda 0x9f24                      /* VERA_DATA1 */
        sta x16__gt_glyph,x
        inx
        cpx #8
        bne gg_fetch
        lda 0x9f25  /*VERA_CTRL*/         /* ADDRSEL back to 0 */
        and #0xfe
        sta 0x9f25
    }
}

// ---------------------------------------------------------------------
// Draw one glyph into the bitmap. `code` is a SCREEN code, not PETSCII.
// Set bits become colour pixels through the clipped pset (so text
// clips); clear bits stay transparent.
// ---------------------------------------------------------------------
void x16_gfx_char(unsigned int x, unsigned char y, unsigned char color,
                  unsigned char code) {
    unsigned int row;                   /* int: the KickC port found a
                                        ** char here miscompiled, and the
                                        ** wider type costs nothing */
    unsigned char col;
    unsigned char bits;
    unsigned int ry;

    x16__gfx_glyph(code);
    x16__gp_c = color;

    for (row = 0; row < 8; row++) {
        bits = x16__gt_glyph[row];
        ry = (unsigned int)y;
        ry = ry + row;
        // blank rows plot nothing; rows past 255 are off screen
        if (bits != 0 && ry < 256) {
            x16__gp_y = (unsigned char)ry;
            for (col = 0; col < 8; col++) {
                if (bits & 0x80) {      // leftmost pixel first
                    x16__gp_x = x;
                    x16__gp_x = x16__gp_x + col;
                    x16__gfx_pset_i();
                }
                bits = bits << 1;
            }
        }
    }
}

// ---------------------------------------------------------------------
// A NUL-terminated string, 8 pixels per character. ASCII letters are
// converted to screen codes ('A'-'Z' work as expected).
// ---------------------------------------------------------------------
void x16_gfx_text(unsigned int x, unsigned char y, unsigned char color,
                  const char *s) {
    unsigned char c;

    while (*s) {
        c = (unsigned char)*s;
        // ASCII -> screen code: bit 6 set means the letters/@ block
        if (c & 0x40) {
            c = c & 0x1F;
        }
        x16_gfx_char(x, y, color, c);
        x = x + 8;                      // advance the pen
        s++;
    }
}
