/* =====================================================================
 * x16clib :: x16/math.h -- game math: PRNG, sine tables, atan2, lerp
 * =====================================================================
 * cc65 has rand(), but it is a general portable LCG and there is no trig
 * table anywhere. These are the routines a game loop rewrites.
 *
 * ANGLES ARE BYTES. A full circle is 256, so 64 is 90 degrees and
 * wrap-around is free. The X16's y axis points DOWN the screen, so angle
 * 0 is east (+x) and 64 is south (+y). atan2 and the sine tables agree
 * on that, which means a heading from x16_atan2() feeds straight back in:
 *
 *      a = x16_atan2(tx - x, ty - y);          // aim at the target
 *      x += (x16_sin8(a + 64) * speed) >> 7;   // cos
 *      y += (x16_sin8(a) * speed) >> 7;
 * =====================================================================
 */

#ifndef X16_MATH_H
#define X16_MATH_H

/* Seed the generator. A seed of 0 is nudged to 1: it is xorshift's one
** fixed point. Seed from x16_irq_frames(), or from the KERNAL's
** entropy, if you want a different sequence each run.
*/
void __fastcall__ x16_rnd_seed (unsigned int seed);

/* John Metcalf's 16-bit xorshift: period 65535, a handful of cycles.
** Cheap enough to call per object per frame.
*/
unsigned char x16_rnd8 (void);
unsigned int x16_rnd16 (void);

/* -127..127. */
signed char __fastcall__ x16_sin8 (unsigned char angle);
signed char __fastcall__ x16_cos8 (unsigned char angle);

/* 1..255 -- the signed value biased by 128, handy for volumes and
** scales that must not go negative.
*/
unsigned char __fastcall__ x16_sin8u (unsigned char angle);
unsigned char __fastcall__ x16_cos8u (unsigned char angle);

/* The angle of a vector, 0-255. atan2(0,0) answers 0 (east). */
unsigned char __fastcall__ x16_atan2 (signed char dx, signed char dy);

/* Linear interpolation between two unsigned bytes: t = 0 gives exactly
** a, t = 255 exactly b, and the midpoint is at most one off.
*/
unsigned char __fastcall__ x16_lerp8 (unsigned char a, unsigned char b,
                                      unsigned char t);

#endif /* X16_MATH_H */
