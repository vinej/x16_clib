// =====================================================================
// x16clib :: x16/sprite.c -- VERA hardware sprites
// =====================================================================
// Same code as src_ca65/sprite/sprite.s. The ca65 internals reached the
// shared address computation with `jsr sprite_setptr`; here every
// routine calls x16_sprite_setptr() as a C function -- it is the same
// routine, it was public anyway, and inline asm cannot jsr across
// functions.
//
// Parameter names differ from the header in places (`xpos` for `x`,
// `zv` for `z`): single-letter register names read as registers to an
// inline assembler, so the KickC port's renames are kept.
// =====================================================================

#include <x16/sprite.h>
#include <x16/vera.h>

// The out-parameters are indirected directly -- Oscar64 keeps pointer
// parameters in zero page. The cc65 build used ptr1/ptr2.

volatile char x16__sp_hi;               // sprite*8 high bits
volatile char x16__sp_t0;
volatile char x16__sp_t1;
volatile char x16__sp_t2;
volatile char x16__sp_t3;

// ---------------------------------------------------------------------
// Point data port 0 at one byte of a sprite record, on auto-increment.
// ---------------------------------------------------------------------
void x16_sprite_setptr(unsigned char sprite, unsigned char offset) {
    __asm {
        lda 0x9f25  /*VERA_CTRL*/         /* ADDRSEL = 0, DCSEL untouched */
        and #0xfe
        sta 0x9f25

        lda #0
        sta x16__sp_hi
        lda sprite
        asl
        rol x16__sp_hi
        asl
        rol x16__sp_hi
        asl
        rol x16__sp_hi                  /* hi:A = sprite * 8 */

        clc
        adc offset
        sta 0x9f20                      /* VERA_ADDR_L */
        lda x16__sp_hi
        adc #0xfc                        /* >VRAM_SPRITE_ATTR, + any carry */
        sta 0x9f21                      /* VERA_ADDR_M */
        lda #0x11                        /* ADDR_H_BANK | (VERA_INC_1 << 4) */
        sta 0x9f22                      /* VERA_ADDR_H */
    }
}

void x16_sprites_on(void) {
    __asm {
        lda 0x9f25         /* vera_dcsel 0 (VERA_CTRL) */
        and #0x01                       /* VERA_CTRL_ADDRSEL */
        sta 0x9f25                      /* VERA_CTRL */
        lda 0x9f29  /*VERA_DC_VIDEO*/
        ora #0x40
        sta 0x9f29
    }
}

void x16_sprites_off(void) {
    __asm {
        lda 0x9f25         /* vera_dcsel 0 (VERA_CTRL) */
        and #0x01                       /* VERA_CTRL_ADDRSEL */
        sta 0x9f25                      /* VERA_CTRL */
        lda 0x9f29  /*VERA_DC_VIDEO*/
        and #0xbf
        sta 0x9f29
    }
}

// ---------------------------------------------------------------------
// Coordinates are 10-bit, in the 640x480 display space -- not the
// 320x240 of the bitmap modes.
// ---------------------------------------------------------------------
void x16_sprite_pos(unsigned char sprite, unsigned int xpos,
                    unsigned int ypos) {
    x16_sprite_setptr(sprite, 2);       // SPRITE_ATTR_X_L
    __asm {
        lda xpos
        sta 0x9f23                      /* VERA_DATA0 */
        lda xpos+1
        and #0x03
        sta 0x9f23                      /* VERA_DATA0 */
        lda ypos
        sta 0x9f23                      /* VERA_DATA0 */
        lda ypos+1
        and #0x03
        sta 0x9f23                      /* VERA_DATA0 */
    }
}

void x16_sprite_get_pos(unsigned char sprite, unsigned int *xp,
                        unsigned int *yp) {
    x16_sprite_setptr(sprite, 2);       // SPRITE_ATTR_X_L
    __asm {
        lda 0x9f23                      /* VERA_DATA0 */
        sta x16__sp_t0                  /* x lo */
        lda 0x9f23                      /* VERA_DATA0 */
        and #0x03
        sta x16__sp_t1                  /* x hi */
        lda 0x9f23                      /* VERA_DATA0 */
        sta x16__sp_t2                  /* y lo */
        lda 0x9f23                      /* VERA_DATA0 */
        and #0x03
        sta x16__sp_t3                  /* y hi */

        ldy #0
        lda x16__sp_t0
        sta (xp),y
        lda x16__sp_t2
        sta (yp),y
        iny
        lda x16__sp_t1
        sta (xp),y
        lda x16__sp_t3
        sta (yp),y
    }
}

// ---------------------------------------------------------------------
// The record stores address bits 16:5, so the data must be 32-byte
// aligned; the low five bits are simply dropped.
// ---------------------------------------------------------------------
void x16_sprite_image(unsigned char sprite, unsigned char mode,
                      unsigned long addr) {
    x16_sprite_setptr(sprite, 0);       // SPRITE_ATTR_ADDR_L
    __asm {
        lda addr
        lsr
        lsr
        lsr
        lsr
        lsr                             /* addr bits 7:5 -> 2:0 */
        sta x16__sp_t0
        lda addr+1
        asl
        asl
        asl                             /* addr bits 12:8 -> 7:3 */
        ora x16__sp_t0
        sta 0x9f23        /* byte 0 = addr 12:5 (VERA_DATA0) */

        lda addr+1
        lsr
        lsr
        lsr
        lsr
        lsr                             /* addr bits 15:13 -> 2:0 */
        sta x16__sp_t0
        lda addr+2
        and #0x01
        asl
        asl
        asl                             /* addr bit 16 -> bit 3 */
        ora x16__sp_t0
        ora mode                        /* mode in bit 7 */
        sta 0x9f23        /* byte 1 (VERA_DATA0) */
    }
}

void x16_sprite_flags(unsigned char sprite, unsigned char flags) {
    x16_sprite_setptr(sprite, 6);       // SPRITE_ATTR_FLAGS
    __asm {
        lda flags
        sta 0x9f23                      /* VERA_DATA0 */
    }
}

// ---------------------------------------------------------------------
// Read-modify-write on byte 6. Only valid once the record has been
// written at least once (see the note at the top of the header).
// ---------------------------------------------------------------------
void x16_sprite_z(unsigned char sprite, unsigned char zv) {
    x16_sprite_setptr(sprite, 6);       // SPRITE_ATTR_FLAGS
    __asm {
        lda 0x9f23        /* the read advances the port (VERA_DATA0) */
        and #0xf3
        ora zv
        sta x16__sp_t0
    }
    x16_sprite_setptr(sprite, 6);       // point at byte 6 again to write
    __asm {
        lda x16__sp_t0
        sta 0x9f23                      /* VERA_DATA0 */
    }
}

void x16_sprite_size(unsigned char sprite, unsigned char width,
                     unsigned char height, unsigned char pal_offset) {
    __asm {
        lda width
        and #0x03
        asl
        asl
        asl
        asl                             /* width into bits 5:4 */
        sta x16__sp_t0
        lda height
        and #0x03
        asl
        asl
        asl
        asl
        asl
        asl                             /* height into bits 7:6 */
        ora x16__sp_t0
        sta x16__sp_t0
        lda pal_offset
        and #0x0f                        /* >15 must not corrupt the size */
        ora x16__sp_t0
        sta x16__sp_t0
    }
    x16_sprite_setptr(sprite, 7);       // SPRITE_ATTR_SIZE_PAL
    __asm {
        lda x16__sp_t0
        sta 0x9f23                      /* VERA_DATA0 */
    }
}

// ---------------------------------------------------------------------
// Zero all 128 attribute records. Disables every sprite and, more
// importantly, gives the write-only attribute RAM a known host-side
// shadow, so x16_sprite_z's read-modify-write is meaningful.
// ---------------------------------------------------------------------
void x16_sprite_init_all(void) {
    x16_vera_addr0(X16_INC_1, 0x1FC00); // X16_VRAM_SPRITE_ATTR
    x16_vera_fill(0, 128 * 8);
}
