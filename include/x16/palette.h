/* =====================================================================
 * x16clib :: x16/palette.h -- VERA palette
 * =====================================================================
 * 256 entries of 12-bit colour, one 16-bit word each: 0x0RGB, four bits
 * per channel. 0x0F00 is pure red, 0x00F0 pure green, 0x000F pure blue.
 *
 * The palette lives in VERA's write-only region. Reading an entry back
 * returns the last value the host wrote there, not what the hardware
 * holds -- fine for checking your own writes, useless for discovering
 * the state after a reset.
 * =====================================================================
 */

#ifndef X16_PALETTE_H
#define X16_PALETTE_H

/* Set one entry.
**      x16_pal_set(1, 0x0F00);          entry 1 becomes pure red
*/
void __fastcall__ x16_pal_set (unsigned char index, unsigned int color);

/* Bulk-load entries from RAM: `count` 16-bit colours starting at `src`,
** written to entries `first` .. `first + count - 1`.
**
** count is 1-128. A count of 0 loads nothing.
*/
void __fastcall__ x16_pal_load (const unsigned int *src,
                                unsigned char first,
                                unsigned char count);

#endif /* X16_PALETTE_H */
