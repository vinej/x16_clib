/* =====================================================================
 * x16clib :: x16/fixed.h -- 16x16 multiply and 8.8 fixed point
 * =====================================================================
 * C has no fixed-point type, and a 6502 has no multiplier. These are the
 * two operations a sprite-mover actually needs.
 *
 * An 8.8 value is a signed 16-bit int holding 256 times the real number:
 * 0x0180 is 1.5, 0x0200 is 2.0, 0xFF00 is -1.0. Hold a sprite's position
 * in 8.8, add an 8.8 velocity each frame, and take the high byte as the
 * pixel coordinate. See examples/bounce.c.
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

#ifndef X16_FIXED_H
#define X16_FIXED_H

#include <x16/zpsafe.h>

/* Build an 8.8 constant from a whole part and a 256ths part. */
#define X16_FIX(whole, frac_256)    ((int)(((whole) << 8) | ((frac_256) & 0xFF)))

/* The pixel coordinate of an 8.8 value, rounding toward negative
** infinity -- an arithmetic shift, not a divide.
*/
#define X16_FIX_WHOLE(v)            ((int)(v) >> 8)

/* Unsigned 16 x 16 -> 32. */
unsigned long x16_umul16 (unsigned int a, unsigned int b);

/* Signed 8.8 multiply: (a * b) >> 8, in 8.8.
**      x16_mul88(0x0180, 0x0200) == 0x0300      1.5 * 2.0 == 3.0
*/
int x16_mul88 (int a, int b);

#endif /* X16_FIXED_H */
