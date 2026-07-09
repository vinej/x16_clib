/* =====================================================================
 * x16clib :: x16/float.h -- floating point, via the ROM's FP library
 * =====================================================================
 * The X16 ROM carries a complete C128/C65-compatible floating point
 * library. This is a binding to it, not a reimplementation -- several
 * thousand bytes that cc65's own float support would otherwise link into
 * your program.
 *
 * Everything works on FAC, an implicit accumulator in the ROM's zero
 * page. So this reads as a sequence of operations, not as expressions:
 *
 *      x16_float a, b;
 *      char buf[X16_FP_STRLEN];
 *
 *      x16_f_from_s16(10);  x16_f_store(a);
 *      x16_f_from_s16(4);   x16_f_store(b);
 *      x16_f_load(a);
 *      x16_f_div(b);                       // FAC = 2.5
 *      x16_f_to_str_trim(buf);             // "2.5"
 *
 * COST. Every call crosses a ROM bank. For hot per-frame arithmetic use
 * x16_mul88() from <x16/fixed.h> instead.
 *
 * OPERAND ORDER. The ROM only offers `mem - FAC` and `mem / FAC`.
 * x16_f_sub() and x16_f_div() present the intuitive direction at the cost
 * of two extra bank crossings. x16_f_rsub() and x16_f_rdiv() expose the
 * raw order, which is what you want for 1/x.
 * =====================================================================
 */

#ifndef X16_FLOAT_H
#define X16_FLOAT_H

/* A float in memory. Five bytes; the sixth of FAC is its unpacked sign. */
#define X16_FP_SIZE     5
typedef unsigned char x16_float[X16_FP_SIZE];

/* Enough for anything the ROM formats, including sign and exponent. */
#define X16_FP_STRLEN   16

/* FAC = 0, -FAC, |FAC|, floor(FAC). */
void x16_f_zero (void);
void x16_f_neg (void);
void x16_f_abs (void);
void x16_f_int (void);

/* -1 if FAC < 0, 0 if zero, 1 if positive. */
signed char x16_f_sgn (void);

/* Conversions. x16_f_to_s16 rounds toward zero. */
void __fastcall__ x16_f_from_u8 (unsigned char v);
void __fastcall__ x16_f_from_s16 (int v);
int x16_f_to_s16 (void);

/* FAC <-> a 5-byte float in memory. */
void __fastcall__ x16_f_load (const x16_float m);
void __fastcall__ x16_f_store (x16_float m);

/* FAC op= m. */
void __fastcall__ x16_f_add (const x16_float m);
void __fastcall__ x16_f_sub (const x16_float m);
void __fastcall__ x16_f_mul (const x16_float m);
void __fastcall__ x16_f_div (const x16_float m);
void __fastcall__ x16_f_pow (const x16_float m);

/* FAC = m op FAC -- the ROM's own order, one bank crossing instead of
** three. x16_f_rdiv() is the reciprocal: load x, rdiv by 1.0, get 1/x.
*/
void __fastcall__ x16_f_rsub (const x16_float m);
void __fastcall__ x16_f_rdiv (const x16_float m);
void __fastcall__ x16_f_rpow (const x16_float m);

/* -1 if FAC < m, 0 if equal, 1 if FAC > m. */
signed char __fastcall__ x16_f_cmp (const x16_float m);

/* Each replaces FAC. sin, cos, tan and atan destroy ARG. */
void x16_f_sqrt (void);
void x16_f_ln (void);
void x16_f_exp (void);
void x16_f_sin (void);
void x16_f_cos (void);
void x16_f_tan (void);
void x16_f_atan (void);

/* Format FAC into `buf`, which must hold X16_FP_STRLEN bytes.
**
** The ROM writes its answer into the stack page and these copy it out,
** so the result is yours -- unlike the raw ROM call, whose buffer the
** next deep call or the next conversion would overwrite.
**
** x16_f_to_str keeps the leading space BASIC's PRINT puts before a
** positive number; x16_f_to_str_trim drops it.
*/
void __fastcall__ x16_f_to_str (char *buf);
void __fastcall__ x16_f_to_str_trim (char *buf);

/* Parse a decimal string, not NUL-terminated. */
void __fastcall__ x16_f_from_str (const char *s, unsigned char len);

#endif /* X16_FLOAT_H */
