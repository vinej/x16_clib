/* =====================================================================
 * x16clib :: x16/bitmap2l.h -- 320x240x4 bitmap drawing (2bpp)
 * =====================================================================
 * The framebuffer is 2bpp at VRAM $00000: 4 pixels per byte packed
 * MSB-first, rows of 80 bytes, 19,200 bytes in all. Colours are 0-3;
 * x16_gfx2l_init() loads a paper-and-ink default into palette entries
 * 0-3 (0 white, 1 light gray, 2 dark gray, 3 black) and programs the
 * mode on bare VERA registers -- no KERNAL screen mode exists for it.
 * x16_pal_set()/x16_pal_load() re-colour without touching pixels.
 *
 * x16_gfx2l_pset() and x16_gfx2l_read() clip. The span, rect, line and
 * blit primitives do NOT: they assume their arguments are on screen.
 *
 * x16_gfx2l_clear() runs through the VERA FX 32-bit cache write and
 * needs an FX-capable VERA (47.0.2+); everything else is stock VERA.
 * =====================================================================
 */

#ifndef X16_BITMAP2L_H
#define X16_BITMAP2L_H

#define X16_GFX2L_WIDTH   320
#define X16_GFX2L_HEIGHT  240
#define X16_GFX2L_STRIDE  80

/* 320x240@2bpp on layer 0; layer 1 (text) is disabled. The
** framebuffer is NOT cleared -- call x16_gfx2l_clear().
*/
void x16_gfx2l_init (void);

/* Fill the whole framebuffer with one colour (0-3). */
void x16_gfx2l_clear (unsigned char color);

/* Point VERA data port 0 at the byte holding pixel (x,y) with the
** given increment index (X16_INC_*). Returns x & 3, the pixel's
** position within the byte (0 = leftmost, bits 7:6).
*/
unsigned char x16_gfx2l_setptr (unsigned char inc,
                                             unsigned int x, unsigned int y);

/* Set one pixel. Clips. */
void x16_gfx2l_pset (unsigned int x, unsigned int y,
                                  unsigned char color);

/* Read one pixel: 0-3, or $FF if (x,y) is off screen. */
unsigned char x16_gfx2l_read (unsigned int x, unsigned int y);

/* Horizontal span of len pixels. No clipping. */
void x16_gfx2l_hline (unsigned int x, unsigned int y,
                                   unsigned int len, unsigned char color);

/* Vertical span of len pixels. No clipping. */
void x16_gfx2l_vline (unsigned int x, unsigned int y,
                                   unsigned int len, unsigned char color);

/* Filled rectangle. No clipping. */
void x16_gfx2l_rect (unsigned int x, unsigned int y,
                                  unsigned int w, unsigned int h,
                                  unsigned char color);

/* Rectangle outline. No clipping. */
void x16_gfx2l_frame (unsigned int x, unsigned int y,
                                   unsigned int w, unsigned int h,
                                   unsigned char color);

/* Bresenham line, any direction. Plots through the clipped pset, so
** the line clips at the screen edges.
*/
void x16_gfx2l_line (unsigned int x0, unsigned int y0,
                                  unsigned int x1, unsigned int y1,
                                  unsigned char color);

/* Cache an 8x8 1bpp pattern (8 row bytes, top first, bit 7 leftmost)
** for x16_gfx2l_pattern_rect(). colors = (background << 2) | foreground,
** each 0-3. Patterns tile from the screen origin.
*/
void x16_gfx2l_pattern_set (const unsigned char *pattern,
                                         unsigned char colors);

/* Fill a rectangle with the cached pattern. No clipping. */
void x16_gfx2l_pattern_rect (unsigned int x, unsigned int y,
                                          unsigned int w, unsigned int h);

/* Copy a byte-aligned image from RAM into the bitmap. x bits 1:0 are
** ignored (byte-aligned); wbytes is the width in BYTES (4-pixel
** units); src is row-major. op: 0 copy, 1 OR, 2 AND, 3 XOR.
** No clipping.
*/
void x16_gfx2l_blit (unsigned int x, unsigned int y,
                                  unsigned char wbytes, unsigned char h,
                                  const unsigned char *src,
                                  unsigned char op);

/* Masked blit of pre-shifted column-major data at any pixel position:
** for each of `cols` framebuffer columns, `h` (mask, data) byte PAIRS
** walking down the rows; fb' = (fb AND mask) OR data. The caller
** supplies data already shifted for x & 3. No clipping.
*/
void x16_gfx2l_blitm (unsigned int x, unsigned int y,
                                   unsigned char h, unsigned char cols,
                                   const unsigned char *src);

/* pulls the implementation in with this header */
#pragma compile("bitmap2l.c")

#endif
