/* =====================================================================
 * x16clib :: x16/console.h -- the KERNAL console API
 * =====================================================================
 * A proportional-font terminal over GRAPH: word wrap, paging, inline
 * images, blocking line input. Handy for tools and adventures; games
 * usually want x16/graph.h directly.
 *
 * Usual sequence:
 *
 *      x16_graph_init(NULL);
 *      x16_con_init(0, 0, 0, 0);
 *      x16_con_put_char('H', 1);       ...
 *
 * Characters are ISO/ASCII -- the GRAPH font's encoding, not PETSCII.
 * =====================================================================
 */

#ifndef X16_CONSOLE_H
#define X16_CONSOLE_H

/* Control codes accepted by x16_con_put_char (and x16_graph_put_char)
** for font styling.
*/
#define X16_CON_ATTR_UNDERLINE  0x04
#define X16_CON_ATTR_BOLD       0x06
#define X16_CON_ATTR_ITALICS    0x0B
#define X16_CON_ATTR_OUTLINE    0x0C
#define X16_CON_ATTR_RESET      0x92

/* Open a console in the rectangle; all zeroes = the full screen.
** Clears the area.
*/
void x16_con_init (unsigned int x, unsigned int y,
                                unsigned int width, unsigned int height);

/* wrap 0 breaks lines anywhere; nonzero buffers each word and breaks
** between words. Scrolls when the window is full, paging first if a
** message is set.
*/
void x16_con_put_char (unsigned char c, unsigned char wrap);

/* Line input: BLOCKS until RETURN finishes a line, then returns it one
** character per call, CR last.
*/
unsigned char x16_con_get_char (void);

/* After each full page of output, show `msg` and wait for a key. */
void x16_con_set_paging_message (const char *msg);

/* Scroll freely, never prompt -- the power-on state. */
void x16_con_disable_paging (void);

/* Inline a GRAPH_draw_image-format bitmap at the cursor, like an
** oversized character.
*/
void x16_con_put_image (const unsigned char *image,
                                     unsigned int width,
                                     unsigned int height);

/* pulls the implementation in with this header */
#pragma compile("console.c")

