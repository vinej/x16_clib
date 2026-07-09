/* =====================================================================
 * x16clib :: x16/screen.h -- screen mode, text output, cursor
 * =====================================================================
 * These wrap the KERNAL's screen editor. cc65's <conio.h> covers much of
 * the same ground and is more idiomatic C; reach for this header when you
 * want the KERNAL's own behaviour, the bitmap video mode, or the ADDRSEL
 * guard described below.
 *
 * THE KERNAL REQUIRES ADDRSEL = 0. Several of its screen routines write
 * VERA's address registers before selecting a port, assuming port 0 is
 * already current. Call them with port 1 selected -- which is what
 * x16_vera_addr1() and x16_vera_copy() leave behind -- and the display
 * corrupts. Every routine here that enters the KERNAL clears ADDRSEL
 * first, so mixing them with the VERA routines is safe. Raw CHROUT is
 * not: use x16_screen_chrout().
 * =====================================================================
 */

#ifndef X16_SCREEN_H
#define X16_SCREEN_H

/* Text modes. */
#define X16_MODE_80x60          0x00
#define X16_MODE_80x30          0x01
#define X16_MODE_40x60          0x02
#define X16_MODE_40x30          0x03
#define X16_MODE_40x15          0x04
#define X16_MODE_20x30          0x05
#define X16_MODE_20x15          0x06
#define X16_MODE_22x23          0x07
#define X16_MODE_64x50          0x08
#define X16_MODE_64x25          0x09
#define X16_MODE_32x50          0x0A
#define X16_MODE_32x25          0x0B

/* 320x240 at 256 colours on layer 0, with 40x30 text on layer 1.
** cc65's videomode() cannot reach this one.
*/
#define X16_MODE_320x240        0x80

/* Charsets for x16_screen_charset(). */
#define X16_CHARSET_ISO         1
#define X16_CHARSET_PET_UPPER   2       /* upper case + graphics */
#define X16_CHARSET_PET_LOWER   3       /* upper + lower case */

/* Returns 1 on success, 0 if the mode is unsupported. */
unsigned char __fastcall__ x16_screen_set_mode (unsigned char mode);
unsigned char x16_screen_get_mode (void);

void x16_screen_reset (void);   /* KERNAL CINT: back to default text mode */
void x16_screen_cls (void);

/* CHROUT, with ADDRSEL forced to 0 first. */
void __fastcall__ x16_screen_chrout (unsigned char c);

/* Print a NUL-terminated string. Truncated at 255 bytes. */
void __fastcall__ x16_screen_puts (const char *s);

/* Colour of every subsequent character. Both 0-15; see COLOR_* in
** cc65's <cx16.h>.
*/
void __fastcall__ x16_screen_color (unsigned char fg, unsigned char bg);

/* The border, 0-15. */
void __fastcall__ x16_screen_border (unsigned char color);

void __fastcall__ x16_screen_locate (unsigned char row, unsigned char col);
void __fastcall__ x16_screen_get_cursor (unsigned char *row, unsigned char *col);

void __fastcall__ x16_screen_charset (unsigned char charset);

#endif /* X16_SCREEN_H */
