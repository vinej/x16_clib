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

volatile unsigned char x16__shp2;         // 0 = 8bpp, 1 = 2bpp
volatile unsigned int  x16__shp_w;        // canvas width  (320 / 640)
volatile unsigned int  x16__shp_h;        // canvas height (240 / 480)
volatile unsigned int  x16__sc_cx;        // circle/disc centre
volatile unsigned int  x16__sc_cy;
volatile unsigned char x16__sc_col;

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

// --- flood fill ------------------------------------------------------
unsigned char x16__fq_xl[SHP_FMAX];
unsigned char x16__fq_xh[SHP_FMAX];
unsigned char x16__fq_yl[SHP_FMAX];
unsigned char x16__fq_yh[SHP_FMAX];
unsigned char x16__f_sp;
unsigned char x16__f_ovf;
unsigned char x16__f_tgt;

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

unsigned char x16_gfx2_flood(unsigned int x, unsigned int y,
                             unsigned char color) {
    x16__shp2 = 1;
    x16__shp_w = 640;
    x16__shp_h = 480;
    return x16__shp_flood(x, y, color);
}
