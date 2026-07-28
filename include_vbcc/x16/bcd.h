/* =====================================================================
 * x16clib :: x16/bcd.h -- packed-BCD (decimal-mode) add and subtract
 * =====================================================================
 * Decimal arithmetic through the 65C02's BCD mode, so 8-, 16- and
 * 32-bit packed-BCD values add and subtract the way you read them:
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
 * INTERRUPTS: these run in decimal mode across the operation. The
 * KERNAL's IRQ handler is decimal-safe, so ordinary use is fine. A
 * CUSTOM interrupt handler doing its own arithmetic must clear decimal
 * mode first (the x16_irq_* dispatcher does).
 * =====================================================================
 */

#ifndef X16_BCD_H
#define X16_BCD_H

/* *a += b in packed BCD. Returns 1 if the sum overflowed the width
** (the BCD carry), 0 otherwise; *a keeps the wrapped digits either way.
** (The 32-bit b is a plain long: vbcc passes it in btmp0.) */
unsigned char x16_bcd_add8(__reg("r0/r1") unsigned char *a,
                           __reg("r2") unsigned char b);
unsigned char x16_bcd_add16(__reg("r0/r1") unsigned int *a,
                            __reg("r2/r3") unsigned int b);
unsigned char x16_bcd_add32(__reg("r0/r1") unsigned long *a,
                            unsigned long b);

/* *a -= b in packed BCD. Returns 1 on borrow (the result wrapped below
** zero), 0 otherwise. */
unsigned char x16_bcd_sub8(__reg("r0/r1") unsigned char *a,
                           __reg("r2") unsigned char b);
unsigned char x16_bcd_sub16(__reg("r0/r1") unsigned int *a,
                            __reg("r2/r3") unsigned int b);
unsigned char x16_bcd_sub32(__reg("r0/r1") unsigned long *a,
                            unsigned long b);

/* The upstream in-place forms: value points at a 4-byte packed-BCD
** buffer, low byte first -- a score kept as raw bytes rather than an
** unsigned long. Same result and returns as add32/sub32. */
unsigned char x16_bcd_addto(__reg("r0/r1") unsigned char *value,
                            unsigned long b);
unsigned char x16_bcd_subfrom(__reg("r0/r1") unsigned char *value,
                              unsigned long b);

#endif /* X16_BCD_H */
