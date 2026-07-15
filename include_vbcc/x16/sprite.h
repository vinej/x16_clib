/* =====================================================================
 * x16clib :: x16/sprite.h -- VERA hardware sprites
 * =====================================================================
 * 128 sprites, an 8-byte attribute record each, at VRAM $1FC00.
 *
 * That region is WRITE-ONLY: a read returns the last value this program
 * wrote there, not what the hardware holds. So x16_sprite_z(), which is a
 * read-modify-write, is only meaningful on a record that has already been
 * written. Call x16_sprite_init_all() once at startup to give every
 * record a known shadow.
 *
 * Sprite coordinates are 10-bit and live in the 640x480 display space,
 * NOT the 320x240 of the bitmap modes.
 * =====================================================================
 */

#ifndef X16_SPRITE_H
#define X16_SPRITE_H

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
** attribute RAM the known shadow that x16_sprite_z() depends on. */
void x16_sprite_init_all(void);

/* The sprite renderer as a whole. */
void x16_sprites_on(void);
void x16_sprites_off(void);

/* Position, 10 bits each, in 640x480 display space. */
void x16_sprite_pos(__reg("r0") unsigned char sprite,
                    __reg("r2/r3") unsigned int x, __reg("r4/r5") unsigned int y);
void x16_sprite_get_pos(__reg("r0") unsigned char sprite,
                        __reg("r2/r3") unsigned int *x, __reg("r4/r5") unsigned int *y);

/* Point a sprite at its pixel data. The record stores address bits 16:5,
** so `addr` must be 32-byte aligned -- the low five bits are dropped.
** `mode` is X16_SPRITE_4BPP or X16_SPRITE_8BPP. (addr is a long, passed
** in btmp0.) */
void x16_sprite_image(__reg("r0") unsigned char sprite,
                      __reg("r2") unsigned char mode,
                      unsigned long addr);

/* Byte 6 whole: collision mask in 7:4, Z-depth, vflip, hflip.
**      x16_sprite_flags(0, X16_SPRITE_Z_FRONT | X16_SPRITE_HFLIP);
*/
void x16_sprite_flags(__reg("r0") unsigned char sprite, __reg("r2") unsigned char flags);

/* Just the Z-depth, preserving the rest of byte 6. Read-modify-write:
** see the write-only note above. */
void x16_sprite_z(__reg("r0") unsigned char sprite, __reg("r2") unsigned char z);

/* Size codes and palette offset (0-15). Four chars, one per even
** register (r0, r2, r4, r6). */
void x16_sprite_size(__reg("r0") unsigned char sprite,
                     __reg("r2") unsigned char width,
                     __reg("r4") unsigned char height,
                     __reg("r6") unsigned char pal_offset);

/* Point data port 0 at one byte of a record, on auto-increment, so
** consecutive fields stream through VERA.data0. */
void x16_sprite_setptr(__reg("r0") unsigned char sprite, __reg("r2") unsigned char offset);

#endif /* X16_SPRITE_H */
