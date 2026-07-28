/* =====================================================================
 * x16clib :: x16/number.h -- number formatting and parsing
 * =====================================================================
 * Decimal, hex and binary rendering without printf's footprint, plus a
 * decimal parser.
 *
 * ALL CONVERSIONS SHARE ONE MODULE BUFFER. The returned pointer aims
 * into it, the string is NUL-terminated, and the NEXT CALL OVERWRITES
 * IT -- copy the text out if you need to keep it. The bytes are ASCII
 * (digits, 'A'-'F', '-'), the same values the upstream assembly
 * library produced.
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

#ifndef X16_NUMBER_H
#define X16_NUMBER_H

#include <x16/zpsafe.h>

/* Decimal, no leading zeros ("0" for zero). The signed forms prepend
** '-' to the magnitude, so -32768 renders as "-32768".
*/
char * x16_u8_to_dec  (unsigned char v);
char * x16_u16_to_dec (unsigned int v);
char * x16_s8_to_dec  (signed char v);
char * x16_s16_to_dec (int v);

/* Fixed-width uppercase hex: always 2 (resp. 4) digits. */
char * x16_u8_to_hex  (unsigned char v);
char * x16_u16_to_hex (unsigned int v);

/* Fixed-width binary, MSB first: always 8 (resp. 16) digits. */
char * x16_u8_to_bin  (unsigned char v);
char * x16_u16_to_bin (unsigned int v);

/* Parse len decimal digits from s into *value. Returns 1 on success,
** 0 if a non-digit was found or the value overflowed 16 bits (*value
** is untouched on failure). len is the exact digit count -- there is
** no terminator scan, so a slice of a longer string works.
*/
unsigned char x16_dec_to_u16 (const char *s, unsigned char len,
                              unsigned int *value);

#endif /* X16_NUMBER_H */
