/* =====================================================================
 * x16clib :: x16/irq.h -- VSYNC counter, raster line and sprite
 *                         collision interrupts
 * =====================================================================
 * Chains onto the KERNAL's IRQ vector (CINV, $0314) rather than taking
 * the interrupt over: the KERNAL still scans the keyboard, moves the
 * mouse, blinks the cursor, and acknowledges VSYNC.
 *
 * A C callback runs inside the interrupt. Before it is called the
 * library saves the zero page the interrupted code may own -- the
 * $22-$7F user window KickC allocates from (see x16/zpsafe.h) plus the
 * KERNAL's r0-r15 at $02-$21 -- and restores it after: 126 bytes each
 * way, roughly 2% of a frame, and only
 * when a callback is actually installed. That is what makes a raster
 * handler written in C correct rather than usually-correct: it may call
 * anything, including other x16_* routines, without corrupting whatever
 * computation it interrupted.
 *
 * What a callback must still do is stay SHORT, and save/restore any
 * VERA state it touches -- CTRL (ADDRSEL/DCSEL) and the address of any
 * data port it reprograms -- or the interrupted code's next VERA access
 * lands somewhere else.
 *
 * One difference from the cc65 build: cc65 unhooked automatically at
 * program exit through a linker destructor. KickC has no such
 * mechanism, so if your main() returns to BASIC, call x16_irq_remove()
 * first -- otherwise CINV points into memory BASIC is about to reuse.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** KickC build. The API is identical to the cc65 build's; what differs is
** the delivery. KickC has no linker and no archive format -- it compiles
** the whole program from source and strips what goes unused -- so the
** KickC port is a SOURCE distribution. Include this header; the matching
** implementation in src_kickc/x16/ is compiled in automatically when the
** library path points there:
**
**     kickc -p cx16 -a -I include_kickc -L src_kickc yourprog.c
** --------------------------------------------------------------------- */

#ifndef X16_IRQ_H
#define X16_IRQ_H

#include <x16/zpsafe.h>

/* A raster handler. */
typedef void (*x16_irq_handler) (void);

/* A sprite-collision handler. It receives the collision group bits, the
** top nibble of the ISR.
*/
typedef void (*x16_sprcol_handler) (unsigned char groups);

/* Start counting frames. Idempotent. The raster and collision installers
** call it for you.
*/
void x16_irq_install (void);

/* Restore the previous handler and disable every source this library
** owns -- including AFLOW, so a PCM stream in progress stops. Leaving
** AFLOW enabled with the handler unhooked would assert the IRQ line
** forever: nothing else acknowledges it. Idempotent.
*/
void x16_irq_remove (void);

/* The frame counter, which wraps at 256. Byte subtraction wraps
** correctly, so deltas stay valid across the wrap:
**
**      unsigned char start = x16_irq_frames();
**      ...work...
**      elapsed = (unsigned char)(x16_irq_frames() - start);
*/
unsigned char x16_irq_frames (void);

/* Block until the next frame boundary. Waits for the counter to change
** rather than polling VERA, so it can neither miss a frame nor spin
** twice within one. Requires enabled interrupts -- it hangs otherwise,
** notably under x16emu's headless -testbench, which runs no video.
*/
void x16_vsync_wait (void);

/* ---------------------------------------------------------------------
 * Raster interrupts.
 *
 * Call `handler` when VERA reaches `line` (0-511; the visible display is
 * 0-479), every frame. This is how a status bar sits over a scrolling
 * playfield: change the display registers in a handler at the split line
 * and change them back in a second handler, or at VSYNC.
 * ------------------------------------------------------------------ */
void x16_irq_line_install (unsigned int line, x16_irq_handler handler);
void x16_irq_line_remove (void);

/* ---------------------------------------------------------------------
 * Sprite collisions, in hardware.
 *
 * VERA compares the collision masks of every sprite pair once per frame,
 * at the end of rendering. Two sprites collide when their masks -- the
 * top nibble of attribute byte 6, see x16_sprite_flags() -- share a bit
 * AND their rectangles overlap. Far cheaper than x16_collide16() across
 * a cast of sprites, though it tells you the groups, not the pair.
 *
 * `handler` may be NULL: the groups still accumulate for
 * x16_sprite_collisions(), but nothing is called.
 * ------------------------------------------------------------------ */
void x16_irq_sprcol_install (x16_sprcol_handler handler);
void x16_irq_sprcol_remove (void);

/* Read and clear the collision groups seen since the last call. 0 means
** none. Requires x16_irq_sprcol_install(); a NULL handler is fine, and
** is how you poll instead of being called.
**
** The read-and-clear is atomic against the accumulating interrupt.
*/
unsigned char x16_sprite_collisions (void);

#endif /* X16_IRQ_H */
