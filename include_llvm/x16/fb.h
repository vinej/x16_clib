/* =====================================================================
 * x16clib :: x16/fb.h -- the KERNAL framebuffer driver API
 * =====================================================================
 * The ROM's low-level pixel layer: a cursor machine. Position the
 * cursor, and every get/set advances it, so runs of pixels cost no
 * per-pixel address math. The default driver is 320x240 at 8bpp in
 * VRAM $00000; GRAPH can install a different one.
 *
 * CALL x16_graph_init() FIRST (x16/graph.h). The FB entry points
 * dispatch through vectors it installs; before that they point nowhere.
 *
 * This is the ROM's drawing surface, driver-abstracted and cursor-
 * based. The library's own x16_bitmap_* (x16/bitmap.h) is the fast
 * direct-VERA alternative when you know the mode.
 * =====================================================================
 */

#ifndef X16_FB_H
#define X16_FB_H

/* A pixel filter for x16_fb_filter_pixels(): receives a color, returns
** the replacement. It runs under the ROM's inner loop: keep it small,
** and do not touch VERA or call the fb/graph API from it.
*/
typedef unsigned char (*x16_fb_filter) (unsigned char color);

/* Reinitialize the active driver: mode registers, VRAM base. */
void x16_fb_init (void);

/* Geometry of the active driver. Returns the depth in bits per pixel
** (8 for the default driver) and fills in the pixel dimensions.
*/
unsigned char x16_fb_get_info (unsigned int *width,
                                            unsigned int *height);

/* Set `count` palette entries from `start` (count 0 means all 256).
** `data` is count*2 bytes of VERA GB/R words, exactly the
** x16/palette.h format.
*/
void x16_fb_set_palette (const void *data, unsigned char start,
                                      unsigned char count);

/* Park the cursor at a pixel. */
void x16_fb_cursor_position (unsigned int x, unsigned int y);

/* Drop the cursor one scanline -- cheaper than a full reposition. The
** API passes x for drivers that need it; the default driver keeps its
** own position and ignores it.
*/
void x16_fb_cursor_next_line (unsigned int x);

/* Single pixels at the cursor. Each advances it by one. */
unsigned char x16_fb_get_pixel (void);
void x16_fb_set_pixel (unsigned char color);

/* Runs of pixels at the cursor. Each advances it by `count`; a count of
** 0 moves nothing.
*/
void x16_fb_get_pixels (void *dst, unsigned int count);
void x16_fb_set_pixels (const void *src, unsigned int count);

/* Draw the pattern's 1-bits in `color`, MSB first; 0-bits leave the
** underlying pixels alone. Advances the cursor by 8. This is how a
** glyph row or a 1bpp image row lands in one call.
*/
void x16_fb_set_8_pixels (unsigned char pattern,
                                       unsigned char color);

/* The two-color version: where `mask` has a 1 (MSB first), draw fg if
** the pattern bit is 1, bg if it is 0; mask 0-bits leave the pixel
** alone. Advances the cursor by 8.
*/
void x16_fb_set_8_pixels_opaque (unsigned char pattern,
                                              unsigned char mask,
                                              unsigned char fg,
                                              unsigned char bg);

/* `count` pixels of `color` from the cursor. step 0/1 is a solid run
** (hardware-accelerated); a larger step spaces the pixels -- step 320
** on the default driver is a vertical line.
*/
void x16_fb_fill_pixels (unsigned int count, unsigned int step,
                                      unsigned char color);

/* Rewrite `count` pixels from the cursor through `filter` -- palette
** remapping, highlight, dim.
*/
void x16_fb_filter_pixels (unsigned int count,
                                        x16_fb_filter filter);

/* Copy a horizontal span of `count` pixels from (sx,sy) to (tx,ty). */
void x16_fb_move_pixels (unsigned int sx, unsigned int sy,
                                      unsigned int tx, unsigned int ty,
                                      unsigned int count);

#endif /* X16_FB_H */
