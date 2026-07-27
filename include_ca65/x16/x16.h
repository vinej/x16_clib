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
#include <x16/bitmap8l.h>         /* 320x240x256 drawing */
#include <x16/bitmap2h.h>         /* 640x480@2bpp drawing */
#include <x16/bitmap2l.h>         /* 320x240@2bpp drawing */
#include <x16/bitmap4l.h>         /* 320x240x16 drawing */
#include <x16/bitmap4h.h>         /* VERA_2 640x480x16 drawing */
#include <x16/bitmap8h.h>         /* VERA_2 640x480x256 drawing */
#include <x16/shapes.h>         /* circle/disc/flood for both bitmaps */
#include <x16/verafx.h>         /* hardware multiply, fills, lines, triangles */
#include <x16/verafx_utils.h>   /* the raw FX register knobs, one at a time */
#include <x16/psg.h>            /* 16-voice PSG */
#include <x16/ym.h>             /* YM2151 FM */
#include <x16/pcm.h>            /* PCM FIFO, and AFLOW streaming */
#include <x16/input.h>          /* joystick, mouse, keyboard */
#include <x16/keyboard.h>       /* kbd buffer injection, modifiers, keymap */
#include <x16/mouse.h>          /* raw MOUSE_CONFIG, scan, wheel */
#include <x16/clock.h>          /* jiffy timer, RTC date/time */
#include <x16/i2c.h>            /* the I2C bus: SMC, RTC NVRAM */
#include <x16/graph.h>          /* KERNAL GRAPH: lines, rects, ovals, text */
#include <x16/fb.h>             /* KERNAL framebuffer driver: pixel cursor */
#include <x16/console.h>        /* KERNAL console: wrap, paging, line input */
#include <x16/irq.h>            /* VSYNC, raster and collision interrupts */
#include <x16/bank.h>           /* banked RAM, and a whole-bank allocator */
#include <x16/mem.h>            /* KERNAL block ops, incl. LZSA2 depacking */
#include <x16/load.h>           /* load and save, including into VRAM */
#include <x16/fileio.h>         /* OPEN/CHKIN/CHRIN: streamed channel I/O */
#include <x16/iec.h>            /* raw IEC / serial bus control */
#include <x16/dir.h>            /* walk a device directory */
#include <x16/ringbuffer.h>     /* an 8 KB FIFO in a HIRAM bank */
#include <x16/stack.h>          /* an 8 KB LIFO in a HIRAM bank */
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
#include <x16/string.h>         /* NUL-terminated strings: copy, case, find, slice, sort */
#include <x16/sort.h>           /* in-place insertion sorts, typed + comparator */
#include <x16/bcd.h>            /* packed-BCD add/sub, 8/16/32-bit */
#include <x16/bits.h>           /* masked bit set/clr/put/test, nibble helpers */
#include <x16/number.h>         /* dec/hex/bin formatting and decimal parsing */
#include <x16/tscrunch.h>       /* TSCrunch depacking, faster than ZX0 */

/* Diagnostic: the address the linker gave the scratch block. Nothing
** depends on the value; it moves as cc65's own zero-page footprint does.
*/
unsigned char x16_zp_base (void);

#endif /* X16_H */
