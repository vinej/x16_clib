/* =====================================================================
 * x16clib :: x16/tile.h -- tilemap cells and layer configuration
 * =====================================================================
 * The tile_* routines address layer 1, which in the default text modes
 * is the text screen. They read L1_CONFIG and L1_MAPBASE at run time
 * rather than assuming a screen mode, so they keep working after a mode
 * change.
 *
 * The KERNAL's default text setup is L1_CONFIG = 0x60 (map 128x64,
 * 1bpp) with MAPBASE = 0xD8, i.e. the map at $1B000. A cell is two
 * bytes: screen code, then colour attribute (fg | bg<<4).
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

#ifndef X16_TILE_H
#define X16_TILE_H

/* Layer config byte: map height (7:6) | map width (5:4) | T256C (3) |
** bitmap (2) | bpp (1:0).
*/
#define X16_LAYER_BPP_1         0
#define X16_LAYER_BPP_2         1
#define X16_LAYER_BPP_4         2
#define X16_LAYER_BPP_8         3
#define X16_LAYER_BITMAP        0x04    /* bitmap instead of tile mode */
#define X16_LAYER_T256C         0x08    /* 256-colour text */

#define X16_LAYER_MAPW_32       0x00
#define X16_LAYER_MAPW_64       0x10
#define X16_LAYER_MAPW_128      0x20
#define X16_LAYER_MAPW_256      0x30
#define X16_LAYER_MAPH_32       0x00
#define X16_LAYER_MAPH_64       0x40
#define X16_LAYER_MAPH_128      0x80
#define X16_LAYER_MAPH_256      0xC0

/* Unpack the result of x16_tile_get(). */
#define X16_TILE_CODE(v)        ((unsigned char)((v) & 0xFF))
#define X16_TILE_ATTR(v)        ((unsigned char)((v) >> 8))

/* These take one layer number (0 or 1) and leave the other layer
** alone.
*/
void x16_layer_on (unsigned char layer);
void x16_layer_off (unsigned char layer);

void x16_layer_set_config (unsigned char layer, unsigned char config);

/* mapbase is the VRAM address >> 9, so the map must be 512-aligned. */
void x16_layer_set_mapbase (unsigned char layer, unsigned char mapbase);

/* tilebase is (base >> 11) << 2, or'd with the tile size bits. */
void x16_layer_set_tilebase (unsigned char layer, unsigned char tilebase);

/* 12-bit hardware scroll, 0-4095. */
void x16_layer_scroll_x (unsigned char layer, unsigned int value);
void x16_layer_scroll_y (unsigned char layer, unsigned int value);

/* Point data port 0 at a layer-1 cell, on auto-increment. Leaves
** ADDRSEL = 0, so calling the KERNAL afterwards is safe.
*/
void x16_tile_setptr (unsigned char col, unsigned char row);

void x16_tile_put (unsigned char col, unsigned char row,
                   unsigned char code, unsigned char attr);

/* Returns code | attr<<8. Use X16_TILE_CODE / X16_TILE_ATTR. */
unsigned int x16_tile_get (unsigned char col, unsigned char row);

/* pulls the implementation in with this header */
#pragma compile("tile.c")

#endif /* X16_TILE_H */
