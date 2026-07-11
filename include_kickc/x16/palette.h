/* =====================================================================
 * x16clib :: x16/palette.h -- VERA palette
 * =====================================================================
 * 256 entries of 12-bit colour at VRAM $1FA00, two bytes each:
 *      byte 0 = Green<<4 | Blue
 *      byte 1 = Red             (high nibble unused)
 *
 * So a 12-bit $0RGB colour stores little-endian exactly as written:
 * 0x0F00 is pure red, 0x00F0 pure green, 0x000F pure blue. That is why
 * the API takes the colour as one 16-bit word.
 *
 * Caution: the palette lives in VERA's write-only window. Reading an
 * entry returns the last value the host wrote there, not what the
 * hardware holds -- fine for reading back your own writes, useless for
 * discovering the state after a reset.
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

#ifndef X16_PALETTE_H
#define X16_PALETTE_H

#include <x16/zpsafe.h>

/* Set one entry.
**      x16_pal_set(1, 0x0F00);          entry 1 becomes pure red
*/
void x16_pal_set (unsigned char index, unsigned int color);

/* Bulk-load entries from RAM: `count` 16-bit colours starting at `src`,
** written to entries `first` .. `first + count - 1`.
**
** count is 1-128. A count of 0 loads nothing.
*/
void x16_pal_load (const unsigned int *src, unsigned char first,
                   unsigned char count);

#endif /* X16_PALETTE_H */
