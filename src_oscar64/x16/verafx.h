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
** Oscar64 build. The API is identical to the cc65 build's; what differs
** is the delivery. Oscar64 compiles the whole program at once and strips
** what goes unused, so this port is a SOURCE distribution: headers and
** implementations sit side by side in src_oscar64/x16/, and the
** `#pragma compile` at the bottom of this header pulls the matching .c
** in automatically:
**
**     oscar64 -tm=x16 -n -i=src_oscar64 -o=YOURPROG.PRG yourprog.c
** --------------------------------------------------------------------- */

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
 * that x16_gfx8l_init() selects, and NEITHER CLIPS -- keep every coordinate
 * on screen.
 * ------------------------------------------------------------------ */

/* The same arguments and the same endpoints as x16_gfx8l_line(), drawn by
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
** tile 0; clear, the ray wraps around the map edges.
*/
void x16_fx_affine_on (unsigned long tiles, unsigned long map,
                                    unsigned char size, unsigned char clip);

/* Aim the sampler: start at texel (x, y) -- 0-1023 each -- and step by
** (dx, dy) per read. The steps are SIGNED, in 1/512-texel units, so 512
** is one texel per read, 256 doubles the texture, 1024 halves it; bit
** 15 multiplies by 32, as for the line helper. Sampling starts at texel
** centres (the subpixel part is seeded to 0.5).
*/
void x16_fx_affine_ray (unsigned int x, unsigned int y,
                                     int dx, int dy);

/* Fetch `count` texels (>= 1) along the ray into VRAM: one port 1 read,
** one port 0 write per texel. Aim the ray first, and point port 0 at
** the destination yourself -- x16_vera_addr0(X16_INC_1, dest) -- with
** whatever increment the destination wants.
*/
void x16_fx_affine_span (unsigned int count);

/* pulls the implementation in with this header */
#pragma compile("verafx.c")

#endif /* X16_VERAFX_H */
