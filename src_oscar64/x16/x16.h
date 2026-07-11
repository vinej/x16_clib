/* =====================================================================
 * x16clib :: x16.h -- umbrella header
 * =====================================================================
 * A C library for the Commander X16, wrapping hand-written 6502 routines.
 *
 *     oscar64 -tm=x16 -n -i=src_oscar64 -o=YOURPROG.PRG yourprog.c
 *
 * Including this header pulls in the whole ported API, and costs nothing
 * at run time: Oscar64 compiles the whole program at once and strips
 * every function your program does not call. Include the individual
 * headers under x16/ instead if you prefer.
 *
 * Conventions
 * -----------
 * Every symbol this library exposes is prefixed `x16_` (internals use
 * `x16__`, two underscores -- do not call those). Oscar64's own headers
 * (<stdio.h>, <c64/vic.h>-style hardware maps, ...) stay usable
 * alongside it.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** Oscar64 build. The API is identical to the cc65 build's; only the
** delivery differs. There is no x16c.lib here: the distribution is
** SOURCE, headers and implementations side by side, and each header's
** `#pragma compile` stands in for the archive. The hand-written
** assembly is the same as the other toolchains', carried inside
** __asm {} bodies; Oscar64 places each function's parameters in zero
** page and the asm reads them by name, so the cc65 build's marshalling
** shims simply do not exist.
**
** All 27 modules of the cc65 build are here.
** --------------------------------------------------------------------- */

#ifndef X16_H
#define X16_H

#ifndef __X16__
#error x16clib targets the Commander X16: compile with -tm=x16
#endif

#include <x16/vera.h>           /* VRAM data ports: fill, copy, FX probe */
#include <x16/screen.h>         /* screen mode, text output, cursor */
#include <x16/palette.h>        /* 256 entries of 12-bit colour */
#include <x16/tile.h>           /* tilemap cells, layer config, scroll */
#include <x16/sprite.h>         /* 128 hardware sprites */
#include <x16/bitmap.h>         /* 320x240x256 drawing */
#include <x16/verafx.h>         /* hardware multiply, fills, lines, triangles */
#include <x16/irq.h>            /* VSYNC, raster and collision interrupts */
#include <x16/psg.h>            /* 16-voice PSG, and ASR envelopes */
#include <x16/ym.h>             /* YM2151 FM */
#include <x16/input.h>          /* joystick, mouse, keyboard */
#include <x16/pcm.h>            /* PCM FIFO, and AFLOW streaming */
#include <x16/adpcm.h>          /* IMA ADPCM: 4:1 compressed audio */
#include <x16/bank.h>           /* banked RAM, and a whole-bank allocator */
#include <x16/mem.h>            /* KERNAL block ops, incl. LZSA2 depacking */
#include <x16/load.h>           /* load and save, including into VRAM */
#include <x16/dos.h>            /* the DOS command channel */
#include <x16/bmx.h>            /* the X16's native bitmap file format */
#include <x16/zx0.h>            /* ZX0 depacking, tighter than LZSA2 */
#include <x16/fixed.h>          /* 8.8 fixed point, 16x16 multiply */
#include <x16/math.h>           /* PRNG, sine tables, atan2, lerp */
#include <x16/collide.h>        /* bounding-box overlap */
#include <x16/clip.h>           /* Cohen-Sutherland line clipping */
#include <x16/buffers.h>        /* a ring buffer and a stack */
#include <x16/float.h>          /* the ROM's floating point library */

#endif /* X16_H */
