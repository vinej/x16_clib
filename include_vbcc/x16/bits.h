/* =====================================================================
 * x16clib :: x16/bits.h -- bit and nibble helpers (vbcc)
 * =====================================================================
 * Masked read-modify-write on a byte in memory, plus nibble packing.
 * C can of course write `*p |= mask` itself; these exist so C and
 * assembly callers share one implementation, and because x16_bit_put
 * turns a flag into a set-or-clear without a branch at the call site.
 * =====================================================================
 */

#ifndef X16_BITS_H
#define X16_BITS_H

/* Set (OR) or clear (AND NOT) the masked bits of *addr. */
void x16_bit_set(__reg("r0/r1") unsigned char *addr,
                 __reg("r2") unsigned char mask);
void x16_bit_clr(__reg("r0/r1") unsigned char *addr,
                 __reg("r2") unsigned char mask);

/* on != 0 sets the masked bits, on == 0 clears them. */
void x16_bit_put(__reg("r0/r1") unsigned char *addr,
                 __reg("r2") unsigned char mask,
                 __reg("r4") unsigned char on);

/* Returns *addr & mask: nonzero iff any masked bit is set. */
unsigned char x16_bit_test(__reg("r0/r1") const unsigned char *addr,
                           __reg("r2") unsigned char mask);

/* Nibble helpers: the high or low four bits of v, in bits 3:0. */
unsigned char x16_hinib(__reg("a") unsigned char v);
unsigned char x16_lonib(__reg("a") unsigned char v);

/* (hi << 4) | lo, both masked to their nibble first. */
unsigned char x16_catnib(__reg("r0") unsigned char hi,
                         __reg("r2") unsigned char lo);

#endif /* X16_BITS_H */
