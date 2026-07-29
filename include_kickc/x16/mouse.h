/* =====================================================================
 * x16clib :: x16/mouse.h -- the full KERNAL mouse surface
 * =====================================================================
 * x16/input.h covers the everyday calls, and stays this family's front
 * door:
 *
 *      x16_mouse_show(n)     show cursor sprite n   (0xFF: keep sprite)
 *      x16_mouse_hide()
 *      x16_mouse_get(&x,&y)  position + button mask
 *
 * New here: the raw MOUSE_CONFIG -- which also sizes the mouse field --
 * an explicit scan, and a get that adds the scroll wheel.
 * =====================================================================
 */

#ifndef X16_MOUSE_H
#define X16_MOUSE_H

#include <x16/zpsafe.h>

/* Button bits, from x16_mse_get()/x16_mouse_get(). */
#define X16_MSE_BUTTON_LEFT     0x01
#define X16_MSE_BUTTON_RIGHT    0x02
#define X16_MSE_BUTTON_MIDDLE   0x04
#define X16_MSE_BUTTON_4        0x10
#define X16_MSE_BUTTON_5        0x20

/* x16_mse_config() show selectors. */
#define X16_MSE_HIDE            0x00
#define X16_MSE_SHOW_KEEP       0xFF    /* show, keep the cursor sprite */

/* The raw MOUSE_CONFIG. `show` is X16_MSE_HIDE, X16_MSE_SHOW_KEEP, or a
** cursor sprite number. `width8`/`height8` bound the mouse field in
** 8-pixel units -- both 0 keeps the current bounds, which is what the
** x16_mouse_show()/x16_mouse_hide() shortcuts pass.
*/
void x16_mse_config (unsigned char show, unsigned char width8,
                                  unsigned char height8);

/* Sample the mouse once. The KERNAL's IRQ already does this every
** frame; you only need it if you have taken the interrupt over.
*/
void x16_mse_scan (void);

/* Position through the first two pointers, X16_MSE_BUTTON_* mask
** through the third; returns the signed scroll-wheel movement since the
** last read. x16_mouse_get() is the wheel-less shorthand.
*/
signed char x16_mse_get (unsigned int *x, unsigned int *y,
                                      unsigned char *buttons);

#endif /* X16_MOUSE_H */
