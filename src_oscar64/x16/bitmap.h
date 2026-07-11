/* =====================================================================
 * x16clib :: x16/bitmap.h -- 320x240x256 bitmap drawing
 * =====================================================================
 * The framebuffer is 8bpp at VRAM $00000, one byte per pixel, rows of
 * 320. A pixel lives at y*320 + x.
 *
 * x16_gfx_pset() clips. The line, rect and frame primitives do NOT --
 * they assume their arguments are on screen. Clipping every span would
 * cost more than it saves for a caller that already knows its geometry.
 *
 * Nothing here changes the screen mode except x16_gfx_init(). The drawing
 * routines only touch VRAM, so they also work on an off-screen buffer.
 *
 * cc65's TGI driver covers similar ground portably; these are faster and
 * know they are on a VERA.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** Oscar64 build. The API is identical to the cc65 build's; what differs
** is the delivery. Oscar64 compiles the whole program at once and strips
** what goes unused, so this port is a SOURCE distribution: headers and
** implementations sit side by side in src_oscar64/x16/, and the
** `#pragma compile` at the bottom of this header pulls the matching .c
** in automatically:
**
**     oscar64 -tm=x16 -n -i=src_oscar64 -o=YOURPROG.PRG yourprog.c
** --------------------------------------------------------------------- */

#ifndef X16_BITMAP_H
#define X16_BITMAP_H

#define X16_GFX_WIDTH   320
#define X16_GFX_HEIGHT  240

/* 320x240@256c on layer 0, 40x30 text on layer 1.
** Returns 1 on success, 0 if the mode is unsupported.
*/
unsigned char x16_gfx_init (void);

void x16_gfx_clear (unsigned char color);

/* Clipped. */
void x16_gfx_pset (unsigned int x, unsigned char y,
                                unsigned char color);

/* Unclipped from here down. */
void x16_gfx_hline (unsigned int x, unsigned char y,
                                 unsigned int len, unsigned char color);

/* len is 1-255: a column of a 240-row screen never needs more. */
void x16_gfx_vline (unsigned int x, unsigned char y,
                                 unsigned char len, unsigned char color);

void x16_gfx_rect (unsigned int x, unsigned char y,
                                unsigned int w, unsigned char h,
                                unsigned char color);

void x16_gfx_frame (unsigned int x, unsigned char y,
                                 unsigned int w, unsigned char h,
                                 unsigned char color);

/* Bresenham, any direction. Pre-clip with <x16/clip.h> if the endpoints
** might leave the screen.
*/
void x16_gfx_line (unsigned int x0, unsigned char y0,
                                unsigned int x1, unsigned char y1,
                                unsigned char color);

/* Circles DO clip, at every edge: the outline plots through the clipped
** x16_gfx_pset() and the fill through clamped spans. Radius 0-120.
*/
void x16_gfx_circle (unsigned int cx, unsigned char cy,
                                  unsigned char r, unsigned char color);
void x16_gfx_disc (unsigned int cx, unsigned char cy,
                                unsigned char r, unsigned char color);

/* Draw one glyph from the charset the KERNAL keeps at VRAM $1F000. Set
** bits become `color`; clear bits stay transparent, so glyphs overlay
** whatever is beneath. `code` is a SCREEN code, not PETSCII. Text clips.
*/
void x16_gfx_char (unsigned int x, unsigned char y,
                                unsigned char color, unsigned char code);

/* A NUL-terminated string, 8 pixels per character. ASCII letters convert
** to screen codes, so "HELLO" reads as you would expect.
*/
void x16_gfx_text (unsigned int x, unsigned char y,
                                unsigned char color, const char *s);

/* Scanline flood fill of the 4-connected region under the seed. Filling
** with the colour already there is a no-op.
**
** Returns 1 when the region was filled completely, 0 when the span stack
** (170 deep) overflowed and the fill is INCOMPLETE. Pathological shapes
** -- long thin spirals -- are what overflow it.
*/
unsigned char x16_gfx_flood (unsigned int x, unsigned char y,
                                          unsigned char color);

/* pulls the implementation in with this header */
#pragma compile("bitmap.c")

#endif /* X16_BITMAP_H */
