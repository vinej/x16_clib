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

/* The same arguments and endpoints as x16_gfx8l_line(). (Five args: color
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

/* ---------------------------------------------------------------------
 * The affine helper -- the rotozoom/mode-7 sampler.
 *
 * VERA turns port 1's reads into texture fetches: an 8x8-tile map (one
 * byte per tile, no attributes) defines a square texture, and two
 * fixed-point counters walk a sampling ray across it. The workflow is
 * always the same three steps:
 *
 *      x16_fx_affine_on(tiles, map, size, clip);   // once
 *      x16_fx_affine_ray(x, y, dx, dy);            // per scanline
 *      x16_vera_addr0(X16_INC_1, dest);
 *      x16_fx_affine_span(width);                  // DATA1 -> DATA0
 *
 * The ray aims the sampler; after that EVERY read of data port 1
 * returns the texel under the ray and steps it by (dx, dy). The span is
 * the mode-7 inner loop: it copies `count` texels from port 1 to
 * wherever the caller pointed port 0. A rotated, scaled scanline is one
 * ray plus one span, with dx/dy coming from a sine table and the zoom.
 *
 * Affine mode STAYS ON between calls -- that is the point. Call
 * x16_fx_off() when you are done, or port 1 keeps reading texels.
 * ------------------------------------------------------------------ */

/* Enter affine mode and describe the texture. `tiles` and `map` are
** VRAM addresses, each 2 KB ALIGNED -- the hardware register only holds
** address bits 16:11. Tiles are 8x8 at 8 bpp: 64 bytes each, up to 256
** of them. The map is one byte per tile, `size` codes its dimensions:
** 0 = 2x2 tiles (a 16x16-texel texture), 1 = 8x8, 2 = 32x32,
** 3 = 128x128. `clip` bit 0 set makes reads outside the map return
** tile 0; clear, the ray wraps around the map edges. (Two longs: tiles
** in btmp0, map in btmp1.) */
void x16_fx_affine_on(unsigned long tiles, unsigned long map,
                      __reg("r0") unsigned char size, __reg("r2") unsigned char clip);

/* Aim the sampler: start at texel (x, y) -- 0-1023 each -- and step by
** (dx, dy) per read. The steps are SIGNED, in 1/512-texel units, so 512
** is one texel per read, 256 doubles the texture, 1024 halves it; bit
** 15 multiplies by 32, as for the line helper. Sampling starts at texel
** centres (the subpixel part is seeded to 0.5). */
void x16_fx_affine_ray(__reg("r0/r1") unsigned int x, __reg("r2/r3") unsigned int y,
                       __reg("r4/r5") int dx, __reg("r6/r7") int dy);

/* Fetch `count` texels (>= 1) along the ray into VRAM: one port 1 read,
** one port 0 write per texel. Aim the ray first, and point port 0 at
** the destination yourself -- x16_vera_addr0(X16_INC_1, dest) -- with
** whatever increment the destination wants. */
void x16_fx_affine_span(__reg("r0/r1") unsigned int count);

#endif /* X16_VERAFX_H */
