/* =====================================================================
 * x16clib :: x16/verafx_utils.h -- low-level VERA FX primitives
 * =====================================================================
 * Raw building blocks for custom FX workflows: FX_CTRL/MULT control,
 * cache fill/write/cycle toggles, 32-bit cache loading, multiplier
 * accumulator triggers, increment/position registers, 16-bit hop, and
 * polygon-fill reads.
 *
 * These are deliberately separate from x16/verafx.h. That header is the
 * high-level helper bundle -- multiply, fill, line, triangle -- each of
 * which programs the FX registers, does its job and switches FX back
 * off. This one is for code that wants to compose the documented FX
 * registers directly, one knob at a time, when no canned helper fits.
 *
 * The same capability rule applies: probe with x16_vera_has_fx() FIRST.
 * On older VERA these routines write to registers that do not exist,
 * and quietly do the wrong thing rather than failing.
 *
 * Two things to know:
 *
 *   Nothing here resets FX_CTRL on the way out. Every routine restores
 *   DCSEL to 0 (and leaves ADDRSEL alone), but the FX state you set
 *   STAYS SET -- that is the point. Call x16_fxu_off() when done, or
 *   ordinary VRAM addressing keeps behaving strangely for everyone
 *   downstream.
 *
 *   The x16_fx_* helpers clear FX_CTRL and FX_MULT themselves, so mixing
 *   the two levels works in one direction only: compose with fxu_*,
 *   then a call to any high-level helper wipes the slate.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** Oscar64 build. The API is identical to the cc65 build's; what differs
** is the delivery. Oscar64 compiles the whole program at once and strips
** what goes unused, so this port is a SOURCE distribution: headers and
** implementations sit side by side in src_oscar64/x16/, and the
** `#pragma compile` at the bottom of this header pulls the matching .c
** in automatically:
**
**     oscar64 -tm=x16 -n -i=src_oscar64 -o=YOURPROG.PRG yourprog.c
** --------------------------------------------------------------------- */

#ifndef X16_VERAFX_UTILS_H
#define X16_VERAFX_UTILS_H

/* Disable the FX helpers: FX_CTRL and FX_MULT to 0, DCSEL back to 0.
** Safe whether or not anything was ever enabled.
*/
void x16_fxu_off (void);

/* ---------------------------------------------------------------------
 * FX_CTRL -- the master control byte (DCSEL=2).
 *
 * Bits 1:0 are the Addr1 Mode (0 normal, 1 line, 2 polygon, 3 affine);
 * the rest are the flags the toggles below name. Compose values from
 * the VERA FX reference, or use the per-flag helpers and never build a
 * mask at all.
 * ------------------------------------------------------------------ */

unsigned char x16_fxu_get_ctrl (void);

/* Replace the whole byte / OR bits in / AND bits out. */
void x16_fxu_set_ctrl (unsigned char v);
void x16_fxu_ctrl_on (unsigned char bits);
void x16_fxu_ctrl_off (unsigned char bits);

/* Set only the Addr1 Mode field (bits 1:0), leaving every flag alone. */
void x16_fxu_addr1_mode (unsigned char mode);

/* The FX_CTRL flags, one pair each. Cache write: a DATA0/1 store writes
** the whole 32-bit cache. Cache fill: a DATA0/1 read latches the byte
** into the cache. Cache cycle, transparency (zero bytes leave VRAM
** alone), 4-bit mode and the 16-bit hop are the reference's remaining
** flags, verbatim.
*/
void x16_fxu_cache_write_on (void);
void x16_fxu_cache_write_off (void);
void x16_fxu_cache_fill_on (void);
void x16_fxu_cache_fill_off (void);
void x16_fxu_cache_cycle_on (void);
void x16_fxu_cache_cycle_off (void);
void x16_fxu_transparent_on (void);
void x16_fxu_transparent_off (void);
void x16_fxu_4bit_on (void);
void x16_fxu_4bit_off (void);
void x16_fxu_hop_on (void);
void x16_fxu_hop_off (void);

/* ---------------------------------------------------------------------
 * FX_MULT and the 32-bit cache (DCSEL=2 and 6).
 * ------------------------------------------------------------------ */

/* Write the FX_MULT byte: multiplier enable, subtract, accumulate,
** index controls -- the reference's bitfield, uninterpreted.
*/
void x16_fxu_set_mult (unsigned char v);

/* Load all four bytes of the 32-bit cache: L, M, H, U. With the
** multiplier on, L/M and H/U are its two signed 16-bit operands.
*/
void x16_fxu_set_cache (unsigned char l, unsigned char m,
                        unsigned char h, unsigned char u);

/* Clear the multiplier accumulator / trigger multiply-then-accumulate.
** Both are READ-triggered registers; these wrap the reads.
*/
void x16_fxu_reset_accum (void);
void x16_fxu_accumulate (void);

/* One read of DATA0/DATA1 -- filling the cache when Cache Fill is on --
** returning the byte read. And one write of the given cache nibble mask
** to DATA0/DATA1, flushing the cache when Cache Write is on (mask 0
** writes all four bytes).
*/
unsigned char x16_fxu_cache_fill0 (void);
unsigned char x16_fxu_cache_fill1 (void);
void x16_fxu_cache_write0 (unsigned char mask);
void x16_fxu_cache_write1 (unsigned char mask);

/* ---------------------------------------------------------------------
 * Increments, positions, affine bases, polygon readback (DCSEL=3,4,5).
 * ------------------------------------------------------------------ */

/* The X/Y increment registers: 15-bit signed 6.9 fixed point in
** 1/512ths, bit 15 = x32 -- the line/poly/affine step encoding.
*/
void x16_fxu_set_incr (unsigned int xi, unsigned int yi);

/* The X/Y position registers (10 bits each) and their subpixel bytes. */
void x16_fxu_set_pos (unsigned int x, unsigned int y);
void x16_fxu_set_subpos (unsigned char xs, unsigned char ys);

/* Read POLY_FILL_L/H, the polygon helper's per-row answer: low byte in
** the low half, high byte in the high half. See the VERA FX reference
** for the bit layout -- it changes meaning with the 4-bit flag.
*/
unsigned int x16_fxu_get_poly_fill (void);

/* Raw writes of a precomposed FX_TILEBASE/FX_MAPBASE byte, for affine
** setups the packed x16_fx_affine_on() arguments cannot express.
*/
void x16_fxu_set_tilebase (unsigned char v);
void x16_fxu_set_mapbase (unsigned char v);

/* pulls the implementation in with this header */
#pragma compile("verafx_utils.c")

#endif /* X16_VERAFX_UTILS_H */
