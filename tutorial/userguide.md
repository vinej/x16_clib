# x16clib User Guide

A function-by-function guide to **x16clib**, the C library for the
Commander X16: each function with its parameters and a small example.

For background, design notes and hardware pitfalls, see the
[README](../README.md). This guide is about *using* the API.

Every module in the library has a section here, and every entry point a
signature. The text is the headers' own: `include_ca65/x16/*.h` carries
each module's contract, and **the header is the authority** -- if this
guide and a header ever disagree, the header is right.

---

## Table of contents

1. [Getting started](#getting-started)
2. [`x16/vera.h` — VRAM data ports](#x16verah--vram-data-ports)
3. [`x16/screen.h` — screen mode, text, cursor](#x16screenh--screen-mode-text-cursor)
4. [`x16/palette.h` — the VERA palette](#x16paletteh--the-vera-palette)
5. [`x16/tile.h` — tilemap cells and layers](#x16tileh--tilemap-cells-and-layers)
6. [`x16/sprite.h` — hardware sprites](#x16spriteh--hardware-sprites)
7. [`x16/bitmap8l.h` — 320x240 bitmap drawing](#x16bitmap8lh--320x240-bitmap-drawing)
8. [`x16/verafx.h` — VERA FX acceleration](#x16verafxh--vera-fx-acceleration)
9. [`x16/psg.h` — the 16-voice PSG](#x16psgh--the-16-voice-psg)
10. [`x16/ym.h` — the YM2151 FM chip](#x16ymh--the-ym2151-fm-chip)
11. [`x16/pcm.h` — PCM audio and streaming](#x16pcmh--pcm-audio-and-streaming)
12. [`x16/adpcm.h` — IMA ADPCM decoding](#x16adpcmh--ima-adpcm-decoding)
13. [`x16/input.h` — joystick, mouse, keyboard](#x16inputh--joystick-mouse-keyboard)
14. [`x16/irq.h` — VSYNC, raster and collision interrupts](#x16irqh--vsync-raster-and-collision-interrupts)
15. [`x16/bank.h` — banked RAM](#x16bankh--banked-ram)
16. [`x16/mem.h` — KERNAL block operations](#x16memh--kernal-block-operations)
17. [`x16/load.h` — load and save](#x16loadh--load-and-save)
18. [`x16/dos.h` — the DOS command channel](#x16dosh--the-dos-command-channel)
19. [`x16/bmx.h` — BMX image files](#x16bmxh--bmx-image-files)
20. [`x16/zx0.h` — ZX0 decompression](#x16zx0h--zx0-decompression)
21. [`x16/fixed.h` — 8.8 fixed point](#x16fixedh--88-fixed-point)
22. [`x16/math.h` — game math](#x16mathh--game-math)
23. [`x16/collide.h` — bounding-box overlap](#x16collideh--bounding-box-overlap)
24. [`x16/clip.h` — line clipping](#x16cliph--line-clipping)
25. [`x16/buffers.h` — ring buffer and stack](#x16buffersh--ring-buffer-and-stack)
26. [`x16/float.h` — ROM floating point](#x16floath--rom-floating-point)
27. [`x16/bitmap2h.h` — 640x480x4 bitmap drawing (2bpp)](#x16bitmap2hh--640x480x4-bitmap-drawing-2bpp)
28. [`x16/bitmap2l.h` — 320x240x4 bitmap drawing (2bpp)](#x16bitmap2lh--320x240x4-bitmap-drawing-2bpp)
29. [`x16/bitmap4h.h` — VERA_2 640x480x16 SDRAM bitmap drawing](#x16bitmap4hh--vera_2-640x480x16-sdram-bitmap-drawing)
30. [`x16/bitmap4l.h` — 320x240x16 bitmap drawing (4bpp)](#x16bitmap4lh--320x240x16-bitmap-drawing-4bpp)
31. [`x16/bitmap8h.h` — VERA_2 640x480x256 SDRAM bitmap drawing](#x16bitmap8hh--vera_2-640x480x256-sdram-bitmap-drawing)
32. [`x16/shapes.h` — circle / disc / ellipse / flood / polygon / rounded rect / arc / pie / bezier, both modes](#x16shapesh--circle--disc--ellipse--flood--polygon--rounded-rect--arc--pie--bezier-both-modes)
33. [`x16/verafx_utils.h` — low-level VERA FX primitives](#x16verafx_utilsh--low-level-vera-fx-primitives)
34. [`x16/string.h` — NUL-terminated string toolkit](#x16stringh--nul-terminated-string-toolkit)
35. [`x16/sort.h` — in-place sorting of arrays](#x16sorth--in-place-sorting-of-arrays)
36. [`x16/bcd.h` — packed-BCD (decimal-mode) add and subtract](#x16bcdh--packed-bcd-decimal-mode-add-and-subtract)
37. [`x16/bits.h` — bit and nibble helpers](#x16bitsh--bit-and-nibble-helpers)
38. [`x16/number.h` — number formatting and parsing](#x16numberh--number-formatting-and-parsing)
39. [`x16/tscrunch.h` — TSCrunch decompression](#x16tscrunchh--tscrunch-decompression)
40. [`x16/fileio.h` — generic KERNAL file/channel I/O](#x16fileioh--generic-kernal-filechannel-io)
41. [`x16/iec.h` — low-level IEC / serial bus wrappers](#x16iech--low-level-iec--serial-bus-wrappers)
42. [`x16/dir.h` — reading a directory](#x16dirh--reading-a-directory)
43. [`x16/ringbuffer.h` — an 8 KB FIFO ring in a HIRAM bank](#x16ringbufferh--an-8-kb-fifo-ring-in-a-hiram-bank)
44. [`x16/stack.h` — an 8 KB LIFO stack in a HIRAM bank](#x16stackh--an-8-kb-lifo-stack-in-a-hiram-bank)
45. [`x16/keyboard.h` — keyboard buffer and layout helpers](#x16keyboardh--keyboard-buffer-and-layout-helpers)
46. [`x16/mouse.h` — the full KERNAL mouse surface](#x16mouseh--the-full-kernal-mouse-surface)
47. [`x16/clock.h` — the jiffy timer and the real-time clock](#x16clockh--the-jiffy-timer-and-the-real-time-clock)
48. [`x16/i2c.h` — the I2C bus: SMC, RTC, and friends](#x16i2ch--the-i2c-bus-smc-rtc-and-friends)
49. [`x16/console.h` — the KERNAL console API](#x16consoleh--the-kernal-console-api)
50. [`x16/graph.h` — the KERNAL GRAPH drawing API](#x16graphh--the-kernal-graph-drawing-api)
51. [`x16/fb.h` — the KERNAL framebuffer driver API](#x16fbh--the-kernal-framebuffer-driver-api)
52. [`x16/spi.h` — VERA SPI controller helpers](#x16spih--vera-spi-controller-helpers)
53. [`x16/serial.h` — the serial / WiFi card UARTs](#x16serialh--the-serial--wifi-card-uarts)
54. [`x16/zimodem.h` — ZiModem (ESP32 WiFi) over the serial card](#x16zimodemh--zimodem-esp32-wifi-over-the-serial-card)
55. [`x16/audiorom.h` — the AUDIO ROM bank's API, wrapped](#x16audioromh--the-audio-rom-banks-api-wrapped)
56. [`x16/wavfile.h` — parse a WAV/RIFF header](#x16wavfileh--parse-a-wavriff-header)
57. [`x16/zsm.h` — compact ZSM stream player](#x16zsmh--compact-zsm-stream-player)
58. [`x16/vdc.h` — VERA display composer helpers](#x16vdch--vera-display-composer-helpers)
59. [`x16/int16.h` — 16-bit integer arithmetic](#x16int16h--16-bit-integer-arithmetic)
60. [`x16/int32.h` — 32-bit integer arithmetic](#x16int32h--32-bit-integer-arithmetic)
61. [`x16/double.h` — 64-bit software floating point (binary64)](#x16doubleh--64-bit-software-floating-point-binary64)
62. [`x16/filepick.h` — a file browser on a panel](#x16filepickh--a-file-browser-on-a-panel)

---

## Getting started

### Compiling a program

With **cc65** (the prebuilt library ships in `dist_ca65\`):

```
cl65 -t cx16 -O -I include_ca65 -o PROG.PRG prog.c dist_ca65\x16c.lib
```

With **llvm-mos** (`-mreserve-zp=16` is required — the link fails
without it):

```
mos-cx16-clang -Os -mreserve-zp=16 -I include_llvm \
    -o PROG.PRG prog.c dist_llvm\libx16c.a
```

The API is identical under both toolchains; every example in this guide
builds unchanged under either. Include the umbrella header and you have
everything — the linker keeps only the modules you actually call:

```c
#include <x16/x16.h>

int main(void)
{
    x16_screen_cls();
    x16_screen_puts("HELLO FROM X16CLIB\r");
    return 0;
}
```

Run it in the emulator:

```
x16emu -prg PROG.PRG -run
```

### Three rules that apply everywhere

1. **`X16_INC_*` is not `VERA_INC_*`.** This library's increment
   constants are raw indices (0–15). cc65's `<cx16.h>` constants with
   the same names are pre-shifted (its `VERA_INC_1` is `0x10`). Pass
   `X16_INC_*` to `x16_*` functions and `VERA_INC_*` to cc65's;
   never mix them.

2. **Part of VRAM is write-only.** `$1F9C0–$1FFFF` — the PSG, the
   palette and the sprite attributes — reads back the last value *your
   program* wrote, not the hardware's state. Initialise before you
   read-modify-write (`x16_sprite_init_all()`, `x16_psg_init()`).

3. **The scratch block is not reentrant.** The library keeps 16
   zero-page bytes. An interrupt handler must not call an `x16_*`
   routine that uses them — unless it is a callback installed through
   `x16/irq.h`, whose wrapper saves and restores everything for you.

One diagnostic from the umbrella header:

```c
unsigned char x16_zp_base(void);
```

Returns the zero-page address the linker gave that scratch block.
Purely informational:

```c
printf("scratch block at $%02X\n", x16_zp_base());
```

---

## `x16/vera.h` — VRAM data ports

VERA has two data ports. Point one at a VRAM address with an
auto-increment, then stream bytes through it. For a *single* VRAM byte
use cc65's `vpeek()`/`vpoke()`; these routines are for runs.

**Increment constants** (`X16_INC_0` … `X16_INC_640`): how far the
address steps after each access. Beyond powers of two there are
row-sized steps: `X16_INC_40`, `X16_INC_80` (text rows), `X16_INC_320`
(a bitmap row). Or with `X16_DECR` to walk backwards.

**VRAM landmarks**: `X16_VRAM_BITMAP` (0x00000), `X16_VRAM_SPRITE_DATA`
(0x13000), `X16_VRAM_TEXT` (0x1B000), `X16_VRAM_CHARSET` (0x1F000),
`X16_VRAM_PSG` (0x1F9C0), `X16_VRAM_PALETTE` (0x1FA00),
`X16_VRAM_SPRITE_ATTR` (0x1FC00).

### `void x16_vera_addr0(unsigned char inc, unsigned long addr)`
### `void x16_vera_addr1(unsigned char inc, unsigned long addr)`

Point data port 0 (or 1) at a 17-bit VRAM address, with an increment.

- `inc` — an `X16_INC_*` constant, optionally `| X16_DECR`.
- `addr` — the VRAM address; only bit 16 of the high half matters.

```c
/* Step through the text tilemap two bytes at a time: screen codes
** only, skipping the colour attributes. */
x16_vera_addr0(X16_INC_2, X16_VRAM_TEXT);
```

### `void x16_vera_fill(unsigned char value, unsigned int count)`

Write `value` `count` times through port 0, which must already point at
the destination. The increment decides the shape: `X16_INC_1` fills a
run, `X16_INC_320` stripes down a bitmap column. Count 0 writes nothing.

```c
x16_vera_addr0(X16_INC_2, X16_VRAM_TEXT);
x16_vera_fill('*', 80);                 /* stars across the top row */
```

### `void x16_vera_copy(unsigned int count)`

Copy `count` bytes from port 0 (source) to port 1 (destination), each
walking at its own increment.

```c
/* Duplicate the top text row onto the second row. */
x16_vera_addr0(X16_INC_1, X16_VRAM_TEXT);
x16_vera_addr1(X16_INC_1, X16_VRAM_TEXT + 256);
x16_vera_copy(160);
```

Note: `x16_vera_copy()` leaves port 1 selected. Call a KERNAL screen
routine afterwards only through `x16/screen.h`, which resets ADDRSEL.

### `unsigned char x16_vera_has_fx(void)`

Returns 1 if the VERA firmware carries the FX register set (v0.3.1+,
emulator R44+), else 0. Call it before using anything in
`x16/verafx.h`.

```c
if (x16_vera_has_fx()) {
    x16_fx_clear(320u * 240u, X16_VRAM_BITMAP);
} else {
    x16_gfx8l_clear(0);                   /* software fallback */
}
```

---

## `x16/screen.h` — screen mode, text, cursor

Wrappers over the KERNAL's screen editor. Their extra value over raw
KERNAL calls: each one clears ADDRSEL first, so they stay safe after
`x16_vera_addr1()`/`x16_vera_copy()` have left port 1 selected.

**Mode constants**: `X16_MODE_80x60`, `X16_MODE_80x30`,
`X16_MODE_40x60`, `X16_MODE_40x30`, `X16_MODE_40x15`, `X16_MODE_20x30`,
`X16_MODE_20x15`, `X16_MODE_22x23`, `X16_MODE_64x50`, `X16_MODE_64x25`,
`X16_MODE_32x50`, `X16_MODE_32x25`, and `X16_MODE_320x240` (the bitmap
mode cc65's `videomode()` cannot reach).

### `unsigned char x16_screen_set_mode(unsigned char mode)`

Switch screen modes. Returns 1 on success, 0 if the mode is
unsupported.

```c
if (!x16_screen_set_mode(X16_MODE_40x30)) {
    x16_screen_puts("MODE NOT AVAILABLE\r");
}
```

### `unsigned char x16_screen_get_mode(void)`

The current mode, as one of the constants above.

```c
unsigned char saved = x16_screen_get_mode();
x16_screen_set_mode(X16_MODE_320x240);
/* ...draw... */
x16_screen_set_mode(saved);             /* put it back */
```

### `void x16_screen_reset(void)`

KERNAL `CINT`: back to the default text mode, default charset, cleared
screen. The heavy hammer.

```c
x16_screen_reset();                     /* undo everything visual */
```

### `void x16_screen_cls(void)`

Clear the screen and home the cursor.

```c
x16_screen_cls();
```

### `void x16_screen_chrout(unsigned char c)`

Print one PETSCII character — KERNAL `CHROUT` with ADDRSEL forced to 0
first. Control codes work (`'\r'` is newline).

```c
x16_screen_chrout('A');
x16_screen_chrout('\r');
```

### `void x16_screen_puts(const char *s)`

Print a NUL-terminated string. Truncated at 255 bytes.

```c
x16_screen_puts("SCORE: 1000\r");
```

### `void x16_screen_color(unsigned char fg, unsigned char bg)`

Colour of every character printed from now on. Both 0–15 (the standard
palette: 0 black, 1 white, 2 red, …; cc65's `COLOR_*` constants in
`<cx16.h>` name them).

```c
x16_screen_color(7, 0);                 /* yellow on black */
x16_screen_puts("WARNING\r");
```

### `void x16_screen_border(unsigned char color)`

The border colour, 0–15.

```c
x16_screen_border(2);                   /* red border */
```

### `void x16_screen_locate(unsigned char row, unsigned char col)`

Move the cursor. Note the order: row first, like the KERNAL, not x/y.

```c
x16_screen_locate(10, 5);               /* row 10, column 5 */
x16_screen_puts("HERE");
```

### `void x16_screen_get_cursor(unsigned char *row, unsigned char *col)`

Read the cursor position back.

```c
unsigned char r, c;
x16_screen_get_cursor(&r, &c);
x16_screen_locate(r + 1, c);            /* one row down, same column */
```

### `void x16_screen_charset(unsigned char charset)`

Select the character set: `X16_CHARSET_ISO`, `X16_CHARSET_PET_UPPER`
(upper case + graphics, the power-on default) or
`X16_CHARSET_PET_LOWER` (upper + lower case).

```c
x16_screen_charset(X16_CHARSET_PET_LOWER);
x16_screen_puts("Now with lower case\r");
```

### `void x16_screen_get_size (unsigned char *cols, unsigned char *rows)`

The LIVE text grid, after whatever x16_screen_set_mode() left behind
-- not the 80x60 default.

### `unsigned char x16_screen_scode (unsigned char petscii)`

PETSCII to screen code, for a caller building its own tile data.

### `void x16_screen_addr (unsigned char row, unsigned char col)`

 Direct text-map access
 CHROUT costs several hundred cycles a character once the editor's
 scroll checks, colour handling and cursor bookkeeping are paid for. A
 program that repaints a whole text screen -- a spreadsheet, a file
 browser, any full-screen TUI -- cannot afford that, so these write
 VERA's tile map itself:

     x16_screen_addr(row, col);
     x16_screen_blit("READY.", 6, X16_TEXT_COLOR(1, 6));

 The address auto-increments, so a whole line costs one set-up and two
 stores per column, and consecutive runs can be chained.

 The KERNAL is not involved and neither is its cursor: these do not
 scroll, do not wrap, and do not move the CHROUT cursor. Do not print
 past the end of a row.

 Text is PETSCII on the way in -- the same bytes you would give CHROUT
 -- and is folded to screen codes for you.


 The colour byte these take: foreground | background << 4, the same
 layout x16_screen_color() builds.

#define X16_TEXT_COLOR(fg, bg)  ((unsigned char)(((fg) & 0x0F) | ((bg) << 4)))

 Point VERA port 0 (or port 1) at a character cell.

### `void x16_screen_addr1 (unsigned char row, unsigned char col)`

### `void x16_screen_blit (const char *text, unsigned char count, unsigned char color)`

Write a run of characters, all one colour, at the current port-0
address. Count is 1-255.

### `void x16_screen_blitfill (unsigned char count, unsigned char color, unsigned char ch)`

The same, with one repeated character: the usual way to blank part of
a line.

### `void x16_screen_scroll (unsigned char top, unsigned char left, unsigned char height, unsigned char width, unsigned char distance, unsigned char down)`

Slide a rectangle of the text screen up (down = 0) or down (down = 1)
by `distance` rows.

Re-rendering a whole grid to scroll one line pays for every cell; for
a spreadsheet or a directory listing most of that cost is formatting
the contents, not drawing them. This moves the picture inside VRAM,
so only the row that appears has to be rendered.

The rows uncovered at the trailing edge keep their old contents -- you
draw what belongs there. Nothing happens when `distance` is 0, or when
it is large enough that nothing would survive; repaint in that case.

Vertical only: scrolling sideways would move a row onto itself.

---

## `x16/palette.h` — the VERA palette

256 entries of 12-bit colour, one 16-bit word each, format `0x0RGB`:
`0x0F00` pure red, `0x00F0` pure green, `0x000F` pure blue. Remember
the palette is in the write-only region — you can read back only what
you wrote yourself.

### `void x16_pal_set(unsigned char index, unsigned int color)`

Set one entry.

```c
x16_pal_set(1, 0x0F80);                 /* entry 1: orange */
```

### `void x16_pal_load(const unsigned int *src, unsigned char first, unsigned char count)`

Bulk-load `count` colours (1–128; 0 loads nothing) from RAM into
entries `first` … `first + count - 1`.

```c
static const unsigned int fire[4] = { 0x0000, 0x0800, 0x0F40, 0x0FF0 };
x16_pal_load(fire, 16, 4);              /* entries 16-19 */
```

---

## `x16/tile.h` — tilemap cells and layers

The `x16_tile_*` routines address **layer 1** — the text screen in the
default modes — and read the layer's registers at run time, so they
keep working after a mode change. A text cell is two bytes: screen
code, then colour attribute (`fg | bg << 4`).

**Config constants** for `x16_layer_set_config()`: colour depth
`X16_LAYER_BPP_1/2/4/8`, `X16_LAYER_BITMAP` (bitmap instead of tile
mode), `X16_LAYER_T256C` (256-colour text), map size
`X16_LAYER_MAPW_32/64/128/256` and `X16_LAYER_MAPH_32/64/128/256`.

### `void x16_layer_on(unsigned char layer)`
### `void x16_layer_off(unsigned char layer)`

Enable or disable one layer (0 or 1) without touching the other.

```c
x16_layer_off(0);                       /* hide the playfield…      */
x16_layer_on(1);                        /* …keep the text HUD shown */
```

### `void x16_layer_set_config(unsigned char layer, unsigned char config)`

Write the layer's config byte, assembled from the constants above.

```c
/* Layer 0: a 64x32 tilemap of 4bpp tiles. */
x16_layer_set_config(0, X16_LAYER_MAPW_64 | X16_LAYER_MAPH_32 |
                        X16_LAYER_BPP_4);
```

### `void x16_layer_set_mapbase(unsigned char layer, unsigned char mapbase)`

Where the layer's map lives. `mapbase` is the VRAM address `>> 9`, so
the map must be 512-byte aligned.

```c
x16_layer_set_mapbase(0, 0x10000UL >> 9);   /* map at VRAM $10000 */
```

### `void x16_layer_set_tilebase(unsigned char layer, unsigned char tilebase)`

Where the tile images live plus the tile size:
`(addr >> 11) << 2`, or'd with the size bits (bit 0: tile width 16,
bit 1: tile height 16; both clear means 8x8).

```c
x16_layer_set_tilebase(0, (0x14000UL >> 11) << 2);  /* 8x8 tiles at $14000 */
```

### `void x16_layer_scroll_x(unsigned char layer, unsigned int value)`
### `void x16_layer_scroll_y(unsigned char layer, unsigned int value)`

12-bit hardware scroll, 0–4095.

```c
unsigned int cam = 0;
for (;;) {
    x16_vsync_wait();
    x16_layer_scroll_x(0, cam++ & 0x0FFF);   /* scroll the playfield */
}
```

### `void x16_tile_setptr(unsigned char col, unsigned char row)`

Point data port 0 at a layer-1 cell, auto-incrementing, and leave
ADDRSEL at 0. Stream cells through `VERA.data0` afterwards.

```c
x16_tile_setptr(0, 5);                  /* start of row 5 */
```

### `void x16_tile_put(unsigned char col, unsigned char row, unsigned char code, unsigned char attr)`

Write one cell: screen code + colour attribute.

```c
x16_tile_put(10, 5, 0x01, 0x61);        /* 'A', white on blue */
```

### `unsigned int x16_tile_get(unsigned char col, unsigned char row)`

Read one cell back as `code | attr << 8`. Unpack with
`X16_TILE_CODE(v)` and `X16_TILE_ATTR(v)`.

```c
unsigned int v = x16_tile_get(10, 5);
if (X16_TILE_CODE(v) == 0x01) { /* there is an 'A' there */ }
```

---

## `x16/sprite.h` — hardware sprites

128 sprites, an 8-byte attribute record each. Two things to remember:
the attribute RAM is **write-only** (call `x16_sprite_init_all()` once
so read-modify-writes have a known shadow), and coordinates are in
**640x480 display space** whatever the screen mode.

**Constants**: colour depth `X16_SPRITE_4BPP` / `X16_SPRITE_8BPP`;
Z-depth `X16_SPRITE_Z_DISABLED` / `_BEHIND` / `_MIDDLE` / `_FRONT`;
flips `X16_SPRITE_HFLIP` / `X16_SPRITE_VFLIP`; sizes
`X16_SPRITE_SIZE_8/16/32/64`; and the `X16_SPRITE_ATTR_*` byte offsets
for `x16_sprite_setptr()`.

### `void x16_sprite_init_all(void)`

Zero all 128 records: every sprite disabled, and the write-only RAM
given a known shadow. Call once at startup.

```c
x16_sprite_init_all();
x16_sprites_on();
```

### `void x16_sprites_on(void)`
### `void x16_sprites_off(void)`

The sprite renderer as a whole.

```c
x16_sprites_off();                      /* hide everything at once */
```

### `void x16_sprite_pos(unsigned char sprite, unsigned int x, unsigned int y)`

Position sprite 0–127; `x` and `y` are 10-bit, in 640x480 space.

```c
x16_sprite_pos(0, 320, 240);            /* dead centre */
```

### `void x16_sprite_get_pos(unsigned char sprite, unsigned int *x, unsigned int *y)`

Read a position back (from the shadow — see the write-only note).

```c
unsigned int sx, sy;
x16_sprite_get_pos(0, &sx, &sy);
x16_sprite_pos(0, sx + 1, sy);          /* nudge right */
```

### `void x16_sprite_image(unsigned char sprite, unsigned char mode, unsigned long addr)`

Point a sprite at its pixel data in VRAM.

- `mode` — `X16_SPRITE_4BPP` or `X16_SPRITE_8BPP`.
- `addr` — must be 32-byte aligned; the low five bits are dropped.

```c
x16_sprite_image(0, X16_SPRITE_8BPP, 0x13000UL);
```

### `void x16_sprite_flags(unsigned char sprite, unsigned char flags)`

Write attribute byte 6 whole: collision mask (bits 7:4), Z-depth,
vflip, hflip. A sprite becomes visible when its Z-depth is non-zero.

```c
x16_sprite_flags(0, X16_SPRITE_Z_FRONT | X16_SPRITE_HFLIP);
```

### `void x16_sprite_z(unsigned char sprite, unsigned char z)`

Change only the Z-depth, preserving the rest of byte 6. This is a
read-modify-write, so it needs the shadow from `x16_sprite_init_all()`.

```c
x16_sprite_z(0, X16_SPRITE_Z_DISABLED); /* hide sprite 0, keep flags */
```

### `void x16_sprite_size(unsigned char sprite, unsigned char width, unsigned char height, unsigned char pal_offset)`

Size codes (`X16_SPRITE_SIZE_8/16/32/64`, independently per axis) and
the palette offset 0–15 (colour index = pixel value + 16*offset, for
4bpp images).

```c
x16_sprite_size(0, X16_SPRITE_SIZE_16, X16_SPRITE_SIZE_16, 0);
```

### `void x16_sprite_setptr(unsigned char sprite, unsigned char offset)`

Point data port 0 at one byte of a sprite's record
(`X16_SPRITE_ATTR_*`), auto-incrementing — for streaming several fields
through `VERA.data0` yourself.

```c
x16_sprite_setptr(0, X16_SPRITE_ATTR_X_L);  /* then write x lo, x hi… */
```

A complete minimal sprite:

```c
x16_sprite_init_all();
x16_sprite_image(0, X16_SPRITE_8BPP, X16_VRAM_SPRITE_DATA);
x16_sprite_size(0, X16_SPRITE_SIZE_16, X16_SPRITE_SIZE_16, 0);
x16_sprite_pos(0, 320, 240);
x16_sprite_flags(0, X16_SPRITE_Z_FRONT);
x16_sprites_on();
```

---

## `x16/bitmap8l.h` — 320x240 bitmap drawing

An 8bpp framebuffer at VRAM $00000: one byte per pixel, rows of
`X16_GFX8L_WIDTH` (320), `X16_GFX8L_HEIGHT` (240) rows. Only
`x16_gfx8l_pset()`, the circles and text clip; **lines, rects and frames
do not** — keep their arguments on screen, or pre-clip with
`x16/clip.h`.

### `unsigned char x16_gfx8l_init(void)`

Switch to 320x240@256c on layer 0 with 40x30 text on layer 1. Returns
1 on success. Everything below assumes this mode (though the drawing
routines only touch VRAM, so they also work on an off-screen buffer).

```c
if (!x16_gfx8l_init()) return 1;
```

### `void x16_gfx8l_clear(unsigned char color)`

Fill the whole framebuffer with one colour.

```c
x16_gfx8l_clear(0);                       /* black */
```

### `void x16_gfx8l_pset(unsigned int x, unsigned char y, unsigned char color)`

Plot one pixel. Clipped: off-screen coordinates are safely ignored.

```c
x16_gfx8l_pset(160, 120, 2);              /* red dot in the middle */
```

### `void x16_gfx8l_hline(unsigned int x, unsigned char y, unsigned int len, unsigned char color)`

Horizontal run of `len` pixels starting at (x, y). Unclipped.

```c
x16_gfx8l_hline(0, 120, 320, 1);          /* white line across */
```

### `void x16_gfx8l_vline(unsigned int x, unsigned char y, unsigned char len, unsigned char color)`

Vertical run. `len` is 1–255 (a 240-row screen never needs more).
Unclipped.

```c
x16_gfx8l_vline(160, 0, 240, 1);          /* white line down */
```

### `void x16_gfx8l_rect(unsigned int x, unsigned char y, unsigned int w, unsigned char h, unsigned char color)`

Filled rectangle. Unclipped.

```c
x16_gfx8l_rect(100, 80, 120, 80, 6);      /* filled blue box */
```

### `void x16_gfx8l_frame(unsigned int x, unsigned char y, unsigned int w, unsigned char h, unsigned char color)`

Rectangle outline, one pixel thick. Unclipped.

```c
x16_gfx8l_frame(99, 79, 122, 82, 1);      /* white border around it */
```

### `void x16_gfx8l_line(unsigned int x0, unsigned char y0, unsigned int x1, unsigned char y1, unsigned char color)`

Bresenham line, any direction. Unclipped — see `x16_clip_line()` for
endpoints that may leave the screen.

```c
x16_gfx8l_line(0, 0, 319, 239, 5);        /* green diagonal */
```

### `void x16_gfx8l_circle(unsigned int cx, unsigned char cy, unsigned char r, unsigned char color)`
### `void x16_gfx8l_disc(unsigned int cx, unsigned char cy, unsigned char r, unsigned char color)`

Circle outline and filled disc. Radius 0–120. These DO clip, at every
edge.

```c
x16_gfx8l_circle(160, 120, 60, 1);        /* ring */
x16_gfx8l_disc(160, 120, 20, 2);          /* solid red centre */
```

### `void x16_gfx8l_char(unsigned int x, unsigned char y, unsigned char color, unsigned char code)`

Draw one glyph from the KERNAL's charset at VRAM $1F000. Set bits
become `color`, clear bits stay transparent. `code` is a **screen
code**, not PETSCII. Clips.

```c
x16_gfx8l_char(8, 8, 1, 0x01);            /* screen code 1 = 'A' */
```

### `void x16_gfx8l_text(unsigned int x, unsigned char y, unsigned char color, const char *s)`

A NUL-terminated string, 8 pixels per character. ASCII letters convert
to screen codes for you, so plain strings read as expected. Clips.

```c
x16_gfx8l_text(100, 4, 1, "GAME OVER");
```

### `unsigned char x16_gfx8l_flood(unsigned int x, unsigned char y, unsigned char color)`

Scanline flood fill of the 4-connected region under the seed. Filling
with the colour already there is a no-op. Returns 1 when the region was
filled completely, 0 when the internal span stack (170 deep) overflowed
and the fill is incomplete — only pathological shapes (long thin
spirals) do that.

```c
x16_gfx8l_circle(160, 120, 60, 1);
x16_gfx8l_flood(160, 120, 3);             /* fill the inside cyan */
```

### `void x16_gfx8l_setptr (unsigned char inc, unsigned int x, unsigned char y)`

Point VERA data port 0 at (x,y) with the given increment index
(X16_INC_*). The packed planes hand back x's position within the
byte; at 8bpp a pixel IS a byte, so there is nothing to return.
Unclipped. The escape hatch for custom inner loops.

### `unsigned char x16_gfx8l_read (unsigned int x, unsigned char y)`

Unclipped from here down.

The colour at (x,y). Unlike the packed planes' read(), off-screen
coordinates are not rejected -- they read the wrapped address.

### `void x16_gfx8l_pattern_set (const unsigned char *pattern, unsigned char bg, unsigned char fg)`

Patterns and blits -- the same surface x16/bitmap2h.h has
Two-way parity between the engines: a program can move between
320x240x256 and 640x480x4 without losing a primitive. Two of these
differ from their 2bpp counterparts, and both differences come from
one byte being one pixel here rather than four.

Neither blit clips; keep them on screen.


An 8x8 1bpp pattern (8 row bytes, bit 7 leftmost) cached for
x16_gfx8l_pattern_rect(). Background and foreground are whole bytes --
the 2bpp version packs two 2-bit colours into one argument, which
8bpp colours do not fit in.

Patterns anchor to the screen origin, so adjacent fills always knit
together.

### `void x16_gfx8l_pattern_rect (unsigned int x, unsigned int y, unsigned int w, unsigned int h)`

### `void x16_gfx8l_blit (unsigned int x, unsigned int y, unsigned char w, unsigned char h, const unsigned char *src, unsigned char op)`

Copy a row-major image from RAM into the bitmap, one byte per pixel.
`w` is in PIXELS (1-255). op: 0 copy, 1 OR, 2 AND, 3 XOR.

### `void x16_gfx8l_blitm (unsigned int x, unsigned int y, unsigned char w, unsigned char h, const unsigned char *src)`

A masked blit: a source byte of 0 leaves the screen alone.

At 8bpp the mask IS the data -- colour 0 means transparent -- so the
source is plain row-major pixels, where x16_gfx2h_blitm() needs
interleaved (mask, data) pairs and pre-shifted columns.

---

## `x16/verafx.h` — VERA FX acceleration

Hardware multiply, 4x-speed fills and copies, hardware lines and filled
triangles. **Requires VERA firmware v0.3.1+** — check
`x16_vera_has_fx()` first; on older VERA these routines quietly do the
wrong thing. Every routine here leaves FX disabled and DCSEL at 0 on
the way out.

### `void x16_fx_off(void)`

Disable FX and restore DCSEL. Safe whether or not FX was ever enabled;
you rarely need it, since the other routines already clean up.

```c
x16_fx_off();                           /* belt and braces */
```

### `long x16_fx_mult(int a, int b)`

Signed 16x16 → 32 multiply in hardware. Far faster than
`x16_umul16()`, and signed — but it clobbers four bytes of VRAM scratch
at `X16_VRAM_FX_SCRATCH` ($1F800, an unused corner).

```c
long area = x16_fx_mult(dx, dy);
```

### `void x16_fx_fill(unsigned char value, unsigned int count, unsigned long addr)`

Fill `count` bytes of VRAM at `addr` with `value`, about four times
faster than a byte loop (the 32-bit cache writes four bytes per
access). A remainder count is finished one byte at a time.

```c
x16_fx_fill(2, 320u * 240u, X16_VRAM_BITMAP);   /* red screen, fast */
```

### `void x16_fx_clear(unsigned int count, unsigned long addr)`

`x16_fx_fill()` with value 0.

```c
x16_fx_clear(320u * 240u, X16_VRAM_BITMAP);     /* fast black */
```

### `void x16_fx_copy(unsigned long src, unsigned long dst, unsigned int count)`

VRAM-to-VRAM copy through the 32-bit cache, about 4x a byte loop.
`dst` must be **4-byte aligned**; `src` needs no alignment.

```c
/* Copy the top half of the framebuffer onto the bottom half. */
x16_fx_copy(X16_VRAM_BITMAP, X16_VRAM_BITMAP + 38400UL, 38400u);
```

### `void x16_fx_transp_on(void)`
### `void x16_fx_transp_off(void)`

Transparent VRAM writes: while on, a zero byte written to a data port
leaves the target alone, so colour 0 acts as transparency for blits.
Note that every *other* `x16_fx_*` routine turns this off again on its
way out — enable, do your writes, disable.

```c
x16_fx_transp_on();
x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP + 100);
x16_vera_fill(0, 8);                    /* writes nothing: 0 is transparent */
x16_fx_transp_off();
```

### `void x16_fx_line(unsigned int x0, unsigned char y0, unsigned int x1, unsigned char y1, unsigned char color)`

The same endpoints as `x16_gfx8l_line()`, but VERA carries the Bresenham
error: the CPU does one store per pixel. Assumes the `x16_gfx8l_init()`
framebuffer. Does **not** clip.

```c
x16_fx_line(0, 239, 319, 0, 7);         /* yellow diagonal, hardware */
```

### `void x16_fx_triangle(const x16_point *a, const x16_point *b, const x16_point *c, unsigned char color)`

Filled triangle; vertices in any order. `x16_point` is
`{ unsigned int x; unsigned char y; }` — **do not reorder its fields**,
they are copied byte-for-byte onto the assembly's operand block. The
rasterisation is half-open: the bottom row is not drawn, so two
triangles sharing an edge paint it once, not twice.

```c
x16_point a = { 160,  20 };
x16_point b = {  40, 200 };
x16_point c = { 280, 200 };
x16_fx_triangle(&a, &b, &c, 4);         /* purple triangle */
```

### `void x16_fx_affine_span (unsigned int count)`

Fetch `count` texels (>= 1) along the ray into VRAM: one port 1 read,
one port 0 write per texel. Aim the ray first, and point port 0 at
the destination yourself -- x16_vera_addr0(X16_INC_1, dest) -- with
whatever increment the destination wants.

### `void x16_fx_affine_ray (unsigned int x, unsigned int y, int dx, int dy)`

Aim the sampler: start at texel (x, y) -- 0-1023 each -- and step by
(dx, dy) per read. The steps are SIGNED, in 1/512-texel units, so 512
is one texel per read, 256 doubles the texture, 1024 halves it; bit
15 multiplies by 32, as for the line helper. Sampling starts at texel
centres (the subpixel part is seeded to 0.5).

---

## `x16/psg.h` — the 16-voice PSG

A voice is four bytes in VRAM: frequency word, pan|volume,
waveform|width. The PSG is in the write-only region, so call
`x16_psg_init()` before anything that read-modify-writes
(`x16_psg_note_off()`, the envelopes).

**Constants**: panning `X16_PSG_PAN_LEFT` / `_RIGHT` / `_BOTH` (neither
bit set is silence); waveforms `X16_PSG_WAVE_PULSE` / `_SAWTOOTH` /
`_TRIANGLE` / `_NOISE`.

**`X16_PSG_HZ(hz)`** converts a pitch in Hz to the frequency word
(`freq = hz * 2.68435…`). A literal folds at compile time; a variable
costs one 32-bit multiply. `X16_PSG_HZ(440)` is 1181.

### `void x16_psg_init(void)`

Silence all 16 voices and give the write-only region a known shadow.
Call first.

```c
x16_psg_init();
```

### `void x16_psg_set_freq(unsigned char voice, unsigned int freq)`

Set voice 0–15's frequency word. It writes the high byte first so the
pitch never passes through a garbage intermediate value.

```c
x16_psg_set_freq(0, X16_PSG_HZ(440));   /* A4 */
```

### `void x16_psg_set_vol(unsigned char voice, unsigned char vol, unsigned char pan)`

Volume 0–63 plus an `X16_PSG_PAN_*` constant.

```c
x16_psg_set_vol(0, 40, X16_PSG_PAN_BOTH);
```

### `void x16_psg_set_wave(unsigned char voice, unsigned char wave, unsigned char width)`

Waveform and width 0–63: the duty cycle for `PULSE`, an XOR amount for
the other waveforms.

```c
x16_psg_set_wave(0, X16_PSG_WAVE_PULSE, 32);    /* square wave */
```

A beep, complete:

```c
x16_psg_init();
x16_psg_set_freq(0, X16_PSG_HZ(440));
x16_psg_set_wave(0, X16_PSG_WAVE_PULSE, 32);
x16_psg_set_vol(0, 40, X16_PSG_PAN_BOTH);       /* sounds now */
```

### `void x16_psg_note_off(unsigned char voice)`

Volume to zero, panning kept. Safe from an ISR.

```c
x16_psg_note_off(0);
```

### `void x16_psg_voice_ptr(unsigned char voice, unsigned char offset)`

Point data port 0 at one register (offset 0–3) of a voice,
auto-incrementing — for driving the PSG registers yourself.

```c
x16_psg_voice_ptr(0, 0);                /* then stream 4 bytes to data0 */
```

### The ASR envelopes

The volume decay everybody hand-rolls in the frame loop. Attack ramps
up to `peak`, sustain holds, release ramps to silence. Set the voice's
frequency, wave and pan first; the envelope drives only the volume
bits.

### `void x16_psg_env_start(unsigned char voice, unsigned char peak, unsigned char attack, unsigned char sustain, unsigned char release)`

Arm an envelope.

- `peak` — target volume 0–63.
- `attack` — volume steps per frame going up; 0 jumps straight to peak.
- `sustain` — frames to hold; 255 holds until `x16_psg_env_release()`.
  0 releases at once.
- `release` — volume steps per frame going down; 0 holds at the peak
  until `x16_psg_env_stop()`.

### `void x16_psg_env_release(unsigned char voice)`

Enter the release phase now (the "key up" of a held note).

### `void x16_psg_env_stop(unsigned char voice)`

Silence and disarm immediately.

### `void x16_psg_env_tick(void)`

Advance every armed envelope one step. Call once per frame — from a
VSYNC callback is fine, the IRQ wrapper saves the zero page for you.

All four together:

```c
x16_psg_init();
x16_psg_set_freq(0, X16_PSG_HZ(880));
x16_psg_set_wave(0, X16_PSG_WAVE_PULSE, 32);
x16_psg_set_vol(0, 0, X16_PSG_PAN_BOTH);        /* pan only, vol 0 */
x16_psg_env_start(0, 60, 8, 20, 4);             /* ping */

for (;;) {
    x16_vsync_wait();
    x16_psg_env_tick();
    if (key_pressed)  x16_psg_env_release(0);   /* let it fade */
    if (panic_button) x16_psg_env_stop(0);      /* silence now */
}
```

---

## `x16/ym.h` — the YM2151 FM chip

Two ways in, and they do not mix freely: `x16_ym_write()` hits chip
registers directly (fast, complete access, but leaves the ROM driver's
volume/pan shadows stale); everything else goes through the ROM driver
and keeps those shadows coherent. Pick one style per program.

Note the argument order: `x16_ym_write()`/`x16_ym_poke()` take
`(register, value)`; the note API takes `(channel, ...)`.

**Constants**: panning `X16_YM_PAN_OFF/LEFT/RIGHT/BOTH`;
`X16_YM_NOTE(octave, semitone)` packs a note (octave 0–7, semitone
1–12 with 1 = C); `X16_YM_NOTE_RELEASE` (0) releases;
`X16_YM_RETRIGGER` / `X16_YM_HOLD` for the retrigger argument.

### `unsigned char x16_ym_init(void)`

Reset the chip and load the default instrument patches. Returns 0 if
the machine has no YM2151. Must precede the patch functions.

```c
if (!x16_ym_init()) { x16_screen_puts("NO FM CHIP\r"); return 1; }
```

### `unsigned char x16_ym_write(unsigned char reg, unsigned char value)`

Raw register write — the only route to the LFO and per-operator
envelopes. Returns 0 if the chip stayed busy.

```c
x16_ym_write(0x0F, 0x00);               /* noise off */
```

### `void x16_ym_poke(unsigned char reg, unsigned char value)`

The same write, through the ROM driver, keeping its shadows coherent.
Use this one if you also use the note API.

```c
x16_ym_poke(0x0F, 0x00);
```

### `unsigned char x16_ym_busy(void)`

1 while the chip is busy. Not an error — just wait.

```c
while (x16_ym_busy()) { }
```

### `unsigned char x16_ym_patch_rom(unsigned char channel, unsigned char patch)`

Load a ROM instrument (0–162) onto channel 0–7.

```c
x16_ym_patch_rom(0, 16);                /* an organ-ish preset */
```

### `unsigned char x16_ym_patch_ram(unsigned char channel, const void *patch)`

Load an instrument definition from RAM.

```c
extern const unsigned char my_patch[]; /* your own voice data */
x16_ym_patch_ram(1, my_patch);
```

### `unsigned char x16_ym_note_bas(unsigned char channel, unsigned char note, unsigned char retrigger)`

Play a packed note from `X16_YM_NOTE()`; 0 releases. This is the one
for playing tunes.

```c
x16_ym_note_bas(0, X16_YM_NOTE(4, 10), X16_YM_RETRIGGER);   /* A-4 */
```

### `void x16_ym_note(unsigned char channel, unsigned char kc, unsigned char kf, unsigned char retrigger)`

A raw YM2151 key code and key fraction — the key fraction is a pitch
bend, which `x16_ym_note_bas()` cannot express.

```c
x16_ym_note(0, 0x4A, 32, X16_YM_HOLD);  /* bend without retriggering */
```

### `void x16_ym_release_note(unsigned char channel)`

Key up: start the patch's release envelope.

```c
x16_ym_release_note(0);
```

### `unsigned char x16_ym_vol(unsigned char channel, unsigned char atten)`

Channel volume as attenuation: 0 is the patch's own volume, larger is
quieter.

```c
x16_ym_vol(0, 8);                       /* a bit quieter */
```

### `unsigned char x16_ym_pan(unsigned char channel, unsigned char pan)`

An `X16_YM_PAN_*` constant.

```c
x16_ym_pan(0, X16_YM_PAN_LEFT);
```

### `unsigned char x16_ym_drum(unsigned char channel, unsigned char note)`

Play a percussion note, 25–87, using the ROM's drum mapping.

```c
x16_ym_drum(7, 35);                     /* kick, roughly */
```

### `unsigned char x16_ym_get_pan(unsigned char channel)`
### `unsigned char x16_ym_get_vol(unsigned char channel)`

Read the ROM driver's shadows. They agree with the chip only if you
have written through `x16_ym_poke`/`_vol`/`_pan`, not raw
`x16_ym_write`.

```c
unsigned char v = x16_ym_get_vol(0);
x16_ym_vol(0, v + 4);                   /* fade one step */
```

---

## `x16/pcm.h` — PCM audio and streaming

VERA's PCM channel is a 4 KB FIFO. Samples are two's-complement
signed. **Startup order matters**: rate 0, prime the FIFO, then the
real rate — starting on an empty FIFO underruns at once.

**Constants** for `x16_pcm_ctrl()`: `X16_PCM_VOLUME(v)` (0–15),
`X16_PCM_STEREO`, `X16_PCM_16BIT`, `X16_PCM_RESET`.
`X16_PCM_RATE_MAX` (128) is full speed, 48828 Hz; rate 0 stops.

### `void x16_pcm_ctrl(unsigned char ctrl)`

Set format and volume in one control byte.

```c
x16_pcm_ctrl(X16_PCM_16BIT | X16_PCM_VOLUME(15));   /* 16-bit mono, max */
```

### `unsigned char x16_pcm_rate(unsigned char rate)`

Set the sample rate: 128 is 48828 Hz, so `rate = hz / 381.5` roughly;
64 is ~24.4 kHz. Returns the rate actually written (`rate` clamped to
128) — the register cannot be read back, so the return value is the
only way to see what landed.

```c
unsigned char r = x16_pcm_rate(200);    /* r == 128: it clamped */
```

### `void x16_pcm_reset(void)`

Clear the FIFO, keeping the current format and volume. Immediate
silence.

```c
x16_pcm_reset();
```

### `unsigned char x16_pcm_full(void)`
### `unsigned char x16_pcm_empty(void)`

FIFO status flags, 1 or 0.

```c
while (!x16_pcm_full()) x16_pcm_put(next_sample());
```

### `void x16_pcm_put(unsigned char sample)`

Push one sample byte. The hardware drops it if the FIFO is full. Safe
from an ISR.

```c
x16_pcm_put(0);                         /* one byte of silence */
```

### `void x16_pcm_write(const void *src, unsigned int count)`

Push a block, up to the FIFO's 4 KB. Does **not** throttle: bytes
written past a full FIFO are discarded. Meant for priming; pace longer
data with `x16_pcm_full()` or use the streamer.

```c
x16_pcm_rate(0);                        /* stopped */
x16_pcm_ctrl(X16_PCM_VOLUME(15));
x16_pcm_write(clip, sizeof clip);       /* prime */
x16_pcm_rate(64);                       /* ~24 kHz, plays now */
```

### The AFLOW streamer

Plays a buffer longer than 4 KB: VERA raises the AFLOW interrupt when
the FIFO drops below a quarter full and the handler refills it. The
FIFO is primed before the DAC starts, so playback cannot underrun at
t=0. Requires enabled interrupts. Note `x16_irq_remove()` stops a
stream — with the handler unhooked, nothing could acknowledge AFLOW.

### `void x16_pcm_stream_start(const void *data, unsigned int count, unsigned char rate)`

Start streaming `count` bytes from `data`. Set format and volume with
`x16_pcm_ctrl()` first. `rate` 0 primes without playing.

### `void x16_pcm_stream_stop(void)`

Stop refilling. What is already queued keeps playing; call
`x16_pcm_reset()` for immediate silence.

### `unsigned char x16_pcm_stream_active(void)`

1 while data remains to hand over. Reaches 0 once the last byte is in
the FIFO — which may still be playing.

```c
x16_pcm_ctrl(X16_PCM_VOLUME(15));
x16_pcm_stream_start(song, sizeof song, 64);
while (x16_pcm_stream_active()) {
    /* game keeps running; the interrupt does the refilling */
}
x16_pcm_stream_stop();
```

### `void x16_pcm_stream_start_bank (unsigned int offset, unsigned long count, unsigned char bank, unsigned char rate)`

The same, for a sample living in banked RAM -- which is where anything
longer than a sound effect has to live.

`offset` is 0-8191 within the bank window; `count` is a 24-bit byte
total, so a whole song spanning many banks is one call, and the top
byte of the long is ignored. `bank` is where the sample starts; the
refiller rolls $C000 back to $A000 and steps the bank as it goes, and
always restores the interrupted code's bank before returning.

---

## `x16/adpcm.h` — IMA ADPCM decoding

4:1 audio compression: 16-bit samples stored as 4-bit deltas, so a
second of 16-bit mono at 16 kHz is 8 KB instead of 32. This is the
canonical IMA/DVI algorithm from WAV files, low nibble first. Decoder
state carries across calls, so a long sample decodes a slice at a time.

### `void x16_adpcm_init(void)`

Reset the decoder: predictor 0, step index 0.

```c
x16_adpcm_init();
```

### `void x16_adpcm_set_state(int predictor, unsigned char index)`

An IMA WAV block header carries an initial predictor and step index —
set them before decoding that block's payload.

```c
x16_adpcm_set_state(hdr_predictor, hdr_index);
```

### `int x16_adpcm_predictor(void)`
### `unsigned char x16_adpcm_index(void)`

Read the state back — to checkpoint a stream you will resume later.

```c
int save_p = x16_adpcm_predictor();
unsigned char save_i = x16_adpcm_index();
```

### `int x16_adpcm_nibble(unsigned char code)`

Decode one 4-bit code to a signed 16-bit sample.

```c
int s = x16_adpcm_nibble(byte & 0x0F);  /* low nibble decodes first */
```

### `void x16_adpcm_block(const void *src, void *dst, unsigned int count)`

Decode `count` **source** bytes into signed 16-bit little-endian
samples: two samples — four bytes — out for every byte in, so `dst`
must hold `count * 4` bytes.

```c
x16_adpcm_init();
x16_adpcm_block(compressed, samples, sizeof compressed);
x16_pcm_ctrl(X16_PCM_16BIT | X16_PCM_VOLUME(15));
x16_pcm_stream_start(samples, sizeof compressed * 4, 64);
```

---

## `x16/input.h` — joystick, mouse, keyboard

Thin wrappers over the KERNAL: no driver to install, and the SNES pad's
full button set exposed.

**Joystick bits are active low** — a pressed button reads 0:
`X16_JOY_B/Y/SELECT/START/UP/DOWN/LEFT/RIGHT/A/X/L/R`.
**Mouse buttons** (active high, normal): `X16_MOUSE_LEFT/RIGHT/MIDDLE`.
**Keyboard unpackers**: `X16_KEY_CHAR(v)`, `X16_KEY_COUNT(v)`.

### `void x16_joy_scan(void)`

Sample every joystick. The KERNAL's IRQ already does this once a frame;
you only need it if you have taken the interrupt over.

```c
x16_joy_scan();                         /* only with the IRQ taken over */
```

### `unsigned int x16_joy_get(unsigned char joy, unsigned char *present)`

Read pad `joy`: 0 is the keyboard-as-joystick, 1–4 are gamepads.
Returns the button bits (active low); `*present` becomes 1 or 0.

```c
unsigned char present;
unsigned int b = x16_joy_get(1, &present);
if (present && !(b & X16_JOY_LEFT))  x--;   /* note the ! */
if (present && !(b & X16_JOY_RIGHT)) x++;
```

### `void x16_mouse_show(unsigned char cursor)`
### `void x16_mouse_hide(void)`

Show the mouse pointer — `0xFF` keeps the current cursor sprite, a
smaller number selects cursor sprite n — or hide it.

```c
x16_mouse_show(0xFF);
```

### `unsigned char x16_mouse_get(unsigned int *x, unsigned int *y)`

Returns the button mask (`X16_MOUSE_*`); writes the position through
the pointers.

```c
unsigned int mx, my;
if (x16_mouse_get(&mx, &my) & X16_MOUSE_LEFT) {
    x16_gfx8l_pset(mx >> 1, my >> 1, 1);  /* 640x480 -> 320x240 */
}
```

### `unsigned char x16_key_get(void)`

The next key as PETSCII, or 0 if nothing is waiting. Non-blocking.

```c
unsigned char c = x16_key_get();
if (c == 'Q') running = 0;
```

### `unsigned char x16_key_wait(void)`

Blocks until a key arrives.

```c
x16_screen_puts("PRESS ANY KEY\r");
x16_key_wait();
```

### `unsigned int x16_key_peek(void)`

The next key without consuming it, plus the queue depth, packed as
`key | queued << 8`. **When the queue is empty only the count is
meaningful** — always test it first:

```c
unsigned int p = x16_key_peek();
if (X16_KEY_COUNT(p) && X16_KEY_CHAR(p) == 'Y') {
    x16_key_get();                      /* now consume it */
}
```

---

## `x16/irq.h` — VSYNC, raster and collision interrupts

`x16_irq_install()` chains onto the KERNAL's IRQ vector — the KERNAL
still scans the keyboard and acknowledges VSYNC. The library unhooks
itself at exit even if you forget.

**Callbacks may be written in plain C.** Before calling one, the
library saves cc65's zero-page runtime and its own scratch (42 bytes,
~950 cycles, only when a callback is installed), so a callback may call
anything — C code, any `x16_*` routine. It must still stay short, and
save/restore any VERA state it touches (CTRL, and the address of any
data port it reprograms).

Handler types:

```c
typedef void (*x16_irq_handler)(void);
typedef void __fastcall__ (*x16_sprcol_handler)(unsigned char groups);
```

### `void x16_irq_install(void)`

Start counting frames. Idempotent; the raster and collision installers
call it for you.

```c
x16_irq_install();
```

### `void x16_irq_remove(void)`

Restore the previous handler and disable every source this library owns
— **including AFLOW, so a PCM stream in progress stops** (nothing else
could acknowledge it). Idempotent. Called automatically at program
exit.

```c
x16_irq_remove();
```

### `unsigned char x16_irq_frames(void)`

The frame counter, wrapping at 256. Byte subtraction wraps correctly,
so deltas stay valid across the wrap:

```c
unsigned char start = x16_irq_frames();
do_work();
elapsed = (unsigned char)(x16_irq_frames() - start);
```

### `void x16_vsync_wait(void)`

Block until the next frame boundary. Waits for the counter to *change*,
so it can neither miss a frame nor spin twice within one. Requires
enabled interrupts — under the emulator's headless `-testbench` (no
video, no VSYNC) it would hang.

```c
for (;;) {
    update();
    x16_vsync_wait();                   /* one iteration per frame */
    draw();
}
```

### `void x16_irq_line_install(unsigned int line, x16_irq_handler handler)`
### `void x16_irq_line_remove(void)`

Call `handler` when VERA's beam reaches `line` (0–511; the visible
display is 0–479), every frame. This is how a fixed status bar sits
over a scrolling playfield: change the display registers at the split
and change them back at VSYNC.

```c
void split(void)
{
    x16_layer_scroll_x(0, 0);           /* HUD region doesn't scroll */
}

x16_irq_line_install(400, split);
/* per frame, before line 400: x16_layer_scroll_x(0, cam); */
```

### `void x16_irq_sprcol_install(x16_sprcol_handler handler)`
### `void x16_irq_sprcol_remove(void)`

Hardware sprite collisions: VERA compares the collision masks of every
sprite pair once per frame (mask = top nibble of attribute byte 6, set
via `x16_sprite_flags()`). Two sprites collide when their masks share a
bit AND their rectangles overlap. `handler` may be `NULL` — the groups
still accumulate for polling.

### `unsigned char x16_sprite_collisions(void)`

Read **and clear** the collision groups seen since the last call; 0
means none. Atomic against the accumulating interrupt. Requires
`x16_irq_sprcol_install()` (a NULL handler is how you poll).

```c
x16_irq_sprcol_install(NULL);           /* poll mode */
for (;;) {
    x16_vsync_wait();
    if (x16_sprite_collisions()) {      /* something touched something */
        on_hit();
    }
}
```

---

## `x16/bank.h` — banked RAM

`RAM_BANK` selects which 8 KB bank appears at $A000–$BFFF
(`X16_BANK_SIZE` = 8192). Bank 0 belongs to the KERNAL; banks 1–255 are
yours. Offsets are 0–8191 into the window. Every routine here saves and
restores `RAM_BANK`, and the bulk copies auto-advance across bank
boundaries.

### `void x16_bank_set(unsigned char bank)`
### `unsigned char x16_bank_get(void)`

Select / read the current bank. Both safe from an ISR.

```c
unsigned char old = x16_bank_get();
x16_bank_set(3);
/* ...use BANK_RAM[] from <cx16.h> directly... */
x16_bank_set(old);
```

### `unsigned char x16_bank_peek(unsigned char bank, unsigned int offset)`
### `void x16_bank_poke(unsigned char bank, unsigned int offset, unsigned char value)`

One byte, any bank, without disturbing the current mapping.

```c
x16_bank_poke(3, 0, 42);
if (x16_bank_peek(3, 0) == 42) { /* round trip */ }
```

### `void x16_mem_to_bank(const void *src, unsigned char bank, unsigned int offset, unsigned int count)`
### `void x16_bank_to_mem(unsigned char bank, unsigned int offset, void *dst, unsigned int count)`

Bulk copies between low RAM and banked RAM. A run that reaches the end
of a bank continues at offset 0 of the next.

```c
char level[4096];
x16_mem_to_bank(level, 2, 0, sizeof level);     /* stash in bank 2  */
x16_bank_to_mem(2, 0, level, sizeof level);     /* and fetch back   */
```

### `void x16_bank_copy_far(unsigned char src_bank, unsigned int src_offset, unsigned char dst_bank, unsigned int dst_offset, unsigned int count)`

Banked RAM to banked RAM. Only one bank fits in the window at a time,
so it bounces through a 128-byte low-RAM buffer. Both sides
auto-advance across bank edges.

```c
x16_bank_copy_far(2, 0, 3, 0, X16_BANK_SIZE);   /* clone bank 2 -> 3 */
```

### The whole-bank allocator

Hands out bank *numbers* from a bitmap over banks 1–255; it never
touches `RAM_BANK` itself. Before `x16_bank_alloc_init()` nothing is
allocatable, so a forgotten init fails cleanly.

### `void x16_bank_alloc_init(unsigned char first, unsigned char last)`

Define the pool, both bounds inclusive, `first <= last`. Calling it
again resets the pool.

### `unsigned char x16_bank_alloc(void)`

The lowest free bank, or 0 when the pool is exhausted.

### `void x16_bank_free(unsigned char bank)`

Give a bank back. There is no ownership record: freeing a bank that was
never allocated quietly marks it allocatable — don't.

### `unsigned char x16_bank_reserve(unsigned char bank)`

Claim a specific bank. Returns 1 if it was free and is now yours, 0 if
already taken or outside the pool.

```c
x16_bank_alloc_init(1, 63);             /* a 512K machine, minus bank 0 */

unsigned char b = x16_bank_alloc();
if (b == 0) die("OUT OF BANKS");

x16_bank_reserve(63);                   /* keep 63 for the save file */
/* ... */
x16_bank_free(b);
```

---

## `x16/mem.h` — KERNAL block operations

Wrappers over the KERNAL's `MEMORY_FILL`, `MEMORY_COPY`, `MEMORY_CRC`
and `MEMORY_DECOMPRESS` — hand-unrolled loops, far faster than C.

**The special property**: addresses in $9F00–$9FFF are *not*
incremented during the operation. Pass `X16_VERA_DATA0` or
`X16_VERA_DATA1` as a source or target and the operation streams
through VERA's data port at whatever increment the port is set to —
which is how you fill, copy, CRC or **decompress straight into VRAM**.

### `void x16_mem_fill(void *dst, unsigned int count, unsigned char value)`

Set `count` bytes at `dst` to `value`. Count 0 fills nothing.

```c
char buf[512];
x16_mem_fill(buf, sizeof buf, 0);
```

### `void x16_mem_copy(const void *src, void *dst, unsigned int count)`

Copy `count` bytes; the regions may overlap.

```c
x16_mem_copy(buf, buf + 1, 511);        /* overlapping shift is fine */
```

### `unsigned int x16_mem_crc(const void *addr, unsigned int count)`

CRC-16/IBM-3740 of a block. An empty block returns the algorithm's
initial value, 0xFFFF.

```c
if (x16_mem_crc(save, sizeof save) != expected) { /* corrupt */ }
```

### `void *x16_mem_decompress(const void *src, void *dst)`

Decompress a raw LZSA2 block — **the depacker is in ROM, free**.
Returns one past the last output byte, so the unpacked length is the
return value minus `dst`. Cannot decompress in place. Compress assets
with:

```
lzsa -r -f2 original.bin compressed.lzsa
```

(the `-r` matters: a raw block, no frame header).

```c
/* Unpack a compressed tileset straight into video memory --
** no staging buffer, no second copy. */
x16_vera_addr0(X16_INC_1, X16_VRAM_SPRITE_DATA);
x16_mem_decompress(tiles_lzsa, X16_VERA_DATA0);
```

---

## `x16/load.h` — load and save

Device 8 (`X16_DEVICE_SD`) is the SD card. Filenames are
**(pointer, length)**, not NUL-terminated — pass `strlen(name)` or a
literal count.

The **secondary address** (`sa`) says how to treat the file's 2-byte
PRG header; it does not choose where the bytes land:

| Constant | Meaning |
|---|---|
| `X16_SA_ADDR` | skip the header, load at `dest` |
| `X16_SA_HEADER` | skip it, load where the header itself says |
| `X16_SA_RAW` | no header: load the whole file at `dest` |

### `unsigned char x16_fs_load(const char *name, unsigned char len, unsigned char device, unsigned char sa, void *dest, unsigned int *end)`

Load a file. Returns 0 on success, else the KERNAL error code. `*end`
receives the address one past the last byte loaded — pass `NULL` if you
don't care.

```c
char buf[8192];
unsigned int end;
if (x16_fs_load("LEVEL1.BIN", 10, X16_DEVICE_SD,
                X16_SA_RAW, buf, &end) == 0) {
    unsigned int size = end - (unsigned int)buf;
}
```

### `unsigned char x16_fs_save(const char *name, unsigned char len, unsigned char device, const void *start, const void *end)`

Write `[start, end)` — `end` exclusive — as a PRG with the usual 2-byte
header. Returns 0 on success, else the KERNAL error code.

```c
if (x16_fs_save("SCORES.DAT", 10, X16_DEVICE_SD,
                scores, scores + sizeof scores) != 0) {
    x16_dos_status();                   /* ask WHY -- see x16/dos.h */
}
```

### `unsigned char x16_fs_vload(const char *name, unsigned char len, unsigned char device, unsigned long vaddr)`

Load straight into VRAM at `vaddr`, skipping the PRG header. No cc65
equivalent exists. Returns 0 on success.

```c
x16_gfx8l_init();
x16_fs_vload("TITLE.BIN", 9, X16_DEVICE_SD, X16_VRAM_BITMAP);
```

### `void x16_fs_setname(const char *name, unsigned char len)`

KERNAL `SETNAM`, for driving `OPEN` and friends yourself via
`<cbm.h>`.

```c
x16_fs_setname("DATA,S,R", 8);
```

One trap worth knowing: KERNAL `OPEN` **succeeds for a missing file**;
the error only surfaces later. When a load or read misbehaves, ask the
DOS status (below) for the real reason.

### `unsigned int x16_fs_prg_entry (const char *name, unsigned char len, unsigned char device)`

The SYS address out of a PRG's BASIC stub, read WITHOUT loading it.

A launcher needs the entry address before it hands the machine over,
and loading the file to find out is the one thing it cannot do -- the
load would overwrite the launcher asking the question. This opens the
file, parses the stub where it lies, and closes it again.

Returns 0 if the file cannot be read or does not begin with a stub;
no PRG can start at 0, so that doubles as "no entry here".

---

## `x16/dos.h` — the DOS command channel

`x16_fs_load()`/`x16_fs_save()` report *that* they failed, never *why*.
The answer lives on channel 15: every command is answered with a status
line like `62,FILE NOT FOUND,00,00`. Codes below `X16_DOS_OK_BELOW`
(20) are success; `X16_DOS_NO_CHANNEL` (255) means the channel would
not open. The first status read after power-on returns 73, the DOS
version banner — by design.

### `void x16_dos_set_device(unsigned char device)`

Which drive the commands go to. Defaults to 8, the SD card.

```c
x16_dos_set_device(9);                  /* a second drive */
```

### `unsigned char x16_dos_status(void)`

Read the drive's pending status line; returns the numeric code.

### `const char *x16_dos_msg(void)`

The reply text of the last command, NUL-terminated. The next command
overwrites it — copy it if you need to keep it.

```c
if (x16_fs_save("A.DAT", 5, 8, buf, buf + 100) != 0) {
    x16_dos_status();
    printf("%s\n", x16_dos_msg());      /* e.g. "63,FILE EXISTS,00,00" */
}
```

### `unsigned char x16_dos_cmd(const char *cmd, unsigned char len)`

Send a raw DOS command and fetch the reply. Length 0 sends nothing and
just reads the pending status.

```c
x16_dos_cmd("CP2", 3);                  /* switch to partition 2 */
```

### `unsigned char x16_dos_delete(const char *name, unsigned char len)`
### `unsigned char x16_dos_mkdir(const char *name, unsigned char len)`
### `unsigned char x16_dos_rmdir(const char *name, unsigned char len)`
### `unsigned char x16_dos_chdir(const char *name, unsigned char len)`

One-call wrappers; each returns the status code. Filenames are
(pointer, length). For `chdir`, `"//"` is the root.

```c
x16_dos_mkdir("SAVES", 5);
x16_dos_chdir("SAVES", 5);
/* ...write save files here... */
x16_dos_chdir("//", 2);                 /* back to the root */
x16_dos_delete("OLD.DAT", 7);
x16_dos_rmdir("TEMP", 4);
```

### `unsigned char x16_dos_rename(const char *oldname, unsigned char oldlen, const char *newname, unsigned char newlen)`

Rename a file.

```c
x16_dos_rename("SCORES.DAT", 10, "SCORES.BAK", 10);
```

### `unsigned char x16_dos_lasterr (void)`

The status code the last x16_dos_* call came back with: 0-19 success,
20-99 error, X16_DOS_NO_CHANNEL (255) when the command channel would
not open. Every routine here already returns its code; this re-reads
the last one, for call sites that could not keep it.

A command abandoned locally for being too long returns 255 from the
call itself but sends nothing, so it does not update this.

---

## `x16/bmx.h` — BMX image files

BMX is the X16's native bitmap format — the one the community's tools
and Prog8 write. Version 1: a 16-byte header, the palette in VERA's own
layout, then the pixels.

Error codes: `X16_BMX_ERR_IO` (open/read/write failed — including a
file that simply is not there), `X16_BMX_ERR_FORMAT` (not a BMX, or not
version 1), `X16_BMX_ERR_PACKED` (compressed BMX not supported).

The image description (field order is load-bearing — do not reorder):

```c
typedef struct {
    unsigned int  width;
    unsigned int  height;
    unsigned char bpp;          /* 1, 2, 4 or 8 */
    unsigned char palstart;     /* first palette index */
    unsigned int  palcount;     /* 1-256 entries */
    unsigned char border;
    unsigned int  stride;       /* VRAM bytes between row starts */
} x16_bmx_info;
```

### `unsigned char x16_bmx_load(const char *name, unsigned char len, unsigned char device, unsigned long vaddr)`

Load a BMX: palette into the VERA palette, pixels into VRAM at `vaddr`.
Returns 0 on success, else an `X16_BMX_ERR_*` code. Rows land `stride`
bytes apart (320 by default, the full-screen stride) — so a 320-wide
image loads contiguously and a narrower one lands as a "stamp".

```c
x16_gfx8l_init();
if (x16_bmx_load("TITLE.BMX", 9, X16_DEVICE_SD, X16_VRAM_BITMAP)) {
    x16_dos_status();
    x16_screen_puts(x16_dos_msg());     /* the real reason */
}
```

### `void x16_bmx_get_info(x16_bmx_info *out)`

Read what the last load found (or what the next save will write).

```c
x16_bmx_info info;
x16_bmx_get_info(&info);
printf("%ux%u, %ubpp\n", info.width, info.height, info.bpp);
```

### `void x16_bmx_set_info(const x16_bmx_info *in)`

Describe the image before saving. `bpp` defaults to 8, `palcount` to
256, `stride` to 320.

### `unsigned char x16_bmx_save(const char *name, unsigned char len, unsigned char device, unsigned long vaddr)`

Write a BMX from VRAM. Describe the image first. Caveat: the palette
region is write-only, so the palette you save is only meaningful if
this program set those entries itself.

```c
x16_bmx_info shot = { 320, 240, 8, 0, 256, 0, 320 };
x16_bmx_set_info(&shot);
x16_bmx_save("SHOT.BMX", 8, X16_DEVICE_SD, X16_VRAM_BITMAP);
```

### `unsigned char x16_bmx_load_hires (const char *name, unsigned char device)`

Load a BMX into the VERA_2 640x480 SDRAM bitmap instead (the gfx8h
engine): the palette goes to the VERA_2 palette, the pixel rows land
640 bytes apart from SDRAM offset 0, so a full-width 640x480x8 image
is a plain contiguous load. Select the mode first (x16_gfx8h_init()).

REQUIRES THE VERA_2 LAYER -- feature-detect with x16_gfx8h_has() from
x16/bitmap8h.h first. On stock hardware (and the emulator) the file
still parses, but every pixel write goes to open bus.

Unlike x16_bmx_load() there is no length argument: `name` is an
ordinary NUL-terminated C string. Returns 0 on success, else an
X16_BMX_ERR_* code, and publishes the header fields either way.

### `unsigned char x16_bmx_lasterr (void)`

Why the last x16_bmx_* call failed: the X16_BMX_ERR_* it returned, or
0 after a call that worked. For call sites that could not keep the
return value.

---

## `x16/zx0.h` — ZX0 decompression

ZX0 packs tighter than the ROM's LZSA2, at the cost of carrying the
decoder in your program. RAM to RAM **only**: the match copier reads
the output back, so unlike `x16_mem_decompress()` it cannot write
through VERA's data port, and cannot decompress in place.

Compress with `salvador` (or `zx0` in its default mode — this decodes
the modern v2 stream, not `-classic`):

```
salvador data.bin data.zx0
```

### `void *x16_zx0_decompress(const void *src, void *dst)`

Returns one past the last output byte.

```c
char out[4096];
unsigned int n = (char *)x16_zx0_decompress(packed, out) - out;
```

---

## `x16/fixed.h` — 8.8 fixed point

C has no fixed-point type and a 6502 has no multiplier; these are the
two operations a sprite-mover needs. An 8.8 value is a signed 16-bit
int holding 256 times the real number: `0x0180` is 1.5, `0xFF00` is
-1.0.

**Macros**: `X16_FIX(whole, frac_256)` builds a constant;
`X16_FIX_WHOLE(v)` takes the pixel part (an arithmetic shift, rounding
toward negative infinity).

### `unsigned long x16_umul16(unsigned int a, unsigned int b)`

Unsigned 16x16 → 32 multiply.

```c
unsigned long bytes = x16_umul16(width, height);
```

### `int x16_mul88(int a, int b)`

Signed 8.8 multiply: `(a * b) >> 8`, staying in 8.8.

```c
int speed = X16_FIX(1, 128);            /* 1.5 */
int scaled = x16_mul88(speed, X16_FIX(2, 0));   /* 3.0 == 0x0300 */
```

The canonical use — sub-pixel movement (see `examples/bounce.c`):

```c
int x = X16_FIX(160, 0), vx = X16_FIX(0, 96);   /* 0.375 px/frame */
for (;;) {
    x16_vsync_wait();
    x += vx;
    x16_sprite_pos(0, X16_FIX_WHOLE(x) * 2, 240);
}
```

---

## `x16/math.h` — game math

**Angles are bytes**: a full circle is 256, so 64 is 90° and
wrap-around is free. Angle 0 is east (+x) and 64 is south (+y — the
screen's y axis points down). `x16_atan2()` and the sine tables agree
on that, so a heading feeds straight back into movement.

### `void x16_rnd_seed(unsigned int seed)`

Seed the PRNG. A seed of 0 is nudged to 1 (xorshift's fixed point).
Seed from the frame counter for a different sequence each run.

```c
x16_rnd_seed(x16_irq_frames() | 0x100);
```

### `unsigned char x16_rnd8(void)`
### `unsigned int x16_rnd16(void)`

John Metcalf's 16-bit xorshift: period 65535, a handful of cycles —
cheap enough per object per frame.

```c
unsigned char dice = (x16_rnd8() % 6) + 1;
unsigned int anywhere = x16_rnd16() % 320;
```

### `signed char x16_sin8(unsigned char angle)`
### `signed char x16_cos8(unsigned char angle)`

Sine and cosine scaled to -127…127.

```c
x += (x16_cos8(heading) * speed) >> 7;
y += (x16_sin8(heading) * speed) >> 7;
```

### `unsigned char x16_sin8u(unsigned char angle)`
### `unsigned char x16_cos8u(unsigned char angle)`

The same wave biased by 128, giving 1…255 — handy for volumes and
scales that must not go negative.

```c
x16_psg_set_vol(0, x16_sin8u(t) >> 2, X16_PSG_PAN_BOTH);  /* tremolo */
```

### `unsigned char x16_atan2(signed char dx, signed char dy)`

The angle of a vector, 0–255. `atan2(0, 0)` answers 0 (east).

```c
unsigned char aim = x16_atan2(tx - x, ty - y);  /* face the target */
```

### `unsigned char x16_lerp8(unsigned char a, unsigned char b, unsigned char t)`

Linear interpolation: `t = 0` gives exactly `a`, `t = 255` exactly `b`,
the midpoint at most one off.

```c
unsigned char fade = x16_lerp8(0, 63, t);       /* volume ramp */
```

---

## `x16/collide.h` — bounding-box overlap

Axis-aligned box tests. Edges that merely touch do **not** overlap: a
box at x=0 of width 10 and one at x=10 are adjacent, not colliding.

### `unsigned char x16_collide8(unsigned char ax, unsigned char ay, unsigned char aw, unsigned char ah, unsigned char bx, unsigned char by, unsigned char bw, unsigned char bh)`

Two boxes of unsigned bytes: position and size of A, then of B. Returns
1 on overlap. The edge sums are 9-bit, so a box may run past x=255 —
but a *coordinate* cannot, so this cannot describe the right half of a
640-wide display; use `x16_collide16()` there.

```c
if (x16_collide8(px, py, 16, 16, ex, ey, 16, 16)) {
    player_hit();
}
```

### `unsigned char x16_collide16(const x16_box16 *a, const x16_box16 *b)`

The 16-bit version, for anything in display space (640x480 in the
default modes — sprite coordinates are in those units). `x16_box16` is
`{ unsigned int x, y, w, h; }` — field order is load-bearing, do not
reorder.

```c
x16_box16 player = { 600, 100, 32, 32 };
x16_box16 pickup = { 610, 110,  16, 16 };
if (x16_collide16(&player, &pickup)) collect();
```

---

## `x16/clip.h` — line clipping

Cohen-Sutherland clipping for the line routines, which assume on-screen
endpoints. Coordinates are **signed** and may lie anywhere within
±4095. The clip rectangle is inclusive and defaults to the full 320x240
bitmap. Linking this module does not drag the bitmap module in.

The segment type (field order is load-bearing):

```c
typedef struct { int x0; int y0; int x1; int y1; } x16_line;
```

### `void x16_clip_set(unsigned int xmin, unsigned int ymin, unsigned int xmax, unsigned int ymax)`

Change the rectangle; all four bounds inclusive.

```c
x16_clip_set(0, 0, 319, 199);           /* keep the HUD rows clean */
```

### `unsigned char x16_clip_line(x16_line *seg)`

Clip `*seg` against the rectangle. Returns 1 if any of it is visible —
with `*seg` replaced by the visible part — or 0 if it lies entirely
outside (in which case `*seg` is unspecified).

```c
x16_line seg = { -50, 120, 400, 130 };  /* both ends off screen */
if (x16_clip_line(&seg)) {
    x16_gfx8l_line(seg.x0, seg.y0, seg.x1, seg.y1, 1);
}
```

---

## `x16/buffers.h` — ring buffer and stack

One static byte FIFO and one static byte LIFO, 256 bytes of storage
each, capacity 255. `get` and `pop` return -1 when empty, like
`getchar()`. **Not safe across an interrupt boundary** — if one side
runs in an ISR, bracket the other side's call in a critical section.

### `void x16_rb_init(void)`

Empty the ring buffer.

### `unsigned char x16_rb_put(unsigned char b)`

Append a byte. Returns 1 if stored, 0 if the buffer was full (the byte
is dropped).

### `int x16_rb_get(void)`

The oldest byte, or -1 when empty.

### `unsigned char x16_rb_count(void)`

How many bytes are queued.

```c
x16_rb_init();
x16_rb_put('A');
x16_rb_put('B');
while (x16_rb_count()) {
    int c = x16_rb_get();               /* 'A' first: FIFO */
    x16_screen_chrout((unsigned char)c);
}
```

### `void x16_stk_init(void)`

Empty the stack.

### `unsigned char x16_stk_push(unsigned char b)`

Push a byte. Returns 1 if pushed, 0 if the stack was full (255 deep).

### `int x16_stk_pop(void)`

The top byte, or -1 when empty.

### `unsigned char x16_stk_depth(void)`

How many bytes are stacked.

```c
x16_stk_init();
x16_stk_push(1);                        /* remembered game states */
x16_stk_push(2);
if (x16_stk_depth()) {
    int last = x16_stk_pop();           /* 2 first: LIFO */
}
```

---

## `x16/float.h` — ROM floating point

A binding to the complete floating-point library in the X16's ROM —
several thousand bytes cc65's own float support would otherwise link
in. Everything operates on **FAC**, an implicit accumulator, so code
reads as a sequence of operations, not expressions.

**Cost**: every call crosses a ROM bank. For hot per-frame arithmetic
use `x16_mul88()` instead; floats are for setup-time math.

**Types**: `x16_float` is a 5-byte in-memory float
(`X16_FP_SIZE` = 5); `X16_FP_STRLEN` (16) is enough for anything the
ROM formats.

### `void x16_f_zero(void)` / `void x16_f_neg(void)` / `void x16_f_abs(void)` / `void x16_f_int(void)`

FAC = 0, -FAC, |FAC|, floor(FAC).

```c
x16_f_from_s16(-3);
x16_f_abs();                            /* FAC = 3 */
```

### `signed char x16_f_sgn(void)`

-1 if FAC < 0, 0 if zero, 1 if positive.

```c
if (x16_f_sgn() < 0) { /* negative */ }
```

### `void x16_f_from_u8(unsigned char v)` / `void x16_f_from_s16(int v)`

Load FAC from an integer.

### `int x16_f_to_s16(void)`

FAC as a 16-bit int, rounding toward zero.

```c
x16_f_from_s16(7);
x16_f_sqrt();
int r = x16_f_to_s16();                 /* 2: sqrt(7) truncated */
```

### `void x16_f_load(const x16_float m)` / `void x16_f_store(x16_float m)`

Move FAC to and from a 5-byte float in memory. This is how you keep
more than one value.

```c
x16_float pi_ish;
x16_f_from_str("3.14159", 7);
x16_f_store(pi_ish);
```

### `void x16_f_add(const x16_float m)` / `x16_f_sub` / `x16_f_mul` / `x16_f_div` / `x16_f_pow`

FAC op= m, in the intuitive direction (`x16_f_sub(b)` is FAC - b).

```c
x16_float a, b;
x16_f_from_s16(10);  x16_f_store(a);
x16_f_from_s16(4);   x16_f_store(b);
x16_f_load(a);
x16_f_div(b);                           /* FAC = 2.5 */
```

### `void x16_f_rsub(const x16_float m)` / `x16_f_rdiv` / `x16_f_rpow`

FAC = m op FAC — the ROM's own operand order, one bank crossing
instead of three. `x16_f_rdiv()` is the reciprocal idiom:

```c
x16_float one;
x16_f_from_u8(1);  x16_f_store(one);
x16_f_from_s16(8);
x16_f_rdiv(one);                        /* FAC = 1/8 = 0.125 */
```

### `signed char x16_f_cmp(const x16_float m)`

-1 if FAC < m, 0 if equal, 1 if FAC > m.

```c
if (x16_f_cmp(limit) > 0) { /* over the limit */ }
```

### `void x16_f_sqrt(void)` / `x16_f_ln` / `x16_f_exp` / `x16_f_sin` / `x16_f_cos` / `x16_f_tan` / `x16_f_atan`

Each replaces FAC with the function of itself. Angles are radians.
sin, cos, tan and atan destroy ARG (the ROM's second accumulator), so
don't interleave them with an in-flight `rsub`/`rdiv`.

```c
x16_f_from_str("0.5", 3);
x16_f_sin();                            /* FAC = sin(0.5) */
```

### `void x16_f_to_str(char *buf)` / `void x16_f_to_str_trim(char *buf)`

Format FAC into `buf` (at least `X16_FP_STRLEN` bytes). Unlike the raw
ROM call — which writes into the stack page — the result is copied out
and is yours to keep. `_trim` drops the leading space BASIC prints
before a positive number.

```c
char buf[X16_FP_STRLEN];
x16_f_to_str_trim(buf);
x16_screen_puts(buf);
```

### `void x16_f_from_str(const char *s, unsigned char len)`

Parse a decimal string — (pointer, length), not NUL-terminated.

```c
x16_f_from_str("-12.5E2", 7);           /* FAC = -1250 */
```

### `void x16_f_mul (const x16_float m)`

### `void x16_f_pow (const x16_float m)`

### `void x16_f_rpow (const x16_float m)`

### `void x16_f_exp (void)`

### `void x16_f_ln (void)`

### `void x16_f_cos (void)`

### `void x16_f_tan (void)`

### `void x16_f_atan (void)`

---

## `x16/bitmap2h.h` — 640x480x4 bitmap drawing (2bpp)

The full 640x480 the VERA can drive: 2bpp, 4 pixels per byte packed
MSB-first, rows of 160 bytes at VRAM $00000 (76,800 bytes). A pixel
byte lives at y*160 + (x >> 2). Colours are 0-3 out of the first four
palette entries; x16_gfx2h_init() loads white / light gray / dark gray
/ black -- recolour with <x16/palette.h> without touching a pixel.

x16_gfx2h_pset() and x16_gfx2h_read() clip. The span, rect, line and
blit primitives do NOT: they assume their arguments are on screen
(the 8bpp module's policy, for the same reason).

x16_gfx2h_clear() fills through the VERA FX 32-bit cache: it needs an
FX-capable VERA (47.0.2+, probe with x16_vera_has_fx()).

```c
#define X16_GFX2H_WIDTH   640
#define X16_GFX2H_HEIGHT  480
#define X16_GFX2H_STRIDE  160
```

### `void x16_gfx2h_init (void)`

Program the mode on bare VERA registers (there is no KERNAL screen
mode for it): layer 0 bitmap 2bpp 640-wide at 1:1 scale, layer 1 off,
sprites left alone, palette 0-3 defaulted. Does NOT clear the pixels.

### `void x16_gfx2h_clear (unsigned char color)`

Full-screen fill with one colour, ~4x faster than a CPU loop.

### `unsigned char x16_gfx2h_setptr (unsigned char inc, unsigned int x, unsigned int y)`

Point VERA data port 0 at the byte holding (x,y) with the given
increment index (X16_INC_*); returns x & 3, the pixel's
position within that byte. The escape hatch for custom inner loops.

### `void x16_gfx2h_pset (unsigned int x, unsigned int y, unsigned char color)`

Clipped.

### `unsigned char x16_gfx2h_read (unsigned int x, unsigned int y)`

0-3, or $FF when (x,y) is off screen.

### `void x16_gfx2h_hline (unsigned int x, unsigned int y, unsigned int len, unsigned char color)`
### `void x16_gfx2h_vline (unsigned int x, unsigned int y, unsigned int len, unsigned char color)`
### `void x16_gfx2h_rect (unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char color)`
### `void x16_gfx2h_frame (unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char color)`

Unclipped from here down.

### `void x16_gfx2h_line (unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned char color)`

Bresenham, any direction; plots through the clipped pset, so lines
may safely leave the screen.

### `void x16_gfx2h_pattern_set (const unsigned char *pattern, unsigned char colors)`
### `void x16_gfx2h_pattern_rect (unsigned int x, unsigned int y, unsigned int w, unsigned int h)`

An 8x8 1bpp pattern (8 row bytes, bit 7 leftmost) expanded once and
cached; colors = (background << 2) | foreground. Patterns anchor to
the screen origin, so adjacent fills always knit together.

### `void x16_gfx2h_blit (unsigned int x, unsigned int y, unsigned char wbytes, unsigned char h, const unsigned char *src, unsigned char op)`

Copy a byte-aligned image (row-major, wbytes per row = 4-pixel units;
x bits 1:0 are ignored) from RAM into the bitmap. op: 0 copy, 1 OR,
2 AND, 3 XOR.

### `void x16_gfx2h_blitm (unsigned int x, unsigned int y, unsigned char h, unsigned char cols, const unsigned char *src)`

Masked blit of pre-shifted column-major data at ANY x: for each of
`cols` framebuffer byte columns, `h` (mask, data) byte pairs walking
down the rows -- fb = (fb & mask) | data. The caller supplies data
already shifted for x & 3; pre-shifted glyph caches are what make
proportional text affordable (~160 masked 8x8 glyphs per frame).
h is 1-127.

---

## `x16/bitmap2l.h` — 320x240x4 bitmap drawing (2bpp)

The framebuffer is 2bpp at VRAM $00000: 4 pixels per byte packed
MSB-first, rows of 80 bytes, 19,200 bytes in all. Colours are 0-3;
x16_gfx2l_init() loads a paper-and-ink default into palette entries
0-3 (0 white, 1 light gray, 2 dark gray, 3 black) and programs the
mode on bare VERA registers -- no KERNAL screen mode exists for it.
x16_pal_set()/x16_pal_load() re-colour without touching pixels.

x16_gfx2l_pset() and x16_gfx2l_read() clip. The span, rect, line and
blit primitives do NOT: they assume their arguments are on screen.

x16_gfx2l_clear() runs through the VERA FX 32-bit cache write and
needs an FX-capable VERA (47.0.2+); everything else is stock VERA.

```c
#define X16_GFX2L_WIDTH   320
#define X16_GFX2L_HEIGHT  240
```

### `void x16_gfx2l_init (void)`

320x240@2bpp on layer 0; layer 1 (text) is disabled. The
framebuffer is NOT cleared -- call x16_gfx2l_clear().

### `void x16_gfx2l_clear (unsigned char color)`

Fill the whole framebuffer with one colour (0-3).

### `unsigned char x16_gfx2l_setptr (unsigned char inc, unsigned int x, unsigned int y)`

Point VERA data port 0 at the byte holding pixel (x,y) with the
given increment index (X16_INC_*). Returns x & 3, the pixel's
position within the byte (0 = leftmost, bits 7:6).

### `void x16_gfx2l_pset (unsigned int x, unsigned int y, unsigned char color)`

Set one pixel. Clips.

### `unsigned char x16_gfx2l_read (unsigned int x, unsigned int y)`

Read one pixel: 0-3, or $FF if (x,y) is off screen.

### `void x16_gfx2l_hline (unsigned int x, unsigned int y, unsigned int len, unsigned char color)`

Horizontal span of len pixels. No clipping.

### `void x16_gfx2l_vline (unsigned int x, unsigned int y, unsigned int len, unsigned char color)`

Vertical span of len pixels. No clipping.

### `void x16_gfx2l_rect (unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char color)`

Filled rectangle. No clipping.

### `void x16_gfx2l_frame (unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char color)`

Rectangle outline. No clipping.

### `void x16_gfx2l_line (unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned char color)`

Bresenham line, any direction. Plots through the clipped pset, so
the line clips at the screen edges.

### `void x16_gfx2l_pattern_set (const unsigned char *pattern, unsigned char colors)`

Cache an 8x8 1bpp pattern (8 row bytes, top first, bit 7 leftmost)
for x16_gfx2l_pattern_rect(). colors = (background << 2) | foreground,
each 0-3. Patterns tile from the screen origin.

### `void x16_gfx2l_pattern_rect (unsigned int x, unsigned int y, unsigned int w, unsigned int h)`

Fill a rectangle with the cached pattern. No clipping.

### `void x16_gfx2l_blit (unsigned int x, unsigned int y, unsigned char wbytes, unsigned char h, const unsigned char *src, unsigned char op)`

Copy a byte-aligned image from RAM into the bitmap. x bits 1:0 are
ignored (byte-aligned); wbytes is the width in BYTES (4-pixel
units); src is row-major. op: 0 copy, 1 OR, 2 AND, 3 XOR.
No clipping.

### `void x16_gfx2l_blitm (unsigned int x, unsigned int y, unsigned char h, unsigned char cols, const unsigned char *src)`

Masked blit of pre-shifted column-major data at any pixel position:
for each of `cols` framebuffer columns, `h` (mask, data) byte PAIRS
walking down the rows; fb' = (fb AND mask) OR data. The caller
supplies data already shifted for x & 3. No clipping.

---

## `x16/bitmap4h.h` — VERA_2 640x480x16 SDRAM bitmap drawing

Requires the MiSTer VERA_2 bitmap layer: the framebuffer is NOT VERA
VRAM but the VERA_2 20-bit SDRAM byte address space behind $9F60-
$9F6F. Feature-detect with x16_gfx4h_has() before relying on it --
on stock hardware (and the emulator) every routine here writes into
open bus.

The framebuffer is 4bpp, two pixels per byte, rows of 320 bytes:
offset = y*320 + (x>>1), 153,600 bytes in all. The VERA_2 layer has its
own 16-entry palette ($9F66-$9F68), separate from VERA's.

x16_gfx4h_pset() and x16_gfx4h_read() clip. The span, rect, line and
blit primitives do NOT: they assume their arguments are on screen.

```c
#define X16_GFX4H_WIDTH   640
#define X16_GFX4H_HEIGHT  480
```

### `unsigned char x16_gfx4h_has (void)`

 VERA_2 SDRAM stride indices for x16_gfx4h_setptr().
#ifndef X16_INC2_1
#define X16_INC2_1      0x0
#define X16_INC2_0      0x1
#define X16_INC2_2      0x2
#define X16_INC2_4      0x3
#define X16_INC2_8      0x4
#define X16_INC2_16     0x5
#define X16_INC2_32     0x6
#define X16_INC2_64     0x7
#define X16_INC2_128    0x8
#define X16_INC2_256    0x9
#define X16_INC2_320    0xA
#define X16_INC2_640    0xB
#define X16_INC2_NEG1   0xC
#define X16_INC2_NEG2   0xD
#define X16_INC2_NEG320 0xE
#define X16_INC2_NEG640 0xF
#endif

 1 if the VERA_2 bitmap layer answers (ID reads back $B5), else 0.

### `void x16_gfx4h_init (void)`

Enable the layer at 640x480@4bpp and load a 16-entry gray palette.

### `void x16_gfx4h_off (void)`

Disable the VERA_2 bitmap layer.

### `void x16_gfx4h_passthru_on (void)`
### `void x16_gfx4h_passthru_off (void)`

Pass the stock VERA picture through / composite the layer again.

### `void x16_gfx4h_pal_gray (void)`

Load the 16-entry grayscale ramp (what init uses).

### `void x16_gfx4h_pal_set (unsigned char index, unsigned char lo, unsigned char hi)`

Set one VERA_2 palette entry: lo = (G << 4) | B, hi = R.

### `void x16_gfx4h_pal_load (const unsigned char *src, unsigned char first, unsigned char count)`

Load count entries from src (lo, hi byte pairs) starting at first.
count 0 loads nothing.

### `void x16_gfx4h_setptr (unsigned char inc, unsigned int x, unsigned int y)`

Point the VERA_2 DATA port at pixel (x,y) with an X16_INC2_* stride.

### `void x16_gfx4h_clear (unsigned char color)`

Fill the whole framebuffer with one colour.

### `void x16_gfx4h_pset (unsigned int x, unsigned int y, unsigned char color)`

Set one pixel. Clips.

### `unsigned char x16_gfx4h_read (unsigned int x, unsigned int y)`

Read one pixel: 0-15, or $FF if (x,y) is off screen.

### `void x16_gfx4h_hline (unsigned int x, unsigned int y, unsigned int len, unsigned char color)`
### `void x16_gfx4h_vline (unsigned int x, unsigned int y, unsigned int len, unsigned char color)`

Spans. No clipping.

### `void x16_gfx4h_rect (unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char color)`
### `void x16_gfx4h_frame (unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char color)`

Rectangles. No clipping.

### `void x16_gfx4h_line (unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned char color)`

Bresenham line, any direction; clips through pset.

### `void x16_gfx4h_pattern_set (const unsigned char *pattern, unsigned char bg, unsigned char fg)`

Cache an 8x8 1bpp pattern for x16_gfx4h_pattern_rect().

### `void x16_gfx4h_pattern_rect (unsigned int x, unsigned int y, unsigned int w, unsigned int h)`

Fill a rectangle with the cached pattern. No clipping.

### `void x16_gfx4h_blit (unsigned int x, unsigned int y, unsigned char w, unsigned char h, const unsigned char *src, unsigned char op)`

Copy rows of pixels from RAM (row-major, one byte per pixel PAIR;
the row is (w+1)/2 bytes). w is 1-255 pixels. op: 0 copy, 1 OR,
2 AND, 3 XOR. No clipping.

### `void x16_gfx4h_blitm (unsigned int x, unsigned int y, unsigned char w, unsigned char h, const unsigned char *src)`

Masked blit: colour 0 is transparent. Same layout as blit.

### `void x16_gfx4h_copy (unsigned long src, unsigned long dst, unsigned long len)`

VERA_2 hardware SDRAM-to-SDRAM copy of len bytes (20-bit byte
addresses, stride +1), then wait for the blitter to finish.

### `void x16_gfx4h_copy_wait (void)`

Wait for a previous copy to finish (copy already waits).

---

## `x16/bitmap4l.h` — 320x240x16 bitmap drawing (4bpp)

The framebuffer is 4bpp at VRAM $00000: 2 pixels per byte packed
MSB-first, rows of 160 bytes, 38,400 bytes in all. Colours are 0-15;
x16_gfx4l_init() loads a default 16-entry palette and programs the
mode on bare VERA registers -- no KERNAL screen mode exists for it.
x16_pal_set()/x16_pal_load() re-colour without touching pixels.

x16_gfx4l_pset() and x16_gfx4l_read() clip. The span, rect, line and
blit primitives do NOT: they assume their arguments are on screen.

```c
#define X16_GFX4L_WIDTH   320
#define X16_GFX4L_HEIGHT  240
```

### `void x16_gfx4l_init (void)`

320x240@4bpp on layer 0; layer 1 (text) is disabled. The
framebuffer is NOT cleared -- call x16_gfx4l_clear().

### `void x16_gfx4l_clear (unsigned char color)`

Fill the whole framebuffer with one colour (0-15).

### `void x16_gfx4l_setptr (unsigned char inc, unsigned int x, unsigned char y)`

Point VERA data port 0 at the byte holding pixel (x,y) with the
given increment index (X16_INC_*). The left pixel of the byte is
the high nibble.

### `void x16_gfx4l_pset (unsigned int x, unsigned char y, unsigned char color)`

Set one pixel. Clips.

### `unsigned char x16_gfx4l_read (unsigned int x, unsigned char y)`

Read one pixel: 0-15, or $FF if (x,y) is off screen.

### `void x16_gfx4l_hline (unsigned int x, unsigned char y, unsigned int len, unsigned char color)`

Horizontal span of len pixels. No clipping.

### `void x16_gfx4l_vline (unsigned int x, unsigned char y, unsigned char len, unsigned char color)`

Vertical span of len pixels (1-255). No clipping.

### `void x16_gfx4l_rect (unsigned int x, unsigned char y, unsigned int w, unsigned char h, unsigned char color)`

Filled rectangle. No clipping.

### `void x16_gfx4l_frame (unsigned int x, unsigned char y, unsigned int w, unsigned char h, unsigned char color)`

Rectangle outline. No clipping.

### `void x16_gfx4l_line (unsigned int x0, unsigned char y0, unsigned int x1, unsigned char y1, unsigned char color)`

Bresenham line, any direction. Plots through the clipped pset, so
the line clips at the screen edges.

### `void x16_gfx4l_char (unsigned int x, unsigned char y, unsigned char color, unsigned char code)`

Draw one 8x8 glyph from the VERA charset. code is a SCREEN code,
not PETSCII. Background pixels are left alone. Clips through pset.

### `void x16_gfx4l_text (unsigned int x, unsigned char y, unsigned char color, const char *s)`

Draw a NUL-terminated PETSCII string, 8 pixels per glyph.

### `void x16_gfx4l_pattern_set (const unsigned char *pattern, unsigned char bg, unsigned char fg)`

Cache an 8x8 1bpp pattern (8 row bytes, top first, bit 7 leftmost)
for x16_gfx4l_pattern_rect(). Patterns tile from the screen origin.

### `void x16_gfx4l_pattern_rect (unsigned int x, unsigned char y, unsigned int w, unsigned char h)`

Fill a rectangle with the cached pattern. No clipping.

### `void x16_gfx4l_blit (unsigned int x, unsigned char y, unsigned char w, unsigned char h, const unsigned char *src, unsigned char op)`

Copy rows of pixels from RAM into the bitmap. w is in PIXELS
(1-255); src is row-major, one byte per pixel PAIR (the row is
(w+1)/2 bytes). op: 0 copy, 1 OR, 2 AND, 3 XOR. No clipping.

### `void x16_gfx4l_blitm (unsigned int x, unsigned char y, unsigned char w, unsigned char h, const unsigned char *src)`

Masked blit: colour 0 is transparent. Same layout as blit.

---

## `x16/bitmap8h.h` — VERA_2 640x480x256 SDRAM bitmap drawing

Requires the MiSTer VERA_2 bitmap layer: the framebuffer is NOT VERA
VRAM but the VERA_2 20-bit SDRAM byte address space behind $9F60-
$9F6F. Feature-detect with x16_gfx8h_has() before relying on it --
on stock hardware (and the emulator) every routine here writes into
open bus.

The framebuffer is 8bpp, one byte per pixel, rows of 640 bytes:
offset = y*640 + x, 307,200 bytes in all. The VERA_2 layer has its
own 256-entry palette ($9F66-$9F68), separate from VERA's.

x16_gfx8h_pset() and x16_gfx8h_read() clip. The span, rect, line and
blit primitives do NOT: they assume their arguments are on screen.

```c
#define X16_GFX8H_WIDTH   640
#define X16_GFX8H_HEIGHT  480
```

### `unsigned char x16_gfx8h_has (void)`

 VERA_2 SDRAM stride indices for x16_gfx8h_setptr().
#ifndef X16_INC2_1
#define X16_INC2_1      0x0
#define X16_INC2_0      0x1
#define X16_INC2_2      0x2
#define X16_INC2_4      0x3
#define X16_INC2_8      0x4
#define X16_INC2_16     0x5
#define X16_INC2_32     0x6
#define X16_INC2_64     0x7
#define X16_INC2_128    0x8
#define X16_INC2_256    0x9
#define X16_INC2_320    0xA
#define X16_INC2_640    0xB
#define X16_INC2_NEG1   0xC
#define X16_INC2_NEG2   0xD
#define X16_INC2_NEG320 0xE
#define X16_INC2_NEG640 0xF
#endif

 1 if the VERA_2 bitmap layer answers (ID reads back $B5), else 0.

### `void x16_gfx8h_init (void)`

Enable the layer at 640x480@8bpp and load a grayscale palette.

### `void x16_gfx8h_off (void)`

Disable the VERA_2 bitmap layer.

### `void x16_gfx8h_passthru_on (void)`
### `void x16_gfx8h_passthru_off (void)`

Pass the stock VERA picture through / composite the layer again.

### `void x16_gfx8h_pal_gray (void)`

Load the 256-entry grayscale ramp (what init uses).

### `void x16_gfx8h_pal_set (unsigned char index, unsigned char lo, unsigned char hi)`

Set one VERA_2 palette entry: lo = (G << 4) | B, hi = R.

### `void x16_gfx8h_pal_load (const unsigned char *src, unsigned char first, unsigned char count)`

Load count entries from src (lo, hi byte pairs) starting at first.
count 0 loads nothing.

### `void x16_gfx8h_setptr (unsigned char inc, unsigned int x, unsigned int y)`

Point the VERA_2 DATA port at pixel (x,y) with an X16_INC2_* stride.

### `void x16_gfx8h_clear (unsigned char color)`

Fill the whole framebuffer with one colour.

### `void x16_gfx8h_pset (unsigned int x, unsigned int y, unsigned char color)`

Set one pixel. Clips.

### `unsigned int x16_gfx8h_read (unsigned int x, unsigned int y)`

Read one pixel: 0-255, or 0xFFFF if (x,y) is off screen (every
8-bit value is a valid colour, so the sentinel needs the high byte).

### `void x16_gfx8h_hline (unsigned int x, unsigned int y, unsigned int len, unsigned char color)`
### `void x16_gfx8h_vline (unsigned int x, unsigned int y, unsigned int len, unsigned char color)`

Spans. No clipping.

### `void x16_gfx8h_rect (unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char color)`
### `void x16_gfx8h_frame (unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char color)`

Rectangles. No clipping.

### `void x16_gfx8h_line (unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned char color)`

Bresenham line, any direction; clips through pset.

### `void x16_gfx8h_pattern_set (const unsigned char *pattern, unsigned char bg, unsigned char fg)`

Cache an 8x8 1bpp pattern for x16_gfx8h_pattern_rect().

### `void x16_gfx8h_pattern_rect (unsigned int x, unsigned int y, unsigned int w, unsigned int h)`

Fill a rectangle with the cached pattern. No clipping.

### `void x16_gfx8h_blit (unsigned int x, unsigned int y, unsigned char w, unsigned char h, const unsigned char *src, unsigned char op)`

Copy rows of pixels from RAM (row-major, one byte per pixel).
w is 1-255 pixels. op: 0 copy, 1 OR, 2 AND, 3 XOR. No clipping.

### `void x16_gfx8h_blitm (unsigned int x, unsigned int y, unsigned char w, unsigned char h, const unsigned char *src)`

Masked blit: colour 0 is transparent. Same layout as blit.

### `void x16_gfx8h_copy (unsigned long src, unsigned long dst, unsigned long len)`

VERA_2 hardware SDRAM-to-SDRAM copy of len bytes (20-bit byte
addresses, stride +1), then wait for the blitter to finish.

### `void x16_gfx8h_copy_wait (void)`

Wait for a previous copy to finish (copy already waits).

---

## `x16/shapes.h` — circle / disc / ellipse / flood / polygon / rounded rect / arc / pie / bezier, both modes

                           rounded rect / arc / pie / bezier, both modes
One shape implementation, bound at call time to the engine each entry
point names:

  x16_gfx8l_*   plot on the 8bpp bitmap  (<x16/bitmap8l.h>,  320x240)
  x16_gfx2h_*  plot on the 2bpp bitmap  (<x16/bitmap2h.h>, 640x480)

The two families differ only in the width of the vertical coordinate,
which follows the pset() of the module they draw on: 8bpp y is a byte
(0-239), 2bpp y is 16-bit (0-479).

Clipping, per shape:
  - circle: the outline plots through the clipping pset(), so it clips
    at every screen edge for free.
  - disc: the fill plots through the UNCLIPPED hline() -- keep a disc on
    screen, exactly as for the line/rect primitives.
  - flood: bounds-checked against the canvas, so it never reads or
    writes off screen.
  - polygon / arc / pie / bezier outlines plot through the clipping
    pset(); the filled variants (fpolygon, frrect, pie) use hline and
    should be kept on screen, as for disc.

### `void x16_gfx8l_circle (unsigned int cx, unsigned char cy, unsigned char r, unsigned char color)`
### `void x16_gfx8l_disc (unsigned int cx, unsigned char cy, unsigned char r, unsigned char color)`

--- 8bpp (320x240) --------------------------------------------------

Midpoint circle outline / filled disc. Radius 0-120.

### `void x16_gfx8l_ellipse (unsigned int cx, unsigned char cy, unsigned char rx, unsigned char ry, unsigned char color)`
### `void x16_gfx8l_fellipse (unsigned int cx, unsigned char cy, unsigned char rx, unsigned char ry, unsigned char color)`

Axis-aligned ellipse outline / filled ellipse (the error-form midpoint
walk). rx and ry each 0-255; the outline clips through pset, the fill
does not (keep it on screen).

### `unsigned char x16_gfx8l_flood (unsigned int x, unsigned char y, unsigned char color)`

Scanline flood fill of the 4-connected region under the seed. Filling
with the colour already there is a no-op. Returns 1 when the fill was
complete, 0 when the span stack (96 seeds) overflowed and the fill is
INCOMPLETE -- pathological shapes (long thin spirals) are what overflow.

### `void x16_gfx8l_polygon (unsigned int cx, unsigned char cy, unsigned char r, unsigned char sides, unsigned char rotation, unsigned char color)`
### `void x16_gfx8l_fpolygon (unsigned int cx, unsigned char cy, unsigned char r, unsigned char sides, unsigned char rotation, unsigned char color)`

Regular convex N-gon (sides 3-24), outline / filled. rotation is a byte
angle (0=east, 64=south, matching sin8/cos8); the first vertex points
that way. Radius 0-255.

### `void x16_gfx8l_rrect (unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char r, unsigned char color)`
### `void x16_gfx8l_frrect (unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char r, unsigned char color)`

Rounded rectangle, outline / filled. (x,y) = top-left, w/h = size, r =
corner radius (clamped to min(w,h)/2). Coordinates are 16-bit here.

### `void x16_gfx8l_arc (unsigned int cx, unsigned char cy, unsigned char r, unsigned char a0, unsigned char a1, unsigned char color)`
### `void x16_gfx8l_pie (unsigned int cx, unsigned char cy, unsigned char r, unsigned char a0, unsigned char a1, unsigned char color)`

Arc: a circle-outline slice from byte-angle a0 to a1 (a0==a1 = full
circle). x16_gfx8l_pie fills the matching wedge.

### `void x16_gfx8l_bezier (const unsigned int *pts, unsigned char color)`

Cubic Bezier through four control points, passed as an 8-element array
pts[] = { x0,y0, x1,y1, x2,y2, x3,y3 } (16-bit each) -- too many scalars
to travel through the register-ABI toolchains, so a pointer is used.

### `void x16_gfx2h_circle (unsigned int cx, unsigned int cy, unsigned char r, unsigned char color)`
### `void x16_gfx2h_disc (unsigned int cx, unsigned int cy, unsigned char r, unsigned char color)`
### `void x16_gfx2h_ellipse (unsigned int cx, unsigned int cy, unsigned char rx, unsigned char ry, unsigned char color)`
### `void x16_gfx2h_fellipse (unsigned int cx, unsigned int cy, unsigned char rx, unsigned char ry, unsigned char color)`
### `unsigned char x16_gfx2h_flood (unsigned int x, unsigned int y, unsigned char color)`
### `void x16_gfx2h_polygon (unsigned int cx, unsigned int cy, unsigned char r, unsigned char sides, unsigned char rotation, unsigned char color)`
### `void x16_gfx2h_fpolygon (unsigned int cx, unsigned int cy, unsigned char r, unsigned char sides, unsigned char rotation, unsigned char color)`
### `void x16_gfx2h_rrect (unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char r, unsigned char color)`
### `void x16_gfx2h_frrect (unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char r, unsigned char color)`
### `void x16_gfx2h_arc (unsigned int cx, unsigned int cy, unsigned char r, unsigned char a0, unsigned char a1, unsigned char color)`
### `void x16_gfx2h_pie (unsigned int cx, unsigned int cy, unsigned char r, unsigned char a0, unsigned char a1, unsigned char color)`
### `void x16_gfx2h_bezier (const unsigned int *pts, unsigned char color)`

--- 2bpp (640x480) --------------------------------------------------

---

## `x16/verafx_utils.h` — low-level VERA FX primitives

Raw building blocks for custom FX workflows: FX_CTRL/MULT control,
cache fill/write/cycle toggles, 32-bit cache loading, multiplier
accumulator triggers, increment/position registers, 16-bit hop, and
polygon-fill reads.

These are deliberately separate from x16/verafx.h. That header is the
high-level helper bundle -- multiply, fill, line, triangle -- each of
which programs the FX registers, does its job and switches FX back
off. This one is for code that wants to compose the documented FX
registers directly, one knob at a time, when no canned helper fits.

The same capability rule applies: probe with x16_vera_has_fx() FIRST.
On older VERA these routines write to registers that do not exist,
and quietly do the wrong thing rather than failing.

Two things to know:

  Nothing here resets FX_CTRL on the way out. Every routine restores
  DCSEL to 0 (and leaves ADDRSEL alone), but the FX state you set
  STAYS SET -- that is the point. Call x16_fxu_off() when done, or
  ordinary VRAM addressing keeps behaving strangely for everyone
  downstream.

  The x16_fx_* helpers clear FX_CTRL and FX_MULT themselves, so mixing
  the two levels works in one direction only: compose with fxu_*,
  then a call to any high-level helper wipes the slate.

### `void x16_fxu_off (void)`

Disable the FX helpers: FX_CTRL and FX_MULT to 0, DCSEL back to 0.
Safe whether or not anything was ever enabled.

### `unsigned char x16_fxu_get_ctrl (void)`

FX_CTRL -- the master control byte (DCSEL=2).

Bits 1:0 are the Addr1 Mode (0 normal, 1 line, 2 polygon, 3 affine);
the rest are the flags the toggles below name. Compose values from
the VERA FX reference, or use the per-flag helpers and never build a
mask at all.

### `void x16_fxu_set_ctrl (unsigned char v)`
### `void x16_fxu_ctrl_on (unsigned char bits)`
### `void x16_fxu_ctrl_off (unsigned char bits)`

Replace the whole byte / OR bits in / AND bits out.

### `void x16_fxu_addr1_mode (unsigned char mode)`

Set only the Addr1 Mode field (bits 1:0), leaving every flag alone.

### `void x16_fxu_cache_write_on (void)`
### `void x16_fxu_cache_write_off (void)`
### `void x16_fxu_cache_fill_on (void)`
### `void x16_fxu_cache_fill_off (void)`
### `void x16_fxu_cache_cycle_on (void)`
### `void x16_fxu_cache_cycle_off (void)`
### `void x16_fxu_transparent_on (void)`
### `void x16_fxu_transparent_off (void)`
### `void x16_fxu_4bit_on (void)`
### `void x16_fxu_4bit_off (void)`
### `void x16_fxu_hop_on (void)`
### `void x16_fxu_hop_off (void)`

The FX_CTRL flags, one pair each. Cache write: a DATA0/1 store writes
the whole 32-bit cache. Cache fill: a DATA0/1 read latches the byte
into the cache. Cache cycle, transparency (zero bytes leave VRAM
alone), 4-bit mode and the 16-bit hop are the reference's remaining
flags, verbatim.

### `void x16_fxu_set_mult (unsigned char v)`

FX_MULT and the 32-bit cache (DCSEL=2 and 6).

Write the FX_MULT byte: multiplier enable, subtract, accumulate,
index controls -- the reference's bitfield, uninterpreted.

### `void x16_fxu_set_cache (unsigned char l, unsigned char m, unsigned char h, unsigned char u)`

Load all four bytes of the 32-bit cache: L, M, H, U. With the
multiplier on, L/M and H/U are its two signed 16-bit operands.

### `void x16_fxu_reset_accum (void)`
### `void x16_fxu_accumulate (void)`

Clear the multiplier accumulator / trigger multiply-then-accumulate.
Both are READ-triggered registers; these wrap the reads.

### `unsigned char x16_fxu_cache_fill0 (void)`
### `unsigned char x16_fxu_cache_fill1 (void)`
### `void x16_fxu_cache_write0 (unsigned char mask)`
### `void x16_fxu_cache_write1 (unsigned char mask)`

One read of DATA0/DATA1 -- filling the cache when Cache Fill is on --
returning the byte read. And one write of the given cache nibble mask
to DATA0/DATA1, flushing the cache when Cache Write is on (mask 0
writes all four bytes).

### `void x16_fxu_set_incr (unsigned int xi, unsigned int yi)`

Increments, positions, affine bases, polygon readback (DCSEL=3,4,5).

The X/Y increment registers: 15-bit signed 6.9 fixed point in
1/512ths, bit 15 = x32 -- the line/poly/affine step encoding.

### `void x16_fxu_set_pos (unsigned int x, unsigned int y)`
### `void x16_fxu_set_subpos (unsigned char xs, unsigned char ys)`

The X/Y position registers (10 bits each) and their subpixel bytes.

### `unsigned int x16_fxu_get_poly_fill (void)`

Read POLY_FILL_L/H, the polygon helper's per-row answer: low byte in
the low half, high byte in the high half. See the VERA FX reference
for the bit layout -- it changes meaning with the 4-bit flag.

### `void x16_fxu_set_tilebase (unsigned char v)`
### `void x16_fxu_set_mapbase (unsigned char v)`

Raw writes of a precomposed FX_TILEBASE/FX_MAPBASE byte, for affine
setups the packed x16_fx_affine_on() arguments cannot express.

---

## `x16/string.h` — NUL-terminated string toolkit

The assembly library's string modules: fundamentals (length, copy,
append, compare, hash), case folding, character classification,
searching, slicing/trimming, and a pointer-array sort. Strings are
NUL-terminated, at most 255 characters plus the NUL, and nothing here
bounds-checks -- make your target buffers big enough.

cc65's own <string.h> overlaps some of this; these match the assembly
library's exact semantics (lengths in a byte, capped copies that
always NUL-terminate, compare answering -1/0/1) and share no C
runtime code.

ENCODINGS. The copy/search/slice routines are pure memory ops and do
not care what the bytes mean. Case folding and the case-sensitive
predicates do: PETSCII and ISO place the letters at different codes,
so those come in pairs -- x16_str_upper() for PETSCII bytes,
x16_str_upper_iso() for ISO bytes -- and the two genuinely swap
directions (PETSCII "lower" is numerically ISO "upper"; that is the
charset, not a bug). Mind cc65's side of the same trap: by default
the compiler translates C string literals to PETSCII, so feed the
_iso routines bytes you built yourself, not literals.

### `unsigned char x16_str_length (const char *s)`

Fundamentals (string/string.s)

Length in characters, up to the first NUL. A run of 256+ bytes
without a NUL reports 0.

### `unsigned char x16_str_copy (char *target, const char *source)`

target = source, overwriting. Returns the length copied.

### `unsigned char x16_str_ncopy (char *target, const char *source, unsigned char maxlength)`

Copy at most maxlength characters, then NUL-terminate (always).
Returns the length of the target string.

### `unsigned char x16_str_append (char *target, const char *suffix)`

target += suffix. Returns the length of the resulting string.

### `unsigned char x16_str_nappend (char *target, const char *suffix, unsigned char maxlength)`

Append, but never let the target exceed maxlength characters; the
suffix is cut to fit, and if the target is already at or past the cap
nothing changes. Returns the length of the resulting string.

### `signed char x16_str_compare (const char *s1, const char *s2)`

Strict byte-order compare, for sorting: -1 if s1 < s2, 0 if equal,
1 if greater. A prefix sorts before its extension.

### `unsigned char x16_str_hash (const char *s)`

An 8-bit rolling hash: hash(-1) = 179, then rol-and-XOR per
character. Cheap identity check, not cryptography.

### `unsigned char x16_str_lowerchar (unsigned char c)`
### `unsigned char x16_str_upperchar (unsigned char c)`
### `unsigned char x16_str_lowerchar_iso (unsigned char c)`
### `unsigned char x16_str_upperchar_iso (unsigned char c)`

Case folding (string/case.s)

Fold one character; anything that is not a letter passes through.

### `unsigned char x16_str_lower (char *s)`
### `unsigned char x16_str_upper (char *s)`
### `unsigned char x16_str_lower_iso (char *s)`
### `unsigned char x16_str_upper_iso (char *s)`

Fold a whole string in place. Returns its length.

### `signed char x16_str_compare_nocase (const char *s1, const char *s2)`
### `signed char x16_str_compare_nocase_iso (const char *s1, const char *s2)`

Case-insensitive compare: both sides folded, then -1/0/1 like
x16_str_compare.

### `unsigned char x16_str_isdigit (unsigned char c)`
### `unsigned char x16_str_isxdigit (unsigned char c)`
### `unsigned char x16_str_islower (unsigned char c)`
### `unsigned char x16_str_isspace (unsigned char c)`

Classification (string/ctype.s) -- each answers 0 or 1

The same in both encodings: '0'-'9' (48-57); hex digits 0-9 plus
65-70 and 97-102; the byte range 97-122; space, TAB, CR, LF,
shift-CR (141) and shift-space (160).

### `unsigned char x16_str_isupper (unsigned char c)`
### `unsigned char x16_str_isupper_iso (unsigned char c)`
### `unsigned char x16_str_isletter (unsigned char c)`
### `unsigned char x16_str_isletter_iso (unsigned char c)`
### `unsigned char x16_str_isprint (unsigned char c)`
### `unsigned char x16_str_isprint_iso (unsigned char c)`

Encoding-specific: PETSCII upper case lives at 97-122 and 193-218,
ISO upper case at 65-90. A letter is lower or upper; printable is
32-127/160-255 in PETSCII, 32-126/160-255 in ISO.

### `unsigned char x16_str_find (const char *s, unsigned char c)`
### `unsigned char x16_str_rfind (const char *s, unsigned char c)`

Searching (string/find.s)

First index of c scanning left to right (find) or right to left
(rfind); 255 when it is not there.

### `unsigned char x16_str_contains (const char *s, unsigned char c)`

1 if c occurs in s.

### `unsigned char x16_str_find_eol (const char *s)`

First index of a CR (13) or LF (10); 255 when the string has neither.

### `unsigned char x16_str_pattern_match (const char *s, const char *pattern)`

Wildcard match: '?' matches any single character, '*' any run
including none; everything else matches itself, case-sensitively.
Answers 1 on a match. Each '*' in the pattern costs 4 bytes of CPU
stack while matching.

### `void x16_str_left (char *target, const char *source, unsigned char length)`
### `void x16_str_right (char *target, const char *source, unsigned char length)`

Slicing and trimming (string/slice.s)

Copy the first / last `length` characters of source into target,
NUL-terminated. `length` must not exceed the source length.

### `void x16_str_slice (char *target, const char *source, unsigned char start, unsigned char length)`

Copy `length` characters starting at index `start`. The run must lie
within the source.

### `unsigned char x16_str_ltrim (char *s)`
### `unsigned char x16_str_rtrim (char *s)`
### `unsigned char x16_str_trim (char *s)`

Drop whitespace -- the x16_str_isspace set -- from the left end, the
right end, or both, in place. Return the new length.

### `void x16_str_sort (const char **array, unsigned int count)`

Sorting (string/strsort.s)

Sort an array of string pointers ascending by content, with
x16_str_compare's ordering. The strings never move; only the pointer
array is permuted. Insertion sort: fine for menu-sized arrays.

---

## `x16/sort.h` — in-place sorting of arrays

Insertion sort: O(n^2) but tiny and stable, which is right for the
modest arrays a 6502 sorts. All variants sort ascending, in place.

cc65 has qsort(), but it drags in the full comparator-callback
machinery for every element width; these typed entries carry their
comparison inline and cost far less to call for the common cases.

### `void x16_sort_u8 (unsigned char *arr, unsigned int count)`
### `void x16_sort_s8 (signed char *arr, unsigned int count)`
### `void x16_sort_u16 (unsigned int *arr, unsigned int count)`
### `void x16_sort_s16 (int *arr, unsigned int count)`

Byte and word elements, unsigned or signed, ascending. `count` is
the ELEMENT count, not a byte size.

### `typedef unsigned char (*x16_sort_cmp_t) (const void *a, const void *b)`
### `void x16_sort (void *base, unsigned int count, x16_sort_cmp_t cmp)`

The generic engine: 2-byte elements (pointers, pairs, 16-bit
handles), ordered by your comparator. It receives the addresses of
two ELEMENTS -- for an array of pointers that is a pointer to the
pointer -- and returns nonzero iff element a must sort AFTER
element b. Equal elements keep their original order (stable).

The comparator runs inside the sort: it must not call x16_sort()
itself, and must leave the library's zero-page block alone (any C
function does; hand-written assembly must save X16_P4-P7).

---

## `x16/bcd.h` — packed-BCD (decimal-mode) add and subtract

Decimal arithmetic through the 65C02's BCD mode, so 8-, 16- and
32-bit packed-BCD values add and subtract the way you read them:

     0x0987 + 0x1111 = 0x2098        (not the binary 0x1A98)

Each byte holds two decimal digits. The point is to skip the costly
binary->decimal conversion a game score or clock would otherwise
need every frame: keep the count in BCD and print its hex form
(x16_u16_to_hex), which already reads as decimal.

Signed and unsigned share one routine per width -- decimal add/sub
does not know the difference. Pick the width; the interpretation is
yours.

INTERRUPTS: these run in decimal mode across the operation. The
KERNAL's IRQ handler is decimal-safe, so ordinary use is fine. A
CUSTOM interrupt handler doing its own arithmetic must clear decimal
mode first (the x16_irq_* dispatcher does).

### `unsigned char x16_bcd_add8 (unsigned char *a, unsigned char b)`
### `unsigned char x16_bcd_add16 (unsigned int *a, unsigned int b)`
### `unsigned char x16_bcd_add32 (unsigned long *a, unsigned long b)`

a += b in packed BCD. Returns 1 if the sum overflowed the width
 (the BCD carry), 0 otherwise; *a keeps the wrapped digits either way.

### `unsigned char x16_bcd_sub8 (unsigned char *a, unsigned char b)`
### `unsigned char x16_bcd_sub16 (unsigned int *a, unsigned int b)`
### `unsigned char x16_bcd_sub32 (unsigned long *a, unsigned long b)`

a -= b in packed BCD. Returns 1 on borrow (the result wrapped below
 zero), 0 otherwise.

### `unsigned char x16_bcd_addto (unsigned char *value, unsigned long b)`
### `unsigned char x16_bcd_subfrom (unsigned char *value, unsigned long b)`

The upstream in-place forms: value points at a 4-byte packed-BCD
buffer, low byte first -- a score kept as raw bytes rather than an
unsigned long. Same result and returns as add32/sub32.

---

## `x16/bits.h` — bit and nibble helpers

Masked read-modify-write on a byte in memory, plus nibble packing.
C can of course write `*p |= mask` itself; these exist so C and
assembly callers share one implementation, and because x16_bit_put
turns a flag into a set-or-clear without a branch at the call site.

### `void x16_bit_set (unsigned char *addr, unsigned char mask)`
### `void x16_bit_clr (unsigned char *addr, unsigned char mask)`

Set (OR) or clear (AND NOT) the masked bits of *addr.

### `void x16_bit_put (unsigned char *addr, unsigned char mask, unsigned char on)`

on != 0 sets the masked bits, on == 0 clears them.

### `unsigned char x16_bit_test (const unsigned char *addr, unsigned char mask)`

Returns *addr & mask: nonzero iff any masked bit is set.

### `unsigned char x16_hinib (unsigned char v)`
### `unsigned char x16_lonib (unsigned char v)`

Nibble helpers: the high or low four bits of v, in bits 3:0.

### `unsigned char x16_catnib (unsigned char hi, unsigned char lo)`

(hi << 4) | lo, both masked to their nibble first.

---

## `x16/number.h` — number formatting and parsing

Decimal, hex and binary rendering without printf's footprint, plus a
decimal parser.

ALL CONVERSIONS SHARE ONE MODULE BUFFER. The returned pointer aims
into it, the string is NUL-terminated, and the NEXT CALL OVERWRITES
IT -- copy the text out if you need to keep it. The bytes are ASCII
(digits, 'A'-'F', '-'), the same values the upstream assembly
library produced.

### `char * x16_u8_to_dec (unsigned char v)`
### `char * x16_u16_to_dec (unsigned int v)`
### `char * x16_s8_to_dec (signed char v)`
### `char * x16_s16_to_dec (int v)`

Decimal, no leading zeros ("0" for zero). The signed forms prepend
'-' to the magnitude, so -32768 renders as "-32768".

### `char * x16_u8_to_hex (unsigned char v)`
### `char * x16_u16_to_hex (unsigned int v)`

Fixed-width uppercase hex: always 2 (resp. 4) digits.

### `char * x16_u8_to_bin (unsigned char v)`
### `char * x16_u16_to_bin (unsigned int v)`

Fixed-width binary, MSB first: always 8 (resp. 16) digits.

### `unsigned char x16_dec_to_u16 (const char *s, unsigned char len, unsigned int *value)`

Parse len decimal digits from s into *value. Returns 1 on success,
0 if a non-digit was found or the value overflowed 16 bits (*value
is untouched on failure). len is the exact digit count -- there is
no terminator scan, so a slice of a longer string works.

---

## `x16/tscrunch.h` — TSCrunch decompression

TSCrunch (Antonio Savona) is a byte-aligned LZ+RLE built to maximise
6502 decode speed -- the other end of the trade from ZX0: unpacks
markedly faster, packs a little looser. Crunch with:

     tscrunch data.bin data.tsc      (plain memory crunch)

RAM to RAM only: the match copier reads the output back, so this
cannot write through VERA's data port, and cannot decompress in
place (forward copies only).

### `void * x16_tsc_decompress (const void *src, void *dst)`

Returns one past the last output byte, so the unpacked length is the
return value minus `dst`.

---

## `x16/fileio.h` — generic KERNAL file/channel I/O

Streamed file/channel I/O: OPEN/CLOSE, CHKIN/CHKOUT, CHRIN/CHROUT,
READST, and the setup calls that feed them. For one-shot PRG LOAD/SAVE
use x16/load.h; when a call fails and you want to know WHY, ask the
command channel via x16/dos.h.

The usual dance, writing then reading a SEQ file on device 8:

     x16_fio_open_write("DATA.SEQ,S,W", 12, 2, 8, 2);
     x16_fio_chrout(...);            // as many as you like
     x16_fio_close_named(2);

     x16_fio_open_read("DATA.SEQ,S,R", 12, 2, 8, 2);
     do { b = x16_fio_chrin(); ... } while (!x16_fio_readst());
     x16_fio_close_named(2);

Filenames are (pointer, length), not NUL-terminated. Calls that can
fail return 0 on success, else the KERNAL error code -- the same
convention as x16_fs_load().

cc65's <cbm.h> has cbm_k_* twins for the raw wrappers; these exist so
the same API is available in every port of the library.

```c
#define X16_FIO_DEV_KEYBOARD    0
#define X16_FIO_DEV_SCREEN      3
#define X16_FIO_DEV_DISK        8
#define X16_FIO_LFN_COMMAND     15
#define X16_FIO_SA_NONE         0
#define X16_FIO_SA_COMMAND      15
```

End-of-file bit in x16_fio_readst()'s answer.

```c
#define X16_FIO_ST_EOF          0x40
```

### `void x16_fio_set_lfs (unsigned char lfn, unsigned char device, unsigned char secondary)`
### `void x16_fio_set_name (const char *name, unsigned char len)`

--- raw KERNAL wrappers -------------------------------------------

### `unsigned char x16_fio_open (void)`
### `void x16_fio_close (unsigned char lfn)`

0 on success, else the KERNAL error code. Uses the name and numbers
given to the two calls above.

### `unsigned char x16_fio_chkin (unsigned char lfn)`
### `unsigned char x16_fio_chkout (unsigned char lfn)`

Select a logical file for input/output. 0 on success, else the KERNAL
error code (3 = file not open).

### `void x16_fio_clrchn (void)`

Back to keyboard and screen.

### `unsigned char x16_fio_chrin (void)`

One byte from the current input channel.

### `void x16_fio_chrout (unsigned char b)`

One byte to the current output channel.

### `unsigned char x16_fio_readst (void)`

The KERNAL status byte; X16_FIO_ST_EOF set means end of file.

### `unsigned char x16_fio_getin (void)`
### `void x16_fio_close_all (void)`

One byte, 0 if nothing is waiting. On a file channel it reads like
x16_fio_chrin().

### `void x16_fio_close_device (unsigned char device)`

every file

### `unsigned char x16_fio_open_named (const char *name, unsigned char len, unsigned char lfn, unsigned char device, unsigned char secondary)`

--- composites ----------------------------------------------------

SETNAM + SETLFS + OPEN in one call. 0 on success, else the KERNAL
error code.

### `unsigned char x16_fio_open_read (const char *name, unsigned char len, unsigned char lfn, unsigned char device, unsigned char secondary)`
### `unsigned char x16_fio_open_write (const char *name, unsigned char len, unsigned char lfn, unsigned char device, unsigned char secondary)`

...then also select the file for input (CHKIN) or output (CHKOUT).

### `void x16_fio_close_named (unsigned char lfn)`

CLRCHN + CLOSE.

---

## `x16/iec.h` — low-level IEC / serial bus wrappers

Direct access to the classic Commodore serial bus KERNAL calls. Most
programs should use x16/fileio.h, x16/load.h, x16/dos.h or x16/bmx.h
instead; this gate is for protocols that need explicit bus control.

Reading the drive status line by hand, the way the DOS wedge does:

     x16_iec_talk_channel(8, 15);    // TALK 8, secondary $6F
     do { c = x16_iec_acptr(); ... } while (c != 0x0D);
     x16_iec_untalk();

The composite *_channel calls OR the X16_IEC_CMD_* base into the
secondary for you; the raw second/tksa calls take the finished byte.

Secondary-address command bases, OR'd with the channel (0-15).

```c
#define X16_IEC_CMD_DATA        0x60
#define X16_IEC_CMD_CLOSE       0xE0
#define X16_IEC_CMD_OPEN        0xF0
```

### `void x16_iec_listen (unsigned char device)`
### `void x16_iec_talk (unsigned char device)`

--- raw KERNAL wrappers -------------------------------------------

### `void x16_iec_second (unsigned char cmd)`
### `void x16_iec_tksa (unsigned char cmd)`

The secondary command byte after LISTEN / after TALK.

### `void x16_iec_ciout (unsigned char b)`
### `unsigned char x16_iec_acptr (void)`
### `void x16_iec_unlisten (void)`
### `void x16_iec_untalk (void)`

One byte out to the listener / in from the talker.

### `void x16_iec_set_timeout (unsigned char t)`

KERNAL SETTMO. A no-op in ROM r49, kept for completeness.

### `unsigned char x16_iec_readst (void)`

The serial/KERNAL status byte, as x16_fio_readst().

### `int x16_iec_macptr (unsigned char count, void *dest)`
### `int x16_iec_mciout (unsigned char count, const void *src)`

X16 block transfers for the current channel (after CHKIN/CHKOUT).
A count of 0 lets the implementation choose. Returns the byte count
actually transferred, or -1 when the channel cannot do block
transfers -- fall back to acptr/ciout one byte at a time.

The pointer always advances. (The raw KERNAL call takes carry set to
pin it on one address for port I/O; that mode is not exposed here.)

### `void x16_iec_open_channel (unsigned char device, unsigned char secondary)`
### `void x16_iec_data_channel (unsigned char device, unsigned char secondary)`
### `void x16_iec_close_channel (unsigned char device, unsigned char secondary)`

--- composites ----------------------------------------------------

LISTEN device, then the OPEN / DATA / CLOSE secondary command.

### `void x16_iec_talk_channel (unsigned char device, unsigned char secondary)`

TALK device, then the DATA secondary command.

---

## `x16/dir.h` — reading a directory

A drive hands its directory over as a BASIC program listing -- link
words, line numbers, quoted names. These routines walk that so you
never see it:

     char name[40];
     if (!x16_dir_open(0, 0, 8)) return;     // "$", device 8
     while (x16_dir_next(name, sizeof name)) {
         if (x16_dir_type() == X16_DIR_TYPE_PRG) {
             // name, x16_dir_blocks() ...
         }
     }
     x16_dir_close();

The header line naming the volume comes back as X16_DIR_TYPE_HOST and
the trailing "BLOCKS FREE." line as X16_DIR_TYPE_NONE with an empty
name, rather than being hidden -- a file browser wants to skip them, a
disk info panel wants to show them, and this way neither has to
re-parse anything.

The directory occupies logical file 3, clear of the loader's 1 and the
command channel's 15. Only one directory can be open at a time.

cc65's <cbm.h> has cbm_opendir/cbm_readdir over its own file table;
these stand alone and also classify the header and trailer lines.

What x16_dir_type() reports for the entry x16_dir_next() just read.

```c
#define X16_DIR_TYPE_NONE       0       /* no name on the line: "BLOCKS FREE." */
#define X16_DIR_TYPE_PRG        1
#define X16_DIR_TYPE_SEQ        2
#define X16_DIR_TYPE_USR        3
#define X16_DIR_TYPE_REL        4
#define X16_DIR_TYPE_DIR        5
#define X16_DIR_TYPE_HOST       6       /* the header line naming the volume */
```

### `unsigned char x16_dir_open (const char *path, unsigned char len, unsigned char device)`

Open a directory for reading. A length of 0 asks for "$", the current
directory; otherwise `path` names one, (pointer, length) style, not
NUL-terminated. Returns 1 if the directory opened, 0 if not.

### `unsigned char x16_dir_next (char *buf, unsigned char size)`

Read the next entry. The name arrives NUL-terminated in `buf`,
truncated to fit (size 2-255). Returns 1 if an entry was read, 0 at
the end of the listing.

### `unsigned char x16_dir_type (void)`

Describe the entry x16_dir_next() just read.

### `unsigned int x16_dir_blocks (void)`

X16_DIR_TYPE_*

### `void x16_dir_close (void)`

the listing's block count

Finished with the directory.

---

## `x16/ringbuffer.h` — an 8 KB FIFO ring in a HIRAM bank

A first-in-first-out queue whose 8 KB of storage is one whole
banked-RAM bank ($A000-$BFFF). Tell it which bank to own with
x16_ring_init(), then put and get bytes or words:

     x16_ring_init(6);               // take bank 6 for the queue
     x16_ring_put('H');
     x16_ring_putw(300);
     b = x16_ring_get();             // 'H' -- FIFO order
     w = x16_ring_getw();            // 300

The head, tail and fill counters live in low RAM; only the queued data
sits in the bank, and every call saves and restores RAM_BANK. The bank
number can come from anywhere -- a constant, or x16_bank_alloc().

There are NO over/underflow guards: the capacity is 8191 bytes, and
checking x16_ring_isfull()/x16_ring_isempty() is on you. One queue
exists; init again (same or another bank) to reset it.

The small 256-byte ring that needs no bank is x16_rb_* in
x16/buffers.h.

```c
#define X16_RING_CAPACITY       8191
```

### `void x16_ring_init (unsigned char bank)`

Claim a bank and empty the queue.

### `void x16_ring_put (unsigned char b)`
### `void x16_ring_putw (unsigned int w)`

Enqueue a byte, or a word (low byte first).

### `unsigned char x16_ring_get (void)`
### `unsigned int x16_ring_getw (void)`
### `unsigned int x16_ring_size (void)`

Dequeue them again, oldest first.

### `unsigned int x16_ring_free (void)`

bytes queued

### `unsigned char x16_ring_isempty (void)`

usable bytes free

### `unsigned char x16_ring_isfull (void)`

1 if empty

---

## `x16/stack.h` — an 8 KB LIFO stack in a HIRAM bank

A last-in-first-out stack whose 8 KB of storage is one whole
banked-RAM bank ($A000-$BFFF). Tell it which bank to own with
x16_stack_init(), then push and pop bytes or words:

     x16_stack_init(5);              // take bank 5 for the stack
     x16_stack_push(42);
     x16_stack_pushw(1000);
     w = x16_stack_popw();           // 1000 -- LIFO order
     b = x16_stack_pop();            // 42

It grows downward from the top of the bank. The stack pointer lives in
low RAM, so only the data itself sits in the bank, and every call
saves and restores RAM_BANK -- a stack in bank 5 and your own use of
bank 7 in between never trip over each other.

There are NO over/underflow guards: the capacity is 8191 bytes, and
checking x16_stack_isfull()/x16_stack_isempty() is on you. One stack
exists; init again (same or another bank) to reset it.

The small 256-byte stack that needs no bank is x16_stk_* in
x16/buffers.h.

```c
#define X16_STACK_CAPACITY      8191
```

### `void x16_stack_init (unsigned char bank)`

Claim a bank and empty the stack.

### `void x16_stack_push (unsigned char b)`
### `void x16_stack_pushw (unsigned int w)`

Push a byte, or a word.

### `unsigned char x16_stack_pop (void)`
### `unsigned int x16_stack_popw (void)`
### `unsigned int x16_stack_size (void)`

Pop them again, newest first.

### `unsigned int x16_stack_free (void)`

bytes stored

### `unsigned char x16_stack_isempty (void)`

bytes free

### `unsigned char x16_stack_isfull (void)`

1 if empty

---

## `x16/keyboard.h` — keyboard buffer and layout helpers

The X16-specific keyboard surface beyond x16/input.h. That header
already covers consuming keys, and its functions are this family's
read side:

     x16_key_get()   next key, or 0        (upstream kbd + GETIN)
     x16_key_wait()  block for a key
     x16_key_peek()  look without taking   (upstream kbd_peek)

New here: injecting keys into the KERNAL's buffer (self-typing demos,
macros, tests), the live modifier bitfield, and the keyboard layout.

x16_kbd_get_modifiers() bits.

```c
#define X16_KBD_MOD_SHIFT       0x01
#define X16_KBD_MOD_ALT         0x02
#define X16_KBD_MOD_CTRL        0x04
#define X16_KBD_MOD_CAPS        0x10
#define X16_KBD_MOD_ALTGR       (X16_KBD_MOD_ALT | X16_KBD_MOD_CTRL)
```

A layout name never exceeds this, NUL included.

```c
#define X16_KBD_KEYMAP_LEN      14
```

### `void x16_kbd_scan (void)`

Scan the keyboard once. The KERNAL's IRQ already does this every
frame; you only need it if you have taken the interrupt over.

### `void x16_kbd_put (unsigned char key)`

Append a PETSCII key to the keyboard buffer, as if typed. Read it
back with x16_key_get()/x16_key_peek().

### `unsigned char x16_kbd_get_modifiers (void)`

The modifiers held down right now, as X16_KBD_MOD_* bits.

### `unsigned char x16_kbd_get_keymap (char *name)`

Copies the active layout's NUL-terminated name -- e.g. "en-us" --
into `name` (X16_KBD_KEYMAP_LEN bytes are always enough) and returns
the layout index.

### `unsigned char x16_kbd_set_keymap (const char *name)`

Switch layouts by name. Returns 1 on success; 0 leaves the previous
layout active. The name must match the ROM's spelling byte for byte.

---

## `x16/mouse.h` — the full KERNAL mouse surface

x16/input.h covers the everyday calls, and stays this family's front
door:

     x16_mouse_show(n)     show cursor sprite n   (0xFF: keep sprite)
     x16_mouse_hide()
     x16_mouse_get(&x,&y)  position + button mask

New here: the raw MOUSE_CONFIG -- which also sizes the mouse field --
an explicit scan, and a get that adds the scroll wheel.

Button bits, from x16_mse_get()/x16_mouse_get().

```c
#define X16_MSE_BUTTON_LEFT     0x01
#define X16_MSE_BUTTON_RIGHT    0x02
#define X16_MSE_BUTTON_MIDDLE   0x04
#define X16_MSE_BUTTON_4        0x10
#define X16_MSE_BUTTON_5        0x20
```

x16_mse_config() show selectors.

```c
#define X16_MSE_HIDE            0x00
#define X16_MSE_SHOW_KEEP       0xFF    /* show, keep the cursor sprite */
```

### `void x16_mse_config (unsigned char show, unsigned char width8, unsigned char height8)`

The raw MOUSE_CONFIG. `show` is X16_MSE_HIDE, X16_MSE_SHOW_KEEP, or a
cursor sprite number. `width8`/`height8` bound the mouse field in
8-pixel units -- both 0 keeps the current bounds, which is what the
x16_mouse_show()/x16_mouse_hide() shortcuts pass.

### `void x16_mse_scan (void)`

Sample the mouse once. The KERNAL's IRQ already does this every
frame; you only need it if you have taken the interrupt over.

### `signed char x16_mse_get (unsigned int *x, unsigned int *y, unsigned char *buttons)`

Position through the first two pointers, X16_MSE_BUTTON_* mask
through the third; returns the signed scroll-wheel movement since the
last read. x16_mouse_get() is the wheel-less shorthand.

---

## `x16/clock.h` — the jiffy timer and the real-time clock

Two clocks. The 24-bit jiffy timer ticks at 60 Hz under the KERNAL's
IRQ and wraps daily -- cc65's clock() reads the same counter, but
only these calls can SET it, or tick it by hand when you own the
interrupt. The RTC is the battery-backed date/time chip, read and
written as a whole through x16_date_time.

### `void x16_clock_update (void)`

 Field order is the KERNAL's r0-r3 date/time layout -- do not reorder.
typedef struct {
    unsigned char year;         /* since 1900: 126 means 2026
    unsigned char month;        /* 1-12
    unsigned char day;          /* 1-31
    unsigned char hours;        /* 0-23
    unsigned char minutes;      /* 0-59
    unsigned char seconds;      /* 0-59
    unsigned char jiffies;      /* 60ths of a second
    unsigned char weekday;      /* 1 = Monday
} x16_date_time;

 Tick the jiffy timer by one, exactly as the KERNAL's IRQ does every
 frame. Call it from your own handler if you have taken VSYNC over,
 or clock() and the timer stand still.

### `unsigned long x16_clock_get_timer (void)`

The jiffy counter, 0 to 0xFFFFFF. It keeps counting through a set,
so measure intervals by subtraction.

### `void x16_clock_set_timer (unsigned long jiffies)`

Set the jiffy counter; bits 24-31 are ignored.

### `void x16_clock_get_date_time (x16_date_time *dt)`
### `void x16_clock_set_date_time (const x16_date_time *dt)`

Read/write the RTC. The ROM does not validate -- pass sane fields.

---

## `x16/i2c.h` — the I2C bus: SMC, RTC, and friends

The X16's system devices hang off one I2C bus: the SMC (power,
keyboard, mouse) at $42 and the RTC at $6F, whose offsets $20-$5F are
64 bytes of battery-backed NVRAM -- free save-game/settings storage.

Errors are NAKs. The byte read returns 0xFFFF for one, the writes
return 0, so a missing device is detectable rather than fatal.

BE CAREFUL WHAT YOU WRITE. SMC offset $01 is the power switch: a
stray x16_i2c_write_byte(X16_I2C_SMC, 0x01, 0) turns the machine off.

7-bit device addresses.

```c
#define X16_I2C_SMC     0x42
#define X16_I2C_RTC     0x6F
```

The first NVRAM offset in the RTC, and how many bytes follow.

```c
#define X16_I2C_RTC_NVRAM       0x20
#define X16_I2C_RTC_NVRAM_LEN   0x40
```

### `unsigned int x16_i2c_read_byte (unsigned char device, unsigned char offset)`

One byte from a device register. Returns 0-255, or 0xFFFF on NAK.

### `unsigned char x16_i2c_write_byte (unsigned char device, unsigned char offset, unsigned char value)`

One byte to a device register. Returns 1 on success, 0 on NAK.

### `unsigned char x16_i2c_batch_read (unsigned char device, void *buf, unsigned int count, unsigned char fixed)`

Read `count` bytes from the device's CURRENT internal offset --
position it first, e.g. with an x16_i2c_read_byte of the offset
before the ones you want. `fixed` 0 fills the buffer normally;
nonzero parks every byte at buf[0] (stream into a port). Returns 1
on success, 0 on error.

### `unsigned int x16_i2c_batch_write (unsigned char device, const void *buf, unsigned int count)`

Write `count` bytes in one transaction. buf[0] is the register
offset, the data follows -- the I2C wire format. Returns the number
of bytes written, or 0xFFFF on error.

---

## `x16/console.h` — the KERNAL console API

A proportional-font terminal over GRAPH: word wrap, paging, inline
images, blocking line input. Handy for tools and adventures; games
usually want x16/graph.h directly.

Usual sequence:

     x16_graph_init(NULL);
     x16_con_init(0, 0, 0, 0);
     x16_con_put_char('H', 1);       ...

Characters are ISO/ASCII -- the GRAPH font's encoding, not PETSCII.

Control codes accepted by x16_con_put_char (and x16_graph_put_char)
for font styling.

```c
#define X16_CON_ATTR_UNDERLINE  0x04
#define X16_CON_ATTR_BOLD       0x06
#define X16_CON_ATTR_ITALICS    0x0B
#define X16_CON_ATTR_OUTLINE    0x0C
#define X16_CON_ATTR_RESET      0x92
```

### `void x16_con_init (unsigned int x, unsigned int y, unsigned int width, unsigned int height)`

Open a console in the rectangle; all zeroes = the full screen.
Clears the area.

### `void x16_con_put_char (unsigned char c, unsigned char wrap)`

wrap 0 breaks lines anywhere; nonzero buffers each word and breaks
between words. Scrolls when the window is full, paging first if a
message is set.

### `unsigned char x16_con_get_char (void)`

Line input: BLOCKS until RETURN finishes a line, then returns it one
character per call, CR last.

### `void x16_con_set_paging_message (const char *msg)`

After each full page of output, show `msg` and wait for a key.

### `void x16_con_disable_paging (void)`

Scroll freely, never prompt -- the power-on state.

### `void x16_con_put_image (const unsigned char *image, unsigned int width, unsigned int height)`

Inline a GRAPH_draw_image-format bitmap at the cursor, like an
oversized character.

---

## `x16/graph.h` — the KERNAL GRAPH drawing API

The ROM's GEOS-derived drawing layer: lines, rects, ovals, images,
proportional text, all clipped to a window, drawn through the active
framebuffer driver (x16/fb.h).

x16_graph_init(NULL) is the entry point for the whole family: it
switches the display to 320x240@8bpp, installs the default driver,
resets window/colors/font, and clears. Get text mode back with
x16_screen_set_mode() or x16_screen_reset() (x16/screen.h).

Colors: `stroke` is the pen -- lines, glyphs, and shape outlines,
including the one-pixel border of filled shapes; `fill` paints the
interiors; `background` is what clearing paints. A "filled" rect or
oval is therefore two-tone unless stroke == fill.

Style bits, as x16_graph_get_char_size() takes and returns them.
These are the ROM's GEOS bits (x16-rom graphics/fonts/fonts.inc) --
not the 1/2/4 the upstream assembly library documents, which the ROM
never accepted.

```c
#define X16_GRAPH_STYLE_UNDERLINE       0x80
#define X16_GRAPH_STYLE_BOLD            0x40
#define X16_GRAPH_STYLE_REVERSE         0x20
#define X16_GRAPH_STYLE_ITALIC          0x10
#define X16_GRAPH_STYLE_OUTLINE         0x08
```

### `void x16_graph_init (const void *driver)`

 x16_graph_get_char_size() results. On a printable character the first
 three fields are set; on a control code only `style` is.

typedef struct {
    unsigned char baseline;     /* rows from glyph top to the baseline
    unsigned char width;
    unsigned char height;
    unsigned char style;        /* the style a control code selects
} x16_char_size;

 `driver` is an FB_* vector table, or NULL for the default
 320x240@8bpp driver.

### `void x16_graph_clear (void)`

Clear the current window to the background color.

### `void x16_graph_set_window (unsigned int x, unsigned int y, unsigned int width, unsigned int height)`
### `void x16_graph_set_colors (unsigned char stroke, unsigned char fill, unsigned char background)`
### `void x16_graph_draw_line (unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)`

Clip all drawing to this rectangle. All zeroes = full screen.

### `void x16_graph_draw_rect (unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int radius, unsigned char fill)`

fill 0 outlines in the stroke color, nonzero fills. `radius` is
accepted for API parity; the ROM currently ignores it.

### `void x16_graph_move_rect (unsigned int sx, unsigned int sy, unsigned int tx, unsigned int ty, unsigned int width, unsigned int height)`

Copy a rectangle; source and target may overlap. ROM quirk, verified
against r49: moving DOWN (ty > sy) copies height+1 rows, moving up
copies exactly height.

### `void x16_graph_draw_oval (unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned char fill)`

The oval inscribes its bounding box. fill as in draw_rect.

### `void x16_graph_draw_image (unsigned int x, unsigned int y, const unsigned char *image, unsigned int width, unsigned int height)`

`image` is width*height 8-bit pixels, row by row.

### `void x16_graph_set_font (const void *font)`

NULL restores the system font. A custom font is in GEOS format; it
must sit below $A000.

### `unsigned char x16_graph_get_char_size (unsigned char c, unsigned char style, x16_char_size *out)`

Measure `c` in `style`. Returns 1 for a printable character (fills
baseline/width/height), 0 for a control code (fills `style` with the
style that code selects).

### `unsigned char x16_graph_put_char (unsigned int *x, unsigned int *y, unsigned char c)`

Draw `c` at the position in *x and *y -- y is the BASELINE -- and
advance both to the next character position. Returns 1 if it landed
inside the window, 0 if it was clipped. Characters are ISO/ASCII,
and control codes move the pen or restyle it (see CON_ATTR_* in
x16/console.h).

---

## `x16/fb.h` — the KERNAL framebuffer driver API

The ROM's low-level pixel layer: a cursor machine. Position the
cursor, and every get/set advances it, so runs of pixels cost no
per-pixel address math. The default driver is 320x240 at 8bpp in
VRAM $00000; GRAPH can install a different one.

CALL x16_graph_init() FIRST (x16/graph.h). The FB entry points
dispatch through vectors it installs; before that they point nowhere.

This is the ROM's drawing surface, driver-abstracted and cursor-
based. The library's own x16_bitmap_* (x16/bitmap.h) is the fast
direct-VERA alternative when you know the mode.

### `void x16_fb_init (void)`

 A pixel filter for x16_fb_filter_pixels(): receives a color, returns
 the replacement. It runs under the ROM's inner loop: keep it small,
 and do not touch VERA or call the fb/graph API from it.

typedef unsigned char (__fastcall__ *x16_fb_filter) (unsigned char color);

 Reinitialize the active driver: mode registers, VRAM base.

### `unsigned char x16_fb_get_info (unsigned int *width, unsigned int *height)`

Geometry of the active driver. Returns the depth in bits per pixel
(8 for the default driver) and fills in the pixel dimensions.

### `void x16_fb_set_palette (const void *data, unsigned char start, unsigned char count)`

Set `count` palette entries from `start` (count 0 means all 256).
`data` is count*2 bytes of VERA GB/R words, exactly the
x16/palette.h format.

### `void x16_fb_cursor_position (unsigned int x, unsigned int y)`

Park the cursor at a pixel.

### `void x16_fb_cursor_next_line (unsigned int x)`

Drop the cursor one scanline -- cheaper than a full reposition. The
API passes x for drivers that need it; the default driver keeps its
own position and ignores it.

### `unsigned char x16_fb_get_pixel (void)`
### `void x16_fb_set_pixel (unsigned char color)`

Single pixels at the cursor. Each advances it by one.

### `void x16_fb_get_pixels (void *dst, unsigned int count)`
### `void x16_fb_set_pixels (const void *src, unsigned int count)`

Runs of pixels at the cursor. Each advances it by `count`; a count of
0 moves nothing.

### `void x16_fb_set_8_pixels (unsigned char pattern, unsigned char color)`

Draw the pattern's 1-bits in `color`, MSB first; 0-bits leave the
underlying pixels alone. Advances the cursor by 8. This is how a
glyph row or a 1bpp image row lands in one call.

### `void x16_fb_set_8_pixels_opaque (unsigned char pattern, unsigned char mask, unsigned char fg, unsigned char bg)`

The two-color version: where `mask` has a 1 (MSB first), draw fg if
the pattern bit is 1, bg if it is 0; mask 0-bits leave the pixel
alone. Advances the cursor by 8.

### `void x16_fb_fill_pixels (unsigned int count, unsigned int step, unsigned char color)`

`count` pixels of `color` from the cursor. step 0/1 is a solid run
(hardware-accelerated); a larger step spaces the pixels -- step 320
on the default driver is a vertical line.

### `void x16_fb_filter_pixels (unsigned int count, x16_fb_filter filter)`

Rewrite `count` pixels from the cursor through `filter` -- palette
remapping, highlight, dim.

### `void x16_fb_move_pixels (unsigned int sx, unsigned int sy, unsigned int tx, unsigned int ty, unsigned int count)`

Copy a horizontal span of `count` pixels from (sx,sy) to (tx,ty).

---

## `x16/spi.h` — VERA SPI controller helpers

VERA exposes an SPI master at two registers: writing DATA starts one
full-duplex byte transfer, BUSY stays set until the received byte can
be read back, and SELECT asserts chip-select. These are the raw
primitives -- select, clock speed, byte exchange -- for talking to
whatever hangs off that bus.

What hangs off it on a stock X16 is the SD CARD, and the KERNAL's DOS
drives it through these same registers. Do not run transfers of your
own while a load or save is in flight, and leave the bus deselected
(x16_spi_deselect()) when you are done, or the next directory listing
will find the card mid-conversation.

The usual SD dance: x16_spi_slow() + 80 clocks of x16_spi_read() with
the card DESELECTED, then select, command, response -- but that
protocol is the card's, not VERA's, and this header stops at bytes.

Auto-TX is VERA's bulk-read accelerator: while it is on, every read of
DATA starts the next $FF transfer by itself. Turn it on, read DATA
once to prime, then x16_spi_autotx_read() streams at full clock.

SPI_CTRL bits, as x16_spi_get_ctrl() returns them. BUSY is read-only:
while it is set the transfer is still shifting.

```c
#define X16_SPI_SELECT   0x01   /* 1 asserts chip-select, 0 releases it */
#define X16_SPI_SLOWCLK  0x02   /* 1 = ~390 kHz, 0 = ~12.5 MHz */
#define X16_SPI_AUTOTX   0x04   /* reading DATA starts a $FF transfer */
#define X16_SPI_BUSY     0x80   /* read-only */
```

### `unsigned char x16_spi_get_ctrl (void)`
### `void x16_spi_set_ctrl (unsigned char bits)`

The raw control register, for saving/restoring around your own use.

### `void x16_spi_wait (void)`

Block until the active transfer finishes (BUSY clears). The byte
routines below already wait; this is for hand-rolled sequences.

### `void x16_spi_select (void)`
### `void x16_spi_deselect (void)`
### `void x16_spi_slow (void)`

Chip-select, clock speed and Auto-TX, one bit each.

### `void x16_spi_fast (void)`

~390 kHz: SD card initialisation

### `void x16_spi_autotx_on (void)`
### `void x16_spi_autotx_off (void)`

~12.5 MHz

### `unsigned char x16_spi_transfer (unsigned char out)`

Transmit `out`, wait, and return the byte that came back.

### `void x16_spi_write (unsigned char out)`

Transmit `out` and wait; the received byte is discarded.

### `unsigned char x16_spi_read (void)`

Transmit $FF (the bus idle pattern), wait, return the received byte.

### `unsigned char x16_spi_autotx_read (void)`

Wait, then read DATA in Auto-TX mode -- the read itself starts the
next $FF transfer, so a loop of these streams back-to-back.

### `void x16_spi_read_bytes (void *dest, unsigned int count)`
### `void x16_spi_write_bytes (const void *src, unsigned int count)`

Bulk exchanges: `count` bytes into or out of RAM, one transfer each.
Reads transmit $FF for every byte, as SD cards expect.

---

## `x16/serial.h` — the serial / WiFi card UARTs

The X16 serial / WiFi card carries up to two 16C550-style UARTs in the
expansion I/O window ($9F60-$9FF8, on 8-byte boundaries; the standard
card populates $9F60 and $9F68). The WiFi half is an ESP32 running
ZiModem firmware, driven as an AT-command modem over UART 0 -- that
protocol lives in <x16/zimodem.h>; this header is the bytes.

     if (x16_ser_detect()) {
         x16_ser_init(x16_ser_uart0(), X16_SER_BAUD_9600);
         x16_ser_puts("hello\r\n");
         while ((c = x16_ser_get()) == -1)
             ;                        // poll (bound this yourself!)
     }

x16_ser_init() programs 8N1, FIFOs on, auto-flow -- and NO interrupts:
this module polls, so it composes with <x16/irq.h> without touching
it. The UART handed to init becomes "the current one" for every other
call; call init again to point them elsewhere.

The blocking calls (x16_ser_get_wait, x16_ser_put and everything built
on them) spin on the UART's status register with no timeout. They are
for a card that is really there -- detect first.

Baud-rate divisors for x16_ser_init(), from the card's 14.7456 MHz
clock: divisor = 14745600 / (16 * baud).

```c
#define X16_SER_BAUD_921600     0x0001
#define X16_SER_BAUD_460800     0x0002
#define X16_SER_BAUD_230400     0x0004
#define X16_SER_BAUD_115200     0x0008
#define X16_SER_BAUD_57600      0x0010
#define X16_SER_BAUD_38400      0x0018
#define X16_SER_BAUD_28800      0x0020
#define X16_SER_BAUD_19200      0x0030
#define X16_SER_BAUD_14400      0x0040
#define X16_SER_BAUD_9600       0x0060
#define X16_SER_BAUD_4800       0x00C0
#define X16_SER_BAUD_2400       0x0180
#define X16_SER_BAUD_1200       0x0300
#define X16_SER_BAUD_600        0x0600
#define X16_SER_BAUD_300        0x0C00
```

### `unsigned char x16_ser_detect (void)`
### `unsigned int x16_ser_uart0 (void)`
### `unsigned int x16_ser_uart1 (void)`

Scan the expansion window for UART chips. Returns how many answered
(0, 1 or 2); their base addresses come back from x16_ser_uart0() and
x16_ser_uart1() (0 = none). The probe fingerprints registers a 16C550
must have -- a floating bus does not answer it by accident.

### `void x16_ser_init (unsigned int base, unsigned int divisor)`

Program `base` for 8N1, FIFOs, auto-flow, no interrupts, and make it
the current UART. `divisor` is an X16_SER_BAUD_* constant.

### `unsigned char x16_ser_avail (void)`

1 if a received byte is waiting, else 0. Never blocks.

### `int x16_ser_get (void)`

Read one byte without blocking: the byte, or -1 if none was waiting.

### `unsigned char x16_ser_get_wait (void)`

Read one byte, spinning until one arrives. Hardware only.

### `void x16_ser_put (unsigned char b)`

Send one byte, waiting for room in the transmit FIFO.

### `void x16_ser_puts (const char *s)`
### `void x16_ser_write (const void *data, unsigned char len)`

Send a NUL-terminated string / a counted (binary-safe) run of bytes.
A len of 0 means 256.

### `unsigned int x16_ser_read_until (char *buf, unsigned int max, const char *match)`

Read into buf until the NUL-terminated needle `match` has gone past,
or `max` bytes are stored. The needle is included in the buffer.
Returns the byte count actually stored. Blocks between bytes.

### `void x16_ser_discard_until (const char *match)`

Read and discard bytes until `match` has gone past. Blocks.

---

## `x16/zimodem.h` — ZiModem (ESP32 WiFi) over the serial card

The WiFi half of the serial card is an ESP32 running ZiModem firmware,
driven as a Hayes-style modem: "AT..." command lines out, "OK\r\n"
back. This layer frames the commands and matches the replies over
<x16/serial.h>'s primitives; it is not the ESP32 firmware.

     x16_zi_init(x16_ser_uart0(), X16_SER_BAUD_115200);
     x16_zi_cmd("atw\"myssid,password\"");    // join a network
     x16_zi_wait_ok();
     x16_zi_get_ip(ip);                       // "192.168.1.23"

zi_init leaves the same UART selected that x16_ser_init() did, so the
x16_ser_* calls keep working alongside these -- stream data with them
once "atd" has a connection up.

EVERY REPLY-READING CALL BLOCKS with no timeout, spinning on the UART
until the modem's answer arrives (only x16_zi_cmd, x16_zi_hexdecode
and x16_zi_delay never read). They are for a board that is really
there: x16_ser_detect() first. Command strings are sent as-is plus
CR/LF -- ZiModem is case-insensitive, but send 7-bit ASCII.

### `void x16_zi_init (unsigned int uart, unsigned int divisor)`

Program the UART (as x16_ser_init does), wake the ESP32, abort any
stream left running, apply the standard config (echo off, verbose
result codes, stream mode) and wait for its "OK". Takes a couple of
hundred ms in settle delays. Blocks.

### `void x16_zi_reset (void)`

ATZ: return the modem to its saved profile. Blocks.

### `void x16_zi_cmd (const char *cmd)`

Send one AT command line, CR/LF appended -- pure transmit, so follow
with x16_zi_wait_ok() (or your own read) to consume the reply.

### `void x16_zi_wait_ok (void)`

Read and discard the reply up to and including "OK\r\n". Blocks.

### `void x16_zi_get_ip (char *buf)`

Fetch the current IPv4 address into buf (>= 25 bytes) as a
NUL-terminated dotted-quad. Blocks.

### `unsigned char x16_zi_hex_open (const char *name)`
### `unsigned char x16_zi_hex_chunk (unsigned char *buf)`
### `void x16_zi_hex_close (void)`

Hex-mode file download: open a filename/URL, pull chunks until one
comes back empty, close.

     if (x16_zi_hex_open(url) == 0) {
         while ((n = x16_zi_hex_chunk(buf)) != 0)
             use(buf, n);
         x16_zi_hex_close();
     }

open returns 0 when the transfer started, 1 if the file was not
found. chunk decodes one line -- up to 44 raw bytes -- into buf and
returns the count, 0 when the file is done. All three block.

### `unsigned char x16_zi_hexdecode (unsigned char *dest, const char *src, unsigned char ndigits)`

The hex-mode payload decoder: `ndigits` (even) uppercase ASCII hex
digits from src pack into ndigits/2 bytes at dest. Returns the byte
count. Pure computation -- never touches the UART.

### `void x16_zi_delay (unsigned char ticks)`

A coarse busy-wait, ~40 ms per tick at 8 MHz -- self-contained, so it
needs neither the jiffy IRQ nor the KERNAL.

---

## `x16/audiorom.h` — the AUDIO ROM bank's API, wrapped

The X16 ROM ships an audio driver in its own bank: FM patches, PSG
and YM volume/pan/attenuation shadows, note conversion tables, and
the BASIC FMPLAY/PSGPLAY engine. These wrappers cross into that bank
with the KERNAL's JSRFAR, so they are safe to call from anywhere.

The ar_ layer and this library's own x16_psg_* / x16_ym_* modules
both drive the same hardware, but only the ROM keeps shadows of what
it wrote. Pick one layer per voice: raw x16_psg_* writes are
invisible to x16_ar_psg_read() and will not be re-applied by the
ROM's attenuation arithmetic.

     x16_ar_audio_init();
     x16_ar_ym_playnote(0, x16_ar_note_midi2fm(69), 0, 0);

Unless noted otherwise a return of 0 means success and 1 means the
ROM reported failure (a YM busy timeout, an out-of-range input).

BLOCKING: x16_ar_fmplaystring() and x16_ar_psgplaystring() play the
whole string before returning, pacing themselves on the 60 Hz jiffy
clock -- which only ticks while the VSYNC interrupt is running. The
chordstring calls strike their notes and return immediately.

x16_ar_ym_get_chip_type() results.

```c
#define X16_AR_YM_NONE          0
#define X16_AR_YM_OPP           1
#define X16_AR_YM_OPM           2
#define X16_AR_YM_UNKNOWN       3
```

Pan values for the setpan calls.

```c
#define X16_AR_PAN_OFF          0
#define X16_AR_PAN_LEFT         1
#define X16_AR_PAN_RIGHT        2
#define X16_AR_PAN_BOTH         3
```

### `unsigned char x16_ar_audio_init (void)`

One-call setup: YM init, PSG init, default FM patches on all eight
channels. Equivalent to ym_init + psg_init + ym_loaddefpatches.

### `unsigned char x16_ar_fmfreq (unsigned char channel, unsigned int hz, unsigned char noretrigger)`
### `unsigned char x16_ar_fmnote (unsigned char channel, unsigned char octnote, unsigned char kf, unsigned char noretrigger)`
### `unsigned char x16_ar_fmvib (unsigned char speed, unsigned char depth)`
### `unsigned char x16_ar_psgfreq (unsigned char voice, unsigned int hz)`
### `unsigned char x16_ar_psgnote (unsigned char voice, unsigned char octnote, unsigned char kf)`
### `unsigned char x16_ar_psgwav (unsigned char voice, unsigned char waveform)`

BASIC-compatible helpers. `octnote` packs (octave << 4) | note with
note 1-12 (1 = C); note 0 releases the channel. `noretrigger` nonzero
changes pitch without restarting the envelope.

### `void x16_ar_playstring_voice (unsigned char voice)`
### `void x16_ar_fmplaystring (const char *s, unsigned char len)`
### `void x16_ar_psgplaystring (const char *s, unsigned char len)`
### `void x16_ar_fmchordstring (const char *s, unsigned char len)`
### `void x16_ar_psgchordstring (const char *s, unsigned char len)`

Play-string engine (FMPLAY/PSGPLAY syntax). The playstring calls
BLOCK until the music ends -- see the header comment.

CHARSET TRAP: the ROM parser matches note letters against $41-$5A,
the codes BASIC strings use. cc65's default charmap translates an
uppercase literal to shifted PETSCII ($C3 for 'C'), which the parser
will NOT recognise. Include <ascii_charmap.h> before the literal, or
spell the string in hex.

### `unsigned char x16_ar_note_bas2fm (unsigned char octnote)`
### `unsigned char x16_ar_note_bas2midi (unsigned char octnote)`
### `unsigned int x16_ar_note_bas2psg (unsigned char octnote, unsigned char kf)`
### `unsigned char x16_ar_note_fm2bas (unsigned char kc)`
### `unsigned char x16_ar_note_fm2midi (unsigned char kc)`
### `unsigned int x16_ar_note_fm2psg (unsigned char kc, unsigned char kf)`
### `unsigned int x16_ar_note_freq2bas (unsigned int hz)`
### `unsigned int x16_ar_note_freq2fm (unsigned int hz)`
### `unsigned int x16_ar_note_freq2midi (unsigned int hz)`
### `unsigned int x16_ar_note_freq2psg (unsigned int hz)`
### `unsigned char x16_ar_note_midi2bas (unsigned char midinote)`
### `unsigned char x16_ar_note_midi2fm (unsigned char midinote)`
### `unsigned int x16_ar_note_midi2psg (unsigned char midinote, unsigned char kf)`
### `unsigned int x16_ar_note_psg2bas (unsigned int freq)`
### `unsigned int x16_ar_note_psg2fm (unsigned int freq)`
### `unsigned int x16_ar_note_psg2midi (unsigned int freq)`

Note conversions, between four pitch spaces: BASIC oct/note bytes,
MIDI note numbers, YM KC/KF pairs and 17-bit-VERA PSG frequencies.
An out-of-range input converts to 0.

The word-returning calls pack two bytes: low byte = the note/KC,
high byte = KF (the fractional semitone), except the psg-frequency
returns, which are one 16-bit number.
KERNAL DEFECTS in the audio bank, found while testing this wrapper
against x16-rom r49. The wrappers pass arguments through correctly;
these are the ROM's own, so they are documented rather than papered
over, and test_ca65/runner8.c pins each one:

  notecon_midi2bas  indexes the bas2midi table instead of midi2bas,
                    so x16_ar_note_midi2bas() returns the wrong note
                    (MIDI 60 gives 59, not 65). bas2midi is fine.
  psg_getatten      loads the value into .A, then RESTORE_BANK
                    overwrites .A with the saved RAM bank, and only
                    then copies to .X -- so x16_ar_psg_getatten()
                    hands back the RAM bank current before the call.
                    psg_getpan does its copy first and is correct.

BASIC oct/note, while you are here, is octave*16 + note + 1: twelve
notes then four unused codes per octave, so middle C is 65 and the
codes in the gaps convert to 0.

### `void x16_ar_psg_init (void)`
### `void x16_ar_psg_playfreq (unsigned char voice, unsigned int freq)`
### `void x16_ar_psg_setfreq (unsigned char voice, unsigned int freq)`
### `void x16_ar_psg_setvol (unsigned char voice, unsigned char vol)`
### `void x16_ar_psg_setatten (unsigned char voice, unsigned char atten)`
### `void x16_ar_psg_setpan (unsigned char voice, unsigned char pan)`
### `unsigned char x16_ar_psg_getatten (unsigned char voice)`
### `unsigned char x16_ar_psg_getpan (unsigned char voice)`

ROM PSG layer. Registers are the PSG's own map: 4 per voice --
0 freq low, 1 freq high, 2 = L/R gate | volume, 3 = waveform | duty.

### `void x16_ar_psg_write (unsigned char reg, unsigned char value)`
### `void x16_ar_psg_write_fast (unsigned char reg, unsigned char value)`
### `unsigned char x16_ar_psg_read (unsigned char reg, unsigned char cooked)`

Raw register access that still keeps the ROM's shadows coherent.
`cooked` nonzero reads volumes with attenuation applied; zero reads
back exactly what was written. x16_ar_psg_write_fast() assumes the
caller already pointed VERA at the PSG -- it is the bare fast path.

### `unsigned char x16_ar_ym_init (void)`
### `unsigned char x16_ar_ym_loaddefpatches (void)`

ROM YM/FM layer. `kc` is the YM2151 key code, `kf` the key fraction.

### `unsigned char x16_ar_ym_loadpatch (unsigned char channel, unsigned char patch)`
### `unsigned char x16_ar_ym_loadpatch_ram (unsigned char channel, const void *patch)`

Load one of the 32 ROM patches, or a 26-byte patch image from RAM.

### `unsigned char x16_ar_ym_loadpatchlfn (unsigned char channel, unsigned char lfn)`
### `unsigned char x16_ar_ym_playnote (unsigned char channel, unsigned char kc, unsigned char kf, unsigned char noretrigger)`
### `unsigned char x16_ar_ym_playdrum (unsigned char channel, unsigned char midinote)`
### `unsigned char x16_ar_ym_setnote (unsigned char channel, unsigned char kc, unsigned char kf)`
### `unsigned char x16_ar_ym_setdrum (unsigned char channel, unsigned char midinote)`
### `unsigned char x16_ar_ym_trigger (unsigned char channel, unsigned char noretrigger)`
### `unsigned char x16_ar_ym_release (unsigned char channel)`
### `unsigned char x16_ar_ym_setatten (unsigned char channel, unsigned char atten)`
### `unsigned char x16_ar_ym_setpan (unsigned char channel, unsigned char pan)`
### `unsigned char x16_ar_ym_getatten (unsigned char channel)`
### `unsigned char x16_ar_ym_getpan (unsigned char channel)`

Load a patch from an already-OPENed logical file.

### `unsigned char x16_ar_ym_write (unsigned char reg, unsigned char value)`
### `unsigned char x16_ar_ym_read (unsigned char reg, unsigned char cooked)`

Raw YM register access through the ROM, which is what keeps its
shadows honest -- x16_ym_write() in ym.h does not. `cooked` nonzero
reads TL values with attenuation applied.

### `unsigned char x16_ar_ym_get_chip_type (void)`

Which chip the ROM detected at init: X16_AR_YM_*.

---

## `x16/wavfile.h` — parse a WAV/RIFF header

Reads a RIFF/WAVE header out of a memory buffer and publishes the
PCM format, so the numbers can go straight to the PCM layer in
x16/pcm.h. Parsing from RAM keeps this independent of how the file
was read (x16_fs_load(), MACPTR, a RAM bank...); the caller streams
the bulk sample data itself.

     unsigned char buf[...];              // >= header + data start
     x16_wav_info  wav;

     if (x16_wav_parse_header(buf)) {
         x16_wav_get_info(&wav);
         x16_pcm_rate(0);
         x16_pcm_ctrl(X16_PCM_VOLUME(15)
                      | (wav.channels == 2 ? X16_PCM_STEREO : 0)
                      | (wav.bits == 16 ? X16_PCM_16BIT : 0));
         x16_pcm_stream_start(buf + wav.data_off,
                              (unsigned int)wav.data_len, 64);
     }

Only the header is validated: format code 1 is PCM, anything else
(IMA ADPCM is 17 -- see x16/adpcm.h) is reported, not rejected.
The buffer must hold everything up to and including the start of the
"data" chunk header; for a canonical WAV that is the first 44 bytes.

### `unsigned char x16_wav_parse_header (const void *buf)`

 The published header fields. Block-copied from the assembly module,
 so the order is load-bearing. Do not reorder.

typedef struct {
    unsigned char format;       /* audio format code: 1 = integer PCM
    unsigned char channels;     /* 1 = mono, 2 = stereo
    unsigned long rate;         /* sample rate in Hz
    unsigned char bits;         /* bits per sample: 8 or 16
    unsigned int  data_off;     /* byte offset of the samples in the buffer
    unsigned long data_len;     /* sample data length in bytes
} x16_wav_info;

 Parse the header in `buf`. Returns 1 on success with the fields
 published, 0 if the buffer is not RIFF/WAVE, has a data chunk before
 its fmt chunk, or runs a kilobyte of chunks without finding data.

### `void x16_wav_get_info (x16_wav_info *out)`

What the last successful parse found.

### `unsigned long x16_wav_rate (void)`
### `unsigned long x16_wav_data_len (void)`

Shortcuts for the two most-wanted fields.

---

## `x16/zsm.h` — compact ZSM stream player

Plays ZSM revision 1 music streams (the Commander X16 community's
standard tracker export: PSG writes, YM2151 writes, delays, a loop
point) resident in normal 16-bit address space.

     if (x16_zsm_init(song) == 0) {
         for (;;) {
             ...once per tick -- x16_zsm_get_tickrate() Hz...
             if (!(x16_zsm_tick() & X16_ZSM_ACTIVE)) break;
         }
     }

The player advances only when x16_zsm_tick() is called: hook it to a
VSYNC handler's flag or a timer yourself. Tick from the MAIN LOOP --
the player uses the library's shared zero-page scratch, which an
interrupt handler must not touch (see x16/x16.h).

PCM: EXTCMD channel-0 commands set VERA's AUDIO_CTRL/AUDIO_RATE, and
command 2 triggers instruments from the file's optional PCM table
(memory-resident samples up to 64 KB offsets/lengths) through the
AFLOW streamer of x16/pcm.h. A file whose PCM table is present but
unsupported fails x16_zsm_init() with X16_ZSM_ERR_PCM.

x16_zsm_init() results, also remembered for x16_zsm_lasterr().

```c
#define X16_ZSM_ERR_NONE        0
#define X16_ZSM_ERR_MAGIC       1   /* not a ZSM file */
#define X16_ZSM_ERR_VERSION     2   /* a revision newer than 1 */
#define X16_ZSM_ERR_RANGE       3   /* loop/PCM offset needs >16 bits */
#define X16_ZSM_ERR_PCM         4   /* PCM table present but unusable */
```

Status bits from x16_zsm_status() and x16_zsm_tick().

```c
#define X16_ZSM_ACTIVE          0x01
#define X16_ZSM_LOOP            0x02
#define X16_ZSM_EOF             0x04
```

### `unsigned char x16_zsm_init (const void *header)`

Initialize from a ZSM file image (16-byte header first). Returns 0
and starts in the playing state, or an X16_ZSM_ERR_* code. Only
16-bit loop offsets are supported.

### `unsigned char x16_zsm_lasterr (void)`

Why the last x16_zsm_init() failed -- X16_ZSM_ERR_NONE after one
that worked. The init already returns this code; this keeps it
readable afterwards.

### `void x16_zsm_init_stream (const void *stream, const void *loop)`

Initialize from a raw headerless command stream. `loop` is the
address to rewind to at EOF, or NULL to just stop. The tick rate
defaults to 60.

### `void x16_zsm_play (void)`
### `void x16_zsm_stop (void)`
### `void x16_zsm_rewind (void)`

Pause / resume / restart. x16_zsm_stop() also stops any PCM stream
and silences the DAC; what the PSG and YM are holding keeps
sounding -- silence those with their own APIs if you need to.

### `unsigned int x16_zsm_get_tickrate (void)`

The header's tick rate in Hz (usually 60).

### `unsigned char x16_zsm_status (void)`
### `unsigned char x16_zsm_tick (void)`

X16_ZSM_* bits; x16_zsm_tick() advances playback by one tick first.
A finished, non-looping stream reads X16_ZSM_EOF and stays inactive.

### `unsigned char x16_zsm_pcm_present (void)`

1 if the loaded file carries a usable PCM instrument table.

### `void x16_zsm_pcm_trigger (unsigned char instrument)`

Manually fire a PCM instrument from that table (the stream's EXTCMD
command 2 does the same). Out-of-range indexes and unsupported
samples are silently ignored.

---

## `x16/vdc.h` — VERA display composer helpers

The display composer is VERA's own output stage: video mode, layer
enables, pixel scaling, border colour and the active display window
($9F29-$9F2C behind DCSEL 0/1). This drives VERA itself -- not the
C128's 8563 chip that the VDC name usually means.

     x16_vdc_set_scale(0x40, 0x40);        // 2x pixels: 320x240
     x16_vdc_set_border(6);                // blue border
     x16_vdc_set_active(32, 608, 24, 456); // letterbox the picture

Every call leaves DCSEL at 0, the state the rest of the library
assumes, so these can be mixed freely with the other x16_* video
calls.

DC_VIDEO bits, for get/set_video and set_layers/layer_on/off.

```c
#define X16_VDC_OUTPUT_OFF      0x00
#define X16_VDC_OUTPUT_VGA      0x01
#define X16_VDC_OUTPUT_NTSC     0x02
#define X16_VDC_OUTPUT_RGB      0x03
#define X16_VDC_CHROMA_DISABLE  0x04
#define X16_VDC_240P            0x08
#define X16_VDC_LAYER0          0x10
#define X16_VDC_LAYER1          0x20
#define X16_VDC_SPRITES         0x40
#define X16_VDC_FIELD           0x80    /* read-only interlace field bit */
```

### `unsigned char x16_vdc_get_video (void)`
### `void x16_vdc_set_video (unsigned char video)`

The raw DC_VIDEO byte. Setting ignores bit 7 (FIELD is read-only).

### `void x16_vdc_set_output (unsigned char mode)`
### `void x16_vdc_set_layers (unsigned char mask)`
### `void x16_vdc_layer_on (unsigned char mask)`
### `void x16_vdc_layer_off (unsigned char mask)`

Change only the output mode bits (X16_VDC_OUTPUT_*), or only the
three enable bits. x16_vdc_set_layers() replaces all three at once;
layer_on/layer_off set and clear the ones in `mask`.

### `unsigned int x16_vdc_get_scale (void)`
### `void x16_vdc_set_scale (unsigned char hscale, unsigned char vscale)`

Pixel scaling: $80 = one output pixel per input pixel (640x480),
$40 = 2x (320x240), $20 = 4x. The get packs HSCALE in the low byte,
VSCALE in the high.

### `unsigned char x16_vdc_get_border (void)`
### `void x16_vdc_set_border (unsigned char index)`
### `void x16_vdc_get_active_raw (x16_vdc_active *out)`
### `void x16_vdc_set_active_raw (const x16_vdc_active *in)`
### `void x16_vdc_set_active (unsigned int hstart, unsigned int hstop, unsigned int vstart, unsigned int vstop)`
### `void x16_vdc_fullscreen (void)`
### `unsigned char x16_vdc_get_version (x16_vdc_version *out)`

The border palette index, shown outside the active window.

---

## `x16/int16.h` — 16-bit integer arithmetic

WHY THIS EXISTS IN A C LIBRARY. C has a native 16-bit int, and for a
plain a + b the compiler's operator is the right tool -- x16_i16_add()
computes nothing cc65's `+` does not. The module is here anyway, for
two reasons. First, the project ships FULL parity with the upstream
assembly library, so an assembly-side caller and a C-side caller see
the same surface and the same tested code paths. Second, the
composites really do earn their keep in C: x16_i16_divmod() hands
back quotient AND remainder from one division (cc65's `/` and `%`
divide twice), x16_i16_sqrt() is an integer square root the language
simply lacks, and x16_i16_to_dec() renders decimal without printf's
footprint.

The single-shift, compare and widening entries are the parity layer:
use them when you want the library's exact semantics (documented
per function), use the C operator when you just want arithmetic.

### `int x16_i16_from_u8 (unsigned char v)`

Widening. A C cast does the same; the parity entries exercise the
library's own path.

### `int x16_i16_from_s8 (signed char v)`

zero-extend

### `int x16_i16_add (int a, int b)`
### `int x16_i16_sub (int a, int b)`
### `int x16_i16_neg (int a)`
### `int x16_i16_abs (int a)`

sign-extend

Two's complement makes add, subtract, negate and multiply identical
for signed and unsigned: pass either, take the bits. x16_i16_mul()
keeps only the low 16 bits of the product -- for the full 32-bit
product use x16_umul16() from <x16/fixed.h>.

### `int x16_i16_mul (int a, int b)`

|-32768| stays -32768

### `int x16_i16_shl (int a)`
### `unsigned int x16_i16_shr (unsigned int a)`
### `int x16_i16_asr (int a)`

Shift by ONE, three ways: left, logical right (zero fill), and
arithmetic right (sign fill). The arithmetic form is the one C
leaves implementation-defined for negative values.

### `signed char x16_i16_cmpu (unsigned int a, unsigned int b)`
### `signed char x16_i16_cmps (int a, int b)`

-1 if a < b, 0 if equal, 1 if a > b.

### `unsigned int x16_i16_divmod (unsigned int a, unsigned int b, unsigned int *rem)`
### `int x16_i16_divmod_s (int a, int b, int *rem)`

One division, both results: the quotient comes back, the remainder
lands in *rem. The signed form truncates toward zero and the
remainder takes the sign of the DIVIDEND, exactly as C's / and %
do: -7 / 2 is -3 remainder -1.

Division by zero changes nothing: the call returns a and *rem is
left untouched.

### `unsigned char x16_i16_sqrt (unsigned int v)`

floor(sqrt(v)), 0..255.

### `char * x16_i16_to_dec (unsigned int v)`
### `char * x16_i16_to_dec_s (int v)`

ASCII decimal ('0'-'9' as $30-$39, '-' as $2D), NUL-terminated, no
leading zeros; the signed form prepends '-' to the magnitude, so
-32768 renders as "-32768". The string lives in a module buffer
that the NEXT CALL OVERWRITES -- copy it out if you need to keep it.

---

## `x16/int32.h` — 32-bit integer arithmetic

WHY THIS EXISTS IN A C LIBRARY. cc65 has a native 32-bit long, and
for a plain a + b the compiler's operator is the right tool. The
module is here anyway: the project ships FULL parity with the
upstream assembly library (its DOUBLE.TXT surface), so both sides
see the same routines and the same tested code paths -- and the
composites earn their keep in C. x16_i32_divmod() hands back
quotient AND remainder from ONE long division, where cc65's `/`
and `%` each run their own -- at 32 bits that second division is
real money -- and x16_i32_to_dec() renders decimal without printf's
footprint.

Signed and unsigned share add, subtract, negate, multiply and the
shifts: two's complement makes them identical. Only comparison,
division and decimal output need to know which you meant -- and the
upstream library carries no signed divide or signed renderer at 32
bits, so neither does this port.

### `long x16_i32_from_u16 (unsigned int v)`

Widening and narrowing. A C cast does the same; the parity entries
exercise the library's own path. x16_i32_to_s16() simply drops the
top two bytes.

### `long x16_i32_from_s16 (int v)`

zero-extend

### `int x16_i32_to_s16 (long v)`

sign-extend

### `long x16_i32_add (long a, long b)`
### `long x16_i32_sub (long a, long b)`
### `long x16_i32_neg (long a)`
### `long x16_i32_abs (long a)`

Modulo 2^32; pass signed or unsigned, take the bits.

### `long x16_i32_mul (long a, long b)`

|-2147483648| stays put

### `long x16_i32_shl (long a)`
### `unsigned long x16_i32_shr (unsigned long a)`
### `long x16_i32_asr (long a)`

Shift by ONE, three ways: left, logical right (zero fill), and
arithmetic right (sign fill).

### `signed char x16_i32_cmpu (unsigned long a, unsigned long b)`
### `signed char x16_i32_cmps (long a, long b)`

-1 if a < b, 0 if equal, 1 if a > b.

### `unsigned long x16_i32_divmod (unsigned long a, unsigned long b, unsigned long *rem)`

One division, both results: the quotient comes back, the remainder
lands in *rem. Division by zero changes nothing: the call returns a
and *rem is left untouched.

### `char * x16_i32_to_dec (unsigned long v)`

ASCII decimal ('0'-'9' as $30-$39), NUL-terminated, no leading
zeros. The string lives in a module buffer that the NEXT CALL
OVERWRITES -- copy it out if you need to keep it.

---

## `x16/double.h` — 64-bit software floating point (binary64)

<x16/float.h> binds the ROM's 5-byte float: about 9 significant
digits, fine for graphics, thin for a calculator. This module is a
from-scratch IEEE-754 double -- 8 bytes, ~15-16 significant digits,
the full 1e+/-308 range -- implemented entirely in software (the ROM
has nothing to lean on), with add/sub/mul/div, comparison, sqrt,
exp/ln/pow, trig, hyperbolics and decimal string I/O.

THE SHAPE MIRRORS <x16/float.h>: everything works on an implicit
floating accumulator (call it DAC), so the API reads as a sequence
of operations rather than as expressions:

     x16_double a, b;
     x16_d_from_s16(7);  x16_d_store(a);
     x16_d_from_s16(2);  x16_d_store(b);
     x16_d_load(a);
     x16_d_div(b);                       // DAC = 3.5
     x16_d_to_str();                     // "3.5"

THE FORMAT IS EXACTLY IEEE-754 binary64, LITTLE-ENDIAN: b[0] is the
low mantissa byte, b[7] holds the sign bit and the top seven
exponent bits. 1.0 is {0,0,0,0,0,0,0xF0,0x3F}. A dump from any
modern machine's `double` is byte-for-byte compatible. Subnormals
are flushed to zero, overflow makes an infinity, and rounding is
round-to-nearest-even. NaN and +/-inf are honoured by every
operation (a NaN compares unordered and answers 1).

COST. Every operation is software 64-bit arithmetic -- hundreds of
times slower than 8.8 fixed point. This is for calculators and
offline precision, not per-frame math; see <x16/fixed.h> for that.

A double in memory: 8 bytes, little-endian IEEE-754 binary64.

```c
#define X16_D_SIZE      8
```

Enough for anything x16_d_to_str() formats: sign, 16 digits, point,
exponent, terminator.

```c
#define X16_D_STRLEN    26
```

### `void x16_d_load (const x16_double m)`
### `void x16_d_store (x16_double m)`

DAC <-> an 8-byte double in memory.

### `void x16_d_neg (void)`
### `void x16_d_abs (void)`

DAC = -DAC, |DAC|. Pure sign-bit operations (so they apply to
infinities and NaNs too).

### `void x16_d_from_s16 (int v)`
### `void x16_d_from_s32 (long v)`
### `long x16_d_to_s32 (void)`

Integer conversions. Both widths convert exactly (32 bits fit the
53-bit mantissa). x16_d_to_s32() truncates toward zero; a NaN or an
infinity answers 0, and an out-of-range value clamps to
2147483647 / -2147483648L (the assembly-level overflow carry is not
visible from C).

### `signed char x16_d_cmp (const x16_double m)`

-1 if DAC < *m, 0 if equal, 1 if DAC > *m. Zeroes of either sign
are equal; a NaN on either side is unordered and answers 1.

### `void x16_d_add (const x16_double m)`
### `void x16_d_sub (const x16_double m)`
### `void x16_d_mul (const x16_double m)`
### `void x16_d_div (const x16_double m)`

DAC op= *m, correctly rounded (round-to-nearest-even). The one
caveat: a subtraction that cancels almost the whole significand can
land 1 ulp loose; ordinary sums are exact to the rounding.

### `void x16_d_pow (const x16_double m)`

DAC = DAC ^ *m, via exp(m * ln DAC). m == 0 answers exactly 1 for
any base; otherwise a base <= 0 yields NaN or inf through the log
(there is no integer-power special case).

### `void x16_d_sqrt (void)`
### `void x16_d_exp (void)`
### `void x16_d_ln (void)`
### `void x16_d_sin (void)`
### `void x16_d_cos (void)`
### `void x16_d_tan (void)`
### `void x16_d_atan (void)`
### `void x16_d_sinh (void)`
### `void x16_d_cosh (void)`
### `void x16_d_tanh (void)`

Each replaces DAC.

sqrt: a bit-hack first guess plus six Newton iterations -- full
  precision, and exact for perfect squares (sqrt(4) is exactly 2);
  negative operands answer NaN.
exp/ln: range-reduced Taylor series. ln(0) = -inf, ln(x<0) = NaN.
sin/cos/tan: argument reduced by pi/2 with a single subtraction, so
  a huge argument loses precision; sin(0) = 0 and cos(0) = 1
  exactly. tan is sin/cos. NaN and inf answer NaN.
atan: folds to [0, tan(pi/12)] then a fast series; +/-inf answers
  +/-pi/2.
sinh/cosh/tanh: built on exp; tanh saturates to +/-1 for |x| >= 20.

### `void x16_d_from_str (const char *s, unsigned char len)`

Parse [+/-] digits [ . digits ] [ (E|e) [+/-] digits ] -- len is an
explicit count, no terminator scan, so a slice of a longer string
works. Scaling by the decimal exponent rounds once per power of
ten, so a long mantissa can land a unit in the last place off; the
exactly-representable values (halves, quarters, small integers)
parse exactly.

THE BYTES ARE ASCII: digits $30-$39, and the exponent marker is $45
'E' or $65 'e'. cc65's -t cx16 target maps C string literals to
PETSCII by default -- include <ascii_charmap.h> (or spell bytes
explicitly) when the literal carries letters.

### `char * x16_d_to_str (void)`

Format DAC as a NUL-terminated ASCII decimal string: fixed notation
while the exponent is in -4..20, scientific "d.dddE+NN" beyond;
trailing zeros stripped; "INF", "-INF" and "NAN" for the specials
(ASCII capitals). Up to 16 significant digits; exact short values
print exactly.

The string lives in a module buffer (X16_D_STRLEN bytes) that the
NEXT CALL OVERWRITES -- copy it out if you need to keep it.

---

## `x16/filepick.h` — a file browser on a panel

A directory panel with a mouse and a keyboard: scrolling, descent
into folders, and one question answered -- which file? The caller
does the rest. It is the same browser in every program that opens it,
which is the point: one set of keys, one look, one copy.

     x16_fp_filter("*.bmx");
     if (x16_fp_open() == X16_FPK_PICK) {
         x16_fp_path(buf, sizeof buf);
     }
     x16_fp_close();

The filter is a ';' list of "*.ext" patterns: "*.prg" lists programs,
"*.bmx;*.png" either kind of picture, "*.*" (or NULL) everything.
Directories are always listed whatever the filter says, or there
would be no way to reach the file you wanted. Matching folds case.

THE ACCESSORS COPY into your buffer rather than lending a pointer.
That is the upstream module's contract, and it is load-bearing: the
assembly module can live in a RAM bank, and a banked module cannot
lend a pointer into its bank -- by the time the caller dereferences
it, the bank is no longer mapped. Every port keeps the same shape.

The keys: cursor up/down/Home move, Enter (or a double click, or 'r')
picks a file or descends into a folder, 'a' (or a right click) is the
ALT gesture, 'h' answers with the directory itself, ESC or Run/Stop
(or the panel's x box) cancels. Editing: 'n' makes a folder, 'e'
renames, 'd' deletes after a y/n confirm, 'c' remembers a file and
'v' writes it into the folder on show.

The panel needs VRAM for its listing cache (2,560 bytes, $12000 by
default) and, if enabled, for the save-under copy (5,712 bytes at 80
columns, $14000 by default) -- both clear of the text map at $1B000.

What x16_fp_open() comes back with.

```c
#define X16_FPK_NONE    0       /* cancelled: ESC, Run/Stop, or the x box */
#define X16_FPK_PICK    1       /* a file was chosen: x16_fp_path() has it */
#define X16_FPK_ALT     2       /* the second gesture: right click, or 'a' */
#define X16_FPK_HERE    3       /* 'h': this DIRECTORY, not a file in it */
```

### `void x16_fp_filter (const char *patterns)`

Configuration. All optional; every string is NUL-terminated and NOT
copied, so it must stay valid while the panel is up. NULL restores
the default named in the comment.


Which files to list, as a ';' list of "*.ext" patterns (NULL = "*.*").

### `void x16_fp_primary (const char *patterns)`

Which of the listed files the caller can act on itself (NULL = the
filter). Anything listed that does NOT match is marked [dat] in the
panel, and x16_fp_is_primary() reports which kind was chosen: a
launcher lists "*.*" with a primary of "*.prg", and hands a data file
to a program rather than running it.

### `void x16_fp_start_dir (const char *path)`

Where the browser opens (NULL = "/").

### `void x16_fp_heading (const char *text)`
### `void x16_fp_footing (const char *text)`

The text in front of the path on the header row (NULL = "files in "),
and the reminder along the bottom of the panel.

### `void x16_fp_style (unsigned char panel, unsigned char bar, unsigned char sel)`

Colour bytes (foreground | background << 4) for the panel body, the
header/footer bars, and the selected row. The defaults are blue on
light grey with an inverted selection. The name prompt is drawn blue
on yellow whatever the style: a field that blends in is a field
nobody sees.

### `void x16_fp_charset (unsigned char charset)`

The charset the panel is drawn in (3 = PET upper/lower, the default).
255 leaves whatever the caller had -- there is no way to ask the
KERNAL which charset is loaded, so the browser cannot put back what
it does not know.

### `void x16_fp_cache (unsigned long vaddr)`

Where the 2,560-byte listing cache lives in VRAM (bit 16 picks the
bank; the default is 0x12000UL). VRAM rather than a RAM bank, and not
by preference: the upstream module can be banked, and a banked module
cannot page a bank into the window it is executing from.

### `void x16_fp_saveunder (unsigned char on, unsigned long vaddr)`

Keep what the panel covers and put it back on x16_fp_close(). The
copy lives in VRAM too (the text map IS VRAM): 5,712 bytes at 80
columns, 0x14000UL by default. A launcher that repaints itself does
not need this; a spreadsheet does.

### `unsigned char x16_fp_open (void)`

The session.


Put the panel up on the starting directory and run it until the user
answers. Returns an X16_FPK_* code. X16_FPK_HERE is for a caller that
wants a PLACE rather than a file: the drive is left standing in the
browsed directory whatever the answer, so a bare filename written
afterwards lands there and x16_fp_dir() names it.

### `unsigned char x16_fp_resume (void)`

The same panel again, same directory, same selection: for a caller
that acted on an X16_FPK_ALT and wants the browser back.

### `void x16_fp_close (void)`

Put back what the panel covered and hide the pointer. The DRIVE stays
in the directory that was being browsed: a caller that needs to be
somewhere else should say so with x16_dos_chdir().

### `void x16_fp_redraw (void)`

Paint the panel again, after a caller has drawn over it.

### `unsigned char x16_fp_path (char *dest, unsigned char size)`

What the caller reads back. Each COPIES into `dest`, always
NUL-terminated and truncated to fit; `size` counts the terminator.
Returns how many characters were copied, terminator aside.


The absolute path of the chosen entry.

### `unsigned char x16_fp_name (char *dest, unsigned char size)`

Just its name, without the directory.

### `unsigned char x16_fp_dir (char *dest, unsigned char size)`

The directory being browsed, which is where the drive was left.

### `unsigned char x16_fp_is_primary (void)`

1 when the chosen entry matches the primary pattern (falling back to
the filter, then to "*.*").

### `unsigned char x16_fp_match (const char *name, const char *patterns)`

Does a name match a ';' list of patterns? The same matcher the panel
filters with, exposed because a caller often wants to ask it about a
name of its own. A NULL pattern list matches everything.

### `unsigned char x16_fp_panel_top (void)`
### `unsigned char x16_fp_panel_left (void)`
### `unsigned char x16_fp_panel_width (void)`
### `unsigned char x16_fp_panel_rows (void)`

The panel's geometry, for a caller drawing inside it. Valid once
x16_fp_open() has run: the panel sizes itself to the screen it finds
(80x60 or 40x30).

---

## Where to go next

- **`examples/`** — `hello.c`, `bounce.c` (sprites + fixed point) and
  `numbers.c` (the float library), all buildable with either toolchain.
- **The [README](../README.md)** — the "Things the hardware will get
  you wrong" section is worth reading before debugging anything
  strange.
- **Tests** — `test_ca65/runner.c` exercises every function in this
  guide and doubles as a second source of usage examples.
- For **music** (rather than sound effects), use
  [ZSMKit](https://github.com/mooinglemur/zsmkit) alongside this
  library.
