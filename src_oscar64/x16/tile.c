// =====================================================================
// x16clib :: x16/tile.c -- tilemap cells and layer configuration
// =====================================================================
// Same code as src_ca65/video/tile.s. The ca65 build's layer_index
// helper (layer number -> register offset: L1_CONFIG is 7 bytes past
// L0_CONFIG) is inlined into each function here -- it is three
// instructions, and inline asm cannot jsr across functions.
//
// x16_tile_put and x16_tile_get reach the shared address computation
// by calling x16_tile_setptr as a C function, exactly the jsr the ca65
// build did.
// =====================================================================

#include <x16/tile.h>

// tile_setptr's 17-bit address accumulator and shift scratch:
// +0..+2 = address, +6/+7 = the addend being shifted in.
volatile char x16__tl_t[8];

void x16_layer_on(unsigned char layer) {
    __asm {
        lda 0x9f25         /* vera_dcsel 0: keep ADDRSEL, (VERA_CTRL) */
        and #0x01  /* never write bit 7 (VERA_CTRL_ADDRSEL) */
        sta 0x9f25                      /* VERA_CTRL */
        lda #0x10                       /* VERA_VIDEO_LAYER0_EN */
        ldx layer
        beq lon_go
        lda #0x20                       /* VERA_VIDEO_LAYER1_EN */
    lon_go:
        ora 0x9f29                      /* set the layer-enable bit */
        sta 0x9f29                      /* VERA_DC_VIDEO */
    }
}

void x16_layer_off(unsigned char layer) {
    __asm {
        lda 0x9f25         /* vera_dcsel 0 (VERA_CTRL) */
        and #0x01                       /* VERA_CTRL_ADDRSEL */
        sta 0x9f25                      /* VERA_CTRL */
        lda #0x10                       /* VERA_VIDEO_LAYER0_EN */
        ldx layer
        beq lof_go
        lda #0x20                       /* VERA_VIDEO_LAYER1_EN */
    lof_go:
        eor #0xff                       /* clear the layer-enable bit */
        and 0x9f29
        sta 0x9f29                      /* VERA_DC_VIDEO */
    }
}

// config byte: map height (7:6) | map width (5:4) | T256C (3) |
// bitmap (2) | bpp (1:0)
void x16_layer_set_config(unsigned char layer, unsigned char config) {
    __asm {
        ldx #0                          /* layer -> register offset */
        lda layer
        beq lsc_go
        ldx #7                          /* L1_CONFIG - L0_CONFIG */
    lsc_go:
        lda config
        sta 0x9f2d,x                    /* VERA_L0_CONFIG */
    }
}

void x16_layer_set_mapbase(unsigned char layer, unsigned char mapbase) {
    __asm {
        ldx #0
        lda layer
        beq lsm_go
        ldx #7
    lsm_go:
        lda mapbase
        sta 0x9f2e,x                    /* VERA_L0_MAPBASE */
    }
}

void x16_layer_set_tilebase(unsigned char layer, unsigned char tilebase) {
    __asm {
        ldx #0
        lda layer
        beq lst_go
        ldx #7
    lst_go:
        lda tilebase
        sta 0x9f2f,x                    /* VERA_L0_TILEBASE */
    }
}

// 12-bit hardware scroll
void x16_layer_scroll_x(unsigned char layer, unsigned int value) {
    __asm {
        ldx #0
        lda layer
        beq lsx_go
        ldx #7
    lsx_go:
        lda value
        sta 0x9f30,x                    /* VERA_L0_HSCROLL_L */
        lda value+1
        and #0x0f
        sta 0x9f31,x                    /* VERA_L0_HSCROLL_H */
    }
}

void x16_layer_scroll_y(unsigned char layer, unsigned int value) {
    __asm {
        ldx #0
        lda layer
        beq lsy_go
        ldx #7
    lsy_go:
        lda value
        sta 0x9f32,x                    /* VERA_L0_VSCROLL_L */
        lda value+1
        and #0x0f
        sta 0x9f33,x                    /* VERA_L0_VSCROLL_H */
    }
}

// ---------------------------------------------------------------------
// Point data port 0 at a layer-1 tilemap cell.
//
//   address = (L1_MAPBASE << 9) + (row * mapwidth + col) * 2
//
// mapwidth is 32 << ((L1_CONFIG >> 4) & 3), always a power of two, so
// (row * mapwidth) * 2 is just row shifted left by 6 + that field. The
// product needs 17 bits, hence the three-byte shift.
// ---------------------------------------------------------------------
void x16_tile_setptr(unsigned char col, unsigned char trow) {
    __asm {
        lda 0x9f34                      /* VERA_L1_CONFIG */
        lsr
        lsr
        lsr
        lsr
        and #0x03                        /* map width code 0..3 */
        clc
        adc #6
        tax                             /* shift count 6..9 */

        lda #0
        sta x16__tl_t+1
        sta x16__tl_t+2
        lda trow
        sta x16__tl_t
    tsp_shift:
        asl x16__tl_t
        rol x16__tl_t+1
        rol x16__tl_t+2
        dex
        bne tsp_shift

        /* + column * 2  (up to 9 bits) */
        lda col
        asl
        sta x16__tl_t+6
        lda #0
        rol
        sta x16__tl_t+7

        clc
        lda x16__tl_t
        adc x16__tl_t+6
        sta x16__tl_t
        lda x16__tl_t+1
        adc x16__tl_t+7
        sta x16__tl_t+1
        lda x16__tl_t+2
        adc #0
        sta x16__tl_t+2

        /* + mapbase, which is (register << 9): low byte is always zero. */
        lda 0x9f35                      /* VERA_L1_MAPBASE */
        asl                             /* carry = VRAM address bit 16 */
        sta x16__tl_t+6
        lda #0
        rol
        sta x16__tl_t+7

        clc
        lda x16__tl_t+1
        adc x16__tl_t+6
        sta x16__tl_t+1
        lda x16__tl_t+2
        adc x16__tl_t+7
        sta x16__tl_t+2

        lda 0x9f25  /*VERA_CTRL*/         /* ADDRSEL = 0, DCSEL untouched */
        and #0xfe
        sta 0x9f25
        lda x16__tl_t
        sta 0x9f20                      /* VERA_ADDR_L */
        lda x16__tl_t+1
        sta 0x9f21                      /* VERA_ADDR_M */
        lda x16__tl_t+2
        and #0x01                       /* VERA_ADDR_H_BANK */
        ora #0x10                        /* VERA_INC_1 << 4 */
        sta 0x9f22                      /* VERA_ADDR_H */
    }
}

void x16_tile_put(unsigned char col, unsigned char row,
                  unsigned char code, unsigned char attr) {
    x16_tile_setptr(col, row);
    __asm {
        lda code
        sta 0x9f23                      /* VERA_DATA0 */
        lda attr
        sta 0x9f23                      /* VERA_DATA0 */
    }
}

unsigned int x16_tile_get(unsigned char col, unsigned char row) {
    x16_tile_setptr(col, row);
    return __asm {
        lda 0x9f23                      /* VERA_DATA0 */
        sta accu                           /* screen code */
        lda 0x9f23                      /* VERA_DATA0 */
        sta accu + 1                         /* attribute */
    };
}
