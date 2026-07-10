/* =====================================================================
 * x16clib :: x16/irq.h -- VSYNC counter, raster line and sprite
 *                         collision interrupts
 * =====================================================================
 * x16_irq_install() chains onto the KERNAL's IRQ vector. It does not take
 * the interrupt over: the KERNAL still scans the keyboard, moves the
 * mouse and acknowledges VSYNC. It composes with cc65's own IRQ path.
 *
 * The library unhooks itself at exit even if you forget, so a program
 * that returns from main() cannot leave a live vector pointing into
 * memory BASIC is about to reuse.
 *
 * cc65's waitvsync() is the dependency-free alternative to
 * x16_vsync_wait(): it polls VERA and needs no handler installed. Use
 * this one when you also want the frame counter.
 *
 * ---------------------------------------------------------------------
 * CALLBACKS RUN INSIDE THE INTERRUPT, and may be written in C.
 *
 * Before calling one, the library saves cc65's zero-page runtime and its
 * own scratch block, and restores them afterwards -- 42 bytes, about 950
 * cycles, roughly 0.7% of a frame, and only when a callback is installed.
 * Without that a C handler firing between two halves of an expression
 * would corrupt ptr1/tmp1/sreg in the code it interrupted.
 *
 * So a callback may call anything: C code, and any x16_* routine. What it
 * must still do is stay SHORT, and save/restore any VERA state it
 * touches -- CTRL (ADDRSEL/DCSEL) and the address of any data port it
 * reprograms -- or the interrupted code's next VERA access lands
 * somewhere else.
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

#ifndef X16_IRQ_H
#define X16_IRQ_H

/* A raster handler. */
typedef void (*x16_irq_handler) (void);

/* A sprite-collision handler. It receives the collision group bits, the
** top nibble of the ISR, in the accumulator.
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
void x16_irq_line_install (unsigned int line,
                                        x16_irq_handler handler);
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
