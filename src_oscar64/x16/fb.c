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

// Oscar64 loses an asm write to a C local, even a volatile one, so
// anything the assembly stores to lives at module scope.
volatile unsigned char h_hi;
volatile unsigned char h_lo;
volatile unsigned char w_hi;
volatile unsigned char w_lo;

void x16__fb_tramp(void);
void x16__fb_thunk(void);

// Pointer scratch, pinned in zero page (KickC ignores __zp on
// parameters; see x16/zpsafe.h).
unsigned int* volatile x16__fb_p0;
unsigned int* volatile x16__fb_p1;

volatile unsigned char x16__fb_v;

// The filter bridge. The ROM hands each colour over in A and wants the
// replacement back in A, but KickC decides for itself where a C
// function's argument lives, so the ROM cannot call one directly. The
// asm trampoline parks the colour here and calls a C thunk that takes
// NO arguments and reads it -- a convention both sides can agree on.
volatile unsigned char x16__fb_c;
volatile unsigned char x16__fb_sx;
volatile unsigned char x16__fb_sy;
volatile x16_fb_filter x16__fb_fn = 0;

// Statically initialised, so the compiler cannot fold the address away.
// Typed as function pointers rather than cast to int: Oscar64 rejects
// (unsigned int)&func as a non-constant initializer, and irq.c already
// takes a handler's address this way.
typedef void (*x16__fb_vec)(void);
volatile x16__fb_vec x16__fb_tramp_ptr = &x16__fb_tramp;
volatile x16__fb_vec x16__fb_thunk_ptr = &x16__fb_thunk;

// ---------------------------------------------------------------------
// Reinitialize the active framebuffer driver (mode registers, base).
// ---------------------------------------------------------------------
void x16_fb_init(void) {
    __asm {
        jsr 0xfef6                      // FB_INIT
    }
}

// ---------------------------------------------------------------------
// Width, height and the colour depth in bits per pixel (the return).
// ---------------------------------------------------------------------
unsigned char x16_fb_get_info(unsigned int *width,
                              unsigned int *height) {
    unsigned char *wb;
    __asm {
        jsr 0xfef9       // r0 = width, r1 = height, A = depth (FB_GET_INFO)
        sta x16__fb_v
        lda 0x02                        // r0L
        sta w_lo
        lda 0x03                        // r0H
        sta w_hi
        lda 0x04                        // r1L
        sta h_lo
        lda 0x05                        // r1H
        sta h_hi
    }
    /* Byte stores; see the note in mouse.c. */
    wb = (unsigned char *)width;
    wb[0] = w_lo;
    wb[1] = w_hi;
    wb = (unsigned char *)height;
    wb[0] = h_lo;
    wb[1] = h_hi;
    return x16__fb_v;
}

// ---------------------------------------------------------------------
// `data` is count*2 bytes of VERA GB/R words. count 0 means all 256.
// ---------------------------------------------------------------------
void x16_fb_set_palette(const void *data,
                        unsigned char start,
                        unsigned char count) {
    __asm {
        lda data
        sta 0x02                        // r0L
        lda data+1
        sta 0x03                        // r0H
        lda start
        ldx count
        jsr 0xfefc                      // FB_SET_PALETTE
    }
}

// ---------------------------------------------------------------------
// Park the pixel cursor.
// ---------------------------------------------------------------------
void x16_fb_cursor_position(unsigned int px,
                            unsigned int py) {
    __asm {
        lda px
        sta 0x02                        // r0L
        lda px+1
        sta 0x03                        // r0H
        lda py
        sta 0x04                        // r1L
        lda py+1
        sta 0x05                        // r1H
        jsr 0xfeff                      // FB_CURSOR_POSITION
    }
}

// ---------------------------------------------------------------------
// Move to the next scanline -- cheaper than a full cursor_position.
// The API passes px for drivers that need it; the default 320x240 driver
// keeps its own position and ignores it.
// ---------------------------------------------------------------------
void x16_fb_cursor_next_line(unsigned int px) {
    __asm {
        lda px
        sta 0x02                        // r0L
        lda px+1
        sta 0x03                        // r0H
        jsr 0xff02                      // FB_CURSOR_NEXT_LINE
    }
}

// ---------------------------------------------------------------------
// Read at the cursor and advance it.
// ---------------------------------------------------------------------
unsigned char x16_fb_get_pixel(void) {
    __asm {
        jsr 0xff05                      // FB_GET_PIXEL
        sta x16__fb_v
    }
    return x16__fb_v;
}

// ---------------------------------------------------------------------
// Write at the cursor and advance it.
// ---------------------------------------------------------------------
void x16_fb_set_pixel(unsigned char color) {
    __asm {
        lda color
        jsr 0xff0b                      // FB_SET_PIXEL
    }
}

// ---------------------------------------------------------------------
// A run of pixels out of / into memory. A zero count does nothing.
// ---------------------------------------------------------------------
void x16_fb_get_pixels(void *dst,
                       unsigned int count) {
    if (count == 0) {
        return;
    }
    __asm {
        lda dst
        sta 0x02                        // r0L
        lda dst+1
        sta 0x03                        // r0H
        lda count
        sta 0x04                        // r1L
        lda count+1
        sta 0x05                        // r1H
        jsr 0xff08                      // FB_GET_PIXELS
    }
}

void x16_fb_set_pixels(const void *src,
                       unsigned int count) {
    if (count == 0) {
        return;
    }
    __asm {
        lda src
        sta 0x02                        // r0L
        lda src+1
        sta 0x03                        // r0H
        lda count
        sta 0x04                        // r1L
        lda count+1
        sta 0x05                        // r1H
        jsr 0xff0e                      // FB_SET_PIXELS
    }
}

// ---------------------------------------------------------------------
// Draw the pattern's 1-bits in `color`, MSB first; 0-bits leave the
// underlying pixels alone. Always advances the cursor by 8.
// ---------------------------------------------------------------------
void x16_fb_set_8_pixels(unsigned char pattern,
                         unsigned char color) {
    __asm {
        lda pattern
        ldx color
        jsr 0xff11                      // FB_SET_8_PIXELS
    }
}

// ---------------------------------------------------------------------
// For each 1-bit of `mask` (MSB first): draw fg where the pattern bit
// is 1, bg where it is 0. 0-bits of the mask leave the pixel alone.
// ---------------------------------------------------------------------
void x16_fb_set_8_pixels_opaque(unsigned char pattern,
                                unsigned char mask,
                                unsigned char fg,
                                unsigned char bg) {
    __asm {
        lda pattern
        sta 0x02                        // r0L
        lda mask
        ldx fg
        ldy bg
        jsr 0xff14                      // FB_SET_8_PIXELS_OPAQUE
    }
}

// ---------------------------------------------------------------------
// step 0 or 1 is a solid run (hardware-accelerated); larger steps space
// the pixels out -- vertical lines, dithers.
// ---------------------------------------------------------------------
void x16_fb_fill_pixels(unsigned int count,
                        unsigned int step,
                        unsigned char color) {
    if (count == 0) {
        return;
    }
    __asm {
        lda count
        sta 0x02                        // r0L
        lda count+1
        sta 0x03                        // r0H
        lda step
        sta 0x04                        // r1L
        lda step+1
        sta 0x05                        // r1H
        lda color
        jsr 0xff17                      // FB_FILL_PIXELS
    }
}

// ---------------------------------------------------------------------
// DOES NOT WORK YET ON OSCAR64. Everything around it does -- measured,
// not guessed:
//
//   * the trampoline reaches the ROM: the C thunk runs exactly once per
//     pixel (a counter in it read 4 for a 4-pixel span);
//   * the trampoline's return path is sound: hard-coding `lda #0x42`
//     there puts 0x42 in all four pixels;
//   * the colour arrives intact: a filter that logged its argument saw
//     0x11 four times, and returned 0x12 correctly.
//
// What fails is the value coming back out of the C thunk. It reads as
// garbage that TRACKS THE PROGRAM'S LAYOUT -- 0xED, 0xF7 and 0x03 in
// three builds differing only by an added counter -- so Oscar64 is
// folding or reordering the read-modify-write of x16__fb_c around the
// asm block that also writes it. Splitting it into separate in and out
// bytes did not help (the call then read back 0), and __noinline on the
// filter changed nothing.
//
// The KickC build, which uses the same two-hop bridge, is correct; so is
// cc65, llvm-mos and vbcc. The test is skipped rather than asserted at
// the wrong value -- see test_oscar64/runner8.c.
//
// Run `filter` over count pixels from the cursor: it receives each
// colour and returns the replacement.
//
// See the bridge note at the top of this file for why this goes through
// two hops rather than handing the ROM the C function directly.
// ---------------------------------------------------------------------
void x16_fb_filter_pixels(unsigned int count,
                          x16_fb_filter filter) {
    if (count == 0) {
        return;
    }
    x16__fb_fn = filter;
    __asm {
        lda count
        sta 0x02                        // r0L
        lda count+1
        sta 0x03                        // r0H
        lda x16__fb_tramp_ptr
        sta 0x04                        // r1L
        lda x16__fb_tramp_ptr+1
        sta 0x05                        // r1H
        jsr 0xff1a                      // FB_FILTER_PIXELS
    }
}

// The ROM calls r1's target with A = colour and wants A = the new
// colour back, with X and Y intact -- they are its own loop counters.
void x16__fb_tramp(void) {
    __asm {
        sta x16__fb_c                   /* the colour first: NMOS has no  */
        stx x16__fb_sx                  /* phx/phy, and routing X or Y    */
        sty x16__fb_sy                  /* through A would lose it        */
        jsr fb_call
        ldx x16__fb_sx
        ldy x16__fb_sy
        lda x16__fb_c
        jmp fb_tramp_out
    fb_call:
        jmp (x16__fb_thunk_ptr)
    fb_tramp_out:
    }
}

// The C half of the bridge: no arguments, so KickC and the trampoline
// agree on where the colour is.
// KNOWN BROKEN ON OSCAR64 -- see the note above x16_fb_filter_pixels.
void x16__fb_thunk(void) {
    x16__fb_c = (*x16__fb_fn)(x16__fb_c);
}

// ---------------------------------------------------------------------
// Copy a horizontal span of count pixels from (sx,sy) to (tx,ty).
// ---------------------------------------------------------------------
void x16_fb_move_pixels(unsigned int sx,
                        unsigned int sy,
                        unsigned int tx,
                        unsigned int ty,
                        unsigned int count) {
    if (count == 0) {
        return;
    }
    __asm {
        lda sx
        sta 0x02                        // r0L
        lda sx+1
        sta 0x03                        // r0H
        lda sy
        sta 0x04                        // r1L
        lda sy+1
        sta 0x05                        // r1H
        lda tx
        sta 0x06                        // r2L
        lda tx+1
        sta 0x07                        // r2H
        lda ty
        sta 0x08                        // r3L
        lda ty+1
        sta 0x09                        // r3H
        lda count
        sta 0x0a                        // r4L
        lda count+1
        sta 0x0b                        // r4H
        jsr 0xff1d                      // FB_MOVE_PIXELS
    }
}
