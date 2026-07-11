/* =====================================================================
 * x16clib :: x16/screen.h -- screen mode, text output, cursor
 * =====================================================================
 * Thin bindings over the KERNAL's screen routines, each with one
 * important addition: ADDRSEL is forced to 0 first.
 *
 * Several KERNAL screen routines write VERA's address registers before
 * selecting a port, taking it on faith that port 0 is selected. Call
 * them with ADDRSEL = 1 -- which x16_vera_addr1() and x16_vera_copy()
 * both leave behind -- and the screen corrupts. Going through these
 * wrappers re-establishes the precondition every time.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** Oscar64 build. The API is identical to the cc65 build's; what differs
** is the delivery. Oscar64 compiles the whole program at once and strips
** what goes unused, so this port is a SOURCE distribution: headers and
** implementations sit side by side in src_oscar64/x16/, and the
** `#pragma compile` at the bottom of this header pulls the matching .c
** in automatically:
**
**     oscar64 -tm=x16 -n -i=src_oscar64 -o=YOURPROG.PRG yourprog.c
** --------------------------------------------------------------------- */

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

/* 320x240 at 256 colours on layer 0, with 40x30 text on layer 1. */
#define X16_MODE_320x240        0x80

/* Charsets for x16_screen_charset(). */
#define X16_CHARSET_ISO         1
#define X16_CHARSET_PET_UPPER   2       /* upper case + graphics */
#define X16_CHARSET_PET_LOWER   3       /* upper + lower case */

/* Returns 1 on success, 0 if the mode is unsupported. */
unsigned char x16_screen_set_mode (unsigned char mode);
unsigned char x16_screen_get_mode (void);

void x16_screen_reset (void);   /* KERNAL CINT: back to default text mode */
void x16_screen_cls (void);

/* CHROUT, with ADDRSEL forced to 0 first. */
void x16_screen_chrout (unsigned char c);

/* Print a NUL-terminated string. Truncated at 255 bytes. */
void x16_screen_puts (const char *s);

/* Colour of every subsequent character. Both 0-15. */
void x16_screen_color (unsigned char fg, unsigned char bg);

/* The border, 0-15. */
void x16_screen_border (unsigned char color);

void x16_screen_locate (unsigned char row, unsigned char col);
void x16_screen_get_cursor (unsigned char *row, unsigned char *col);

void x16_screen_charset (unsigned char charset);

/* pulls the implementation in with this header */
#pragma compile("screen.c")

#endif /* X16_SCREEN_H */
