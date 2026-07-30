/* =====================================================================
 * x16clib :: x16/double.h -- 64-bit software floating point (binary64)
 * =====================================================================
 * <x16/float.h> binds the ROM's 5-byte float: about 9 significant
 * digits, fine for graphics, thin for a calculator. This module is a
 * from-scratch IEEE-754 double -- 8 bytes, ~15-16 significant digits,
 * the full 1e+/-308 range -- implemented entirely in software (the ROM
 * has nothing to lean on), with add/sub/mul/div, comparison, sqrt,
 * exp/ln/pow, trig, hyperbolics and decimal string I/O.
 *
 * THE SHAPE MIRRORS <x16/float.h>: everything works on an implicit
 * floating accumulator (call it DAC), so the API reads as a sequence
 * of operations rather than as expressions:
 *
 *      x16_double a, b;
 *      x16_d_from_s16(7);  x16_d_store(a);
 *      x16_d_from_s16(2);  x16_d_store(b);
 *      x16_d_load(a);
 *      x16_d_div(b);                       // DAC = 3.5
 *      x16_d_to_str();                     // "3.5"
 *
 * THE FORMAT IS EXACTLY IEEE-754 binary64, LITTLE-ENDIAN: b[0] is the
 * low mantissa byte, b[7] holds the sign bit and the top seven
 * exponent bits. 1.0 is {0,0,0,0,0,0,0xF0,0x3F}. A dump from any
 * modern machine's `double` is byte-for-byte compatible. Subnormals
 * are flushed to zero, overflow makes an infinity, and rounding is
 * round-to-nearest-even. NaN and +/-inf are honoured by every
 * operation (a NaN compares unordered and answers 1).
 *
 * COST. Every operation is software 64-bit arithmetic -- hundreds of
 * times slower than 8.8 fixed point. This is for calculators and
 * offline precision, not per-frame math; see <x16/fixed.h> for that.
 * =====================================================================
 */

#ifndef X16_DOUBLE_H
#define X16_DOUBLE_H

/* A double in memory: 8 bytes, little-endian IEEE-754 binary64. */
#define X16_D_SIZE      8
typedef unsigned char x16_double[X16_D_SIZE];

/* Enough for anything x16_d_to_str() formats: sign, 16 digits, point,
** exponent, terminator.
*/
#define X16_D_STRLEN    26

/* DAC <-> an 8-byte double in memory. */
void x16_d_load (const x16_double m);
void x16_d_store (x16_double m);

/* DAC = -DAC, |DAC|. Pure sign-bit operations (so they apply to
** infinities and NaNs too).
*/
void x16_d_neg (void);
void x16_d_abs (void);

/* Integer conversions. Both widths convert exactly (32 bits fit the
** 53-bit mantissa). x16_d_to_s32() truncates toward zero; a NaN or an
** infinity answers 0, and an out-of-range value clamps to
** 2147483647 / -2147483648L (the assembly-level overflow carry is not
** visible from C).
*/
void x16_d_from_s16 (int v);
void x16_d_from_s32 (long v);
long x16_d_to_s32 (void);

/* -1 if DAC < *m, 0 if equal, 1 if DAC > *m. Zeroes of either sign
** are equal; a NaN on either side is unordered and answers 1.
*/
signed char x16_d_cmp (const x16_double m);

/* DAC op= *m, correctly rounded (round-to-nearest-even). The one
** caveat: a subtraction that cancels almost the whole significand can
** land 1 ulp loose; ordinary sums are exact to the rounding.
*/
void x16_d_add (const x16_double m);
void x16_d_sub (const x16_double m);
void x16_d_mul (const x16_double m);
void x16_d_div (const x16_double m);

/* DAC = DAC ^ *m, via exp(m * ln DAC). m == 0 answers exactly 1 for
** any base; otherwise a base <= 0 yields NaN or inf through the log
** (there is no integer-power special case).
*/
void x16_d_pow (const x16_double m);

/* Each replaces DAC.
**
** sqrt: a bit-hack first guess plus six Newton iterations -- full
**   precision, and exact for perfect squares (sqrt(4) is exactly 2);
**   negative operands answer NaN.
** exp/ln: range-reduced Taylor series. ln(0) = -inf, ln(x<0) = NaN.
** sin/cos/tan: argument reduced by pi/2 with a single subtraction, so
**   a huge argument loses precision; sin(0) = 0 and cos(0) = 1
**   exactly. tan is sin/cos. NaN and inf answer NaN.
** atan: folds to [0, tan(pi/12)] then a fast series; +/-inf answers
**   +/-pi/2.
** sinh/cosh/tanh: built on exp; tanh saturates to +/-1 for |x| >= 20.
*/
void x16_d_sqrt (void);
void x16_d_exp (void);
void x16_d_ln (void);
void x16_d_sin (void);
void x16_d_cos (void);
void x16_d_tan (void);
void x16_d_atan (void);
void x16_d_sinh (void);
void x16_d_cosh (void);
void x16_d_tanh (void);

/* Parse [+/-] digits [ . digits ] [ (E|e) [+/-] digits ] -- len is an
** explicit count, no terminator scan, so a slice of a longer string
** works. Scaling by the decimal exponent rounds once per power of
** ten, so a long mantissa can land a unit in the last place off; the
** exactly-representable values (halves, quarters, small integers)
** parse exactly.
**
** THE BYTES ARE ASCII: digits $30-$39, and the exponent marker is $45
** 'E' or $65 'e'. Oscar64's -t cx16 target maps C string literals to
** PETSCII by default -- include <ascii_charmap.h> (or spell bytes
** explicitly) when the literal carries letters.
*/
void x16_d_from_str (const char *s, unsigned char len);

/* Format DAC as a NUL-terminated ASCII decimal string: fixed notation
** while the exponent is in -4..20, scientific "d.dddE+NN" beyond;
** trailing zeros stripped; "INF", "-INF" and "NAN" for the specials
** (ASCII capitals). Up to 16 significant digits; exact short values
** print exactly.
**
** The string lives in a module buffer (X16_D_STRLEN bytes) that the
** NEXT CALL OVERWRITES -- copy it out if you need to keep it.
*/
char * x16_d_to_str (void);

/* pulls the implementation in with this header */
#pragma compile("double.c")

#endif /* X16_DOUBLE_H */
