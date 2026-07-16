/* =====================================================================
 * x16clib :: x16.h -- umbrella header (vbcc)
 * =====================================================================
 * A C library for the Commander X16, wrapping hand-written 6502 routines,
 * built with vbcc's native +x16 target:
 *
 *     vc +x16 -I include_vbcc -o PROG.PRG prog.c dist_vbcc\libx16c.a
 *
 * Including this header pulls in the whole API, but costs nothing at run
 * time: vlink links only the library modules your program actually calls.
 * Include the individual headers under include_vbcc/x16 if you prefer.
 *
 * Conventions
 * -----------
 * Every symbol this library exposes is prefixed `x16_`, so vbcc's own
 * <stdio.h> etc. stay usable alongside it.
 *
 * The ABI. vbcc's 6502 backend passes arguments in the zero-page pseudo-
 * registers r0..r7 (and the a/x pair), and returns char in a, int and
 * near-pointer in a/x. Each prototype here pins its arguments to the
 * registers the hand-written routine reads, with __reg(). You never write
 * __reg() yourself -- the header does it -- but it is why the calls are as
 * cheap as a hand asm call.
 *
 * One trap worth naming here. This library's X16_INC_1 and friends are the
 * raw VERA increment indices (1, 2, ...), not pre-shifted into ADDR_H's
 * bits 7:4. Pass X16_INC_* to this header's functions.
 *
 * The zero page
 * -------------
 * The library keeps a 16-byte scratch block that the linker places in the
 * free zero-page window ($02-$7F), clear of vbcc's own r0..r31/sp/btmp
 * registers. It is shared and NOT reentrant: an interrupt handler must not
 * call any x16_* routine that touches it -- in practice, anything taking
 * more than three arguments, or any 16-bit argument. See x16/irq.h for the
 * routines that are safe from an ISR.
 * =====================================================================
 */

#ifndef X16_H
#define X16_H

/* vbcc's +x16 target does not predefine an X16 machine macro (only
** __6502__ and __VBCC__), so key the guard on __VBCC__: this header is the
** vbcc build's, and the __reg() annotations below are vbcc-only syntax. */
#ifndef __VBCC__
#  error x16clib (vbcc build) targets the Commander X16: compile with vc +x16
#endif

#include <x16/vera.h>           /* VRAM data ports: fill, copy, FX probe */
#include <x16/screen.h>         /* screen mode, text output, cursor */
#include <x16/palette.h>        /* 256 entries of 12-bit colour */
#include <x16/tile.h>           /* tilemap cells, layer config, scroll */
#include <x16/sprite.h>         /* 128 hardware sprites */
#include <x16/bitmap.h>         /* 320x240x256 drawing */
#include <x16/bitmap2.h>        /* 640x480x4 drawing (2bpp) */
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
** depends on the value; it moves as vbcc's own zero-page footprint does.
*/
unsigned char x16_zp_base(void);

#endif /* X16_H */
