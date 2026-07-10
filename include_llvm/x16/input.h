/* =====================================================================
 * x16clib :: x16/input.h -- joystick, mouse, keyboard
 * =====================================================================
 * Thin wrappers over the KERNAL. cc65 ships portable joystick and mouse
 * drivers (<joystick.h>, <mouse.h>); these are the X16's own calls, with
 * no driver to install and the SNES pad's full button set exposed.
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

#ifndef X16_INPUT_H
#define X16_INPUT_H

/* Joystick bits are ACTIVE LOW: a pressed button reads 0.
**
**      unsigned char present;
**      unsigned int b = x16_joy_get(1, &present);
**      if (present && !(b & X16_JOY_LEFT)) { ... moving left ... }
*/
#define X16_JOY_B       0x0080
#define X16_JOY_Y       0x0040
#define X16_JOY_SELECT  0x0020
#define X16_JOY_START   0x0010
#define X16_JOY_UP      0x0008
#define X16_JOY_DOWN    0x0004
#define X16_JOY_LEFT    0x0002
#define X16_JOY_RIGHT   0x0001
#define X16_JOY_A       0x8000
#define X16_JOY_X       0x4000
#define X16_JOY_L       0x2000
#define X16_JOY_R       0x1000

/* Mouse buttons, from x16_mouse_get(). */
#define X16_MOUSE_LEFT    0x01
#define X16_MOUSE_RIGHT   0x02
#define X16_MOUSE_MIDDLE  0x04

/* Unpack x16_key_peek(). */
#define X16_KEY_CHAR(v)   ((unsigned char)((v) & 0xFF))
#define X16_KEY_COUNT(v)  ((unsigned char)((v) >> 8))

/* Sample every joystick. The KERNAL's IRQ already does this once a
** frame; you only need it if you have taken the interrupt over.
*/
void x16_joy_scan (void);

/* `joy` is 0 for the keyboard, 1-4 for gamepads. Returns the button
** bits, active low. *present becomes 1 or 0.
*/
unsigned int x16_joy_get (unsigned char joy,
                                       unsigned char *present);

/* 0xFF shows the pointer without changing the cursor sprite; a smaller
** number selects cursor sprite n. The mouse field keeps its size.
*/
void x16_mouse_show (unsigned char cursor);
void x16_mouse_hide (void);

/* Returns the button mask; writes the position through the pointers. */
unsigned char x16_mouse_get (unsigned int *x, unsigned int *y);

/* PETSCII, or 0 if nothing is waiting. Non-blocking. */
unsigned char x16_key_get (void);

/* Blocks. */
unsigned char x16_key_wait (void);

/* The next key without consuming it, plus the queue depth:
** key | queued<<8. Use X16_KEY_CHAR / X16_KEY_COUNT.
**
** When the queue is empty only the COUNT is meaningful. KBDBUF_PEEK
** leaves a stale byte in the character position, so test the count:
**
**      unsigned int p = x16_key_peek();
**      if (X16_KEY_COUNT(p)) { c = X16_KEY_CHAR(p); }
*/
unsigned int x16_key_peek (void);

#endif /* X16_INPUT_H */
