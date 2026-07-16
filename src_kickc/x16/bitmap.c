// =====================================================================
// x16clib :: x16/bitmap.c -- 320x240x256 bitmap drawing
// =====================================================================
// The framebuffer is 8bpp at VRAM $00000, one byte per pixel, rows of
// 320. A pixel is at y*320 + x.
//
// x16_gfx_pset clips. The line/rect primitives do NOT: they assume
// their arguments are on screen.
//
// HOW THIS PORT IS SPLIT. The hot per-pixel paths -- the y*320+x port
// pointer (setptr), the clipped pixel store, the flood fill's VRAM
// scans -- are the same hand-written 6502 as src_ca65/gfx/bitmap.s,
// operating on a module operand block. The ORCHESTRATION around them
// (Bresenham's error steps, the circle's octant walk, the glyph loops,
// the flood's span stack) is C here, mirroring the ca65 control flow
// statement for statement: inline asm cannot jsr another function, and
// those layers spend their time inside the asm they drive.
// =====================================================================

#include <x16/bitmap.h>
#include <x16/vera.h>
#include <x16/screen.h>

// The operand block (the ca65 build's X16_P0..P3).
__mem volatile unsigned int x16__gp_x;
__mem volatile char x16__gp_y;
__mem volatile char x16__gp_c;
__mem volatile char x16__gp_off;        // pset's clip verdict

__mem volatile char x16__gp_t0;         // setptr's 17-bit accumulator
__mem volatile char x16__gp_t1;
__mem volatile char x16__gp_t2;

// ---------------------------------------------------------------------
// Internal: point data port 0 at pixel (gp_x, gp_y) with increment
// index `incr`. y*320 = (y<<8) + (y<<6), so no multiply is needed;
// the result is 17-bit. Stepping by X16_INC_320 (14) then walks
// straight down a column.
// ---------------------------------------------------------------------
void x16__gfx_setptr(__mem unsigned char incr) {
    asm {
        lda incr
        asl
        asl
        asl
        asl
        sta x16__gp_t2                  // increment field, pre-shifted
                                        // (parked here until ADDR_H)
        lda x16__gp_y                   // y << 6
        stz x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        sta x16__gp_t0                  // t1:t0 = y*64

        clc                             // + y<<8, whose low byte is zero
        lda x16__gp_y
        adc x16__gp_t1
        sta x16__gp_t1
        lda #0
        adc #0
        pha                             // bit 16 of y*320

        clc                             // + x
        lda x16__gp_t0
        adc x16__gp_x
        sta x16__gp_t0
        lda x16__gp_t1
        adc x16__gp_x+1
        sta x16__gp_t1
        pla
        adc #0

        ldx x16__gp_t2                  // recover the increment field
        and #$01 /*VERA_ADDR_H_BANK*/
        sta x16__gp_t2
        txa
        ora x16__gp_t2
        tax

        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/
        lda x16__gp_t0
        sta $9f20 /*VERA_ADDR_L*/
        lda x16__gp_t1
        sta $9f21 /*VERA_ADDR_M*/
        stx $9f22 /*VERA_ADDR_H*/
    }
}

// Internal: set pixel (gp_x, gp_y) to gp_c, clipped to 320x240.
void x16__gfx_pset_i(void) {
    asm {
        lda x16__gp_y
        cmp #240 /*GFX_HEIGHT*/
        bcs gpp_off                     // y >= 240

        lda x16__gp_x+1                 // x high byte
        beq gpp_on                      // x < 256, always on screen
        cmp #1
        bne gpp_off                     // x >= 512
        lda x16__gp_x
        cmp #$40                        // 320 = $140: low byte must be < $40
        bcs gpp_off
    gpp_on:
        stz x16__gp_off
        bra gpp_done
    gpp_off:
        lda #1
        sta x16__gp_off
    gpp_done:
    }
    if (x16__gp_off) {
        return;
    }
    x16__gfx_setptr(0);                 // X16_INC_0
    asm {
        lda x16__gp_c
        sta $9f23 /*VERA_DATA0*/
    }
}

// Internal: hline from the operand block, `len` pixels.
void x16__gfx_hline_i(unsigned int len) {
    x16__gfx_setptr(1);                 // X16_INC_1
    x16_vera_fill(x16__gp_c, len);
}

unsigned char x16_gfx_init(void) {
    return x16_screen_set_mode(0x80);   // X16_MODE_320x240
}

// ---------------------------------------------------------------------
// 320*240 = 76800 = $12C00 bytes does not fit vera_fill's 16-bit count:
// pass it naively and it truncates to $2C00, the top 35 rows. Hence two
// halves; port 0 keeps auto-incrementing between the calls. GFX_CLEAR
// regression-tests the LAST pixel rather than the first.
// ---------------------------------------------------------------------
void x16_gfx_clear(unsigned char color) {
    x16_vera_addr0(X16_INC_1, 0x00000); // X16_VRAM_BITMAP
    x16_vera_fill(color, 38400);
    x16_vera_fill(color, 38400);
}

void x16_gfx_pset(unsigned int x, unsigned char y, unsigned char color) {
    x16__gp_x = x;
    x16__gp_y = y;
    x16__gp_c = color;
    x16__gfx_pset_i();
}

void x16_gfx_hline(unsigned int x, unsigned char y, unsigned int len,
                   unsigned char color) {
    x16__gp_x = x;
    x16__gp_y = y;
    x16__gp_c = color;
    x16__gfx_hline_i(len);
}

// len is 1-255: a column of a 240-row screen never needs more. The
// hardware's odd X16_INC_320 makes a vertical line the same tight fill
// loop as a horizontal one.
void x16_gfx_vline(unsigned int x, unsigned char y, unsigned char len,
                   unsigned char color) {
    x16__gp_x = x;
    x16__gp_y = y;
    x16__gp_c = color;
    x16__gfx_setptr(14);                // X16_INC_320
    x16_vera_fill(color, len);
}

// Filled rectangle: a row of hlines.
void x16_gfx_rect(unsigned int x, unsigned char y, unsigned int w,
                  unsigned char h, unsigned char color) {
    unsigned char i;

    x16__gp_x = x;
    x16__gp_c = color;
    for (i = 0; i < h; i++) {
        x16__gp_y = y + i;
        x16__gfx_hline_i(w);
    }
}

// Rectangle outline.
void x16_gfx_frame(unsigned int x, unsigned char y, unsigned int w,
                   unsigned char h, unsigned char color) {
    x16_gfx_hline(x, y, w, color);
    x16_gfx_hline(x, y + h - 1, w, color);
    x16_gfx_vline(x, y, h, color);
    x16_gfx_vline(x + w - 1, y, h, color);
}

// ---------------------------------------------------------------------
// Bresenham, any direction -- the same signed-16-bit error walk as the
// ca65 build (dx = |x1-x0|, dy = -|y1-y0|, err = dx+dy), with each
// pixel going through the clipped pset.
// ---------------------------------------------------------------------
void x16_gfx_line(unsigned int x0, unsigned char y0, unsigned int x1,
                  unsigned char y1, unsigned char color) {
    int lx0;
    int ly0;
    int lx1;
    int ly1;
    int dx;
    int dy;
    int sx;
    int sy;
    int err;
    int e2;

    lx0 = (int)x0;
    ly0 = (int)y0;
    lx1 = (int)x1;
    ly1 = (int)y1;

    // Written as subtractions rather than `d = 0 - d`: KickC constant-
    // folds a call with literal endpoints, and folding `0 - d` drops the
    // negation. gfx_line(3, 5, 11, 13, c) folded to .const dy = ly1-y0
    // -- +8, not -8 -- so err started at dx+|dy|, `e2 <= dx` never held,
    // y never advanced, and the walk ran past its end point forever.
    // Same values, folded correctly. Covered by GFX_LINE_DOWN.
    dx = lx1 - lx0;
    if (dx < 0) {
        dx = lx0 - lx1;                 // dx = |dx|
        sx = -1;
    } else {
        sx = 1;
    }
    dy = ly1 - ly0;
    if (dy < 0) {
        sy = -1;
    } else {
        dy = ly0 - ly1;                 // dy = -|dy|
        sy = 1;
    }
    err = dx + dy;

    x16__gp_c = color;
    for (;;) {
        x16__gp_x = (unsigned int)lx0;
        x16__gp_y = (unsigned char)ly0;
        x16__gfx_pset_i();

        if (lx0 == lx1 && ly0 == ly1) {
            break;
        }
        e2 = err << 1;
        if (e2 >= dy) {
            err = err + dy;
            lx0 = lx0 + sx;
        }
        if (e2 <= dx) {
            err = err + dx;
            ly0 = ly0 + sy;
        }
    }
}

/* --- circle plumbing (the ca65 build's c_* helpers, in C) ---------- */

__mem volatile unsigned int x16__gc_cx;
__mem volatile char x16__gc_cy;

// plot (cx +/- ox, cy +/- oy): the four reflections of one octant
// point. A negative x wraps to a huge unsigned value, which pset
// rejects -- the same off-screen behaviour the ca65 build leaned on.
void x16__gfx_plot4(unsigned char ox, unsigned char oy) {
    unsigned int row;
    unsigned int xr;
    unsigned int xw;

    xr = x16__gc_cx + ox;
    xw = x16__gc_cx - ox;               // may wrap huge: pset rejects it
    row = (unsigned int)x16__gc_cy + oy;
    if (row < 240) {
        x16_gfx_pset(xr, (unsigned char)row, x16__gp_c);
        x16_gfx_pset(xw, (unsigned char)row, x16__gp_c);
    }
    if (x16__gc_cy >= oy) {
        row = (unsigned int)x16__gc_cy - oy;
        x16_gfx_pset(xr, (unsigned char)row, x16__gp_c);
        x16_gfx_pset(xw, (unsigned char)row, x16__gp_c);
    }
}

// two clamped horizontal spans: rows cy +/- oy, half-width ox
void x16__gfx_span2(unsigned char ox, unsigned char oy) {
    unsigned int row;
    int left;
    int right;
    unsigned int len;

    left = (int)x16__gc_cx;             // clamp to the screen
    left = left - (int)ox;
    if (left < 0) {
        left = 0;
    }
    right = (int)x16__gc_cx;
    right = right + (int)ox;
    if (right > 319) {
        right = 319;
    }
    if (right < left) {
        return;                         // entirely off screen
    }
    len = (unsigned int)(right - left) + 1;

    row = (unsigned int)x16__gc_cy + oy;
    if (row < 240) {
        x16_gfx_hline((unsigned int)left, (unsigned char)row, len, x16__gp_c);
    }
    if (oy != 0 && x16__gc_cy >= oy) {  // same row twice: skip the mirror
        row = (unsigned int)x16__gc_cy - oy;
        x16_gfx_hline((unsigned int)left, (unsigned char)row, len, x16__gp_c);
    }
}

// ---------------------------------------------------------------------
// Midpoint circle outline, radius 0-120. Plots through the clipped
// pset, so it clips at every screen edge for free. The error walk is
// the ca65 build's: err starts 1-r; a non-negative error steps x inward
// with err += 2*(y-x)+1, a negative one only err += 2*y+1.
// ---------------------------------------------------------------------
void x16_gfx_circle(unsigned int cx, unsigned char cy, unsigned char r,
                    unsigned char color) {
    unsigned char ox;
    unsigned char oy;
    int err;
    int t;

    x16__gc_cx = cx;
    x16__gc_cy = cy;
    x16__gp_c = color;

    if (r == 0) {
        x16_gfx_pset(cx, cy, color);    // radius 0: a single point
        return;
    }
    ox = r;
    oy = 0;
    err = 1 - (int)r;

    while (oy < ox) {
        x16__gfx_plot4(ox, oy);         // the 8 octant points
        x16__gfx_plot4(oy, ox);

        oy++;
        /* explicit int steps: KickC lacks fragments for mixed-width
        ** expressions like ((int)oy - ox) << 1
        */
        if (err >= 0) {
            ox--;
            t = (int)oy;
            t = t - (int)ox;
            t = t << 1;
            err = err + t + 1;
        } else {
            t = (int)oy;
            t = t << 1;
            err = err + t + 1;
        }
    }
    if (oy == ox) {
        x16__gfx_plot4(ox, oy);         // the final x == y diagonals
    }
}

// Filled circle: clamped horizontal spans instead of points.
void x16_gfx_disc(unsigned int cx, unsigned char cy, unsigned char r,
                  unsigned char color) {
    unsigned char ox;
    unsigned char oy;
    int err;
    int t;

    x16__gc_cx = cx;
    x16__gc_cy = cy;
    x16__gp_c = color;

    ox = r;
    oy = 0;
    err = 1 - (int)r;

    while (oy < ox) {
        x16__gfx_span2(ox, oy);         // rows cy+/-oy, half-width ox
        x16__gfx_span2(oy, ox);         // rows cy+/-ox, half-width oy

        oy++;
        /* explicit int steps: KickC lacks fragments for mixed-width
        ** expressions like ((int)oy - ox) << 1
        */
        if (err >= 0) {
            ox--;
            t = (int)oy;
            t = t - (int)ox;
            t = t << 1;
            err = err + t + 1;
        } else {
            t = (int)oy;
            t = t << 1;
            err = err + t + 1;
        }
    }
    if (oy == ox) {
        x16__gfx_span2(ox, oy);
    }
}

/* --- text ----------------------------------------------------------- */

__mem volatile char x16__gt_glyph[8];

// Internal: fetch screen code `code`'s 8-byte 1bpp glyph from the
// charset the KERNAL keeps at VRAM $1F000, through port 1 so port 0's
// drawing address survives.
void x16__gfx_glyph(__mem unsigned char code) {
    asm {
        lda code                        // glyph address = $1F000 + code*8
        stz x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1                  // t1:A = code * 8

        tax
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        tsb $9f25 /*VERA_CTRL*/         // port 1
        stx $9f20 /*VERA_ADDR_L*/
        lda x16__gp_t1
        clc
        adc #$f0                        // >($1F000 & $FFFF)
        sta $9f21 /*VERA_ADDR_M*/
        lda #$11                        // BANK | (INC_1 << 4): bank 1
        sta $9f22 /*VERA_ADDR_H*/
        ldx #0
    gg_fetch:
        lda $9f24 /*VERA_DATA1*/
        sta x16__gt_glyph,x
        inx
        cpx #8
        bne gg_fetch
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/         // ADDRSEL back to 0
    }
}

// ---------------------------------------------------------------------
// Draw one glyph into the bitmap. `code` is a SCREEN code, not PETSCII.
// Set bits become colour pixels through the clipped pset (so text
// clips); clear bits stay transparent.
// ---------------------------------------------------------------------
void x16_gfx_char(unsigned int x, unsigned char y, unsigned char color,
                  unsigned char code) {
    unsigned int row;                   /* int, or KickC strength-reduces
                                        ** the ++ into `ry` and breaks */
    unsigned char col;
    unsigned char bits;
    unsigned int ry;

    x16__gfx_glyph(code);
    x16__gp_c = color;

    for (row = 0; row < 8; row++) {
        bits = x16__gt_glyph[row];
        ry = (unsigned int)y;
        ry = ry + row;
        // blank rows plot nothing; rows past 255 are off screen
        if (bits != 0 && ry < 256) {
            x16__gp_y = (unsigned char)ry;
            for (col = 0; col < 8; col++) {
                if (bits & 0x80) {      // leftmost pixel first
                    x16__gp_x = x;
                    x16__gp_x = x16__gp_x + col;
                    x16__gfx_pset_i();
                }
                bits = bits << 1;
            }
        }
    }
}

// ---------------------------------------------------------------------
// A NUL-terminated string, 8 pixels per character. ASCII letters are
// converted to screen codes ('A'-'Z' work as expected).
// ---------------------------------------------------------------------
void x16_gfx_text(unsigned int x, unsigned char y, unsigned char color,
                  const char *s) {
    unsigned char c;

    while (*s) {
        c = (unsigned char)*s;
        // ASCII -> screen code: bit 6 set means the letters/@ block
        if (c & 0x40) {
            c = c & 0x1F;
        }
        x16_gfx_char(x, y, color, c);
        x = x + 8;                      // advance the pen
        s++;
    }
}

/* --- flood fill ------------------------------------------------------ */

#define FF_DEPTH 170

__mem volatile unsigned int x16__ff_x;
__mem volatile char x16__ff_y;
__mem volatile unsigned int x16__ff_xl;
__mem volatile unsigned int x16__ff_xr;
__mem volatile unsigned int x16__ff_ax;
__mem volatile char x16__ff_ay;
__mem volatile char x16__ff_tgt;
__mem volatile char x16__ff_cnt0;
__mem volatile char x16__ff_cnt1;
__mem volatile char x16__ff_seg;
__mem volatile char x16__ff_sp;
__mem volatile char x16__ff_ovf;
__mem volatile unsigned int x16__ff_px; // scanrow's walking column
__mem volatile char x16__ff_ny;         // the row being scanned

// The span stack, three parallel byte arrays: no slot arithmetic.
__mem volatile char x16__ff_sxl[FF_DEPTH];
__mem volatile char x16__ff_sxh[FF_DEPTH];
__mem volatile char x16__ff_sy[FF_DEPTH];

// Internal: point port 1 at (ff_ax, ff_ay), INC_1, walking down when
// decr is nonzero. The same y*320+x sum as setptr, on the other port.
void x16__ff_addr1(__mem unsigned char decr) {
    asm {
        lda decr
        beq ffa_up
        lda #$18                        // DECR | (INC_1 << 4)
        bra ffa_have
    ffa_up:
        lda #$10                        // INC_1 << 4
    ffa_have:
        sta x16__gp_t2

        lda x16__ff_ay                  // ay*320 = ay*64 + ay*256
        stz x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        asl
        rol x16__gp_t1
        sta x16__gp_t0
        clc
        lda x16__ff_ay
        adc x16__gp_t1
        sta x16__gp_t1
        lda #0
        adc #0
        pha
        clc                             // + ax
        lda x16__gp_t0
        adc x16__ff_ax
        sta x16__gp_t0
        lda x16__gp_t1
        adc x16__ff_ax+1
        sta x16__gp_t1
        pla
        adc #0
        and #$01 /*VERA_ADDR_H_BANK*/
        ora x16__gp_t2
        tax
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        tsb $9f25 /*VERA_CTRL*/         // port 1
        lda x16__gp_t0
        sta $9f20 /*VERA_ADDR_L*/
        lda x16__gp_t1
        sta $9f21 /*VERA_ADDR_M*/
        stx $9f22 /*VERA_ADDR_H*/
    }
}

// Internal: the pixel at (ff_x, ff_y), via port 1.
unsigned char x16__ff_rd(void) {
    __mem char r;
    x16__ff_ax = x16__ff_x;
    x16__ff_ay = x16__ff_y;
    x16__ff_addr1(0);
    asm {
        lda $9f24 /*VERA_DATA1*/
        sta r
    }
    return r;
}

// Internal: grow the span left from ff_x to the leftmost target pixel,
// reading backwards through port 1's DECR walk. The hot loop is asm.
void x16__ff_growl(void) {
    x16__ff_xl = x16__ff_x;
    if (x16__ff_xl == 0) {
        return;
    }
    x16__ff_ax = x16__ff_xl - 1;        // walk from xl-1 downwards
    x16__ff_ay = x16__ff_y;
    x16__ff_addr1(1);
    asm {
    ffl_scan:
        lda $9f24 /*VERA_DATA1*/
        cmp x16__ff_tgt
        bne ffl_done
        lda x16__ff_xl
        bne ffl_dec
        dec x16__ff_xl+1
    ffl_dec:
        dec x16__ff_xl
        lda x16__ff_xl
        ora x16__ff_xl+1
        bne ffl_scan
    ffl_done:
    }
}

// Internal: grow the span right from ff_x to the rightmost target
// pixel.
void x16__ff_growr(void) {
    x16__ff_xr = x16__ff_x;
    if (x16__ff_xr >= 319) {
        return;
    }
    x16__ff_ax = x16__ff_xr + 1;        // walk from xr+1 upwards
    x16__ff_ay = x16__ff_y;
    x16__ff_addr1(0);
    asm {
    ffr_scan:
        lda $9f24 /*VERA_DATA1*/
        cmp x16__ff_tgt
        bne ffr_done
        inc x16__ff_xr
        bne ffr_chk
        inc x16__ff_xr+1
    ffr_chk:
        lda x16__ff_xr+1                // at column 319 yet?
        cmp #$01
        bne ffr_scan
        lda x16__ff_xr
        cmp #$3f                        // <319
        bcc ffr_scan
    ffr_done:
    }
}

// Internal: scan row ff_ny across columns ff_xl..ff_xr, pushing the
// start of every run of target-coloured pixels. The push is inlined:
// three parallel-array stores indexed by the stack pointer.
void x16__ff_scanrow(void) {
    x16__ff_ax = x16__ff_xl;
    x16__ff_px = x16__ff_xl;
    x16__ff_ay = x16__ff_ny;
    x16__ff_addr1(0);
    asm {
        sec                             // count = xr - xl + 1
        lda x16__ff_xr
        sbc x16__ff_xl
        sta x16__ff_cnt0
        lda x16__ff_xr+1
        sbc x16__ff_xl+1
        sta x16__ff_cnt1
        inc x16__ff_cnt0
        bne ffs_counted
        inc x16__ff_cnt1
    ffs_counted:
        stz x16__ff_seg
    ffs_cell:
        lda $9f24 /*VERA_DATA1*/
        cmp x16__ff_tgt
        bne ffs_break
        lda x16__ff_seg
        bne ffs_step                    // already inside a run
        ldx x16__ff_sp                  // a run begins here: push its start
        cpx #170 /*FF_DEPTH*/
        bcc ffs_room
        lda #1
        sta x16__ff_ovf
        bra ffs_pushed
    ffs_room:
        lda x16__ff_px
        sta x16__ff_sxl,x
        lda x16__ff_px+1
        sta x16__ff_sxh,x
        lda x16__ff_ny
        sta x16__ff_sy,x
        inc x16__ff_sp
    ffs_pushed:
        lda #1
        sta x16__ff_seg
        bra ffs_step
    ffs_break:
        stz x16__ff_seg
    ffs_step:
        inc x16__ff_px
        bne ffs_count
        inc x16__ff_px+1
    ffs_count:
        lda x16__ff_cnt0
        bne ffs_declo
        dec x16__ff_cnt1
    ffs_declo:
        dec x16__ff_cnt0
        lda x16__ff_cnt0
        ora x16__ff_cnt1
        bne ffs_cell
    }
}

// ---------------------------------------------------------------------
// Scanline flood fill of the 4-connected region of the seed's colour.
// 1 if the region was filled completely; 0 if the span stack (170
// pending spans) overflowed on a pathological shape and the fill is
// incomplete. Filling with the colour already under the seed is a
// no-op. Both VERA ports get repointed freely.
// ---------------------------------------------------------------------
unsigned char x16_gfx_flood(unsigned int x, unsigned char y,
                            unsigned char color) {
    unsigned char idx;
    unsigned int len;

    if (y >= 240 || x >= 320) {
        return 1;                       // a seed off screen fills nothing
    }
    x16__ff_x = x;
    x16__ff_y = y;
    x16__ff_tgt = x16__ff_rd();         // the colour being replaced
    if (x16__ff_tgt == color) {
        return 1;                       // already the fill colour: no-op
    }
    x16__gp_c = color;

    x16__ff_sp = 0;
    x16__ff_ovf = 0;
    x16__ff_sxl[0] = (unsigned char)x;
    x16__ff_sxh[0] = (unsigned char)(x >> 8);
    x16__ff_sy[0] = y;
    x16__ff_sp = 1;

    while (x16__ff_sp != 0) {
        x16__ff_sp--;
        idx = x16__ff_sp;
        x16__ff_x = ((unsigned int)x16__ff_sxh[idx] << 8) | x16__ff_sxl[idx];
        x16__ff_y = x16__ff_sy[idx];

        if (x16__ff_rd() != x16__ff_tgt) {
            continue;                   // painted over since it was queued
        }
        x16__ff_growl();
        x16__ff_growr();

        // paint the span
        len = x16__ff_xr - x16__ff_xl + 1;
        x16__gp_x = x16__ff_xl;
        x16__gp_y = x16__ff_y;
        x16__gfx_hline_i(len);

        // queue fresh spans in the rows above and below
        if (x16__ff_y != 0) {
            x16__ff_ny = x16__ff_y - 1;
            x16__ff_scanrow();
        }
        if (x16__ff_y < 239) {
            x16__ff_ny = x16__ff_y + 1;
            x16__ff_scanrow();
        }
    }
    asm {
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/         // leave ADDRSEL 0 behind
    }
    return x16__ff_ovf ? 0 : 1;
}
