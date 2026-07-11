/* =====================================================================
 * x16clib :: x16/sprite.h -- VERA hardware sprites
 * =====================================================================
 * 128 sprites, an 8-byte attribute record each, at VRAM $1FC00:
 *   0  image address bits 12:5
 *   1  mode(7) | image address bits 16:13
 *   2  X bits 7:0
 *   3  X bits 9:8
 *   4  Y bits 7:0
 *   5  Y bits 9:8
 *   6  collision mask(7:4) | Z-depth(3:2) | vflip(1) | hflip(0)
 *   7  height(7:6) | width(5:4) | palette offset(3:0)
 *
 * That region is write-only: reads return the last value the host
 * wrote. Read-modify-write therefore only works on records this program
 * has already initialised. x16_sprite_init_all() does that.
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

#ifndef X16_SPRITE_H
#define X16_SPRITE_H

#include <x16/zpsafe.h>

/* Colour depth, for x16_sprite_image(). */
#define X16_SPRITE_4BPP         0x00
#define X16_SPRITE_8BPP         0x80

/* Z-depth, for x16_sprite_z() and the flags byte. */
#define X16_SPRITE_Z_DISABLED   0x00
#define X16_SPRITE_Z_BEHIND     0x04    /* between background and layer 0 */
#define X16_SPRITE_Z_MIDDLE     0x08    /* between layer 0 and layer 1 */
#define X16_SPRITE_Z_FRONT      0x0C    /* in front of layer 1 */

/* Flips, for the flags byte. */
#define X16_SPRITE_HFLIP        0x01
#define X16_SPRITE_VFLIP        0x02

/* Size codes, for x16_sprite_size(). */
#define X16_SPRITE_SIZE_8       0
#define X16_SPRITE_SIZE_16      1
#define X16_SPRITE_SIZE_32      2
#define X16_SPRITE_SIZE_64      3

/* Byte offsets within an attribute record, for x16_sprite_setptr(). */
#define X16_SPRITE_ATTR_ADDR_L    0
#define X16_SPRITE_ATTR_ADDR_H    1
#define X16_SPRITE_ATTR_X_L       2
#define X16_SPRITE_ATTR_X_H       3
#define X16_SPRITE_ATTR_Y_L       4
#define X16_SPRITE_ATTR_Y_H       5
#define X16_SPRITE_ATTR_FLAGS     6
#define X16_SPRITE_ATTR_SIZE_PAL  7

/* Zero all 128 records. Disables every sprite, and gives the write-only
** attribute RAM the known shadow that x16_sprite_z() depends on.
*/
void x16_sprite_init_all (void);

/* The sprite renderer as a whole. */
void x16_sprites_on (void);
void x16_sprites_off (void);

/* Position, 10 bits each, in 640x480 display space. */
void x16_sprite_pos (unsigned char sprite, unsigned int x, unsigned int y);
void x16_sprite_get_pos (unsigned char sprite,
                         unsigned int *x, unsigned int *y);

/* Point a sprite at its pixel data. The record stores address bits 16:5,
** so `addr` must be 32-byte aligned -- the low five bits are dropped.
** `mode` is X16_SPRITE_4BPP or X16_SPRITE_8BPP.
*/
void x16_sprite_image (unsigned char sprite, unsigned char mode,
                       unsigned long addr);

/* Byte 6 whole: collision mask in 7:4, Z-depth, vflip, hflip.
**      x16_sprite_flags(0, X16_SPRITE_Z_FRONT | X16_SPRITE_HFLIP);
*/
void x16_sprite_flags (unsigned char sprite, unsigned char flags);

/* Just the Z-depth, preserving the rest of byte 6. Read-modify-write:
** see the write-only note above.
*/
void x16_sprite_z (unsigned char sprite, unsigned char z);

/* Size codes and palette offset (0-15). */
void x16_sprite_size (unsigned char sprite, unsigned char width,
                      unsigned char height, unsigned char pal_offset);

/* Point data port 0 at one byte of a record, on auto-increment, so
** consecutive fields stream through the data register.
*/
void x16_sprite_setptr (unsigned char sprite, unsigned char offset);

#endif /* X16_SPRITE_H */
