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
__mem volatile char x16__tl_t[8];

void x16_layer_on(__mem unsigned char layer) {
    asm {
        lda $9f25 /*VERA_CTRL*/         // vera_dcsel 0: keep ADDRSEL,
        and #$01 /*VERA_CTRL_ADDRSEL*/  // never write bit 7
        sta $9f25 /*VERA_CTRL*/
        lda #$10 /*VERA_VIDEO_LAYER0_EN*/
        ldx layer
        beq lon_go
        lda #$20 /*VERA_VIDEO_LAYER1_EN*/
    lon_go:
        tsb $9f29 /*VERA_DC_VIDEO*/
    }
}

void x16_layer_off(__mem unsigned char layer) {
    asm {
        lda $9f25 /*VERA_CTRL*/         // vera_dcsel 0
        and #$01 /*VERA_CTRL_ADDRSEL*/
        sta $9f25 /*VERA_CTRL*/
        lda #$10 /*VERA_VIDEO_LAYER0_EN*/
        ldx layer
        beq lof_go
        lda #$20 /*VERA_VIDEO_LAYER1_EN*/
    lof_go:
        trb $9f29 /*VERA_DC_VIDEO*/
    }
}

// config byte: map height (7:6) | map width (5:4) | T256C (3) |
// bitmap (2) | bpp (1:0)
void x16_layer_set_config(__mem unsigned char layer, __mem unsigned char config) {
    asm {
        ldx #0                          // layer -> register offset
        lda layer
        beq lsc_go
        ldx #7                          // L1_CONFIG - L0_CONFIG
    lsc_go:
        lda config
        sta $9f2d /*VERA_L0_CONFIG*/,x
    }
}

void x16_layer_set_mapbase(__mem unsigned char layer, __mem unsigned char mapbase) {
    asm {
        ldx #0
        lda layer
        beq lsm_go
        ldx #7
    lsm_go:
        lda mapbase
        sta $9f2e /*VERA_L0_MAPBASE*/,x
    }
}

void x16_layer_set_tilebase(__mem unsigned char layer, __mem unsigned char tilebase) {
    asm {
        ldx #0
        lda layer
        beq lst_go
        ldx #7
    lst_go:
        lda tilebase
        sta $9f2f /*VERA_L0_TILEBASE*/,x
    }
}

// 12-bit hardware scroll
void x16_layer_scroll_x(__mem unsigned char layer, __mem unsigned int value) {
    asm {
        ldx #0
        lda layer
        beq lsx_go
        ldx #7
    lsx_go:
        lda value
        sta $9f30 /*VERA_L0_HSCROLL_L*/,x
        lda value+1
        and #$0f
        sta $9f31 /*VERA_L0_HSCROLL_H*/,x
    }
}

void x16_layer_scroll_y(__mem unsigned char layer, __mem unsigned int value) {
    asm {
        ldx #0
        lda layer
        beq lsy_go
        ldx #7
    lsy_go:
        lda value
        sta $9f32 /*VERA_L0_VSCROLL_L*/,x
        lda value+1
        and #$0f
        sta $9f33 /*VERA_L0_VSCROLL_H*/,x
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
void x16_tile_setptr(__mem unsigned char col, __mem unsigned char trow) {
    asm {
        lda $9f34 /*VERA_L1_CONFIG*/
        lsr
        lsr
        lsr
        lsr
        and #$03                        // map width code 0..3
        clc
        adc #6
        tax                             // shift count 6..9

        stz x16__tl_t+1
        stz x16__tl_t+2
        lda trow
        sta x16__tl_t
    tsp_shift:
        asl x16__tl_t
        rol x16__tl_t+1
        rol x16__tl_t+2
        dex
        bne tsp_shift

        // + column * 2  (up to 9 bits)
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

        // + mapbase, which is (register << 9): low byte is always zero.
        lda $9f35 /*VERA_L1_MAPBASE*/
        asl                             // carry = VRAM address bit 16
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

        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/         // ADDRSEL = 0, DCSEL untouched
        lda x16__tl_t
        sta $9f20 /*VERA_ADDR_L*/
        lda x16__tl_t+1
        sta $9f21 /*VERA_ADDR_M*/
        lda x16__tl_t+2
        and #$01 /*VERA_ADDR_H_BANK*/
        ora #$10                        // VERA_INC_1 << 4
        sta $9f22 /*VERA_ADDR_H*/
    }
}

void x16_tile_put(__mem unsigned char col, __mem unsigned char row,
                  __mem unsigned char code, __mem unsigned char attr) {
    x16_tile_setptr(col, row);
    asm {
        lda code
        sta $9f23 /*VERA_DATA0*/
        lda attr
        sta $9f23 /*VERA_DATA0*/
    }
}

unsigned int x16_tile_get(__mem unsigned char col, __mem unsigned char row) {
    __mem unsigned int r;
    x16_tile_setptr(col, row);
    asm {
        lda $9f23 /*VERA_DATA0*/
        sta r                           // screen code
        lda $9f23 /*VERA_DATA0*/
        sta r+1                         // attribute
    }
    return r;
}
