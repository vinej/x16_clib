/* =====================================================================
 * x16clib :: x16/bitmap2.h -- 640x480x4 bitmap drawing (2bpp)
 * =====================================================================
 * The full 640x480 the VERA can drive: 2bpp, 4 pixels per byte packed
 * MSB-first, rows of 160 bytes at VRAM $00000 (76,800 bytes). A pixel
 * byte lives at y*160 + (x >> 2). Colours are 0-3 out of the first four
 * palette entries; x16_gfx2_init() loads white / light gray / dark gray
 * / black -- recolour with <x16/palette.h> without touching a pixel.
 *
 * x16_gfx2_pset() and x16_gfx2_read() clip. The span, rect, line and
 * blit primitives do NOT: they assume their arguments are on screen
 * (the 8bpp module's policy, for the same reason).
 *
 * x16_gfx2_clear() fills through the VERA FX 32-bit cache: it needs an
 * FX-capable VERA (47.0.2+, probe with x16_vera_has_fx()).
 * =====================================================================
 */

#ifndef X16_BITMAP2_H
#define X16_BITMAP2_H

#define X16_GFX2_WIDTH   640
#define X16_GFX2_HEIGHT  480
#define X16_GFX2_STRIDE  160

/* Program the mode on bare VERA registers (there is no KERNAL screen
** mode for it): layer 0 bitmap 2bpp 640-wide at 1:1 scale, layer 1 off,
** sprites left alone, palette 0-3 defaulted. Does NOT clear the pixels.
*/
void x16_gfx2_init (void);

/* Full-screen fill with one colour, ~4x faster than a CPU loop. */
void __fastcall__ x16_gfx2_clear (unsigned char color);

/* Point VERA data port 0 at the byte holding (x,y) with the given
** increment index (X16_VERA_INC_*); returns x & 3, the pixel's
** position within that byte. The escape hatch for custom inner loops.
*/
unsigned char __fastcall__ x16_gfx2_setptr (unsigned char inc,
                                            unsigned int x, unsigned int y);

/* Clipped. */
void __fastcall__ x16_gfx2_pset (unsigned int x, unsigned int y,
                                 unsigned char color);

/* 0-3, or $FF when (x,y) is off screen. */
unsigned char __fastcall__ x16_gfx2_read (unsigned int x, unsigned int y);

/* Unclipped from here down. */
void __fastcall__ x16_gfx2_hline (unsigned int x, unsigned int y,
                                  unsigned int len, unsigned char color);

void __fastcall__ x16_gfx2_vline (unsigned int x, unsigned int y,
                                  unsigned int len, unsigned char color);

void __fastcall__ x16_gfx2_rect (unsigned int x, unsigned int y,
                                 unsigned int w, unsigned int h,
                                 unsigned char color);

void __fastcall__ x16_gfx2_frame (unsigned int x, unsigned int y,
                                  unsigned int w, unsigned int h,
                                  unsigned char color);

/* Bresenham, any direction; plots through the clipped pset, so lines
** may safely leave the screen.
*/
void __fastcall__ x16_gfx2_line (unsigned int x0, unsigned int y0,
                                 unsigned int x1, unsigned int y1,
                                 unsigned char color);

/* An 8x8 1bpp pattern (8 row bytes, bit 7 leftmost) expanded once and
** cached; colors = (background << 2) | foreground. Patterns anchor to
** the screen origin, so adjacent fills always knit together.
*/
void __fastcall__ x16_gfx2_pattern_set (const unsigned char *pattern,
                                        unsigned char colors);

void __fastcall__ x16_gfx2_pattern_rect (unsigned int x, unsigned int y,
                                         unsigned int w, unsigned int h);

/* Copy a byte-aligned image (row-major, wbytes per row = 4-pixel units;
** x bits 1:0 are ignored) from RAM into the bitmap. op: 0 copy, 1 OR,
** 2 AND, 3 XOR.
*/
void __fastcall__ x16_gfx2_blit (unsigned int x, unsigned int y,
                                 unsigned char wbytes, unsigned char h,
                                 const unsigned char *src,
                                 unsigned char op);

/* Masked blit of pre-shifted column-major data at ANY x: for each of
** `cols` framebuffer byte columns, `h` (mask, data) byte pairs walking
** down the rows -- fb = (fb & mask) | data. The caller supplies data
** already shifted for x & 3; pre-shifted glyph caches are what make
** proportional text affordable (~160 masked 8x8 glyphs per frame).
** h is 1-127.
*/
void __fastcall__ x16_gfx2_blitm (unsigned int x, unsigned int y,
                                  unsigned char h, unsigned char cols,
                                  const unsigned char *src);

#endif /* X16_BITMAP2_H */
