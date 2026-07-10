/* =====================================================================
 * x16clib :: x16/vera.h -- VRAM data-port access
 * =====================================================================
 * VERA has two independent data ports. Point one at a VRAM address with
 * an auto-increment, then stream bytes through it. DATA0 always talks to
 * port 0 and DATA1 always to port 1, whatever ADDRSEL says -- which is
 * what lets x16_vera_copy() run a read and a write in the same loop.
 *
 * For a single VRAM byte, use cc65's vpeek()/vpoke() from <cx16.h>.
 * These routines are for runs, where reloading the address per byte
 * would dominate the cost.
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
** --------------------------------------------------------------------- */

#ifndef X16_VERA_H
#define X16_VERA_H

/* ---------------------------------------------------------------------
 * Address increments.
 *
 * These are raw indices, 0-15. cc65's <cx16.h> defines constants with
 * the SAME NAMES minus the prefix (VERA_INC_1 and friends) that are
 * already shifted into ADDR_H bits 7:4, so its VERA_INC_1 is 0x10 while
 * a raw index of 1 is, well, 1. Same bits on the wire, sixteen times
 * apart as constants. Pass the X16_-prefixed ones to this header and
 * cc65's to cc65; never mix them.
 *
 * Or the increment with X16_DECR to walk backwards.
 * ------------------------------------------------------------------ */
#define X16_INC_0       0
#define X16_INC_1       1
#define X16_INC_2       2
#define X16_INC_4       3
#define X16_INC_8       4
#define X16_INC_16      5
#define X16_INC_32      6
#define X16_INC_64      7
#define X16_INC_128     8
#define X16_INC_256     9
#define X16_INC_512     10
#define X16_INC_40      11      /* one 40-column text row */
#define X16_INC_80      12      /* one 80-column text row */
#define X16_INC_160     13
#define X16_INC_320     14      /* one 320-pixel bitmap row */
#define X16_INC_640     15

#define X16_DECR        0x10    /* step backwards */

/* ---------------------------------------------------------------------
 * The VRAM map. 17-bit addresses.
 *
 * $1F9C0-$1FFFF (PSG, palette, sprite attributes) is WRITE-ONLY: reads
 * return the last value the host wrote, not the hardware's real state.
 * Reading back your own writes is fine; inferring state after a reset is
 * not.
 * ------------------------------------------------------------------ */
#define X16_VRAM_BITMAP         0x00000UL       /* 320x240x256 framebuffer */
#define X16_VRAM_SPRITE_DATA    0x13000UL       /* KERNAL's sprite images */
#define X16_VRAM_TEXT           0x1B000UL       /* default text tilemap */
#define X16_VRAM_CHARSET        0x1F000UL
#define X16_VRAM_PSG            0x1F9C0UL       /* 16 voices x 4 bytes */
#define X16_VRAM_PALETTE        0x1FA00UL       /* 256 entries x 2 bytes */
#define X16_VRAM_SPRITE_ATTR    0x1FC00UL       /* 128 sprites x 8 bytes */

/* Point data port 0 (or 1) at a VRAM address.
**
** `addr` comes last so cc65 passes all four of its bytes in registers.
** Only bit 16 of the high half reaches the hardware.
*/
void x16_vera_addr0 (unsigned char inc, unsigned long addr);
void x16_vera_addr1 (unsigned char inc, unsigned long addr);

/* Write `value` `count` times through port 0, which must already point
** at the destination. A count of 0 writes nothing.
**
** The increment decides the shape: X16_INC_1 fills a linear run,
** X16_INC_320 stripes down a bitmap column.
*/
void x16_vera_fill (unsigned char value, unsigned int count);

/* Copy `count` bytes from port 0 to port 1. Point port 0 at the source
** and port 1 at the destination first, each with its own increment.
*/
void x16_vera_copy (unsigned int count);

/* 1 if VERA's firmware carries the FX register set (v0.3.1 and later),
** else 0. Call before anything in <x16/verafx.h>.
*/
unsigned char x16_vera_has_fx (void);

#endif /* X16_VERA_H */
