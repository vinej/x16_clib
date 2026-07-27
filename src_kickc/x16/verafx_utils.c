// =====================================================================
// x16clib :: x16/verafx_utils.c -- low-level VERA FX primitives
// =====================================================================
// Raw building blocks for custom FX workflows: FX_CTRL/MULT control,
// cache fill/write/cycle toggles, 32-bit cache loading, multiplier
// accumulator triggers, increment/position registers, 16-bit hop, and
// polygon-fill reads.
//
// They are deliberately separate from x16/verafx.c. That module is the
// high-level helper bundle; this one is for code that wants to compose
// the documented FX registers directly. Probe with x16_vera_has_fx()
// first, exactly as for the high-level routines.
//
// Unlike the verafx.c helpers, NOTHING here resets FX_CTRL on the way
// out: these are register knobs, and turning one leaves the others
// alone. Call x16_fxu_off() (or x16_fx_off()) when you are done.
//
// The register traffic is the x16_library verafx_utils module, byte
// for byte; only the C carriers differ per toolchain. Every dcsel here
// preserves ADDRSEL (lda CTRL / and #1 / ora #n<<1), and the two
// read-triggered registers use `bit`, not `lda` -- KickC 0.8.6's
// optimizer deletes a dead absolute load out of inline asm (and used to
// NPE trying).
// =====================================================================

#include <x16/verafx_utils.h>

__mem volatile char x16__fxu_t;

// Disable the FX helpers and return DCSEL to 0.
void x16_fxu_off(void) {
    asm {
        lda $9f25                       // vera_dcsel 2
        and #$01
        ora #$04
        sta $9f25
        stz $9f29 /*VERA_FX_CTRL*/
        stz $9f2c /*VERA_FX_MULT*/
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}

// ---------------------------------------------------------------------
// FX_CTRL helpers (DCSEL=2)
// ---------------------------------------------------------------------
unsigned char x16_fxu_get_ctrl(void) {
    __mem char r;
    asm {
        lda $9f25                       // vera_dcsel 2
        and #$01
        ora #$04
        sta $9f25
        lda $9f29 /*VERA_FX_CTRL*/
        sta r
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
    return r;
}

void x16_fxu_set_ctrl(__mem unsigned char v) {
    asm {
        lda $9f25                       // vera_dcsel 2
        and #$01
        ora #$04
        sta $9f25
        lda v
        sta $9f29 /*VERA_FX_CTRL*/
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}

void x16_fxu_ctrl_on(__mem unsigned char bits) {
    asm {
        lda $9f25                       // vera_dcsel 2
        and #$01
        ora #$04
        sta $9f25
        lda bits
        tsb $9f29 /*VERA_FX_CTRL*/
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}

void x16_fxu_ctrl_off(__mem unsigned char bits) {
    asm {
        lda $9f25                       // vera_dcsel 2
        and #$01
        ora #$04
        sta $9f25
        lda bits
        trb $9f29 /*VERA_FX_CTRL*/
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}

// Set only the FX_CTRL Addr1 Mode field (bits 1:0).
void x16_fxu_addr1_mode(__mem unsigned char mode) {
    asm {
        lda mode
        and #$03
        sta x16__fxu_t
        lda $9f25                       // vera_dcsel 2
        and #$01
        ora #$04
        sta $9f25
        lda $9f29 /*VERA_FX_CTRL*/
        and #$fc
        ora x16__fxu_t
        sta $9f29
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}

// The per-flag toggles ride on ctrl_on/ctrl_off, exactly as the ca65
// build's bodies do with a jmp.
void x16_fxu_cache_write_on(void)  { x16_fxu_ctrl_on(0x40); }
void x16_fxu_cache_write_off(void) { x16_fxu_ctrl_off(0x40); }
void x16_fxu_cache_fill_on(void)   { x16_fxu_ctrl_on(0x20); }
void x16_fxu_cache_fill_off(void)  { x16_fxu_ctrl_off(0x20); }
void x16_fxu_cache_cycle_on(void)  { x16_fxu_ctrl_on(0x10); }
void x16_fxu_cache_cycle_off(void) { x16_fxu_ctrl_off(0x10); }
void x16_fxu_transparent_on(void)  { x16_fxu_ctrl_on(0x80); }
void x16_fxu_transparent_off(void) { x16_fxu_ctrl_off(0x80); }
void x16_fxu_4bit_on(void)         { x16_fxu_ctrl_on(0x04); }
void x16_fxu_4bit_off(void)        { x16_fxu_ctrl_off(0x04); }
void x16_fxu_hop_on(void)          { x16_fxu_ctrl_on(0x08); }
void x16_fxu_hop_off(void)         { x16_fxu_ctrl_off(0x08); }

// ---------------------------------------------------------------------
// FX_MULT / cache helpers (DCSEL=2 and 6)
// ---------------------------------------------------------------------
void x16_fxu_set_mult(__mem unsigned char v) {
    asm {
        lda $9f25                       // vera_dcsel 2
        and #$01
        ora #$04
        sta $9f25
        lda v
        sta $9f2c /*VERA_FX_MULT*/
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}

// Set all four bytes of the 32-bit cache: L, M, H, U.
void x16_fxu_set_cache(__mem unsigned char l, __mem unsigned char m,
                       __mem unsigned char h, __mem unsigned char u) {
    asm {
        lda $9f25                       // vera_dcsel 6
        and #$01
        ora #$0c
        sta $9f25
        lda l
        sta $9f29 /*VERA_FX_CACHE_L*/
        lda m
        sta $9f2a /*VERA_FX_CACHE_M*/
        lda h
        sta $9f2b /*VERA_FX_CACHE_H*/
        lda u
        sta $9f2c /*VERA_FX_CACHE_U*/
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}

// Clear the multiplier accumulator -- a READ-triggered register.
void x16_fxu_reset_accum(void) {
    asm {
        lda $9f25                       // vera_dcsel 6
        and #$01
        ora #$0c
        sta $9f25
        bit $9f29 /*VERA_FX_ACCUM_RESET*/
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}

// Trigger multiply-then-accumulate -- also read-triggered.
void x16_fxu_accumulate(void) {
    asm {
        lda $9f25                       // vera_dcsel 6
        and #$01
        ora #$0c
        sta $9f25
        bit $9f2a /*VERA_FX_ACCUM*/
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}

// One DATA0/DATA1 read, filling the cache when Cache Fill is enabled.
unsigned char x16_fxu_cache_fill0(void) {
    __mem char r;
    asm {
        lda $9f23 /*VERA_DATA0*/
        sta r
    }
    return r;
}

unsigned char x16_fxu_cache_fill1(void) {
    __mem char r;
    asm {
        lda $9f24 /*VERA_DATA1*/
        sta r
    }
    return r;
}

// One DATA0/DATA1 write of the cache nibble mask, flushing the cache
// when Cache Write is enabled.
void x16_fxu_cache_write0(__mem unsigned char mask) {
    asm {
        lda mask
        sta $9f23 /*VERA_DATA0*/
    }
}

void x16_fxu_cache_write1(__mem unsigned char mask) {
    asm {
        lda mask
        sta $9f24 /*VERA_DATA1*/
    }
}

// ---------------------------------------------------------------------
// Increment, position, tile/map, and polygon-fill helpers (DCSEL=3,4,5)
// ---------------------------------------------------------------------
void x16_fxu_set_incr(__mem unsigned int xi, __mem unsigned int yi) {
    asm {
        lda $9f25                       // vera_dcsel 3
        and #$01
        ora #$06
        sta $9f25
        lda xi
        sta $9f29 /*VERA_FX_X_INCR_L*/
        lda xi+1
        sta $9f2a /*VERA_FX_X_INCR_H*/
        lda yi
        sta $9f2b /*VERA_FX_Y_INCR_L*/
        lda yi+1
        sta $9f2c /*VERA_FX_Y_INCR_H*/
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}

void x16_fxu_set_pos(__mem unsigned int px, __mem unsigned int py) {
    asm {
        lda $9f25                       // vera_dcsel 4
        and #$01
        ora #$08
        sta $9f25
        lda px
        sta $9f29 /*VERA_FX_X_POS_L*/
        lda px+1
        sta $9f2a /*VERA_FX_X_POS_H*/
        lda py
        sta $9f2b /*VERA_FX_Y_POS_L*/
        lda py+1
        sta $9f2c /*VERA_FX_Y_POS_H*/
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}

void x16_fxu_set_subpos(__mem unsigned char xs, __mem unsigned char ys) {
    asm {
        lda $9f25                       // vera_dcsel 5
        and #$01
        ora #$0a
        sta $9f25
        lda xs
        sta $9f29 /*VERA_FX_X_POS_S*/
        lda ys
        sta $9f2a /*VERA_FX_Y_POS_S*/
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}

// Read POLY_FILL_L/H: low byte in the low half, high in the high half.
unsigned int x16_fxu_get_poly_fill(void) {
    __mem unsigned int r;
    asm {
        lda $9f25                       // vera_dcsel 5
        and #$01
        ora #$0a
        sta $9f25
        lda $9f2b /*VERA_FX_POLY_FILL_L*/
        sta r
        lda $9f2c /*VERA_FX_POLY_FILL_H*/
        sta r+1
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
    return r;
}

// Raw affine base register writes: a precomposed FX_TILEBASE/FX_MAPBASE.
void x16_fxu_set_tilebase(__mem unsigned char v) {
    asm {
        lda $9f25                       // vera_dcsel 2
        and #$01
        ora #$04
        sta $9f25
        lda v
        sta $9f2a /*VERA_FX_TILEBASE*/
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}

void x16_fxu_set_mapbase(__mem unsigned char v) {
    asm {
        lda $9f25                       // vera_dcsel 2
        and #$01
        ora #$04
        sta $9f25
        lda v
        sta $9f2b /*VERA_FX_MAPBASE*/
        lda $9f25                       // vera_dcsel 0
        and #$01
        sta $9f25
    }
}
