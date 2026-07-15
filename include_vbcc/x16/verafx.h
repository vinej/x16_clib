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
 * Every routine here leaves FX disabled and DCSEL back at 0.
 *
 * The multiplier has no result register: triggering it writes four bytes
 * to VRAM $1F800, an unused corner, read straight back. So x16_fx_mult()
 * clobbers four bytes there.
 * =====================================================================
 */

#ifndef X16_VERAFX_H
#define X16_VERAFX_H

/* Where x16_fx_mult() parks its 32-bit result before reading it back. */
#define X16_VRAM_FX_SCRATCH     0x1F800UL

/* Disable FX, restore DCSEL. Safe whether or not FX was ever enabled. */
void x16_fx_off(void);

/* Signed 16 x 16 -> 32, in hardware. Far faster than x16_umul16(), and
** signed, but it costs four bytes of VRAM scratch. (Return is a long, in
** btmp0.) */
long x16_fx_mult(__reg("r0/r1") int a, __reg("r2/r3") int b);

/* Fill `count` bytes of VRAM at `addr` with `value`, about four times
** faster than a byte loop. (addr is a long, in btmp0.) */
void x16_fx_fill(__reg("r0") unsigned char value, __reg("r2/r3") unsigned int count,
                 unsigned long addr);

void x16_fx_clear(__reg("r0/r1") unsigned int count, unsigned long addr);

/* VRAM to VRAM through the 32-bit cache, about four times a byte loop.
**
** `dst` must be 4-BYTE ALIGNED. `src` needs no alignment. (Two longs:
** src in btmp0, dst in btmp1; count in r0/r1.) */
void x16_fx_copy(unsigned long src, unsigned long dst, __reg("r0/r1") unsigned int count);

/* Transparent VRAM writes. While on, a ZERO byte written to a data port
** leaves the target byte alone, so colour 0 acts as transparency. Every
** other x16_fx_* routine resets FX_CTRL on the way out. */
void x16_fx_transp_on(void);
void x16_fx_transp_off(void);

/* ---------------------------------------------------------------------
 * Hardware line and polygon drawing. VERA carries the Bresenham error
 * itself. Both assume the 320x240x256 framebuffer and NEITHER CLIPS.
 * ------------------------------------------------------------------ */

/* The same arguments and endpoints as x16_gfx_line(). (Five args: color
** rides the C soft stack.) */
void x16_fx_line(__reg("r0/r1") unsigned int x0, __reg("r2") unsigned char y0,
                 __reg("r4/r5") unsigned int x1, __reg("r6") unsigned char y1,
                 unsigned char color);

/* A vertex. The three bytes are copied straight onto the assembly's
** operand block, so the field order is load-bearing. Do not reorder. */
typedef struct {
    unsigned int  x;            /* 0-319 */
    unsigned char y;            /* 0-239 */
} x16_point;

/* Filled triangle. The vertices may come in any order. The rasterisation
** is HALF-OPEN: the bottom row, at the largest y, is not drawn. */
void x16_fx_triangle(__reg("r0/r1") const x16_point *a, __reg("r2/r3") const x16_point *b,
                     __reg("r4/r5") const x16_point *c, __reg("r6") unsigned char color);

#endif /* X16_VERAFX_H */
