/* =====================================================================
 * x16clib :: x16/int32.h -- 32-bit integer arithmetic
 * =====================================================================
 * WHY THIS EXISTS IN A C LIBRARY. Oscar64 has a native 32-bit long, and
 * for a plain a + b the compiler's operator is the right tool. The
 * module is here anyway: the project ships FULL parity with the
 * upstream assembly library (its DOUBLE.TXT surface), so both sides
 * see the same routines and the same tested code paths -- and the
 * composites earn their keep in C. x16_i32_divmod() hands back
 * quotient AND remainder from ONE long division, where `/` and `%`
 * each run their own -- at 32 bits that second division is real
 * money -- and x16_i32_to_dec() renders decimal without printf's
 * footprint.
 *
 * Signed and unsigned share add, subtract, negate, multiply and the
 * shifts: two's complement makes them identical. Only comparison,
 * division and decimal output need to know which you meant -- and the
 * upstream library carries no signed divide or signed renderer at 32
 * bits, so neither does this port.
 * =====================================================================
 */

#ifndef X16_INT32_H
#define X16_INT32_H

/* Widening and narrowing. A C cast does the same; the parity entries
** exercise the library's own path. x16_i32_to_s16() simply drops the
** top two bytes.
*/
long x16_i32_from_u16 (unsigned int v);  /* zero-extend */
long x16_i32_from_s16 (int v);           /* sign-extend */
int  x16_i32_to_s16 (long v);

/* Two's complement: signed and unsigned share all five. */
long x16_i32_add (long a, long b);
long x16_i32_sub (long a, long b);
long x16_i32_neg (long a);
long x16_i32_abs (long a);   /* |-2147483648| stays put */
long x16_i32_mul (long a, long b);

/* Shift by ONE, three ways: left, logical right (zero fill), and
** arithmetic right (sign fill).
*/
long          x16_i32_shl (long a);
unsigned long x16_i32_shr (unsigned long a);
long          x16_i32_asr (long a);

/* -1 if a < b, 0 if equal, 1 if a > b. */
signed char x16_i32_cmpu (unsigned long a, unsigned long b);
signed char x16_i32_cmps (long a, long b);

/* One division, both results: the quotient comes back, the remainder
** lands in *rem. Division by zero changes nothing: the call returns a
** and *rem is left untouched.
*/
unsigned long x16_i32_divmod (unsigned long a, unsigned long b,
                                        unsigned long *rem);

/* ASCII decimal ('0'-'9' as $30-$39), NUL-terminated, no leading
** zeros. The string lives in a module buffer that the NEXT CALL
** OVERWRITES -- copy it out if you need to keep it.
*/
char * x16_i32_to_dec (unsigned long v);

/* pulls the implementation in with this header */
#pragma compile("int32.c")

#endif /* X16_INT32_H */
