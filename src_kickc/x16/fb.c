// =====================================================================
// x16clib :: x16/fb.c -- the KERNAL framebuffer driver API
// =====================================================================
// Wrappers over the stable FB_* jump table. The default driver is the
// ROM's 320x240@8bpp bitmap at VRAM $00000; GRAPH can install another.
//
// CALL x16_graph_init() FIRST. The FB entries dispatch through vectors
// GRAPH_INIT installs; before that they point nowhere.
//
// The driver is a cursor machine: position the cursor, then get/set
// pixels advances it. That is what makes runs cheap -- no per-pixel
// address math.
//
// Zero counts are guarded here: the ROM driver's loops treat 0 as 256.
// =====================================================================

#include <x16/fb.h>

void x16__fb_tramp(void);
void x16__fb_thunk(void);

// Pointer scratch, pinned in zero page (KickC ignores __zp on
// parameters; see x16/zpsafe.h).
__address(0x78) unsigned int* volatile x16__fb_p0;
__address(0x7a) unsigned int* volatile x16__fb_p1;

__mem volatile unsigned char x16__fb_v;

// The filter bridge. The ROM hands each colour over in A and wants the
// replacement back in A, but KickC decides for itself where a C
// function's argument lives, so the ROM cannot call one directly. The
// asm trampoline parks the colour here and calls a C thunk that takes
// NO arguments and reads it -- a convention both sides can agree on.
__mem volatile unsigned char x16__fb_c;
__mem x16_fb_filter volatile x16__fb_fn = 0;

// Statically initialised, so KickC cannot fold the address away (the
// same trick irq.c uses for its handler).
__mem volatile unsigned int x16__fb_tramp_ptr = (unsigned int)&x16__fb_tramp;
__mem volatile unsigned int x16__fb_thunk_ptr = (unsigned int)&x16__fb_thunk;

// ---------------------------------------------------------------------
// Reinitialize the active framebuffer driver (mode registers, base).
// ---------------------------------------------------------------------
void x16_fb_init(void) {
    asm {
        jsr $fef6 /*FB_INIT*/
    }
}

// ---------------------------------------------------------------------
// Width, height and the colour depth in bits per pixel (the return).
// ---------------------------------------------------------------------
unsigned char x16_fb_get_info(unsigned int *width,
                              unsigned int *height) {
    __mem unsigned char w_lo;
    __mem unsigned char w_hi;
    __mem unsigned char h_lo;
    __mem unsigned char h_hi;
    asm {
        jsr $fef9 /*FB_GET_INFO*/       // r0 = width, r1 = height, A = depth
        sta x16__fb_v
        lda $02 /*r0L*/
        sta w_lo
        lda $03 /*r0H*/
        sta w_hi
        lda $04 /*r1L*/
        sta h_lo
        lda $05 /*r1H*/
        sta h_hi
    }
    *width  = (unsigned int)w_lo | ((unsigned int)w_hi << 8);
    *height = (unsigned int)h_lo | ((unsigned int)h_hi << 8);
    return x16__fb_v;
}

// ---------------------------------------------------------------------
// `data` is count*2 bytes of VERA GB/R words. count 0 means all 256.
// ---------------------------------------------------------------------
void x16_fb_set_palette(const void *data,
                        __mem unsigned char start,
                        __mem unsigned char count) {
    asm {
        lda data
        sta $02 /*r0L*/
        lda data+1
        sta $03 /*r0H*/
        lda start
        ldx count
        jsr $fefc /*FB_SET_PALETTE*/
    }
}

// ---------------------------------------------------------------------
// Park the pixel cursor.
// ---------------------------------------------------------------------
void x16_fb_cursor_position(__mem unsigned int px,
                            __mem unsigned int py) {
    asm {
        lda px
        sta $02 /*r0L*/
        lda px+1
        sta $03 /*r0H*/
        lda py
        sta $04 /*r1L*/
        lda py+1
        sta $05 /*r1H*/
        jsr $feff /*FB_CURSOR_POSITION*/
    }
}

// ---------------------------------------------------------------------
// Move to the next scanline -- cheaper than a full cursor_position.
// The API passes px for drivers that need it; the default 320x240 driver
// keeps its own position and ignores it.
// ---------------------------------------------------------------------
void x16_fb_cursor_next_line(__mem unsigned int px) {
    asm {
        lda px
        sta $02 /*r0L*/
        lda px+1
        sta $03 /*r0H*/
        jsr $ff02 /*FB_CURSOR_NEXT_LINE*/
    }
}

// ---------------------------------------------------------------------
// Read at the cursor and advance it.
// ---------------------------------------------------------------------
unsigned char x16_fb_get_pixel(void) {
    asm {
        jsr $ff05 /*FB_GET_PIXEL*/
        sta x16__fb_v
    }
    return x16__fb_v;
}

// ---------------------------------------------------------------------
// Write at the cursor and advance it.
// ---------------------------------------------------------------------
void x16_fb_set_pixel(__mem unsigned char color) {
    asm {
        lda color
        jsr $ff0b /*FB_SET_PIXEL*/
    }
}

// ---------------------------------------------------------------------
// A run of pixels out of / into memory. A zero count does nothing.
// ---------------------------------------------------------------------
void x16_fb_get_pixels(void *dst,
                       __mem unsigned int count) {
    if (count == 0) {
        return;
    }
    asm {
        lda dst
        sta $02 /*r0L*/
        lda dst+1
        sta $03 /*r0H*/
        lda count
        sta $04 /*r1L*/
        lda count+1
        sta $05 /*r1H*/
        jsr $ff08 /*FB_GET_PIXELS*/
    }
}

void x16_fb_set_pixels(const void *src,
                       __mem unsigned int count) {
    if (count == 0) {
        return;
    }
    asm {
        lda src
        sta $02 /*r0L*/
        lda src+1
        sta $03 /*r0H*/
        lda count
        sta $04 /*r1L*/
        lda count+1
        sta $05 /*r1H*/
        jsr $ff0e /*FB_SET_PIXELS*/
    }
}

// ---------------------------------------------------------------------
// Draw the pattern's 1-bits in `color`, MSB first; 0-bits leave the
// underlying pixels alone. Always advances the cursor by 8.
// ---------------------------------------------------------------------
void x16_fb_set_8_pixels(__mem unsigned char pattern,
                         __mem unsigned char color) {
    asm {
        lda pattern
        ldx color
        jsr $ff11 /*FB_SET_8_PIXELS*/
    }
}

// ---------------------------------------------------------------------
// For each 1-bit of `mask` (MSB first): draw fg where the pattern bit
// is 1, bg where it is 0. 0-bits of the mask leave the pixel alone.
// ---------------------------------------------------------------------
void x16_fb_set_8_pixels_opaque(__mem unsigned char pattern,
                                __mem unsigned char mask,
                                __mem unsigned char fg,
                                __mem unsigned char bg) {
    asm {
        lda pattern
        sta $02 /*r0L*/
        lda mask
        ldx fg
        ldy bg
        jsr $ff14 /*FB_SET_8_PIXELS_OPAQUE*/
    }
}

// ---------------------------------------------------------------------
// step 0 or 1 is a solid run (hardware-accelerated); larger steps space
// the pixels out -- vertical lines, dithers.
// ---------------------------------------------------------------------
void x16_fb_fill_pixels(__mem unsigned int count,
                        __mem unsigned int step,
                        __mem unsigned char color) {
    if (count == 0) {
        return;
    }
    asm {
        lda count
        sta $02 /*r0L*/
        lda count+1
        sta $03 /*r0H*/
        lda step
        sta $04 /*r1L*/
        lda step+1
        sta $05 /*r1H*/
        lda color
        jsr $ff17 /*FB_FILL_PIXELS*/
    }
}

// ---------------------------------------------------------------------
// Run `filter` over count pixels from the cursor: it receives each
// colour and returns the replacement.
//
// See the bridge note at the top of this file for why this goes through
// two hops rather than handing the ROM the C function directly.
// ---------------------------------------------------------------------
void x16_fb_filter_pixels(__mem unsigned int count,
                          __mem x16_fb_filter filter) {
    if (count == 0) {
        return;
    }
    x16__fb_fn = filter;
    asm {
        lda count
        sta $02 /*r0L*/
        lda count+1
        sta $03 /*r0H*/
        lda x16__fb_tramp_ptr
        sta $04 /*r1L*/
        lda x16__fb_tramp_ptr+1
        sta $05 /*r1H*/
        jsr $ff1a /*FB_FILTER_PIXELS*/
    }
}

// The ROM calls r1's target with A = colour and wants A = the new
// colour back, with X and Y intact -- they are its own loop counters.
void x16__fb_tramp(void) {
    asm {
        phx
        phy
        sta x16__fb_c
        jsr fb_call
        lda x16__fb_c
        ply
        plx
        jmp fb_tramp_out
    fb_call:
        jmp (x16__fb_thunk_ptr)
    fb_tramp_out:
    }
}

// The C half of the bridge: no arguments, so KickC and the trampoline
// agree on where the colour is.
void x16__fb_thunk(void) {
    x16__fb_c = (*x16__fb_fn)(x16__fb_c);
}

// ---------------------------------------------------------------------
// Copy a horizontal span of count pixels from (sx,sy) to (tx,ty).
// ---------------------------------------------------------------------
void x16_fb_move_pixels(__mem unsigned int sx,
                        __mem unsigned int sy,
                        __mem unsigned int tx,
                        __mem unsigned int ty,
                        __mem unsigned int count) {
    if (count == 0) {
        return;
    }
    asm {
        lda sx
        sta $02 /*r0L*/
        lda sx+1
        sta $03 /*r0H*/
        lda sy
        sta $04 /*r1L*/
        lda sy+1
        sta $05 /*r1H*/
        lda tx
        sta $06 /*r2L*/
        lda tx+1
        sta $07 /*r2H*/
        lda ty
        sta $08 /*r3L*/
        lda ty+1
        sta $09 /*r3H*/
        lda count
        sta $0a /*r4L*/
        lda count+1
        sta $0b /*r4H*/
        jsr $ff1d /*FB_MOVE_PIXELS*/
    }
}
