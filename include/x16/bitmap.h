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

#ifndef X16_BITMAP_H
#define X16_BITMAP_H

#define X16_GFX_WIDTH   320
#define X16_GFX_HEIGHT  240

/* 320x240@256c on layer 0, 40x30 text on layer 1.
** Returns 1 on success, 0 if the mode is unsupported.
*/
unsigned char x16_gfx_init (void);

void __fastcall__ x16_gfx_clear (unsigned char color);

/* Clipped. */
void __fastcall__ x16_gfx_pset (unsigned int x, unsigned char y,
                                unsigned char color);

/* Unclipped from here down. */
void __fastcall__ x16_gfx_hline (unsigned int x, unsigned char y,
                                 unsigned int len, unsigned char color);

/* len is 1-255: a column of a 240-row screen never needs more. */
void __fastcall__ x16_gfx_vline (unsigned int x, unsigned char y,
                                 unsigned char len, unsigned char color);

void __fastcall__ x16_gfx_rect (unsigned int x, unsigned char y,
                                unsigned int w, unsigned char h,
                                unsigned char color);

void __fastcall__ x16_gfx_frame (unsigned int x, unsigned char y,
                                 unsigned int w, unsigned char h,
                                 unsigned char color);

/* Bresenham, any direction. */
void __fastcall__ x16_gfx_line (unsigned int x0, unsigned char y0,
                                unsigned int x1, unsigned char y1,
                                unsigned char color);

#endif /* X16_BITMAP_H */
