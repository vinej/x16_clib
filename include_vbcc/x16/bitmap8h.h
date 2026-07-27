/* =====================================================================
 * x16clib :: x16/bitmap8h.h -- VERA_2 640x480x256 SDRAM bitmap drawing
 * =====================================================================
 * Requires the MiSTer VERA_2 bitmap layer: the framebuffer is NOT VERA
 * VRAM but the VERA_2 20-bit SDRAM byte address space behind $9F60-
 * $9F6F. Feature-detect with x16_gfx8h_has() before relying on it --
 * on stock hardware (and the emulator) every routine here writes into
 * open bus.
 *
 * The framebuffer is 8bpp, one byte per pixel, rows of 640 bytes:
 * offset = y*640 + x, 307,200 bytes in all. The VERA_2 layer has its
 * own 256-entry palette ($9F66-$9F68), separate from VERA's.
 *
 * x16_gfx8h_pset() and x16_gfx8h_read() clip. The span, rect, line and
 * blit primitives do NOT: they assume their arguments are on screen.
 * =====================================================================
 */

#ifndef X16_BITMAP8H_H
#define X16_BITMAP8H_H

#define X16_GFX8H_WIDTH   640
#define X16_GFX8H_HEIGHT  480

/* VERA_2 SDRAM stride indices for x16_gfx8h_setptr(). */
#ifndef X16_INC2_1
#define X16_INC2_1      0x0
#define X16_INC2_0      0x1
#define X16_INC2_2      0x2
#define X16_INC2_4      0x3
#define X16_INC2_8      0x4
#define X16_INC2_16     0x5
#define X16_INC2_32     0x6
#define X16_INC2_64     0x7
#define X16_INC2_128    0x8
#define X16_INC2_256    0x9
#define X16_INC2_320    0xA
#define X16_INC2_640    0xB
#define X16_INC2_NEG1   0xC
#define X16_INC2_NEG2   0xD
#define X16_INC2_NEG320 0xE
#define X16_INC2_NEG640 0xF
#endif

/* 1 if the VERA_2 bitmap layer answers (ID reads back $B5), else 0. */
unsigned char x16_gfx8h_has (void);

/* Enable the layer at 640x480@8bpp and load a grayscale palette. */
void x16_gfx8h_init (void);

/* Disable the VERA_2 bitmap layer. */
void x16_gfx8h_off (void);

/* Pass the stock VERA picture through / composite the layer again. */
void x16_gfx8h_passthru_on (void);
void x16_gfx8h_passthru_off (void);

/* Load the 256-entry grayscale ramp (what init uses). */
void x16_gfx8h_pal_gray (void);

/* Set one VERA_2 palette entry: lo = (G << 4) | B, hi = R. */
void x16_gfx8h_pal_set (__reg("r0") unsigned char index,
                       __reg("r2") unsigned char lo,
                       __reg("r4") unsigned char hi);

/* Load count entries from src (lo, hi byte pairs) starting at first.
** count 0 loads nothing.
*/
void x16_gfx8h_pal_load (__reg("r0/r1") const unsigned char *src,
                        __reg("r2") unsigned char first,
                        __reg("r4") unsigned char count);

/* Point the VERA_2 DATA port at pixel (x,y) with an X16_INC2_* stride. */
void x16_gfx8h_setptr (__reg("r4") unsigned char inc,
                      __reg("r0/r1") unsigned int x,
                      __reg("r2/r3") unsigned int y);

/* Fill the whole framebuffer with one colour. */
void x16_gfx8h_clear (__reg("a") unsigned char color);

/* Set one pixel. Clips. */
void x16_gfx8h_pset (__reg("r0/r1") unsigned int x,
                    __reg("r2/r3") unsigned int y,
                    __reg("r4") unsigned char color);

/* Read one pixel: 0-255, or 0xFFFF if (x,y) is off screen (every
** 8-bit value is a valid colour, so the sentinel needs the high byte).
*/
unsigned int x16_gfx8h_read (__reg("r0/r1") unsigned int x,
                            __reg("r2/r3") unsigned int y);

/* Spans. No clipping. */
void x16_gfx8h_hline (__reg("r0/r1") unsigned int x,
                     __reg("r2/r3") unsigned int y,
                     __reg("r4/r5") unsigned int len,
                     __reg("r6") unsigned char color);
void x16_gfx8h_vline (__reg("r0/r1") unsigned int x,
                     __reg("r2/r3") unsigned int y,
                     __reg("r4/r5") unsigned int len,
                     __reg("r6") unsigned char color);

/* Rectangles. No clipping. Four 16-bit arguments fill r0..r7, so
** `color` is the fifth and rides the C soft stack.
*/
void x16_gfx8h_rect (__reg("r0/r1") unsigned int x,
                    __reg("r2/r3") unsigned int y,
                    __reg("r4/r5") unsigned int w,
                    __reg("r6/r7") unsigned int h,
                    unsigned char color);
void x16_gfx8h_frame (__reg("r0/r1") unsigned int x,
                     __reg("r2/r3") unsigned int y,
                     __reg("r4/r5") unsigned int w,
                     __reg("r6/r7") unsigned int h,
                     unsigned char color);

/* Bresenham line, any direction; clips through pset. */
void x16_gfx8h_line (__reg("r0/r1") unsigned int x0,
                    __reg("r2/r3") unsigned int y0,
                    __reg("r4/r5") unsigned int x1,
                    __reg("r6/r7") unsigned int y1,
                    unsigned char color);

/* Cache an 8x8 1bpp pattern for x16_gfx8h_pattern_rect(). */
void x16_gfx8h_pattern_set (__reg("a/x") const unsigned char *pattern,
                           __reg("r0") unsigned char bg,
                           __reg("r1") unsigned char fg);

/* Fill a rectangle with the cached pattern. No clipping. */
void x16_gfx8h_pattern_rect (__reg("r0/r1") unsigned int x,
                            __reg("r2/r3") unsigned int y,
                            __reg("r4/r5") unsigned int w,
                            __reg("r6/r7") unsigned int h);

/* Copy rows of pixels from RAM (row-major, one byte per pixel).
** w is 1-255 pixels. op: 0 copy, 1 OR, 2 AND, 3 XOR. No clipping.
** op is the sixth argument, so it rides the soft stack.
*/
void x16_gfx8h_blit (__reg("r0/r1") unsigned int x,
                    __reg("r2/r3") unsigned int y,
                    __reg("r4") unsigned char w,
                    __reg("r5") unsigned char h,
                    __reg("r6/r7") const unsigned char *src,
                    unsigned char op);

/* Masked blit: colour 0 is transparent. Same layout as blit. */
void x16_gfx8h_blitm (__reg("r0/r1") unsigned int x,
                     __reg("r2/r3") unsigned int y,
                     __reg("r4") unsigned char w,
                     __reg("r5") unsigned char h,
                     __reg("r6/r7") const unsigned char *src);

/* VERA_2 hardware SDRAM-to-SDRAM copy of len bytes (20-bit byte
** addresses, stride +1), then wait for the blitter to finish.
** (Three longs: src in btmp0, dst in btmp1, len in btmp2.)
*/
void x16_gfx8h_copy (unsigned long src, unsigned long dst,
                    unsigned long len);

/* Wait for a previous copy to finish (copy already waits). */
void x16_gfx8h_copy_wait (void);

#endif /* X16_BITMAP8H_H */
