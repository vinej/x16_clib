/* =====================================================================
 * x16clib :: x16/irq.h -- VSYNC frame counter and IRQ hook
 * =====================================================================
 * x16_irq_install() chains onto the KERNAL's IRQ vector. It does not take
 * the interrupt over: the KERNAL still scans the keyboard, moves the
 * mouse and acknowledges VSYNC. It composes with cc65's own IRQ path too.
 *
 * The library unhooks itself at exit even if you forget, so a program
 * that returns from main() cannot leave a live vector pointing into
 * memory BASIC is about to reuse. Calling x16_irq_remove() yourself is
 * still good manners, and is required before any code that relies on the
 * KERNAL's untouched vector.
 *
 * cc65's waitvsync() is the dependency-free alternative to
 * x16_vsync_wait(): it polls VERA and needs no handler installed. Use
 * this one when you also want the frame counter.
 *
 * RESTRICTION. The library's X16_P0..P7 / X16_T0..T7 scratch block is
 * shared and NOT reentrant. An interrupt handler of your own must not
 * call any x16_* routine that touches it -- in practice, anything taking
 * more than three arguments or any 16-bit argument. These are safe from
 * an ISR: x16_irq_frames, x16_bank_set, x16_bank_get, x16_screen_chrout,
 * x16_screen_border, x16_layer_on, x16_layer_off, x16_sprites_on,
 * x16_sprites_off, x16_pcm_put, x16_psg_note_off.
 * =====================================================================
 */

#ifndef X16_IRQ_H
#define X16_IRQ_H

/* Start counting frames. Idempotent. */
void x16_irq_install (void);

/* Restore the previous handler. Idempotent. */
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
** twice within one.
**
** Requires x16_irq_install() and enabled interrupts. It hangs otherwise
** -- notably under x16emu's headless -testbench mode, which runs no
** video and so raises no VSYNC.
*/
void x16_vsync_wait (void);

#endif /* X16_IRQ_H */
