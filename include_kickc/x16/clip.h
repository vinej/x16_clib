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
** KickC build. The API is identical to the cc65 build's; what differs is
** the delivery. KickC has no linker and no archive format -- it compiles
** the whole program from source and strips what goes unused -- so the
** KickC port is a SOURCE distribution. Include this header; the matching
** implementation in src_kickc/x16/ is compiled in automatically when the
** library path points there:
**
**     kickc -p cx16 -a -I include_kickc -L src_kickc yourprog.c
** --------------------------------------------------------------------- */

#ifndef X16_CLIP_H
#define X16_CLIP_H

#include <x16/zpsafe.h>

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

#endif /* X16_CLIP_H */
