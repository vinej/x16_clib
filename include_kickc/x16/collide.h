/* =====================================================================
 * x16clib :: x16/collide.h -- axis-aligned bounding-box overlap
 * =====================================================================
 * Edges that merely touch do NOT overlap: a box at x=0 of width 10 and
 * one at x=10 are adjacent, not colliding.
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

#ifndef X16_COLLIDE_H
#define X16_COLLIDE_H

#include <x16/zpsafe.h>

/* Two boxes of unsigned bytes. Returns 1 if they overlap.
**
** The edge sums are computed in 9 bits, so a box may legitimately run
** past x=255. But a coordinate cannot, which is why this cannot describe
** the right-hand half of a 640-wide display -- use x16_collide16() there.
*/
unsigned char x16_collide8 (
        unsigned char ax, unsigned char ay,
        unsigned char aw, unsigned char ah,
        unsigned char bx, unsigned char by,
        unsigned char bw, unsigned char bh);

/* The 16-bit version, for anything positioned in display space: in the
** default 80x60 text mode the screen is 640x480, and sprite coordinates
** are in those units. Only screen modes 2, 3 and 0x80 halve it to
** 320x240.
**
** The field order here matches the assembly's operand block byte for
** byte; the shim block-copies rather than unpacking. Do not reorder.
*/
typedef struct {
    unsigned int x;
    unsigned int y;
    unsigned int w;
    unsigned int h;
} x16_box16;

unsigned char x16_collide16 (const x16_box16 *a, const x16_box16 *b);

#endif /* X16_COLLIDE_H */
