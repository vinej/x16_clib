/* =====================================================================
 * x16clib :: x16/irq.h -- VSYNC counter, raster line and sprite
 *                         collision interrupts
 * =====================================================================
 * x16_irq_install() chains onto the KERNAL's IRQ vector. It does not take
 * the interrupt over: the KERNAL still scans the keyboard, moves the
 * mouse and acknowledges VSYNC.
 *
 * !! vbcc DIFFERENCE. Unlike the cc65 build, this one does NOT unhook
 * itself at exit -- vbcc's +x16 runtime has no destructor mechanism. A
 * program that installs a handler MUST call x16_irq_remove() before it
 * returns, or the first interrupt after exit jumps through a stale vector.
 *
 * ---------------------------------------------------------------------
 * CALLBACKS RUN INSIDE THE INTERRUPT, and may be written in C.
 *
 * Before calling one, the library saves vbcc's runtime zero page (r0..r31,
 * sp, btmp0..btmp3) and its own scratch block, and restores them
 * afterwards -- only when a callback is installed. So a callback may call
 * anything: C code, and any x16_* routine. What it must still do is stay
 * SHORT, and save/restore any VERA state it touches -- CTRL (ADDRSEL/
 * DCSEL) and the address of any data port it reprograms.
 * =====================================================================
 */

#ifndef X16_IRQ_H
#define X16_IRQ_H

/* A raster handler. */
typedef void (*x16_irq_handler)(void);

/* A sprite-collision handler. It receives the collision group bits, the
** top nibble of the ISR, in the accumulator. */
typedef void (*x16_sprcol_handler)(__reg("a") unsigned char groups);

/* Start counting frames. Idempotent. The raster and collision installers
** call it for you. */
void x16_irq_install(void);

/* Restore the previous handler and disable every source this library
** owns -- including AFLOW. Idempotent. */
void x16_irq_remove(void);

/* The frame counter, which wraps at 256. Byte subtraction wraps
** correctly, so deltas stay valid across the wrap. */
unsigned char x16_irq_frames(void);

/* Block until the next frame boundary. Requires enabled interrupts -- it
** hangs otherwise, notably under x16emu's headless -testbench. */
void x16_vsync_wait(void);

/* ---------------------------------------------------------------------
 * Raster interrupts. Call `handler` when VERA reaches `line` (0-511),
 * every frame.
 * ------------------------------------------------------------------ */
void x16_irq_line_install(__reg("r0/r1") unsigned int line,
                          __reg("r2/r3") x16_irq_handler handler);
void x16_irq_line_remove(void);

/* ---------------------------------------------------------------------
 * Sprite collisions, in hardware. `handler` may be NULL: the groups still
 * accumulate for x16_sprite_collisions(), but nothing is called.
 * ------------------------------------------------------------------ */
void x16_irq_sprcol_install(__reg("a/x") x16_sprcol_handler handler);
void x16_irq_sprcol_remove(void);

/* Read and clear the collision groups seen since the last call. 0 means
** none. The read-and-clear is atomic against the accumulating interrupt. */
unsigned char x16_sprite_collisions(void);

#endif /* X16_IRQ_H */
