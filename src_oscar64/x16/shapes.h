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
void x16_gfx_circle (unsigned int cx, unsigned char cy,
                                  unsigned char r, unsigned char color);
void x16_gfx_disc (unsigned int cx, unsigned char cy,
                                unsigned char r, unsigned char color);

/* Axis-aligned ellipse outline / filled ellipse (the error-form midpoint
** walk). rx and ry each 0-255; the outline clips through pset, the fill
** clamps its spans to the canvas.
*/
void x16_gfx_ellipse (unsigned int cx, unsigned char cy,
                                   unsigned char rx, unsigned char ry,
                                   unsigned char color);
void x16_gfx_fellipse (unsigned int cx, unsigned char cy,
                                    unsigned char rx, unsigned char ry,
                                    unsigned char color);

/* Scanline flood fill of the 4-connected region under the seed. Filling
** with the colour already there is a no-op. Returns 1 when the fill was
** complete, 0 when the span stack (96 seeds) overflowed and the fill is
** INCOMPLETE -- pathological shapes (long thin spirals) are what overflow.
*/
unsigned char x16_gfx_flood (unsigned int x, unsigned char y,
                                          unsigned char color);

/* --- 2bpp (640x480) -------------------------------------------------- */

void x16_gfx2_circle (unsigned int cx, unsigned int cy,
                                   unsigned char r, unsigned char color);
void x16_gfx2_disc (unsigned int cx, unsigned int cy,
                                 unsigned char r, unsigned char color);

void x16_gfx2_ellipse (unsigned int cx, unsigned int cy,
                                    unsigned char rx, unsigned char ry,
                                    unsigned char color);
void x16_gfx2_fellipse (unsigned int cx, unsigned int cy,
                                     unsigned char rx, unsigned char ry,
                                     unsigned char color);

unsigned char x16_gfx2_flood (unsigned int x, unsigned int y,
                                           unsigned char color);

#pragma compile("shapes.c")

/* --- curve shapes (v0.8.0) ------------------------------------------
** Regular N-gon (3..24 sides) about a centre, r = circumradius, rotation
** a byte-angle. f-variant fills. */
void x16_gfx_polygon (unsigned int cx, unsigned char cy, unsigned char r,
                      unsigned char sides, unsigned char rotation,
                      unsigned char color);
void x16_gfx_fpolygon (unsigned int cx, unsigned char cy, unsigned char r,
                       unsigned char sides, unsigned char rotation,
                       unsigned char color);
/* Rounded rectangle: (x,y) top-left, w/h size, r corner radius (clamped
** to min(w,h)/2). f-variant fills. */
void x16_gfx_rrect (unsigned int x, unsigned int y, unsigned int w,
                    unsigned int h, unsigned char r, unsigned char color);
void x16_gfx_frrect (unsigned int x, unsigned int y, unsigned int w,
                     unsigned int h, unsigned char r, unsigned char color);
/* Arc: circle-outline slice from byte-angle a0 to a1 (a0==a1 = full
** circle). x16_gfx_pie fills the matching wedge. */
void x16_gfx_arc (unsigned int cx, unsigned char cy, unsigned char r,
                  unsigned char a0, unsigned char a1, unsigned char color);
void x16_gfx_pie (unsigned int cx, unsigned char cy, unsigned char r,
                  unsigned char a0, unsigned char a1, unsigned char color);
/* Cubic Bezier through four control points, passed as an 8-element array
** pts[] = { x0,y0, x1,y1, x2,y2, x3,y3 }. */
void x16_gfx_bezier (const unsigned int *pts, unsigned char color);

void x16_gfx2_polygon (unsigned int cx, unsigned int cy, unsigned char r,
                       unsigned char sides, unsigned char rotation,
                       unsigned char color);
void x16_gfx2_fpolygon (unsigned int cx, unsigned int cy, unsigned char r,
                        unsigned char sides, unsigned char rotation,
                        unsigned char color);
void x16_gfx2_rrect (unsigned int x, unsigned int y, unsigned int w,
                     unsigned int h, unsigned char r, unsigned char color);
void x16_gfx2_frrect (unsigned int x, unsigned int y, unsigned int w,
                      unsigned int h, unsigned char r, unsigned char color);
void x16_gfx2_arc (unsigned int cx, unsigned int cy, unsigned char r,
                   unsigned char a0, unsigned char a1, unsigned char color);
void x16_gfx2_pie (unsigned int cx, unsigned int cy, unsigned char r,
                   unsigned char a0, unsigned char a1, unsigned char color);
void x16_gfx2_bezier (const unsigned int *pts, unsigned char color);

#endif /* X16_SHAPES_H */
