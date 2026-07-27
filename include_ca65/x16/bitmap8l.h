/* =====================================================================
 * x16clib :: x16/bitmap8l.h -- 320x240x256 bitmap drawing
 * =====================================================================
 * The framebuffer is 8bpp at VRAM $00000, one byte per pixel, rows of
 * 320. A pixel lives at y*320 + x.
 *
 * x16_gfx8l_pset() clips. The line, rect and frame primitives do NOT --
 * they assume their arguments are on screen. Clipping every span would
 * cost more than it saves for a caller that already knows its geometry.
 *
 * Nothing here changes the screen mode except x16_gfx8l_init(). The drawing
 * routines only touch VRAM, so they also work on an off-screen buffer.
 *
 * cc65's TGI driver covers similar ground portably; these are faster and
 * know they are on a VERA.
 * =====================================================================
 */

#ifndef X16_BITMAP8L_H
#define X16_BITMAP8L_H

#define X16_GFX8L_WIDTH   320
#define X16_GFX8L_HEIGHT  240

/* 320x240@256c on layer 0, 40x30 text on layer 1.
** Returns 1 on success, 0 if the mode is unsupported.
*/
unsigned char x16_gfx8l_init (void);

void __fastcall__ x16_gfx8l_clear (unsigned char color);

/* Clipped. */
void __fastcall__ x16_gfx8l_pset (unsigned int x, unsigned char y,
                                unsigned char color);

/* Unclipped from here down. */
void __fastcall__ x16_gfx8l_hline (unsigned int x, unsigned char y,
                                 unsigned int len, unsigned char color);

/* len is 1-255: a column of a 240-row screen never needs more. */
void __fastcall__ x16_gfx8l_vline (unsigned int x, unsigned char y,
                                 unsigned char len, unsigned char color);

void __fastcall__ x16_gfx8l_rect (unsigned int x, unsigned char y,
                                unsigned int w, unsigned char h,
                                unsigned char color);

void __fastcall__ x16_gfx8l_frame (unsigned int x, unsigned char y,
                                 unsigned int w, unsigned char h,
                                 unsigned char color);

/* Bresenham, any direction. Pre-clip with <x16/clip.h> if the endpoints
** might leave the screen.
*/
void __fastcall__ x16_gfx8l_line (unsigned int x0, unsigned char y0,
                                unsigned int x1, unsigned char y1,
                                unsigned char color);

/* circle / disc / flood moved to <x16/shapes.h>: one implementation now
** serves both this 8bpp module and the 2bpp bitmap2 module.
*/

/* Draw one glyph from the charset the KERNAL keeps at VRAM $1F000. Set
** bits become `color`; clear bits stay transparent, so glyphs overlay
** whatever is beneath. `code` is a SCREEN code, not PETSCII. Text clips.
*/
void __fastcall__ x16_gfx8l_char (unsigned int x, unsigned char y,
                                unsigned char color, unsigned char code);

/* A NUL-terminated string, 8 pixels per character. ASCII letters convert
** to screen codes, so "HELLO" reads as you would expect.
*/
void __fastcall__ x16_gfx8l_text (unsigned int x, unsigned char y,
                                unsigned char color, const char *s);

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
void __fastcall__ x16_gfx8l_pattern_set (const unsigned char *pattern,
                                       unsigned char bg, unsigned char fg);

void __fastcall__ x16_gfx8l_pattern_rect (unsigned int x, unsigned int y,
                                        unsigned int w, unsigned int h);

/* Copy a row-major image from RAM into the bitmap, one byte per pixel.
** `w` is in PIXELS (1-255). op: 0 copy, 1 OR, 2 AND, 3 XOR.
*/
void __fastcall__ x16_gfx8l_blit (unsigned int x, unsigned int y,
                                unsigned char w, unsigned char h,
                                const unsigned char *src,
                                unsigned char op);

/* A masked blit: a source byte of 0 leaves the screen alone.
**
** At 8bpp the mask IS the data -- colour 0 means transparent -- so the
** source is plain row-major pixels, where x16_gfx2h_blitm() needs
** interleaved (mask, data) pairs and pre-shifted columns.
*/
void __fastcall__ x16_gfx8l_blitm (unsigned int x, unsigned int y,
                                 unsigned char w, unsigned char h,
                                 const unsigned char *src);

#endif /* X16_BITMAP8L_H */
