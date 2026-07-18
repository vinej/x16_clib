/* =====================================================================
 * x16clib :: x16/shapes.h -- circle / disc / flood for both bitmap modes
 * =====================================================================
 * One shape implementation, bound at call time to the engine each entry
 * point names:
 *
 *   x16_gfx_*   plot on the 8bpp bitmap  (<x16/bitmap.h>,  320x240)
 *   x16_gfx2_*  plot on the 2bpp bitmap  (<x16/bitmap2.h>, 640x480)
 *
 * The two families differ only in the width of the vertical coordinate,
 * which follows the pset() of the module they draw on: 8bpp y is a byte
 * (0-239), 2bpp y is 16-bit (0-479).
 *
 * Clipping, per shape:
 *   - circle: the outline plots through the clipping pset(), so it clips
 *     at every screen edge for free.
 *   - disc: the fill plots through the UNCLIPPED hline() -- keep a disc on
 *     screen, exactly as for the line/rect primitives.
 *   - flood: bounds-checked against the canvas, so it never reads or
 *     writes off screen.
 * =====================================================================
 */

#ifndef X16_SHAPES_H
#define X16_SHAPES_H

/* --- 8bpp (320x240) -------------------------------------------------- */

/* Midpoint circle outline / filled disc. Radius 0-120. */
void __fastcall__ x16_gfx_circle (unsigned int cx, unsigned char cy,
                                  unsigned char r, unsigned char color);
void __fastcall__ x16_gfx_disc (unsigned int cx, unsigned char cy,
                                unsigned char r, unsigned char color);

/* Scanline flood fill of the 4-connected region under the seed. Filling
** with the colour already there is a no-op. Returns 1 when the fill was
** complete, 0 when the span stack (96 seeds) overflowed and the fill is
** INCOMPLETE -- pathological shapes (long thin spirals) are what overflow.
*/
unsigned char __fastcall__ x16_gfx_flood (unsigned int x, unsigned char y,
                                          unsigned char color);

/* --- 2bpp (640x480) -------------------------------------------------- */

void __fastcall__ x16_gfx2_circle (unsigned int cx, unsigned int cy,
                                   unsigned char r, unsigned char color);
void __fastcall__ x16_gfx2_disc (unsigned int cx, unsigned int cy,
                                 unsigned char r, unsigned char color);

unsigned char __fastcall__ x16_gfx2_flood (unsigned int x, unsigned int y,
                                           unsigned char color);

#endif /* X16_SHAPES_H */
