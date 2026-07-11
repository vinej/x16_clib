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

#ifndef X16_VERAFX_H
#define X16_VERAFX_H

#include <x16/zpsafe.h>

/* Where x16_fx_mult() parks its 32-bit result before reading it back. */
#define X16_VRAM_FX_SCRATCH     0x1F800UL

/* Disable FX, restore DCSEL. Safe whether or not FX was ever enabled;
** the other routines already do this on the way out.
*/
void x16_fx_off (void);

/* Signed 16 x 16 -> 32, in hardware. Far faster than x16_umul16(), and
** signed, but it costs four bytes of VRAM scratch.
*/
long x16_fx_mult (int a, int b);

/* Fill `count` bytes of VRAM at `addr` with `value`, about four times
** faster than a byte loop: the 32-bit write cache stores four bytes per
** access. A count that is not a multiple of four is finished off one
** byte at a time.
**
** `addr` comes last so cc65 passes all four bytes in registers.
*/
void x16_fx_fill (unsigned char value, unsigned int count,
                               unsigned long addr);

void x16_fx_clear (unsigned int count, unsigned long addr);

/* VRAM to VRAM through the 32-bit cache, about four times a byte loop.
**
** `dst` must be 4-BYTE ALIGNED -- the cache flushes four bytes at a
** time. `src` needs no alignment. A count that is not a multiple of four
** is finished off one byte at a time with FX switched off.
*/
void x16_fx_copy (unsigned long src, unsigned long dst,
                               unsigned int count);

/* Transparent VRAM writes. While on, a ZERO byte written to a data port
** -- or sitting in a flushed cache -- leaves the target byte alone, so
** colour 0 acts as transparency for blits.
**
** Every other x16_fx_* routine resets FX_CTRL on the way out, which turns
** transparency off again. Enable it, do your writes, disable it.
*/
void x16_fx_transp_on (void);
void x16_fx_transp_off (void);

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
void x16_fx_line (unsigned int x0, unsigned char y0,
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
void x16_fx_triangle (const x16_point *a, const x16_point *b,
                                   const x16_point *c, unsigned char color);

#endif /* X16_VERAFX_H */
