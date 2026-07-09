/* =====================================================================
 * x16clib :: x16/verafx.h -- VERA FX: hardware multiply, fast fills
 * =====================================================================
 * Requires VERA firmware v0.3.1 or later (emulator R44+).
 *
 *      if (x16_vera_has_fx()) { ... }
 *
 * Call that FIRST. On older VERA these routines write to registers that
 * do not exist, and quietly do the wrong thing rather than failing.
 *
 * Every routine here leaves FX disabled and DCSEL back at 0, so ordinary
 * VRAM addressing keeps behaving for everyone downstream.
 *
 * The multiplier has no result register: triggering it writes four bytes
 * to VRAM. They land at VRAM $1F800, an unused corner of the map, and are
 * read straight back. So x16_fx_mult() clobbers four bytes there.
 * =====================================================================
 */

#ifndef X16_VERAFX_H
#define X16_VERAFX_H

/* Where x16_fx_mult() parks its 32-bit result before reading it back. */
#define X16_VRAM_FX_SCRATCH     0x1F800UL

/* Disable FX, restore DCSEL. Safe whether or not FX was ever enabled;
** the other routines already do this on the way out.
*/
void x16_fx_off (void);

/* Signed 16 x 16 -> 32, in hardware. Far faster than x16_umul16(), and
** signed, but it costs four bytes of VRAM scratch.
*/
long __fastcall__ x16_fx_mult (int a, int b);

/* Fill `count` bytes of VRAM at `addr` with `value`, about four times
** faster than a byte loop: the 32-bit write cache stores four bytes per
** access. A count that is not a multiple of four is finished off one
** byte at a time.
**
** `addr` comes last so cc65 passes all four bytes in registers.
*/
void __fastcall__ x16_fx_fill (unsigned char value, unsigned int count,
                               unsigned long addr);

void __fastcall__ x16_fx_clear (unsigned int count, unsigned long addr);

#endif /* X16_VERAFX_H */
