/* =====================================================================
 * x16clib :: x16/bitmap2.h -- 640x480x4 bitmap drawing (2bpp)
 * =====================================================================
 * The full 640x480 the VERA can drive: 2bpp, 4 pixels per byte packed
 * MSB-first, rows of 160 bytes at VRAM $00000 (76,800 bytes). A pixel
 * byte lives at y*160 + (x >> 2). Colours are 0-3 out of the first four
 * palette entries; x16_gfx2_init() loads white / light gray / dark gray
 * / black -- recolour with <x16/palette.h> without touching a pixel.
 *
 * x16_gfx2_pset() and x16_gfx2_read() clip. The span, rect, line and
 * blit primitives do NOT: they assume their arguments are on screen
 * (the 8bpp module's policy, for the same reason).
 *
 * x16_gfx2_clear() fills through the VERA FX 32-bit cache: it needs an
 * FX-capable VERA (47.0.2+, probe with x16_vera_has_fx()).
 * =====================================================================
 */

#ifndef X16_BITMAP2_H
#define X16_BITMAP2_H

#define X16_GFX2_WIDTH   640
#define X16_GFX2_HEIGHT  480
#define X16_GFX2_STRIDE  160

void x16_gfx2_init (void);

void x16_gfx2_clear (__reg("a") unsigned char color);

unsigned char x16_gfx2_setptr (__reg("r4") unsigned char inc,
                               __reg("r0/r1") unsigned int x,
                               __reg("r2/r3") unsigned int y);

/* Clipped. */
void x16_gfx2_pset (__reg("r0/r1") unsigned int x,
                    __reg("r2/r3") unsigned int y,
                    __reg("r4") unsigned char color);

/* 0-3, or $FF when (x,y) is off screen. */
unsigned char x16_gfx2_read (__reg("r0/r1") unsigned int x,
                             __reg("r2/r3") unsigned int y);

/* Unclipped from here down. */
void x16_gfx2_hline (__reg("r0/r1") unsigned int x,
                     __reg("r2/r3") unsigned int y,
                     __reg("r4/r5") unsigned int len,
                     __reg("r6") unsigned char color);

void x16_gfx2_vline (__reg("r0/r1") unsigned int x,
                     __reg("r2/r3") unsigned int y,
                     __reg("r4/r5") unsigned int len,
                     __reg("r6") unsigned char color);

/* Four 16-bit arguments fill r0..r7, so `color` is the fifth and rides
** the C soft stack. That is vbcc's own rule, not a choice made here.
*/
void x16_gfx2_rect (__reg("r0/r1") unsigned int x,
                    __reg("r2/r3") unsigned int y,
                    __reg("r4/r5") unsigned int w,
                    __reg("r6/r7") unsigned int h,
                    unsigned char color);

void x16_gfx2_frame (__reg("r0/r1") unsigned int x,
                     __reg("r2/r3") unsigned int y,
                     __reg("r4/r5") unsigned int w,
                     __reg("r6/r7") unsigned int h,
                     unsigned char color);

/* Bresenham, any direction; plots through the clipped pset, so lines
** may safely leave the screen.
*/
void x16_gfx2_line (__reg("r0/r1") unsigned int x0,
                    __reg("r2/r3") unsigned int y0,
                    __reg("r4/r5") unsigned int x1,
                    __reg("r6/r7") unsigned int y1,
                    unsigned char color);

/* An 8x8 1bpp pattern (8 row bytes, bit 7 leftmost) expanded once and
** cached; colors = (background << 2) | foreground. Patterns anchor to
** the screen origin, so adjacent fills always knit together.
*/
void x16_gfx2_pattern_set (__reg("r0/r1") const unsigned char *pattern,
                           __reg("r2") unsigned char colors);

void x16_gfx2_pattern_rect (__reg("r0/r1") unsigned int x,
                            __reg("r2/r3") unsigned int y,
                            __reg("r4/r5") unsigned int w,
                            __reg("r6/r7") unsigned int h);

/* Copy a byte-aligned image (row-major, wbytes per row = 4-pixel units;
** x bits 1:0 are ignored) from RAM into the bitmap. op: 0 copy, 1 OR,
** 2 AND, 3 XOR -- the sixth argument, so it rides the soft stack.
*/
void x16_gfx2_blit (__reg("r0/r1") unsigned int x,
                    __reg("r2/r3") unsigned int y,
                    __reg("r4") unsigned char wbytes,
                    __reg("r5") unsigned char h,
                    __reg("r6/r7") const unsigned char *src,
                    unsigned char op);

/* Masked blit of pre-shifted column-major data at ANY x: for each of
** `cols` framebuffer byte columns, `h` (mask, data) byte pairs walking
** down the rows -- fb = (fb & mask) | data. The caller supplies data
** already shifted for x & 3; pre-shifted glyph caches are what make
** proportional text affordable (~160 masked 8x8 glyphs per frame).
** h is 1-127.
*/
void x16_gfx2_blitm (__reg("r0/r1") unsigned int x,
                     __reg("r2/r3") unsigned int y,
                     __reg("r4") unsigned char h,
                     __reg("r5") unsigned char cols,
                     __reg("r6/r7") const unsigned char *src);

#endif /* X16_BITMAP2_H */
