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
// `zv` for `z`): single-letter register names collide with KickC's
// inline-asm grammar.
// =====================================================================

#include <x16/sprite.h>
#include <x16/vera.h>

// The out-param pointers, pinned in zero page (KickC ignores __zp on
// parameters; see x16/zpsafe.h). The cc65 build used ptr1/ptr2.
__address(0x78) char* volatile x16__sp_xp;
__address(0x7a) char* volatile x16__sp_yp;

__mem volatile char x16__sp_hi;               // sprite*8 high bits
__mem volatile char x16__sp_t0;
__mem volatile char x16__sp_t1;
__mem volatile char x16__sp_t2;
__mem volatile char x16__sp_t3;

// ---------------------------------------------------------------------
// Point data port 0 at one byte of a sprite record, on auto-increment.
// ---------------------------------------------------------------------
void x16_sprite_setptr(__mem unsigned char sprite, __mem unsigned char offset) {
    asm {
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/         // ADDRSEL = 0, DCSEL untouched

        stz x16__sp_hi
        lda sprite
        asl
        rol x16__sp_hi
        asl
        rol x16__sp_hi
        asl
        rol x16__sp_hi                  // hi:A = sprite * 8

        clc
        adc offset
        sta $9f20 /*VERA_ADDR_L*/
        lda x16__sp_hi
        adc #$fc                        // >VRAM_SPRITE_ATTR, + any carry
        sta $9f21 /*VERA_ADDR_M*/
        lda #$11                        // ADDR_H_BANK | (VERA_INC_1 << 4)
        sta $9f22 /*VERA_ADDR_H*/
    }
}

void x16_sprites_on(void) {
    asm {
        lda $9f25 /*VERA_CTRL*/         // vera_dcsel 0
        and #$01 /*VERA_CTRL_ADDRSEL*/
        sta $9f25 /*VERA_CTRL*/
        lda #$40 /*VERA_VIDEO_SPRITES_EN*/
        tsb $9f29 /*VERA_DC_VIDEO*/
    }
}

void x16_sprites_off(void) {
    asm {
        lda $9f25 /*VERA_CTRL*/         // vera_dcsel 0
        and #$01 /*VERA_CTRL_ADDRSEL*/
        sta $9f25 /*VERA_CTRL*/
        lda #$40 /*VERA_VIDEO_SPRITES_EN*/
        trb $9f29 /*VERA_DC_VIDEO*/
    }
}

// ---------------------------------------------------------------------
// Coordinates are 10-bit, in the 640x480 display space -- not the
// 320x240 of the bitmap modes.
// ---------------------------------------------------------------------
void x16_sprite_pos(__mem unsigned char sprite, __mem unsigned int xpos,
                    __mem unsigned int ypos) {
    x16_sprite_setptr(sprite, 2);       // SPRITE_ATTR_X_L
    asm {
        lda xpos
        sta $9f23 /*VERA_DATA0*/
        lda xpos+1
        and #$03
        sta $9f23 /*VERA_DATA0*/
        lda ypos
        sta $9f23 /*VERA_DATA0*/
        lda ypos+1
        and #$03
        sta $9f23 /*VERA_DATA0*/
    }
}

void x16_sprite_get_pos(__mem unsigned char sprite, unsigned int *xp,
                        unsigned int *yp) {
    x16__sp_xp = (char*)xp;
    x16__sp_yp = (char*)yp;
    x16_sprite_setptr(sprite, 2);       // SPRITE_ATTR_X_L
    asm {
        lda $9f23 /*VERA_DATA0*/
        sta x16__sp_t0                  // x lo
        lda $9f23 /*VERA_DATA0*/
        and #$03
        sta x16__sp_t1                  // x hi
        lda $9f23 /*VERA_DATA0*/
        sta x16__sp_t2                  // y lo
        lda $9f23 /*VERA_DATA0*/
        and #$03
        sta x16__sp_t3                  // y hi

        ldy #0
        lda x16__sp_t0
        sta (x16__sp_xp),y
        lda x16__sp_t2
        sta (x16__sp_yp),y
        iny
        lda x16__sp_t1
        sta (x16__sp_xp),y
        lda x16__sp_t3
        sta (x16__sp_yp),y
    }
}

// ---------------------------------------------------------------------
// The record stores address bits 16:5, so the data must be 32-byte
// aligned; the low five bits are simply dropped.
// ---------------------------------------------------------------------
void x16_sprite_image(__mem unsigned char sprite, __mem unsigned char mode,
                      __mem unsigned long addr) {
    x16_sprite_setptr(sprite, 0);       // SPRITE_ATTR_ADDR_L
    asm {
        lda addr
        lsr
        lsr
        lsr
        lsr
        lsr                             // addr bits 7:5 -> 2:0
        sta x16__sp_t0
        lda addr+1
        asl
        asl
        asl                             // addr bits 12:8 -> 7:3
        ora x16__sp_t0
        sta $9f23 /*VERA_DATA0*/        // byte 0 = addr 12:5

        lda addr+1
        lsr
        lsr
        lsr
        lsr
        lsr                             // addr bits 15:13 -> 2:0
        sta x16__sp_t0
        lda addr+2
        and #$01
        asl
        asl
        asl                             // addr bit 16 -> bit 3
        ora x16__sp_t0
        ora mode                        // mode in bit 7
        sta $9f23 /*VERA_DATA0*/        // byte 1
    }
}

void x16_sprite_flags(__mem unsigned char sprite, __mem unsigned char flags) {
    x16_sprite_setptr(sprite, 6);       // SPRITE_ATTR_FLAGS
    asm {
        lda flags
        sta $9f23 /*VERA_DATA0*/
    }
}

// ---------------------------------------------------------------------
// Read-modify-write on byte 6. Only valid once the record has been
// written at least once (see the note at the top of the header).
// ---------------------------------------------------------------------
void x16_sprite_z(__mem unsigned char sprite, __mem unsigned char zv) {
    x16_sprite_setptr(sprite, 6);       // SPRITE_ATTR_FLAGS
    asm {
        lda $9f23 /*VERA_DATA0*/        // the read advances the port
        and #%11110011
        ora zv
        sta x16__sp_t0
    }
    x16_sprite_setptr(sprite, 6);       // point at byte 6 again to write
    asm {
        lda x16__sp_t0
        sta $9f23 /*VERA_DATA0*/
    }
}

void x16_sprite_size(__mem unsigned char sprite, __mem unsigned char width,
                     __mem unsigned char height, __mem unsigned char pal_offset) {
    asm {
        lda width
        and #$03
        asl
        asl
        asl
        asl                             // width into bits 5:4
        sta x16__sp_t0
        lda height
        and #$03
        asl
        asl
        asl
        asl
        asl
        asl                             // height into bits 7:6
        ora x16__sp_t0
        sta x16__sp_t0
        lda pal_offset
        and #$0f                        // >15 must not corrupt the size
        ora x16__sp_t0
        sta x16__sp_t0
    }
    x16_sprite_setptr(sprite, 7);       // SPRITE_ATTR_SIZE_PAL
    asm {
        lda x16__sp_t0
        sta $9f23 /*VERA_DATA0*/
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
