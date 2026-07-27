/* =====================================================================
 * x16clib :: x16/bitmap4h.h -- VERA_2 640x480x16 SDRAM bitmap drawing
 * =====================================================================
 * Requires the MiSTer VERA_2 bitmap layer: the framebuffer is NOT VERA
 * VRAM but the VERA_2 20-bit SDRAM byte address space behind $9F60-
 * $9F6F. Feature-detect with x16_gfx4h_has() before relying on it --
 * on stock hardware (and the emulator) every routine here writes into
 * open bus.
 *
 * The framebuffer is 4bpp, two pixels per byte, rows of 320 bytes:
 * offset = y*320 + (x>>1), 153,600 bytes in all. The VERA_2 layer has its
 * own 16-entry palette ($9F66-$9F68), separate from VERA's.
 *
 * x16_gfx4h_pset() and x16_gfx4h_read() clip. The span, rect, line and
 * blit primitives do NOT: they assume their arguments are on screen.
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

#ifndef X16_BITMAP4H_H
#define X16_BITMAP4H_H

#define X16_GFX4H_WIDTH   640
#define X16_GFX4H_HEIGHT  480

/* VERA_2 SDRAM stride indices for x16_gfx4h_setptr(). */
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
unsigned char x16_gfx4h_has (void);

/* Enable the layer at 640x480@4bpp and load a 16-entry gray palette. */
void x16_gfx4h_init (void);

/* Disable the VERA_2 bitmap layer. */
void x16_gfx4h_off (void);

/* Pass the stock VERA picture through / composite the layer again. */
void x16_gfx4h_passthru_on (void);
void x16_gfx4h_passthru_off (void);

/* Load the 16-entry grayscale ramp (what init uses). */
void x16_gfx4h_pal_gray (void);

/* Set one VERA_2 palette entry: lo = (G << 4) | B, hi = R. */
void x16_gfx4h_pal_set (unsigned char index,
                                     unsigned char lo, unsigned char hi);

/* Load count entries from src (lo, hi byte pairs) starting at first.
** count 0 loads nothing.
*/
void x16_gfx4h_pal_load (const unsigned char *src,
                                      unsigned char first,
                                      unsigned char count);

/* Point the VERA_2 DATA port at pixel (x,y) with an X16_INC2_* stride. */
void x16_gfx4h_setptr (unsigned char inc,
                                    unsigned int x, unsigned int y);

/* Fill the whole framebuffer with one colour. */
void x16_gfx4h_clear (unsigned char color);

/* Set one pixel. Clips. */
void x16_gfx4h_pset (unsigned int x, unsigned int y,
                                  unsigned char color);

/* Read one pixel: 0-15, or $FF if (x,y) is off screen. */
unsigned char x16_gfx4h_read (unsigned int x, unsigned int y);

/* Spans. No clipping. */
void x16_gfx4h_hline (unsigned int x, unsigned int y,
                                   unsigned int len, unsigned char color);
void x16_gfx4h_vline (unsigned int x, unsigned int y,
                                   unsigned int len, unsigned char color);

/* Rectangles. No clipping. */
void x16_gfx4h_rect (unsigned int x, unsigned int y,
                                  unsigned int w, unsigned int h,
                                  unsigned char color);
void x16_gfx4h_frame (unsigned int x, unsigned int y,
                                   unsigned int w, unsigned int h,
                                   unsigned char color);

/* Bresenham line, any direction; clips through pset. */
void x16_gfx4h_line (unsigned int x0, unsigned int y0,
                                  unsigned int x1, unsigned int y1,
                                  unsigned char color);

/* Cache an 8x8 1bpp pattern for x16_gfx4h_pattern_rect(). */
void x16_gfx4h_pattern_set (const unsigned char *pattern,
                                         unsigned char bg, unsigned char fg);

/* Fill a rectangle with the cached pattern. No clipping. */
void x16_gfx4h_pattern_rect (unsigned int x, unsigned int y,
                                          unsigned int w, unsigned int h);

/* Copy rows of pixels from RAM (row-major, one byte per pixel PAIR;
** the row is (w+1)/2 bytes). w is 1-255 pixels. op: 0 copy, 1 OR,
** 2 AND, 3 XOR. No clipping.
*/
void x16_gfx4h_blit (unsigned int x, unsigned int y,
                                  unsigned char w, unsigned char h,
                                  const unsigned char *src,
                                  unsigned char op);

/* Masked blit: colour 0 is transparent. Same layout as blit. */
void x16_gfx4h_blitm (unsigned int x, unsigned int y,
                                   unsigned char w, unsigned char h,
                                   const unsigned char *src);

/* VERA_2 hardware SDRAM-to-SDRAM copy of len bytes (20-bit byte
** addresses, stride +1), then wait for the blitter to finish.
*/
void x16_gfx4h_copy (unsigned long src, unsigned long dst,
                                  unsigned long len);

/* Wait for a previous copy to finish (copy already waits). */
void x16_gfx4h_copy_wait (void);

#endif
