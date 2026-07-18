// =====================================================================
// x16clib :: x16/shapes.c -- circle / disc / flood for BOTH bitmap modes
// =====================================================================
// One engine-agnostic shape algorithm, bound at call time to the 8bpp
// module (x16_gfx_*) or the 2bpp module (x16_gfx2_*) by a runtime engine
// flag. The circle outline plots through the clipping pset; the disc fill
// clamps its spans to the canvas; the flood is bounds-checked so it never
// reads or writes off screen.
//
//   x16_gfx_circle / _disc  (unsigned int cx, unsigned char cy,
//                            unsigned char r, unsigned char color)
//   x16_gfx_flood           (unsigned int x, unsigned char y,
//                            unsigned char color) -> 1 if filled completely
//   x16_gfx2_circle / _disc (unsigned int cx, unsigned int cy,
//                            unsigned char r, unsigned char color)
//   x16_gfx2_flood          (unsigned int x, unsigned int y,
//                            unsigned char color) -> 1 if filled completely
// =====================================================================

#include <x16/bitmap.h>
#include <x16/bitmap2.h>
#include <x16/shapes.h>

#define SHP_FMAX 96                     // flood seed stack depth

__mem volatile unsigned char x16__shp2;         // 0 = 8bpp, 1 = 2bpp
__mem volatile unsigned int  x16__shp_w;        // canvas width  (320 / 640)
__mem volatile unsigned int  x16__shp_h;        // canvas height (240 / 480)
__mem volatile unsigned int  x16__sc_cx;        // circle/disc centre
__mem volatile unsigned int  x16__sc_cy;
__mem volatile unsigned char x16__sc_col;

// --- engine-flag dispatchers -----------------------------------------
void x16__shp_pset(unsigned int x, unsigned int y, unsigned char c) {
    if (x16__shp2) {
        x16_gfx2_pset(x, y, c);
    } else {
        x16_gfx_pset(x, (unsigned char)y, c);
    }
}

void x16__shp_hline(unsigned int x, unsigned int y, unsigned int len,
                    unsigned char c) {
    if (x16__shp2) {
        x16_gfx2_hline(x, y, len, c);
    } else {
        x16_gfx_hline(x, (unsigned char)y, len, c);
    }
}

unsigned char x16__shp_read(unsigned int x, unsigned int y) {
    if (x16__shp2) {
        return x16_gfx2_read(x, y);
    }
    return x16__gfx_read8(x, (unsigned char)y);
}

// --- circle / disc ---------------------------------------------------
// plot the 4 reflections of one octant point (cx +/- ox, cy +/- oy)
void x16__shp_plot4(unsigned int ox, unsigned int oy) {
    unsigned int row;
    unsigned int xr;
    unsigned int xw;

    xr = x16__sc_cx + ox;
    xw = x16__sc_cx - ox;               // may wrap huge: pset rejects it
    row = x16__sc_cy + oy;
    if (row < x16__shp_h) {
        x16__shp_pset(xr, row, x16__sc_col);
        x16__shp_pset(xw, row, x16__sc_col);
    }
    if (x16__sc_cy >= oy) {
        row = x16__sc_cy - oy;
        x16__shp_pset(xr, row, x16__sc_col);
        x16__shp_pset(xw, row, x16__sc_col);
    }
}

// two clamped horizontal spans: rows cy +/- oy, half-width ox
void x16__shp_span2(unsigned int ox, unsigned int oy) {
    unsigned int row;
    unsigned int left;
    unsigned int right;
    unsigned int len;

    if (x16__sc_cx >= ox) {
        left = x16__sc_cx - ox;
    } else {
        left = 0;                       // clamp to the left edge
    }
    right = x16__sc_cx + ox;
    if (right >= x16__shp_w) {
        right = x16__shp_w - 1;         // clamp to the right edge
    }
    if (right < left) {
        return;                         // entirely off screen
    }
    len = right - left + 1;

    row = x16__sc_cy + oy;
    if (row < x16__shp_h) {
        x16__shp_hline(left, row, len, x16__sc_col);
    }
    if (oy != 0 && x16__sc_cy >= oy) {  // same row twice: skip the mirror
        row = x16__sc_cy - oy;
        x16__shp_hline(left, row, len, x16__sc_col);
    }
}

// Midpoint walk shared by circle (points) and disc (spans). `fill` picks
// which. err starts 1-r; a non-negative error steps x inward.
void x16__shp_walk(unsigned int r, unsigned char fill) {
    unsigned int ox;
    unsigned int oy;
    int err;
    int t;

    if (r == 0) {
        if (fill == 0) {
            x16__shp_pset(x16__sc_cx, x16__sc_cy, x16__sc_col);
        }
        return;
    }
    ox = r;
    oy = 0;
    err = 1 - (int)r;

    while (oy < ox) {
        if (fill != 0) {
            x16__shp_span2(ox, oy);
            x16__shp_span2(oy, ox);
        } else {
            x16__shp_plot4(ox, oy);
            x16__shp_plot4(oy, ox);
        }
        oy++;
        // explicit int steps: KickC lacks fragments for mixed-width
        // expressions like ((int)oy - ox) << 1
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
        if (fill != 0) {
            x16__shp_span2(ox, oy);
        } else {
            x16__shp_plot4(ox, oy);
        }
    }
}

// --- ellipse / fellipse ------------------------------------------------
// The error-form midpoint ellipse (Zingl): quadrant II from (-rx, 0) up
// to (0, ry), mirrored 4 ways by the circle's own plot4 / span2. The
// decision terms reach 2*rx*ry^2 (about 33M at 255/255), so they are
// 32-bit -- KickC has no fragments for long arithmetic, so, like
// fixed.c, the multi-byte work lives in asm over __mem cells and C
// keeps only the control flow. A centre column finishes the flat tips.
__mem volatile unsigned long x16__e_dx;         // the 32-bit decision terms
__mem volatile unsigned long x16__e_dy;
__mem volatile unsigned long x16__e_er;
__mem volatile unsigned long x16__e_e2;
__mem volatile unsigned long x16__e_a2;         // 2*rx^2
__mem volatile unsigned long x16__e_b2;         // 2*ry^2
__mem volatile unsigned int  x16__e_sq;         // a square, handed to asm
__mem volatile unsigned char x16__e_cnt;        // rx, for the setup subtract
__mem volatile unsigned char x16__e_fl;         // step flags: 1 = x, 2 = y

// in: e_sq = ry^2, e_cnt = rx.  builds e_b2 = 2*ry^2 and
// e_dx = ry^2 - rx*2*ry^2 (one 2*ry^2 at a time, at worst ~255 rounds)
void x16__e_setup(void) {
    asm {
        lda x16__e_sq
        sta x16__e_dx
        sta x16__e_b2
        lda x16__e_sq+1
        sta x16__e_dx+1
        sta x16__e_b2+1
        lda #0
        sta x16__e_dx+2
        sta x16__e_dx+3
        sta x16__e_b2+2
        sta x16__e_b2+3
        asl x16__e_b2
        rol x16__e_b2+1
        rol x16__e_b2+2
        ldx x16__e_cnt
        beq es_done
    es_sub:
        sec
        lda x16__e_dx
        sbc x16__e_b2
        sta x16__e_dx
        lda x16__e_dx+1
        sbc x16__e_b2+1
        sta x16__e_dx+1
        lda x16__e_dx+2
        sbc x16__e_b2+2
        sta x16__e_dx+2
        lda x16__e_dx+3
        sbc x16__e_b2+3
        sta x16__e_dx+3
        dex
        bne es_sub
    es_done:
    }
}

// in: e_sq = rx^2.  e_dy = rx^2, e_a2 = 2*rx^2, err = dx + dy
void x16__e_setup2(void) {
    asm {
        lda x16__e_sq
        sta x16__e_dy
        sta x16__e_a2
        lda x16__e_sq+1
        sta x16__e_dy+1
        sta x16__e_a2+1
        lda #0
        sta x16__e_dy+2
        sta x16__e_dy+3
        sta x16__e_a2+2
        sta x16__e_a2+3
        asl x16__e_a2
        rol x16__e_a2+1
        rol x16__e_a2+2
        clc
        lda x16__e_dx
        adc x16__e_dy
        sta x16__e_er
        lda x16__e_dx+1
        adc x16__e_dy+1
        sta x16__e_er+1
        lda x16__e_dx+2
        adc x16__e_dy+2
        sta x16__e_er+2
        lda x16__e_dx+3
        adc x16__e_dy+3
        sta x16__e_er+3
    }
}

// one decision step: e2 = 2*err, then apply Zingl's two updates.
// e_fl bit 0 = x stepped (err += dx += 2ry^2),
//      bit 1 = y stepped (err += dy += 2rx^2).
void x16__e_step(void) {
    asm {
        lda #0
        sta x16__e_fl
        lda x16__e_er                   // e2 = 2*err
        sta x16__e_e2
        lda x16__e_er+1
        sta x16__e_e2+1
        lda x16__e_er+2
        sta x16__e_e2+2
        lda x16__e_er+3
        sta x16__e_e2+3
        asl x16__e_e2
        rol x16__e_e2+1
        rol x16__e_e2+2
        rol x16__e_e2+3
        sec                             // e2 >= dx?  sign of e2 - dx
        lda x16__e_e2
        sbc x16__e_dx
        lda x16__e_e2+1
        sbc x16__e_dx+1
        lda x16__e_e2+2
        sbc x16__e_dx+2
        lda x16__e_e2+3
        sbc x16__e_dx+3
        bmi eq_noxstep
        lda #1
        sta x16__e_fl
        clc                             // err += dx += 2ry^2
        lda x16__e_dx
        adc x16__e_b2
        sta x16__e_dx
        lda x16__e_dx+1
        adc x16__e_b2+1
        sta x16__e_dx+1
        lda x16__e_dx+2
        adc x16__e_b2+2
        sta x16__e_dx+2
        lda x16__e_dx+3
        adc x16__e_b2+3
        sta x16__e_dx+3
        clc
        lda x16__e_er
        adc x16__e_dx
        sta x16__e_er
        lda x16__e_er+1
        adc x16__e_dx+1
        sta x16__e_er+1
        lda x16__e_er+2
        adc x16__e_dx+2
        sta x16__e_er+2
        lda x16__e_er+3
        adc x16__e_dx+3
        sta x16__e_er+3
    eq_noxstep:
        sec                             // e2 <= dy?  sign of dy - e2
        lda x16__e_dy
        sbc x16__e_e2
        lda x16__e_dy+1
        sbc x16__e_e2+1
        lda x16__e_dy+2
        sbc x16__e_e2+2
        lda x16__e_dy+3
        sbc x16__e_e2+3
        bmi eq_noystep
        lda #2
        ora x16__e_fl
        sta x16__e_fl
        clc                             // err += dy += 2rx^2
        lda x16__e_dy
        adc x16__e_a2
        sta x16__e_dy
        lda x16__e_dy+1
        adc x16__e_a2+1
        sta x16__e_dy+1
        lda x16__e_dy+2
        adc x16__e_a2+2
        sta x16__e_dy+2
        lda x16__e_dy+3
        adc x16__e_a2+3
        sta x16__e_dy+3
        clc
        lda x16__e_er
        adc x16__e_dy
        sta x16__e_er
        lda x16__e_er+1
        adc x16__e_dy+1
        sta x16__e_er+1
        lda x16__e_er+2
        adc x16__e_dy+2
        sta x16__e_er+2
        lda x16__e_er+3
        adc x16__e_dy+3
        sta x16__e_er+3
    eq_noystep:
    }
}

void x16__shp_ewalk(unsigned char rx, unsigned char ry, unsigned char fill) {
    unsigned char y;
    unsigned char ox;
    unsigned char i;
    unsigned char fl;
    unsigned int sq;

    sq = 0;                             // ry^2 -> the dx/2ry^2 setup
    for (i = 0; i < ry; i++) {
        sq = sq + ry;
    }
    x16__e_sq = sq;
    x16__e_cnt = rx;
    x16__e_setup();
    sq = 0;                             // rx^2 -> dy, 2rx^2, err
    for (i = 0; i < rx; i++) {
        sq = sq + rx;
    }
    x16__e_sq = sq;
    x16__e_setup2();

    ox = rx;                            // |x|: rx down to 0
    y = 0;
    for (;;) {
        if (fill != 0) {
            x16__shp_span2((unsigned int)ox, (unsigned int)y);
        } else {
            x16__shp_plot4((unsigned int)ox, (unsigned int)y);
        }
        x16__e_step();
        fl = x16__e_fl;
        if ((fl & 2) != 0) {            // y step
            y = y + 1;
        }
        if ((fl & 1) != 0) {            // x step; past 0 ends the walk
            if (ox == 0) {
                break;
            }
            ox = ox - 1;
        }
    }
    while (y < ry) {                    // flat tip: the centre column on
        y = y + 1;                      // to ry
        if (fill != 0) {
            x16__shp_span2(0, (unsigned int)y);
        } else {
            x16__shp_plot4(0, (unsigned int)y);
        }
    }
}

// --- flood fill ------------------------------------------------------
__mem unsigned char x16__fq_xl[SHP_FMAX];
__mem unsigned char x16__fq_xh[SHP_FMAX];
__mem unsigned char x16__fq_yl[SHP_FMAX];
__mem unsigned char x16__fq_yh[SHP_FMAX];
__mem unsigned char x16__f_sp;
__mem unsigned char x16__f_ovf;
__mem unsigned char x16__f_tgt;

void x16__f_push(unsigned int x, unsigned int y) {
    unsigned char i;
    if (x16__f_sp >= SHP_FMAX) {
        x16__f_ovf = 1;                 // full: drop it, report incomplete
        return;
    }
    i = x16__f_sp;
    x16__fq_xl[i] = (unsigned char)x;
    x16__fq_xh[i] = (unsigned char)(x >> 8);
    x16__fq_yl[i] = (unsigned char)y;
    x16__fq_yh[i] = (unsigned char)(y >> 8);
    x16__f_sp = i + 1;
}

// scan xl..xr on row `ry`, pushing the start of every run of the target
void x16__f_scanrow(unsigned int xl, unsigned int xr, unsigned int ry) {
    unsigned int tx;
    unsigned char run;

    run = 0;
    tx = xl;
    while (tx <= xr) {
        if (x16__shp_read(tx, ry) == x16__f_tgt) {
            if (run == 0) {
                x16__f_push(tx, ry);
                run = 1;
            }
        } else {
            run = 0;
        }
        tx++;
    }
}

unsigned char x16__shp_flood(unsigned int sx, unsigned int sy,
                             unsigned char color) {
    unsigned int qx;
    unsigned int qy;
    unsigned int xl;
    unsigned int xr;
    unsigned int len;
    unsigned char i;

    x16__f_sp = 0;
    x16__f_ovf = 0;
    x16__f_tgt = x16__shp_read(sx, sy);
    if (x16__f_tgt == color) {
        return 1;                       // fill with itself: no-op
    }
    x16__f_push(sx, sy);

    while (x16__f_sp != 0) {
        i = x16__f_sp - 1;
        x16__f_sp = i;
        qx = ((unsigned int)x16__fq_xh[i] << 8) | x16__fq_xl[i];
        qy = ((unsigned int)x16__fq_yh[i] << 8) | x16__fq_yl[i];

        if (x16__shp_read(qx, qy) != x16__f_tgt) {
            continue;                   // painted over since it was queued
        }

        xl = qx;                        // widen left
        while (xl != 0 && x16__shp_read(xl - 1, qy) == x16__f_tgt) {
            xl--;
        }
        xr = qx;                        // widen right
        while (xr + 1 < x16__shp_w && x16__shp_read(xr + 1, qy) == x16__f_tgt) {
            xr++;
        }

        len = xr - xl + 1;              // fill the span
        x16__shp_hline(xl, qy, len, color);

        if (qy != 0) {                  // scan the row above and below
            x16__f_scanrow(xl, xr, qy - 1);
        }
        if (qy + 1 < x16__shp_h) {
            x16__f_scanrow(xl, xr, qy + 1);
        }
    }
    if (x16__f_ovf != 0) {
        return 0;
    }
    return 1;
}

// --- public entry points ---------------------------------------------
void x16_gfx_circle(unsigned int cx, unsigned char cy, unsigned char r,
                    unsigned char color) {
    x16__shp2 = 0;
    x16__shp_w = 320;
    x16__shp_h = 240;
    x16__sc_cx = cx;
    x16__sc_cy = (unsigned int)cy;
    x16__sc_col = color;
    x16__shp_walk((unsigned int)r, 0);
}

void x16_gfx_disc(unsigned int cx, unsigned char cy, unsigned char r,
                  unsigned char color) {
    x16__shp2 = 0;
    x16__shp_w = 320;
    x16__shp_h = 240;
    x16__sc_cx = cx;
    x16__sc_cy = (unsigned int)cy;
    x16__sc_col = color;
    x16__shp_walk((unsigned int)r, 1);
}

void x16_gfx_ellipse(unsigned int cx, unsigned char cy, unsigned char rx,
                     unsigned char ry, unsigned char color) {
    x16__shp2 = 0;
    x16__shp_w = 320;
    x16__shp_h = 240;
    x16__sc_cx = cx;
    x16__sc_cy = (unsigned int)cy;
    x16__sc_col = color;
    x16__shp_ewalk(rx, ry, 0);
}

void x16_gfx_fellipse(unsigned int cx, unsigned char cy, unsigned char rx,
                      unsigned char ry, unsigned char color) {
    x16__shp2 = 0;
    x16__shp_w = 320;
    x16__shp_h = 240;
    x16__sc_cx = cx;
    x16__sc_cy = (unsigned int)cy;
    x16__sc_col = color;
    x16__shp_ewalk(rx, ry, 1);
}

unsigned char x16_gfx_flood(unsigned int x, unsigned char y,
                            unsigned char color) {
    x16__shp2 = 0;
    x16__shp_w = 320;
    x16__shp_h = 240;
    return x16__shp_flood(x, (unsigned int)y, color);
}

void x16_gfx2_circle(unsigned int cx, unsigned int cy, unsigned char r,
                     unsigned char color) {
    x16__shp2 = 1;
    x16__shp_w = 640;
    x16__shp_h = 480;
    x16__sc_cx = cx;
    x16__sc_cy = cy;
    x16__sc_col = color;
    x16__shp_walk((unsigned int)r, 0);
}

void x16_gfx2_disc(unsigned int cx, unsigned int cy, unsigned char r,
                   unsigned char color) {
    x16__shp2 = 1;
    x16__shp_w = 640;
    x16__shp_h = 480;
    x16__sc_cx = cx;
    x16__sc_cy = cy;
    x16__sc_col = color;
    x16__shp_walk((unsigned int)r, 1);
}

void x16_gfx2_ellipse(unsigned int cx, unsigned int cy, unsigned char rx,
                      unsigned char ry, unsigned char color) {
    x16__shp2 = 1;
    x16__shp_w = 640;
    x16__shp_h = 480;
    x16__sc_cx = cx;
    x16__sc_cy = cy;
    x16__sc_col = color;
    x16__shp_ewalk(rx, ry, 0);
}

void x16_gfx2_fellipse(unsigned int cx, unsigned int cy, unsigned char rx,
                       unsigned char ry, unsigned char color) {
    x16__shp2 = 1;
    x16__shp_w = 640;
    x16__shp_h = 480;
    x16__sc_cx = cx;
    x16__sc_cy = cy;
    x16__sc_col = color;
    x16__shp_ewalk(rx, ry, 1);
}

unsigned char x16_gfx2_flood(unsigned int x, unsigned int y,
                             unsigned char color) {
    x16__shp2 = 1;
    x16__shp_w = 640;
    x16__shp_h = 480;
    return x16__shp_flood(x, y, color);
}
