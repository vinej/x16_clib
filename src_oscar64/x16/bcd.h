/* =====================================================================
 * x16clib :: x16/bcd.h -- packed-BCD add and subtract
 * =====================================================================
 * Decimal arithmetic on packed BCD, so 8-, 16- and 32-bit values add
 * and subtract the way you read them:
 *
 *      0x0987 + 0x1111 = 0x2098        (not the binary 0x1A98)
 *
 * Each byte holds two decimal digits. The point is to skip the costly
 * binary->decimal conversion a game score or clock would otherwise
 * need every frame: keep the count in BCD and print its hex form
 * (x16_u16_to_hex), which already reads as decimal.
 *
 * Signed and unsigned share one routine per width -- decimal add/sub
 * does not know the difference. Pick the width; the interpretation is
 * yours.
 *
 * Unlike the cc65 build these do NOT run the CPU's decimal mode -- the
 * digit math is done bytewise -- so there is no interrupt caveat here.
 * =====================================================================
 */

#ifndef X16_BCD_H
#define X16_BCD_H


/* *a += b in packed BCD. Returns 1 if the sum overflowed the width
** (the BCD carry), 0 otherwise; *a keeps the wrapped digits either way.
*/
unsigned char x16_bcd_add8  (unsigned char *a, unsigned char b);
unsigned char x16_bcd_add16 (unsigned int *a, unsigned int b);
unsigned char x16_bcd_add32 (unsigned long *a, unsigned long b);

/* *a -= b in packed BCD. Returns 1 on borrow (the result wrapped below
** zero), 0 otherwise.
*/
unsigned char x16_bcd_sub8  (unsigned char *a, unsigned char b);
unsigned char x16_bcd_sub16 (unsigned int *a, unsigned int b);
unsigned char x16_bcd_sub32 (unsigned long *a, unsigned long b);

/* The upstream in-place forms: value points at a 4-byte packed-BCD
** buffer, low byte first -- a score kept as raw bytes rather than an
** unsigned long. Same result and returns as add32/sub32.
*/
unsigned char x16_bcd_addto   (unsigned char *value, unsigned long b);
unsigned char x16_bcd_subfrom (unsigned char *value, unsigned long b);

/* pulls the implementation in with this header */
#pragma compile("bcd.c")

#endif
