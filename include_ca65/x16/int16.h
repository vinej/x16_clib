/* =====================================================================
 * x16clib :: x16/int16.h -- 16-bit integer arithmetic
 * =====================================================================
 * WHY THIS EXISTS IN A C LIBRARY. C has a native 16-bit int, and for a
 * plain a + b the compiler's operator is the right tool -- x16_i16_add()
 * computes nothing cc65's `+` does not. The module is here anyway, for
 * two reasons. First, the project ships FULL parity with the upstream
 * assembly library, so an assembly-side caller and a C-side caller see
 * the same surface and the same tested code paths. Second, the
 * composites really do earn their keep in C: x16_i16_divmod() hands
 * back quotient AND remainder from one division (cc65's `/` and `%`
 * divide twice), x16_i16_sqrt() is an integer square root the language
 * simply lacks, and x16_i16_to_dec() renders decimal without printf's
 * footprint.
 *
 * The single-shift, compare and widening entries are the parity layer:
 * use them when you want the library's exact semantics (documented
 * per function), use the C operator when you just want arithmetic.
 * =====================================================================
 */

#ifndef X16_INT16_H
#define X16_INT16_H

/* Widening. A C cast does the same; the parity entries exercise the
** library's own path.
*/
int __fastcall__ x16_i16_from_u8 (unsigned char v);   /* zero-extend */
int __fastcall__ x16_i16_from_s8 (signed char v);     /* sign-extend */

/* Two's complement makes add, subtract, negate and multiply identical
** for signed and unsigned: pass either, take the bits. x16_i16_mul()
** keeps only the low 16 bits of the product -- for the full 32-bit
** product use x16_umul16() from <x16/fixed.h>.
*/
int __fastcall__ x16_i16_add (int a, int b);
int __fastcall__ x16_i16_sub (int a, int b);
int __fastcall__ x16_i16_neg (int a);
int __fastcall__ x16_i16_abs (int a);         /* |-32768| stays -32768 */
int __fastcall__ x16_i16_mul (int a, int b);

/* Shift by ONE, three ways: left, logical right (zero fill), and
** arithmetic right (sign fill). The arithmetic form is the one C
** leaves implementation-defined for negative values.
*/
int          __fastcall__ x16_i16_shl (int a);
unsigned int __fastcall__ x16_i16_shr (unsigned int a);
int          __fastcall__ x16_i16_asr (int a);

/* -1 if a < b, 0 if equal, 1 if a > b. */
signed char __fastcall__ x16_i16_cmpu (unsigned int a, unsigned int b);
signed char __fastcall__ x16_i16_cmps (int a, int b);

/* One division, both results: the quotient comes back, the remainder
** lands in *rem. The signed form truncates toward zero and the
** remainder takes the sign of the DIVIDEND, exactly as C's / and %
** do: -7 / 2 is -3 remainder -1.
**
** Division by zero changes nothing: the call returns a and *rem is
** left untouched.
*/
unsigned int __fastcall__ x16_i16_divmod (unsigned int a, unsigned int b,
                                          unsigned int *rem);
int __fastcall__ x16_i16_divmod_s (int a, int b, int *rem);

/* floor(sqrt(v)), 0..255. */
unsigned char __fastcall__ x16_i16_sqrt (unsigned int v);

/* ASCII decimal ('0'-'9' as $30-$39, '-' as $2D), NUL-terminated, no
** leading zeros; the signed form prepends '-' to the magnitude, so
** -32768 renders as "-32768". The string lives in a module buffer
** that the NEXT CALL OVERWRITES -- copy it out if you need to keep it.
*/
char * __fastcall__ x16_i16_to_dec   (unsigned int v);
char * __fastcall__ x16_i16_to_dec_s (int v);

#endif /* X16_INT16_H */
