/* =====================================================================
 * x16clib :: x16/graph.h -- the KERNAL GRAPH drawing API
 * =====================================================================
 * The ROM's GEOS-derived drawing layer: lines, rects, ovals, images,
 * proportional text, all clipped to a window, drawn through the active
 * framebuffer driver (x16/fb.h).
 *
 * x16_graph_init(NULL) is the entry point for the whole family: it
 * switches the display to 320x240@8bpp, installs the default driver,
 * resets window/colors/font, and clears. Get text mode back with
 * x16_screen_set_mode() or x16_screen_reset() (x16/screen.h).
 *
 * Colors: `stroke` is the pen -- lines, glyphs, and shape outlines,
 * including the one-pixel border of filled shapes; `fill` paints the
 * interiors; `background` is what clearing paints. A "filled" rect or
 * oval is therefore two-tone unless stroke == fill.
 * =====================================================================
 */

#ifndef X16_GRAPH_H
#define X16_GRAPH_H

/* Style bits, as x16_graph_get_char_size() takes and returns them.
** These are the ROM's GEOS bits (x16-rom graphics/fonts/fonts.inc) --
** not the 1/2/4 the upstream assembly library documents, which the ROM
** never accepted.
*/
#define X16_GRAPH_STYLE_UNDERLINE       0x80
#define X16_GRAPH_STYLE_BOLD            0x40
#define X16_GRAPH_STYLE_REVERSE         0x20
#define X16_GRAPH_STYLE_ITALIC          0x10
#define X16_GRAPH_STYLE_OUTLINE         0x08

/* x16_graph_get_char_size() results. On a printable character the first
** three fields are set; on a control code only `style` is.
*/
typedef struct {
    unsigned char baseline;     /* rows from glyph top to the baseline */
    unsigned char width;
    unsigned char height;
    unsigned char style;        /* the style a control code selects */
} x16_char_size;

/* `driver` is an FB_* vector table, or NULL for the default
** 320x240@8bpp driver.
*/
void x16_graph_init (const void *driver);

/* Clear the current window to the background color. */
void x16_graph_clear (void);

/* Clip all drawing to this rectangle. All zeroes = full screen. */
void x16_graph_set_window (unsigned int x, unsigned int y,
                                        unsigned int width,
                                        unsigned int height);

void x16_graph_set_colors (unsigned char stroke,
                                        unsigned char fill,
                                        unsigned char background);

void x16_graph_draw_line (unsigned int x1, unsigned int y1,
                                       unsigned int x2, unsigned int y2);

/* fill 0 outlines in the stroke color, nonzero fills. `radius` is
** accepted for API parity; the ROM currently ignores it.
*/
void x16_graph_draw_rect (unsigned int x, unsigned int y,
                                       unsigned int width,
                                       unsigned int height,
                                       unsigned int radius,
                                       unsigned char fill);

/* Copy a rectangle; source and target may overlap. ROM quirk, verified
** against r49: moving DOWN (ty > sy) copies height+1 rows, moving up
** copies exactly height.
*/
void x16_graph_move_rect (unsigned int sx, unsigned int sy,
                                       unsigned int tx, unsigned int ty,
                                       unsigned int width,
                                       unsigned int height);

/* The oval inscribes its bounding box. fill as in draw_rect. */
void x16_graph_draw_oval (unsigned int x, unsigned int y,
                                       unsigned int width,
                                       unsigned int height,
                                       unsigned char fill);

/* `image` is width*height 8-bit pixels, row by row. */
void x16_graph_draw_image (unsigned int x, unsigned int y,
                                        const unsigned char *image,
                                        unsigned int width,
                                        unsigned int height);

/* NULL restores the system font. A custom font is in GEOS format; it
** must sit below $A000.
*/
void x16_graph_set_font (const void *font);

/* Measure `c` in `style`. Returns 1 for a printable character (fills
** baseline/width/height), 0 for a control code (fills `style` with the
** style that code selects).
*/
unsigned char x16_graph_get_char_size (unsigned char c,
                                                    unsigned char style,
                                                    x16_char_size *out);

/* Draw `c` at the position in *x and *y -- y is the BASELINE -- and
** advance both to the next character position. Returns 1 if it landed
** inside the window, 0 if it was clipped. Characters are ISO/ASCII,
** and control codes move the pen or restyle it (see CON_ATTR_* in
** x16/console.h).
*/
unsigned char x16_graph_put_char (unsigned int *x,
                                               unsigned int *y,
                                               unsigned char c);

/* pulls the implementation in with this header */
#pragma compile("graph.c")

