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

/* ---------------------------------------------------------------------
 * Hardware line and polygon drawing.
 *
 * VERA carries the Bresenham error itself: the CPU's whole job becomes
 * one store per pixel. Both routines assume the 320x240x256 framebuffer
 * that x16_gfx_init() selects, and NEITHER CLIPS -- keep every coordinate
 * on screen.
 * ------------------------------------------------------------------ */

/* The same arguments and the same endpoints as x16_gfx_line(), drawn by
** the hardware helper instead of a software Bresenham.
*/
void __fastcall__ x16_fx_line (unsigned int x0, unsigned char y0,
                               unsigned int x1, unsigned char y1,
                               unsigned char color);

/* A vertex. The three bytes are copied straight onto the assembly's
** operand block, so the field order is load-bearing. Do not reorder.
*/
typedef struct {
    unsigned int  x;            /* 0-319 */
    unsigned char y;            /* 0-239 */
} x16_point;

/* Filled triangle. The vertices may come in any order.
**
** The rasterisation is HALF-OPEN: the bottom row, at the largest y, is
** not drawn. Two triangles sharing an edge therefore paint it once
** between them, rather than twice.
*/
void __fastcall__ x16_fx_triangle (const x16_point *a, const x16_point *b,
                                   const x16_point *c, unsigned char color);

#endif /* X16_VERAFX_H */
