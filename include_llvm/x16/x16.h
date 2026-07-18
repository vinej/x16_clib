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
 * All entry points are (cc65's default).
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

/* ---------------------------------------------------------------------
** llvm-mos build. The API is identical to the cc65 build's; only the
** calling convention differs, and llvm-mos expresses it in the compiler
** rather than in the declaration. So there is no __fastcall__ here.
**
** llvm-mos passes byte arguments in A, then X, then __rc2, __rc3, ...
** and returns the same way. cc65 pushes all but the last argument on a
** software stack. Object code from the two toolchains cannot be mixed.
**
** COMPILE WITH -mreserve-zp=16.
**
**     mos-cx16-clang -Os -mreserve-zp=16 -I include_llvm \
**         -o PROG.PRG prog.c build_llvm/libx16c.a
**
** Without it the link fails outright:
**
**     ld.lld: error: section '.zp.bss' will not fit in region 'zp':
**             overflowed by 16 bytes
**
** The cx16 target leaves only ninety bytes of zero page ($26-$7F; the
** KERNAL's r0-r15 and llvm-mos's imaginary registers share $02-$25).
** Clang's LTO pass helps itself to as many of those ninety as it finds
** useful, and knows nothing about the sixteen this library reserves for
** its argument block. The flag tells it to leave sixteen alone; it spills
** that much of its own data to ordinary memory instead, which costs on
** the order of eighty bytes of code. The library gets those bytes back
** many times over -- it touches the block in over a thousand places, and
** a zero-page access is a byte and a cycle cheaper than an absolute one.
**
** If your own program also asks for zero page (an explicit
** __attribute__((section(".zp.bss")))), raise the number to match.
** --------------------------------------------------------------------- */

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
#include <x16/bitmap2.h>         /* 640x480@2bpp drawing */
#include <x16/shapes.h>         /* circle/disc/flood for both bitmaps */
#include <x16/verafx.h>         /* hardware multiply, fills, lines, triangles */
#include <x16/psg.h>            /* 16-voice PSG */
#include <x16/ym.h>             /* YM2151 FM */
#include <x16/pcm.h>            /* PCM FIFO, and AFLOW streaming */
#include <x16/input.h>          /* joystick, mouse, keyboard */
#include <x16/irq.h>            /* VSYNC, raster and collision interrupts */
#include <x16/bank.h>           /* banked RAM, and a whole-bank allocator */
#include <x16/mem.h>            /* KERNAL block ops, incl. LZSA2 depacking */
#include <x16/load.h>           /* load and save, including into VRAM */
#include <x16/dos.h>            /* the DOS command channel: status, delete */
#include <x16/bmx.h>            /* the X16's native bitmap file format */
#include <x16/zx0.h>            /* ZX0 depacking, tighter than LZSA2 */
#include <x16/adpcm.h>          /* IMA ADPCM: 4:1 compressed audio */
#include <x16/fixed.h>          /* 8.8 fixed point, 16x16 multiply */
#include <x16/math.h>           /* PRNG, sine tables, atan2, lerp */
#include <x16/collide.h>        /* bounding-box overlap */
#include <x16/clip.h>           /* Cohen-Sutherland line clipping */
#include <x16/buffers.h>        /* a ring buffer and a stack */
#include <x16/float.h>          /* the ROM's floating point library */

/* Diagnostic: the address the linker gave the scratch block. Nothing
** depends on the value; it moves as cc65's own zero-page footprint does.
*/
unsigned char x16_zp_base (void);

#endif /* X16_H */
