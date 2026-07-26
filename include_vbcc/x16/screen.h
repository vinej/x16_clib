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
 *
 * ABI NOTE. Each prototype pins its arguments to the zero-page pseudo-
 * registers the hand-written routine reads (see src_vbcc/video/screen.s).
 * A single-argument char rides in A, a pointer in the a/x pair; the two
 * routines that want a value in X or Y (color, locate) and the one with
 * two pointers (get_cursor) route the extra values through r0/r1, because
 * vbcc will not pass an argument in x or y.
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

/* 320x240 at 256 colours on layer 0, with 40x30 text on layer 1. */
#define X16_MODE_320x240        0x80

/* Charsets for x16_screen_charset(). */
#define X16_CHARSET_ISO         1
#define X16_CHARSET_PET_UPPER   2       /* upper case + graphics */
#define X16_CHARSET_PET_LOWER   3       /* upper + lower case */

/* Returns 1 on success, 0 if the mode is unsupported. */
unsigned char x16_screen_set_mode(__reg("a") unsigned char mode);
unsigned char x16_screen_get_mode(void);

void x16_screen_reset(void);    /* KERNAL CINT: back to default text mode */
void x16_screen_cls(void);

/* CHROUT, with ADDRSEL forced to 0 first. */
void x16_screen_chrout(__reg("a") unsigned char c);

/* Print a NUL-terminated string. Truncated at 255 bytes. */
void x16_screen_puts(__reg("a/x") const char *s);

/* Colour of every subsequent character. Both 0-15. (bg travels in r0
** because vbcc will not pass an argument in x.) */
void x16_screen_color(__reg("a") unsigned char fg, __reg("r0") unsigned char bg);

/* The border, 0-15. */
void x16_screen_border(__reg("a") unsigned char color);

/* Move the text cursor. (row and col travel in r0/r1 because the routine
** wants them in x and y, which vbcc will not pass an argument in.) */
void x16_screen_locate(__reg("r0") unsigned char row, __reg("r1") unsigned char col);

/* Read the cursor position back into two bytes. */
void x16_screen_get_cursor(__reg("a/x") unsigned char *row,
                           __reg("r0/r1") unsigned char *col);

void x16_screen_charset(__reg("a") unsigned char charset);

/* The LIVE text grid, after whatever x16_screen_set_mode() left behind
** -- not the 80x60 default.
*/
void x16_screen_get_size(__reg("a/x") unsigned char *cols,
                         __reg("r0/r1") unsigned char *rows);

/* =====================================================================
** Direct text-map access
** =====================================================================
** CHROUT costs several hundred cycles a character once the editor's
** scroll checks, colour handling and cursor bookkeeping are paid for. A
** program that repaints a whole text screen -- a spreadsheet, a file
** browser, any full-screen TUI -- cannot afford that, so these write
** VERA's tile map itself:
**
**     x16_screen_addr(row, col);
**     x16_screen_blit("READY.", 6, X16_TEXT_COLOR(1, 6));
**
** The address auto-increments, so a whole line costs one set-up and two
** stores per column, and consecutive runs can be chained.
**
** The KERNAL is not involved and neither is its cursor: these do not
** scroll, do not wrap, and do not move the CHROUT cursor. Do not print
** past the end of a row.
**
** Text is PETSCII on the way in -- the same bytes you would give CHROUT
** -- and is folded to screen codes for you.
** =====================================================================
*/

/* The colour byte these take: foreground | background << 4, the same
** layout x16_screen_color() builds.
*/
#define X16_TEXT_COLOR(fg, bg)  ((unsigned char)(((fg) & 0x0F) | ((bg) << 4)))

/* Point VERA port 0 (or port 1) at a character cell. */
void x16_screen_addr(__reg("r0") unsigned char row, __reg("r1") unsigned char col);
void x16_screen_addr1(__reg("r0") unsigned char row, __reg("r1") unsigned char col);

/* PETSCII to screen code, for a caller building its own tile data. */
unsigned char x16_screen_scode(__reg("a") unsigned char petscii);

/* Write a run of characters, all one colour, at the current port-0
** address. Count is 1-255.
*/
void x16_screen_blit(__reg("a/x") const char *text,
                     __reg("r0") unsigned char count,
                     __reg("r1") unsigned char color);

/* The same, with one repeated character: the usual way to blank part of
** a line.
*/
void x16_screen_blitfill(__reg("r0") unsigned char count,
                         __reg("r1") unsigned char color,
                         __reg("r2") unsigned char ch);

/* Slide a rectangle of the text screen up (down = 0) or down (down = 1)
** by `distance` rows.
**
** Re-rendering a whole grid to scroll one line pays for every cell; for
** a spreadsheet or a directory listing most of that cost is formatting
** the contents, not drawing them. This moves the picture inside VRAM,
** so only the row that appears has to be rendered.
**
** The rows uncovered at the trailing edge keep their old contents -- you
** draw what belongs there. Nothing happens when `distance` is 0, or when
** it is large enough that nothing would survive; repaint in that case.
**
** Vertical only: scrolling sideways would move a row onto itself.
*/
void x16_screen_scroll(__reg("r0") unsigned char top,
                       __reg("r1") unsigned char left,
                       __reg("r2") unsigned char height,
                       __reg("r3") unsigned char width,
                       __reg("r4") unsigned char distance,
                       __reg("r5") unsigned char down);

#endif /* X16_SCREEN_H */
