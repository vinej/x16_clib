/* =====================================================================
 * x16clib :: x16/bitmap4l.h -- 320x240x16 bitmap drawing (4bpp)
 * =====================================================================
 * The framebuffer is 4bpp at VRAM $00000: 2 pixels per byte packed
 * MSB-first, rows of 160 bytes, 38,400 bytes in all. Colours are 0-15;
 * x16_gfx4l_init() loads a default 16-entry palette and programs the
 * mode on bare VERA registers -- no KERNAL screen mode exists for it.
 * x16_pal_set()/x16_pal_load() re-colour without touching pixels.
 *
 * x16_gfx4l_pset() and x16_gfx4l_read() clip. The span, rect, line and
 * blit primitives do NOT: they assume their arguments are on screen.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** llvm-mos build. The API is identical to the cc65 build's; only the
** calling convention differs, and llvm-mos expresses it in the compiler
** rather than in the declaration. So there is no here.
**
** llvm-mos passes byte arguments in A, then X, then __rc2, __rc3, ...
** and returns the same way. cc65 pushes all but the last argument on a
** software stack. Object code from the two toolchains cannot be mixed.
** --------------------------------------------------------------------- */

#ifndef X16_BITMAP4L_H
#define X16_BITMAP4L_H

#define X16_GFX4L_WIDTH   320
#define X16_GFX4L_HEIGHT  240

/* 320x240@4bpp on layer 0; layer 1 (text) is disabled. The
** framebuffer is NOT cleared -- call x16_gfx4l_clear().
*/
void x16_gfx4l_init (void);

/* Fill the whole framebuffer with one colour (0-15). */
void x16_gfx4l_clear (unsigned char color);

/* Point VERA data port 0 at the byte holding pixel (x,y) with the
** given increment index (X16_INC_*). The left pixel of the byte is
** the high nibble.
*/
void x16_gfx4l_setptr (unsigned char inc,
                                    unsigned int x, unsigned char y);

/* Set one pixel. Clips. */
void x16_gfx4l_pset (unsigned int x, unsigned char y,
                                  unsigned char color);

/* Read one pixel: 0-15, or $FF if (x,y) is off screen. */
unsigned char x16_gfx4l_read (unsigned int x, unsigned char y);

/* Horizontal span of len pixels. No clipping. */
void x16_gfx4l_hline (unsigned int x, unsigned char y,
                                   unsigned int len, unsigned char color);

/* Vertical span of len pixels (1-255). No clipping. */
void x16_gfx4l_vline (unsigned int x, unsigned char y,
                                   unsigned char len, unsigned char color);

/* Filled rectangle. No clipping. */
void x16_gfx4l_rect (unsigned int x, unsigned char y,
                                  unsigned int w, unsigned char h,
                                  unsigned char color);

/* Rectangle outline. No clipping. */
void x16_gfx4l_frame (unsigned int x, unsigned char y,
                                   unsigned int w, unsigned char h,
                                   unsigned char color);

/* Bresenham line, any direction. Plots through the clipped pset, so
** the line clips at the screen edges.
*/
void x16_gfx4l_line (unsigned int x0, unsigned char y0,
                                  unsigned int x1, unsigned char y1,
                                  unsigned char color);

/* Draw one 8x8 glyph from the VERA charset. code is a SCREEN code,
** not PETSCII. Background pixels are left alone. Clips through pset.
*/
void x16_gfx4l_char (unsigned int x, unsigned char y,
                                  unsigned char color, unsigned char code);

/* Draw a NUL-terminated PETSCII string, 8 pixels per glyph. */
void x16_gfx4l_text (unsigned int x, unsigned char y,
                                  unsigned char color, const char *s);

/* Cache an 8x8 1bpp pattern (8 row bytes, top first, bit 7 leftmost)
** for x16_gfx4l_pattern_rect(). Patterns tile from the screen origin.
*/
void x16_gfx4l_pattern_set (const unsigned char *pattern,
                                         unsigned char bg, unsigned char fg);

/* Fill a rectangle with the cached pattern. No clipping. */
void x16_gfx4l_pattern_rect (unsigned int x, unsigned char y,
                                          unsigned int w, unsigned char h);

/* Copy rows of pixels from RAM into the bitmap. w is in PIXELS
** (1-255); src is row-major, one byte per pixel PAIR (the row is
** (w+1)/2 bytes). op: 0 copy, 1 OR, 2 AND, 3 XOR. No clipping.
*/
void x16_gfx4l_blit (unsigned int x, unsigned char y,
                                  unsigned char w, unsigned char h,
                                  const unsigned char *src,
                                  unsigned char op);

/* Masked blit: colour 0 is transparent. Same layout as blit. */
void x16_gfx4l_blitm (unsigned int x, unsigned char y,
                                   unsigned char w, unsigned char h,
                                   const unsigned char *src);

#endif
