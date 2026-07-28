/* =====================================================================
 * x16clib :: x16/number.h -- number formatting and parsing (vbcc)
 * =====================================================================
 * Decimal, hex and binary rendering without printf's footprint, plus a
 * decimal parser.
 *
 * ALL CONVERSIONS SHARE ONE MODULE BUFFER. The returned pointer aims
 * into it, the string is NUL-terminated, and the NEXT CALL OVERWRITES
 * IT -- copy the text out if you need to keep it. The bytes are ASCII
 * (digits, 'A'-'F', '-'), the same values the upstream assembly
 * library produced -- NOT the PETSCII that -cbmascii makes of a C
 * literal, so compare against explicit bytes, not against "A5".
 * =====================================================================
 */

#ifndef X16_NUMBER_H
#define X16_NUMBER_H

/* Decimal, no leading zeros ("0" for zero). The signed forms prepend
** '-' to the magnitude, so -32768 renders as "-32768". */
char *x16_u8_to_dec(__reg("a") unsigned char v);
char *x16_u16_to_dec(__reg("r0/r1") unsigned int v);
char *x16_s8_to_dec(__reg("a") signed char v);
char *x16_s16_to_dec(__reg("r0/r1") int v);

/* Fixed-width uppercase hex: always 2 (resp. 4) digits. */
char *x16_u8_to_hex(__reg("a") unsigned char v);
char *x16_u16_to_hex(__reg("r0/r1") unsigned int v);

/* Fixed-width binary, MSB first: always 8 (resp. 16) digits. */
char *x16_u8_to_bin(__reg("a") unsigned char v);
char *x16_u16_to_bin(__reg("r0/r1") unsigned int v);

/* Parse len decimal digits from s into *value. Returns 1 on success,
** 0 if a non-digit was found or the value overflowed 16 bits (*value
** is untouched on failure). len is the exact digit count -- there is
** no terminator scan, so a slice of a longer string works. */
unsigned char x16_dec_to_u16(__reg("r0/r1") const char *s,
                             __reg("r2") unsigned char len,
                             __reg("r4/r5") unsigned int *value);

#endif /* X16_NUMBER_H */
