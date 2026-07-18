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
 * Clipping, per shape: the circle outline clips through pset(); a disc
 * fill does NOT (keep it on screen, as for the line/rect primitives); a
 * flood is bounds-checked against the canvas.
 * =====================================================================
 */

#ifndef X16_SHAPES_H
#define X16_SHAPES_H

/* --- 8bpp (320x240) -------------------------------------------------- */

void x16_gfx_circle(__reg("r0/r1") unsigned int cx, __reg("r2") unsigned char cy,
                    __reg("r4") unsigned char r, __reg("r6") unsigned char color);
void x16_gfx_disc(__reg("r0/r1") unsigned int cx, __reg("r2") unsigned char cy,
                  __reg("r4") unsigned char r, __reg("r6") unsigned char color);

/* Returns 1 when the region was filled completely, 0 when the span stack
** (96 seeds) overflowed and the fill is INCOMPLETE. */
unsigned char x16_gfx_flood(__reg("r0/r1") unsigned int x, __reg("r2") unsigned char y,
                            __reg("r4") unsigned char color);

/* --- 2bpp (640x480) -------------------------------------------------- */

void x16_gfx2_circle(__reg("r0/r1") unsigned int cx, __reg("r2/r3") unsigned int cy,
                     __reg("r4") unsigned char r, __reg("r6") unsigned char color);
void x16_gfx2_disc(__reg("r0/r1") unsigned int cx, __reg("r2/r3") unsigned int cy,
                   __reg("r4") unsigned char r, __reg("r6") unsigned char color);

unsigned char x16_gfx2_flood(__reg("r0/r1") unsigned int x, __reg("r2/r3") unsigned int y,
                             __reg("r4") unsigned char color);

#endif /* X16_SHAPES_H */
