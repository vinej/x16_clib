/* =====================================================================
 * x16clib :: x16/bitmap8l.h -- 320x240x256 bitmap drawing
 * =====================================================================
 * The framebuffer is 8bpp at VRAM $00000, one byte per pixel, rows of
 * 320. A pixel lives at y*320 + x.
 *
 * x16_gfx8l_pset() clips. The line, rect and frame primitives do NOT --
 * they assume their arguments are on screen.
 *
 * Nothing here changes the screen mode except x16_gfx8l_init(). The drawing
 * routines only touch VRAM, so they also work on an off-screen buffer.
 * =====================================================================
 */

#ifndef X16_BITMAP8L_H
#define X16_BITMAP8L_H

#define X16_GFX8L_WIDTH   320
#define X16_GFX8L_HEIGHT  240

/* 320x240@256c on layer 0, 40x30 text on layer 1. Returns 1 on success,
** 0 if the mode is unsupported. */
unsigned char x16_gfx8l_init(void);

void x16_gfx8l_clear(__reg("a") unsigned char color);

/* Clipped. */
void x16_gfx8l_pset(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                  __reg("r4") unsigned char color);

/* Unclipped from here down. */
void x16_gfx8l_hline(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                   __reg("r4/r5") unsigned int len, __reg("r6") unsigned char color);

/* len is 1-255: a column of a 240-row screen never needs more. */
void x16_gfx8l_vline(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                   __reg("r4") unsigned char len, __reg("r6") unsigned char color);

/* Five args: color rides the C soft stack. */
void x16_gfx8l_rect(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                  __reg("r4/r5") unsigned int w, __reg("r6") unsigned char h,
                  unsigned char color);

void x16_gfx8l_frame(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                   __reg("r4/r5") unsigned int w, __reg("r6") unsigned char h,
                   unsigned char color);

/* Bresenham, any direction. Pre-clip with <x16/clip.h> if the endpoints
** might leave the screen. Five args: color rides the C soft stack. */
void x16_gfx8l_line(__reg("r0/r1") unsigned int x0, __reg("r2") unsigned char y0,
                  __reg("r4/r5") unsigned int x1, __reg("r6") unsigned char y1,
                  unsigned char color);

/* circle / disc / flood moved to <x16/shapes.h>: one implementation now
** serves both this 8bpp module and the 2bpp bitmap2 module. */

/* Draw one glyph from the charset the KERNAL keeps at VRAM $1F000. Set
** bits become `color`; clear bits stay transparent. `code` is a SCREEN
** code, not PETSCII. Text clips. */
void x16_gfx8l_char(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                  __reg("r4") unsigned char color, __reg("r6") unsigned char code);

/* A NUL-terminated string, 8 pixels per character. ASCII letters convert
** to screen codes. */
void x16_gfx8l_text(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                  __reg("r4") unsigned char color, __reg("r6/r7") const char *s);

/* =====================================================================
** Patterns and blits -- the same surface x16/bitmap2h.h has
** =====================================================================
** Two-way parity between the engines: a program can move between
** 320x240x256 and 640x480x4 without losing a primitive. Two of these
** differ from their 2bpp counterparts, and both differences come from
** one byte being one pixel here rather than four.
**
** Neither blit clips; keep them on screen.
** =====================================================================
*/

/* An 8x8 1bpp pattern (8 row bytes, bit 7 leftmost) cached for
** x16_gfx8l_pattern_rect(). Background and foreground are whole bytes --
** the 2bpp version packs two 2-bit colours into one argument, which
** 8bpp colours do not fit in.
**
** Patterns anchor to the screen origin, so adjacent fills always knit
** together.
*/
void x16_gfx8l_pattern_set(__reg("a/x") const unsigned char *pattern,
                         __reg("r0") unsigned char bg,
                         __reg("r1") unsigned char fg);

void x16_gfx8l_pattern_rect(__reg("r0/r1") unsigned int x,
                          __reg("r2") unsigned char y,
                          __reg("r4/r5") unsigned int w,
                          __reg("r6") unsigned char h);

/* Copy a row-major image from RAM into the bitmap, one byte per pixel.
** `w` is in PIXELS (1-255). op: 0 copy, 1 OR, 2 AND, 3 XOR.
*/
void x16_gfx8l_blit(__reg("r0/r1") unsigned int x,
                  __reg("r2") unsigned char y,
                  __reg("r4") unsigned char w,
                  __reg("r5") unsigned char h,
                  __reg("r6/r7") const unsigned char *src,
                  __reg("r3") unsigned char op);

/* A masked blit: a source byte of 0 leaves the screen alone.
**
** At 8bpp the mask IS the data -- colour 0 means transparent -- so the
** source is plain row-major pixels, where x16_gfx2h_blitm() needs
** interleaved (mask, data) pairs and pre-shifted columns.
*/
void x16_gfx8l_blitm(__reg("r0/r1") unsigned int x,
                   __reg("r2") unsigned char y,
                   __reg("r4") unsigned char w,
                   __reg("r5") unsigned char h,
                   __reg("r6/r7") const unsigned char *src);

#endif /* X16_BITMAP8L_H */
