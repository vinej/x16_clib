/* =====================================================================
 * x16clib :: x16/bits.h -- bit and nibble helpers
 * =====================================================================
 * Masked read-modify-write on a byte in memory, plus nibble packing.
 * C can of course write `*p |= mask` itself; these exist so programs
 * port across the toolchains unchanged, and because x16_bit_put turns
 * a flag into a set-or-clear without a branch at the call site.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** KickC build. The API is identical to the cc65 build's; what differs is
** the delivery. KickC has no linker and no archive format -- it compiles
** the whole program from source and strips what goes unused -- so the
** KickC port is a SOURCE distribution. Include this header; the matching
** implementation in src_kickc/x16/ is compiled in automatically when the
** library path points there:
**
**     kickc -p cx16 -a -I include_kickc -L src_kickc yourprog.c
** --------------------------------------------------------------------- */

#ifndef X16_BITS_H
#define X16_BITS_H

#include <x16/zpsafe.h>

/* Set (OR) or clear (AND NOT) the masked bits of *addr. */
void x16_bit_set (unsigned char *addr, unsigned char mask);
void x16_bit_clr (unsigned char *addr, unsigned char mask);

/* on != 0 sets the masked bits, on == 0 clears them. */
void x16_bit_put (unsigned char *addr, unsigned char mask,
                  unsigned char on);

/* Returns *addr & mask: nonzero iff any masked bit is set. */
unsigned char x16_bit_test (const unsigned char *addr, unsigned char mask);

/* Nibble helpers: the high or low four bits of v, in bits 3:0. */
unsigned char x16_hinib (unsigned char v);
unsigned char x16_lonib (unsigned char v);

/* (hi << 4) | lo, both masked to their nibble first. */
unsigned char x16_catnib (unsigned char hi, unsigned char lo);

#endif /* X16_BITS_H */
