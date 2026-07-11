/* =====================================================================
 * x16clib :: x16/clip.h -- Cohen-Sutherland line clipping
 * =====================================================================
 * The drawing routines are documented as non-clipping. This removes
 * that sharp edge: give x16_clip_line() a segment in 16-bit SIGNED
 * coordinates (anywhere within +/-4095) and it either rejects it or
 * hands back the visible part.
 *
 * The rectangle is inclusive and defaults to the full 320x240 bitmap.
 *
 * This module never calls a drawing routine, so linking it does not
 * drag the bitmap module in behind it.
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
void x16_clip_set (unsigned int xmin, unsigned int ymin,
                   unsigned int xmax, unsigned int ymax);

/* Clip `seg` against the rectangle. Returns 1 if any of it is visible,
** with *seg replaced by the visible part; 0 if it lies entirely outside,
** in which case *seg is unspecified.
*/
unsigned char x16_clip_line (x16_line *seg);

/* pulls the implementation in with this header */
#pragma compile("clip.c")

#endif /* X16_CLIP_H */
