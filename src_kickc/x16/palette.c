// =====================================================================
// x16clib :: x16/palette.c -- VERA palette
// =====================================================================
// Same code as src_ca65/video/palette.s. The palette starts at VRAM
// $1FA00: index*2 sets the carry when it crosses into $FBxx, and that
// carry rolls into the middle address byte -- the adc #0 below.
// =====================================================================

#include <x16/palette.h>

// The bulk-load source pointer, pinned in zero page: KickC ignores
// __zp on parameters and may spill one to main memory, where (src),y
// cannot reach. The cc65 build used X16_PTR0 the same way.
__address(0x78) const char* volatile x16__pl_src;

void x16_pal_set(__mem unsigned char index, __mem unsigned int color) {
    asm {
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/         // ADDRSEL = 0, DCSEL untouched
        lda index
        asl                             // index * 2; carry = addr bit 8
        sta $9f20 /*VERA_ADDR_L*/
        lda #$fa                        // >VRAM_PALETTE
        adc #0                          // the asl's carry rolls it to $FB
        sta $9f21 /*VERA_ADDR_M*/
        lda #$11                        // ADDR_H_BANK | (VERA_INC_1 << 4)
        sta $9f22 /*VERA_ADDR_H*/       // $1FA00 is in bank 1
        lda color
        sta $9f23 /*VERA_DATA0*/        // Green<<4 | Blue
        lda color+1
        sta $9f23 /*VERA_DATA0*/        // Red
    }
}

void x16_pal_load(const unsigned int *src, __mem unsigned char first,
                  __mem unsigned char count) {
    x16__pl_src = (char*)src;
    asm {
        lda count                       // count 0 loads nothing -- without
        beq pl_done                     // this guard the loop would run 256
                                        // times and shred the whole palette
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/
        lda first
        asl
        sta $9f20 /*VERA_ADDR_L*/
        lda #$fa                        // >VRAM_PALETTE
        adc #0
        sta $9f21 /*VERA_ADDR_M*/
        lda #$11                        // ADDR_H_BANK | (VERA_INC_1 << 4)
        sta $9f22 /*VERA_ADDR_H*/
        ldy #0
    pl_loop:
        lda (x16__pl_src),y
        sta $9f23 /*VERA_DATA0*/        // low byte
        iny
        lda (x16__pl_src),y
        sta $9f23 /*VERA_DATA0*/        // high byte
        iny
        dec count
        bne pl_loop
    pl_done:
    }
}
