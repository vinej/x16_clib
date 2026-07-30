/* =====================================================================
 * x16clib :: x16/filepick.h -- a file browser on a panel
 * =====================================================================
 * A directory panel with a mouse and a keyboard: scrolling, descent
 * into folders, and one question answered -- which file? The caller
 * does the rest. It is the same browser in every program that opens it,
 * which is the point: one set of keys, one look, one copy.
 *
 *      x16_fp_filter("*.bmx");
 *      if (x16_fp_open() == X16_FPK_PICK) {
 *          x16_fp_path(buf, sizeof buf);
 *      }
 *      x16_fp_close();
 *
 * The filter is a ';' list of "*.ext" patterns: "*.prg" lists programs,
 * "*.bmx;*.png" either kind of picture, "*.*" (or NULL) everything.
 * Directories are always listed whatever the filter says, or there
 * would be no way to reach the file you wanted. Matching folds case.
 *
 * THE ACCESSORS COPY into your buffer rather than lending a pointer.
 * That is the upstream module's contract, and it is load-bearing: the
 * assembly module can live in a RAM bank, and a banked module cannot
 * lend a pointer into its bank -- by the time the caller dereferences
 * it, the bank is no longer mapped. Every port keeps the same shape.
 *
 * The keys: cursor up/down/Home move, Enter (or a double click, or 'r')
 * picks a file or descends into a folder, 'a' (or a right click) is the
 * ALT gesture, 'h' answers with the directory itself, ESC or Run/Stop
 * (or the panel's x box) cancels. Editing: 'n' makes a folder, 'e'
 * renames, 'd' deletes after a y/n confirm, 'c' remembers a file and
 * 'v' writes it into the folder on show.
 *
 * The panel needs VRAM for its listing cache (2,560 bytes, $12000 by
 * default) and, if enabled, for the save-under copy (5,712 bytes at 80
 * columns, $14000 by default) -- both clear of the text map at $1B000.
 * =====================================================================
 */

#ifndef X16_FILEPICK_H
#define X16_FILEPICK_H

/* What x16_fp_open() comes back with. */
#define X16_FPK_NONE    0       /* cancelled: ESC, Run/Stop, or the x box */
#define X16_FPK_PICK    1       /* a file was chosen: x16_fp_path() has it */
#define X16_FPK_ALT     2       /* the second gesture: right click, or 'a' */
#define X16_FPK_HERE    3       /* 'h': this DIRECTORY, not a file in it */

/* ---------------------------------------------------------------------
 * Configuration. All optional; every string is NUL-terminated and NOT
 * copied, so it must stay valid while the panel is up. NULL restores
 * the default named in the comment.
 */

/* Which files to list, as a ';' list of "*.ext" patterns (NULL = "*.*"). */
void x16_fp_filter (const char *patterns);

/* Which of the listed files the caller can act on itself (NULL = the
** filter). Anything listed that does NOT match is marked [dat] in the
** panel, and x16_fp_is_primary() reports which kind was chosen: a
** launcher lists "*.*" with a primary of "*.prg", and hands a data file
** to a program rather than running it.
*/
void x16_fp_primary (const char *patterns);

/* Where the browser opens (NULL = "/"). */
void x16_fp_start_dir (const char *path);

/* The text in front of the path on the header row (NULL = "files in "),
** and the reminder along the bottom of the panel.
*/
void x16_fp_heading (const char *text);
void x16_fp_footing (const char *text);

/* Colour bytes (foreground | background << 4) for the panel body, the
** header/footer bars, and the selected row. The defaults are blue on
** light grey with an inverted selection. The name prompt is drawn blue
** on yellow whatever the style: a field that blends in is a field
** nobody sees.
*/
void x16_fp_style (unsigned char panel, unsigned char bar,
                            unsigned char sel);

/* The charset the panel is drawn in (3 = PET upper/lower, the default).
** 255 leaves whatever the caller had -- there is no way to ask the
** KERNAL which charset is loaded, so the browser cannot put back what
** it does not know.
*/
void x16_fp_charset (unsigned char charset);

/* Where the 2,560-byte listing cache lives in VRAM (bit 16 picks the
** bank; the default is 0x12000UL). VRAM rather than a RAM bank, and not
** by preference: the upstream module can be banked, and a banked module
** cannot page a bank into the window it is executing from.
*/
void x16_fp_cache (unsigned long vaddr);

/* Keep what the panel covers and put it back on x16_fp_close(). The
** copy lives in VRAM too (the text map IS VRAM): 5,712 bytes at 80
** columns, 0x14000UL by default. A launcher that repaints itself does
** not need this; a spreadsheet does.
*/
void x16_fp_saveunder (unsigned char on, unsigned long vaddr);

/* ---------------------------------------------------------------------
 * The session.
 */

/* Put the panel up on the starting directory and run it until the user
** answers. Returns an X16_FPK_* code. X16_FPK_HERE is for a caller that
** wants a PLACE rather than a file: the drive is left standing in the
** browsed directory whatever the answer, so a bare filename written
** afterwards lands there and x16_fp_dir() names it.
*/
unsigned char x16_fp_open (void);

/* The same panel again, same directory, same selection: for a caller
** that acted on an X16_FPK_ALT and wants the browser back.
*/
unsigned char x16_fp_resume (void);

/* Put back what the panel covered and hide the pointer. The DRIVE stays
** in the directory that was being browsed: a caller that needs to be
** somewhere else should say so with x16_dos_chdir().
*/
void x16_fp_close (void);

/* Paint the panel again, after a caller has drawn over it. */
void x16_fp_redraw (void);

/* ---------------------------------------------------------------------
 * What the caller reads back. Each COPIES into `dest`, always
 * NUL-terminated and truncated to fit; `size` counts the terminator.
 * Returns how many characters were copied, terminator aside.
 */

/* The absolute path of the chosen entry. */
unsigned char x16_fp_path (char *dest, unsigned char size);

/* Just its name, without the directory. */
unsigned char x16_fp_name (char *dest, unsigned char size);

/* The directory being browsed, which is where the drive was left. */
unsigned char x16_fp_dir (char *dest, unsigned char size);

/* 1 when the chosen entry matches the primary pattern (falling back to
** the filter, then to "*.*").
*/
unsigned char x16_fp_is_primary (void);

/* Does a name match a ';' list of patterns? The same matcher the panel
** filters with, exposed because a caller often wants to ask it about a
** name of its own. A NULL pattern list matches everything.
*/
unsigned char x16_fp_match (const char *name, const char *patterns);

/* ---------------------------------------------------------------------
 * The panel's geometry, for a caller drawing inside it. Valid once
 * x16_fp_open() has run: the panel sizes itself to the screen it finds
 * (80x60 or 40x30).
 */
unsigned char x16_fp_panel_top (void);
unsigned char x16_fp_panel_left (void);
unsigned char x16_fp_panel_width (void);
unsigned char x16_fp_panel_rows (void);

/* pulls the implementation in with this header */
#pragma compile("filepick.c")

#endif /* X16_FILEPICK_H */
