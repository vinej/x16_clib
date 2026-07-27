/* =====================================================================
 * x16clib :: x16/keyboard.h -- keyboard buffer and layout helpers
 * =====================================================================
 * The X16-specific keyboard surface beyond x16/input.h. That header
 * already covers consuming keys, and its functions are this family's
 * read side:
 *
 *      x16_key_get()   next key, or 0        (upstream kbd + GETIN)
 *      x16_key_wait()  block for a key
 *      x16_key_peek()  look without taking   (upstream kbd_peek)
 *
 * New here: injecting keys into the KERNAL's buffer (self-typing demos,
 * macros, tests), the live modifier bitfield, and the keyboard layout.
 * =====================================================================
 */

#ifndef X16_KEYBOARD_H
#define X16_KEYBOARD_H

/* x16_kbd_get_modifiers() bits. */
#define X16_KBD_MOD_SHIFT       0x01
#define X16_KBD_MOD_ALT         0x02
#define X16_KBD_MOD_CTRL        0x04
#define X16_KBD_MOD_CAPS        0x10
#define X16_KBD_MOD_ALTGR       (X16_KBD_MOD_ALT | X16_KBD_MOD_CTRL)

/* A layout name never exceeds this, NUL included. */
#define X16_KBD_KEYMAP_LEN      14

/* Scan the keyboard once. The KERNAL's IRQ already does this every
** frame; you only need it if you have taken the interrupt over.
*/
void x16_kbd_scan (void);

/* Append a PETSCII key to the keyboard buffer, as if typed. Read it
** back with x16_key_get()/x16_key_peek().
*/
void __fastcall__ x16_kbd_put (unsigned char key);

/* The modifiers held down right now, as X16_KBD_MOD_* bits. */
unsigned char x16_kbd_get_modifiers (void);

/* Copies the active layout's NUL-terminated name -- e.g. "en-us" --
** into `name` (X16_KBD_KEYMAP_LEN bytes are always enough) and returns
** the layout index.
*/
unsigned char __fastcall__ x16_kbd_get_keymap (char *name);

/* Switch layouts by name. Returns 1 on success; 0 leaves the previous
** layout active. The name must match the ROM's spelling byte for byte.
*/
unsigned char __fastcall__ x16_kbd_set_keymap (const char *name);

#endif /* X16_KEYBOARD_H */
