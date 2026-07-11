// =====================================================================
// x16clib :: x16/palette.c -- VERA palette
// =====================================================================
// Same code as src_ca65/video/palette.s. The palette starts at VRAM
// $1FA00: index*2 sets the carry when it crosses into $FBxx, and that
// carry rolls into the middle address byte -- the adc #0 below.
// =====================================================================

#include <x16/palette.h>

// The bulk load indirects straight through the `src` parameter --
// Oscar64 keeps pointer parameters in zero page, where (src),y reaches
// them. The cc65 build used X16_PTR0 the same way.

void x16_pal_set(unsigned char index, unsigned int color) {
    __asm {
        lda 0x9f25  /*VERA_CTRL*/         /* ADDRSEL = 0, DCSEL untouched */
        and #0xfe
        sta 0x9f25
        lda index
        asl                             /* index * 2; carry = addr bit 8 */
        sta 0x9f20                      /* VERA_ADDR_L */
        lda #0xfa                        /* >VRAM_PALETTE */
        adc #0                          /* the asl's carry rolls it to 0xFB */
        sta 0x9f21                      /* VERA_ADDR_M */
        lda #0x11                        /* ADDR_H_BANK | (VERA_INC_1 << 4) */
        sta 0x9f22       /* 0x1FA00 is in bank 1 (VERA_ADDR_H) */
        lda color
        sta 0x9f23        /* Green<<4 | Blue (VERA_DATA0) */
        lda color+1
        sta 0x9f23        /* Red (VERA_DATA0) */
    }
}

void x16_pal_load(const unsigned int *src, unsigned char first,
                  unsigned char count) {
    __asm {
        lda count                       /* count 0 loads nothing -- without */
        beq pl_done                     /* this guard the loop would run 256 */
                                        /* times and shred the whole palette */
        lda 0x9f25  /*VERA_CTRL*/
        and #0xfe
        sta 0x9f25
        lda first
        asl
        sta 0x9f20                      /* VERA_ADDR_L */
        lda #0xfa                        /* >VRAM_PALETTE */
        adc #0
        sta 0x9f21                      /* VERA_ADDR_M */
        lda #0x11                        /* ADDR_H_BANK | (VERA_INC_1 << 4) */
        sta 0x9f22                      /* VERA_ADDR_H */
        ldy #0
    pl_loop:
        lda (src),y
        sta 0x9f23        /* low byte (VERA_DATA0) */
        iny
        lda (src),y
        sta 0x9f23        /* high byte (VERA_DATA0) */
        iny
        dec count
        bne pl_loop
    pl_done:
    }
}
