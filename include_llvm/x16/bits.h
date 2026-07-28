/* =====================================================================
 * x16clib :: x16/bits.h -- bit and nibble helpers
 * =====================================================================
 * Masked read-modify-write on a byte in memory, plus nibble packing.
 * C can of course write `*p |= mask` itself; these exist so C and
 * assembly callers share one implementation, and because x16_bit_put
 * turns a flag into a set-or-clear without a branch at the call site.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** llvm-mos build. The API is identical to the cc65 build's; only the
** calling convention differs, and llvm-mos expresses it in the compiler
** rather than in the declaration. So there is no __fastcall__ here.
** --------------------------------------------------------------------- */

#ifndef X16_BITS_H
#define X16_BITS_H

/* Set (OR) or clear (AND NOT) the masked bits of *addr. */
void x16_bit_set (unsigned char *addr, unsigned char mask);
void x16_bit_clr (unsigned char *addr, unsigned char mask);

/* on != 0 sets the masked bits, on == 0 clears them. */
void x16_bit_put (unsigned char *addr, unsigned char mask,
                  unsigned char on);

/* Returns *addr & mask: nonzero iff any masked bit is set. */
unsigned char x16_bit_test (const unsigned char *addr,
                            unsigned char mask);

/* Nibble helpers: the high or low four bits of v, in bits 3:0. */
unsigned char x16_hinib (unsigned char v);
unsigned char x16_lonib (unsigned char v);

/* (hi << 4) | lo, both masked to their nibble first. */
unsigned char x16_catnib (unsigned char hi, unsigned char lo);

#endif /* X16_BITS_H */
