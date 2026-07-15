/* =====================================================================
 * x16clib :: x16/math.h -- game math: PRNG, sine tables, atan2, lerp
 * =====================================================================
 * vbcc has no trig table anywhere and rand() from its libvc is a general
 * portable LCG. These are the routines a game loop rewrites.
 *
 * ANGLES ARE BYTES. A full circle is 256, so 64 is 90 degrees and
 * wrap-around is free. The X16's y axis points DOWN the screen, so angle
 * 0 is east (+x) and 64 is south (+y). atan2 and the sine tables agree
 * on that, which means a heading from x16_atan2() feeds straight back in:
 *
 *      a = x16_atan2(tx - x, ty - y);          // aim at the target
 *      x += (x16_sin8(a + 64) * speed) >> 7;   // cos
 *      y += (x16_sin8(a) * speed) >> 7;
 *
 * ABI NOTE. Each prototype pins its arguments to the zero-page pseudo-
 * registers the hand-written routine already reads (see src_vbcc). vbcc
 * places them there at the call site, so the "shim" is usually a single
 * jmp. The a/x pair carries a 16-bit value low-in-a, high-in-x.
 * =====================================================================
 */

#ifndef X16_MATH_H
#define X16_MATH_H

/* Seed the generator. A seed of 0 is nudged to 1: it is xorshift's one
** fixed point. Seed from x16_irq_frames(), or from the KERNAL's
** entropy, if you want a different sequence each run.
*/
void x16_rnd_seed(__reg("a/x") unsigned int seed);

/* John Metcalf's 16-bit xorshift: period 65535, a handful of cycles.
** Cheap enough to call per object per frame.
*/
unsigned char x16_rnd8(void);
unsigned int  x16_rnd16(void);

/* -127..127. */
signed char x16_sin8(__reg("a") unsigned char angle);
signed char x16_cos8(__reg("a") unsigned char angle);

/* 1..255 -- the signed value biased by 128, handy for volumes and
** scales that must not go negative.
*/
unsigned char x16_sin8u(__reg("a") unsigned char angle);
unsigned char x16_cos8u(__reg("a") unsigned char angle);

/* The angle of a vector, 0-255. atan2(0,0) answers 0 (east). (dy travels
** in r0 because vbcc will not pass an argument in x.) */
unsigned char x16_atan2(__reg("a") signed char dx, __reg("r0") signed char dy);

/* Linear interpolation between two unsigned bytes: t = 0 gives exactly
** a, t = 255 exactly b, and the midpoint is at most one off.
*/
unsigned char x16_lerp8(__reg("r0") unsigned char a, __reg("r1") unsigned char b,
                        __reg("a") unsigned char t);

#endif /* X16_MATH_H */
