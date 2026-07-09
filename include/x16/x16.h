/* =====================================================================
 * x16clib :: x16.h -- umbrella header
 * =====================================================================
 * A C library for the Commander X16, wrapping hand-written 6502 routines.
 *
 *     cl65 -t cx16 -O -I include -o PROG.PRG prog.c build\x16c.lib
 *
 * Including this header pulls in the whole API, but costs nothing at run
 * time: ld65 links only the library modules your program actually calls.
 * Include the individual headers under include/x16 instead if you prefer.
 *
 * Conventions
 * -----------
 * Every symbol this library exposes is prefixed `x16_`, so cc65's own
 * <cx16.h>, <conio.h> and <cbm.h> stay usable alongside it. Where cc65
 * already does a job well the library does not duplicate it: use cc65's
 * vpeek()/vpoke() for single VRAM bytes, conio for text output, printf
 * for number formatting, and native int/long arithmetic.
 *
 * All entry points are __fastcall__ (cc65's default).
 *
 * One trap worth naming here. cc65's <cx16.h> defines VERA_INC_1 and
 * friends already shifted into ADDR_H's bits 7:4, so its VERA_INC_1 is
 * 0x10. This library's X16_INC_1 is the raw index, 1. Same bits on the
 * wire, sixteen times apart as constants. Pass X16_INC_* to this header's
 * functions and VERA_INC_* to cc65's; never mix them.
 *
 * The zero page
 * -------------
 * The library keeps a 16-byte scratch block that the linker places inside
 * the $22-$7F user window. It is shared and NOT reentrant: an interrupt
 * handler must not call any x16_* routine that touches it -- in practice,
 * anything taking more than three arguments, or any 16-bit argument. See
 * x16/irq.h for the routines that are safe from an ISR.
 * =====================================================================
 */

#ifndef X16_H
#define X16_H

#ifndef __CX16__
#  error x16clib targets the Commander X16: compile with -t cx16
#endif

#include <x16/vera.h>           /* VRAM data ports: fill, copy, FX probe */
#include <x16/screen.h>         /* screen mode, text output, cursor */
#include <x16/palette.h>        /* 256 entries of 12-bit colour */
#include <x16/tile.h>           /* tilemap cells, layer config, scroll */
#include <x16/sprite.h>         /* 128 hardware sprites */
#include <x16/bitmap.h>         /* 320x240x256 drawing */
#include <x16/verafx.h>         /* hardware multiply, fast fills */
#include <x16/psg.h>            /* 16-voice PSG */
#include <x16/ym.h>             /* YM2151 FM */
#include <x16/pcm.h>            /* PCM FIFO */
#include <x16/input.h>          /* joystick, mouse, keyboard */
#include <x16/irq.h>            /* VSYNC frame counter */
#include <x16/bank.h>           /* banked RAM */
#include <x16/load.h>           /* load and save, including into VRAM */
#include <x16/fixed.h>          /* 8.8 fixed point, 16x16 multiply */
#include <x16/collide.h>        /* bounding-box overlap */
#include <x16/float.h>          /* the ROM's floating point library */

/* Diagnostic: the address the linker gave the scratch block. Nothing
** depends on the value; it moves as cc65's own zero-page footprint does.
*/
unsigned char x16_zp_base (void);

#endif /* X16_H */
