/* =====================================================================
 * x16clib :: x16/clip.h -- Cohen-Sutherland line clipping
 * =====================================================================
 * x16_gfx_line() and x16_fx_line() do not clip: they assume both
 * endpoints are on screen. This removes that sharp edge.
 *
 *      x16_line seg = { -50, 120, 400, 130 };
 *      if (x16_clip_line(&seg)) {
 *          x16_gfx_line(seg.x0, seg.y0, seg.x1, seg.y1, color);
 *      }
 *
 * Coordinates are SIGNED, and may lie anywhere within +/-4095 -- the
 * intersection arithmetic keeps a 24-bit product, so wider inputs
 * overflow silently.
 *
 * The rectangle is inclusive and defaults to the full 320x240 bitmap.
 *
 * This module never calls a drawing routine, so linking it does not
 * drag the bitmap module in behind it.
 * =====================================================================
 */

#ifndef X16_CLIP_H
#define X16_CLIP_H

/* A segment. The four fields are copied straight onto the assembly's
** operand block, so the order is load-bearing. Do not reorder.
*/
typedef struct {
    int x0;
    int y0;
    int x1;
    int y1;
} x16_line;

/* Change the rectangle. All four bounds are inclusive. */
void __fastcall__ x16_clip_set (unsigned int xmin, unsigned int ymin,
                                unsigned int xmax, unsigned int ymax);

/* Clip `seg` against the rectangle. Returns 1 if any of it is visible,
** with *seg replaced by the visible part; 0 if it lies entirely outside,
** in which case *seg is unspecified.
*/
unsigned char __fastcall__ x16_clip_line (x16_line *seg);

#endif /* X16_CLIP_H */
