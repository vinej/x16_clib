/* =====================================================================
 * x16clib :: x16/bitmap.h -- 320x240x256 bitmap drawing
 * =====================================================================
 * The framebuffer is 8bpp at VRAM $00000, one byte per pixel, rows of
 * 320. A pixel lives at y*320 + x.
 *
 * x16_gfx_pset() clips. The line, rect and frame primitives do NOT --
 * they assume their arguments are on screen.
 *
 * Nothing here changes the screen mode except x16_gfx_init(). The drawing
 * routines only touch VRAM, so they also work on an off-screen buffer.
 * =====================================================================
 */

#ifndef X16_BITMAP_H
#define X16_BITMAP_H

#define X16_GFX_WIDTH   320
#define X16_GFX_HEIGHT  240

/* 320x240@256c on layer 0, 40x30 text on layer 1. Returns 1 on success,
** 0 if the mode is unsupported. */
unsigned char x16_gfx_init(void);

void x16_gfx_clear(__reg("a") unsigned char color);

/* Clipped. */
void x16_gfx_pset(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                  __reg("r4") unsigned char color);

/* Unclipped from here down. */
void x16_gfx_hline(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                   __reg("r4/r5") unsigned int len, __reg("r6") unsigned char color);

/* len is 1-255: a column of a 240-row screen never needs more. */
void x16_gfx_vline(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                   __reg("r4") unsigned char len, __reg("r6") unsigned char color);

/* Five args: color rides the C soft stack. */
void x16_gfx_rect(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                  __reg("r4/r5") unsigned int w, __reg("r6") unsigned char h,
                  unsigned char color);

void x16_gfx_frame(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                   __reg("r4/r5") unsigned int w, __reg("r6") unsigned char h,
                   unsigned char color);

/* Bresenham, any direction. Pre-clip with <x16/clip.h> if the endpoints
** might leave the screen. Five args: color rides the C soft stack. */
void x16_gfx_line(__reg("r0/r1") unsigned int x0, __reg("r2") unsigned char y0,
                  __reg("r4/r5") unsigned int x1, __reg("r6") unsigned char y1,
                  unsigned char color);

/* circle / disc / flood moved to <x16/shapes.h>: one implementation now
** serves both this 8bpp module and the 2bpp bitmap2 module. */

/* Draw one glyph from the charset the KERNAL keeps at VRAM $1F000. Set
** bits become `color`; clear bits stay transparent. `code` is a SCREEN
** code, not PETSCII. Text clips. */
void x16_gfx_char(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                  __reg("r4") unsigned char color, __reg("r6") unsigned char code);

/* A NUL-terminated string, 8 pixels per character. ASCII letters convert
** to screen codes. */
void x16_gfx_text(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                  __reg("r4") unsigned char color, __reg("r6/r7") const char *s);

#endif /* X16_BITMAP_H */
