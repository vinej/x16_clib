/* =====================================================================
 * x16clib :: x16/tile.h -- tilemap cells and layer configuration
 * =====================================================================
 * The x16_tile_* routines address layer 1, which in the default text
 * modes is the text screen. They read the layer's config and mapbase
 * registers at run time rather than assuming a screen mode, so they keep
 * working across a call to x16_screen_set_mode().
 *
 * A text cell is two bytes: screen code, then colour attribute
 * (fg | bg<<4).
 * =====================================================================
 */

#ifndef X16_TILE_H
#define X16_TILE_H

/* Layer config byte: map height (7:6) | map width (5:4) | T256C (3) |
** bitmap (2) | bpp (1:0). */
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

/* One layer number at a time; the other layer is left alone. */
void x16_layer_on(__reg("a") unsigned char layer);
void x16_layer_off(__reg("a") unsigned char layer);

void x16_layer_set_config(__reg("r0") unsigned char layer, __reg("r2") unsigned char config);

/* mapbase is the VRAM address >> 9, so the map must be 512-aligned. */
void x16_layer_set_mapbase(__reg("r0") unsigned char layer, __reg("r2") unsigned char mapbase);

/* tilebase is (base >> 11) << 2, or'd with the tile size bits. */
void x16_layer_set_tilebase(__reg("r0") unsigned char layer, __reg("r2") unsigned char tilebase);

/* 12-bit hardware scroll, 0-4095. */
void x16_layer_scroll_x(__reg("r0") unsigned char layer, __reg("r2/r3") unsigned int value);
void x16_layer_scroll_y(__reg("r0") unsigned char layer, __reg("r2/r3") unsigned int value);

/* Point data port 0 at a layer-1 cell, on auto-increment. Leaves
** ADDRSEL = 0, so calling the KERNAL afterwards is safe. */
void x16_tile_setptr(__reg("r0") unsigned char col, __reg("r2") unsigned char row);

void x16_tile_put(__reg("r0") unsigned char col, __reg("r2") unsigned char row,
                  __reg("r4") unsigned char code, __reg("r6") unsigned char attr);

/* Returns code | attr<<8. Use X16_TILE_CODE / X16_TILE_ATTR. */
unsigned int x16_tile_get(__reg("r0") unsigned char col, __reg("r2") unsigned char row);

#endif /* X16_TILE_H */
