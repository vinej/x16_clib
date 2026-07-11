/* =====================================================================
 * x16clib :: x16/math.h -- game math: PRNG, sine tables, atan2, lerp
 * =====================================================================
 * Angles are bytes: a full circle is 256, so 64 = 90 degrees and
 * wrap-around is free. With the X16's y axis pointing DOWN the screen,
 * angle 0 points east (+x) and 64 points south (+y) -- atan2 and the
 * sine tables agree on that, so
 *      x += (x16_sin8(a) * speed) >> 7
 * moves along the heading atan2 returned.
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

#ifndef X16_MATH_H
#define X16_MATH_H

#include <x16/zpsafe.h>

/* Seed the generator. A seed of 0 is nudged to 1: it is xorshift's one
** fixed point. Seed from x16_irq_frames(), or from the KERNAL's
** entropy, if you want a different sequence each run.
*/
void x16_rnd_seed (unsigned int seed);

/* John Metcalf's 16-bit xorshift: period 65535, a handful of cycles.
** Cheap enough to call per object per frame.
*/
unsigned char x16_rnd8 (void);
unsigned int x16_rnd16 (void);

/* -127..127. */
signed char x16_sin8 (unsigned char angle);
signed char x16_cos8 (unsigned char angle);

/* 1..255 -- the signed value biased by 128, handy for volumes and
** scales that must not go negative.
*/
unsigned char x16_sin8u (unsigned char angle);
unsigned char x16_cos8u (unsigned char angle);

/* The angle of a vector, 0-255. atan2(0,0) answers 0 (east). */
unsigned char x16_atan2 (signed char dx, signed char dy);

/* Linear interpolation between two unsigned bytes: t = 0 gives exactly
** a, t = 255 exactly b, and the midpoint is at most one off.
*/
unsigned char x16_lerp8 (unsigned char a, unsigned char b, unsigned char t);

#endif /* X16_MATH_H */
