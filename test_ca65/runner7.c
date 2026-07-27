/* =====================================================================
 * x16clib :: test/runner7.c -- KERNAL wrapper modules
 * =====================================================================
 * Standalone suite for the seven upstream wrapper modules: keyboard,
 * mouse, clock, i2c, and the graph/fb/console drawing family.
 *
 *      .\build_ca65.ps1 -Test -Source test_ca65\runner7.c
 *
 * Independent-path principle throughout: draw through the ROM's FB/GRAPH
 * cursor machine, verify with cc65's vpeek() -- a VRAM address path the
 * library never touches. The i2c tests use the emulator's SMC ($42) and
 * RTC ($6F); the RTC's NVRAM ($20-$5F) is the writable scratch that
 * makes read-back possible without side effects.
 *
 * Headless notes, verified against x16emu's source:
 *  - KBDBUF_PUT injects PETSCII directly, so keyboard tests run headless.
 *  - The jiffy timer does not tick headless (no VSYNC IRQ), so timer
 *    tests assert small deltas that hold both headless and windowed.
 *  - The emulator's RTC does NOT auto-increment the register pointer on
 *    multi-byte reads (rtc_read() keeps i2c_data[0]); real hardware
 *    does. The batch-read test writes the SAME value to two adjacent
 *    NVRAM cells so both behaviors produce the same answer.
 * =====================================================================
 */

#include "testlib.h"
#include <cbm.h>
#include <cx16.h>
#include <string.h>
#include <x16/input.h>
#include <x16/keyboard.h>
#include <x16/mouse.h>
#include <x16/clock.h>
#include <x16/i2c.h>
#include <x16/graph.h>
#include <x16/fb.h>
#include <x16/console.h>
#include <x16/screen.h>

/* The default FB driver's bitmap lives at VRAM $00000, 320 bytes per
** scanline. This is the independent read-back path.
*/
#define FB_VRAM(x, y)   ((unsigned long)(y) * 320u + (x))

/* ------------------------------------------------------------------ */
/* keyboard                                                           */
/* ------------------------------------------------------------------ */

static void kbd_drain(void)
{
    unsigned char guard = 0;

    while (x16_key_get() != 0 && ++guard < 32) {
        /* drop whatever the boot sequence left queued */
    }
}

/* Inject two keys, peek, then consume both. Exercises x16_kbd_scan,
** x16_kbd_put, and the existing peek/get path they feed.
*/
static void test_kbd_put_roundtrip(void)
{
    unsigned int p;
    unsigned char c1, c2;

    x16_kbd_scan();
    kbd_drain();

    x16_kbd_put('A');
    x16_kbd_put('B');
    p = x16_key_peek();
    c1 = x16_key_get();
    c2 = x16_key_get();

    t_check(X16_KEY_COUNT(p) == 2 && X16_KEY_CHAR(p) == 'A' &&
            c1 == 'A' && c2 == 'B', "KBD_PUT_ROUNDTRIP");
}

/* Nothing is held down under the emulator; the held-key bits must all
** read clear. CAPS is a latch, so it is left out of the assertion.
*/
static void test_kbd_modifiers(void)
{
    unsigned char m = x16_kbd_get_modifiers();

    t_check((m & (X16_KBD_MOD_SHIFT | X16_KBD_MOD_ALT | X16_KBD_MOD_CTRL))
            == 0, "KBD_MODIFIERS");
}

/* Ask the ROM for the current layout name, feed that very name back to
** the setter, and read it again. No hardcoded layout string, so no
** assumption about the ROM's spelling or encoding -- if the shims
** mangle the pointer, the bank switch, or the carry, this goes red.
*/
static char km0[X16_KBD_KEYMAP_LEN];
static char km1[X16_KBD_KEYMAP_LEN];

static void test_kbd_keymap_roundtrip(void)
{
    unsigned char idx0, idx1, ok;

    idx0 = x16_kbd_get_keymap(km0);
    if (km0[0] == 0) {
        t_skip("KBD_KEYMAP_ROUNDTRIP");    /* ROM reports no layout name */
        return;
    }
    ok = x16_kbd_set_keymap(km0);
    idx1 = x16_kbd_get_keymap(km1);

    t_check(ok == 1 && idx0 == idx1 && strcmp(km0, km1) == 0,
            "KBD_KEYMAP_ROUNDTRIP");
}

/* An unknown layout must fail AND leave the current one active. */
static void test_kbd_keymap_bad(void)
{
    unsigned char ok;

    x16_kbd_get_keymap(km0);
    ok = x16_kbd_set_keymap("zz-zz");
    x16_kbd_get_keymap(km1);

    t_check(ok == 0 && strcmp(km0, km1) == 0, "KBD_KEYMAP_BAD");
}

/* ------------------------------------------------------------------ */
/* mouse                                                              */
/* ------------------------------------------------------------------ */

/* No mouse is plugged into the headless emulator, but MOUSE_CONFIG and
** MOUSE_GET are pure KERNAL state: show it, read a sane default
** position, no buttons, no wheel, hide it again.
*/
static void test_mse_get(void)
{
    unsigned int x = 0xAAAA, y = 0xAAAA;
    unsigned char b = 0xAA;
    signed char wheel;

    x16_mse_config(1, 0, 0);            /* show cursor 1, keep bounds */
    x16_mse_scan();
    wheel = x16_mse_get(&x, &y, &b);
    x16_mse_config(X16_MSE_HIDE, 0, 0);

    t_check(x <= 639 && y <= 479 && b == 0 && wheel == 0, "MSE_GET");
}

/* The new mse_get and the existing mouse_get must tell one story. */
static void test_mse_matches_mouse(void)
{
    unsigned int x1, y1, x2, y2;
    unsigned char b1, b2;

    b2 = x16_mouse_get(&x2, &y2);
    x16_mse_get(&x1, &y1, &b1);

    t_check(x1 == x2 && y1 == y2 && b1 == b2, "MSE_MATCHES_MOUSE");
}

/* ------------------------------------------------------------------ */
/* clock                                                              */
/* ------------------------------------------------------------------ */

/* All three timer bytes must survive the set/get marshaling. Headless
** the counter stands still (no VSYNC IRQ); windowed it creeps. Either
** way the delta stays tiny.
*/
static void test_clock_timer(void)
{
    unsigned long t;

    x16_clock_set_timer(0x123456UL);
    t = x16_clock_get_timer();

    t_check(t - 0x123456UL < 60, "CLOCK_TIMER");
}

/* UDTIM by hand: three ticks are three jiffies, exactly, when no IRQ
** races us -- allow a few more when one does.
*/
static void test_clock_update(void)
{
    unsigned long t;

    x16_clock_set_timer(100UL);
    x16_clock_update();
    x16_clock_update();
    x16_clock_update();
    t = x16_clock_get_timer();

    t_check(t >= 103 && t <= 113, "CLOCK_UPDATE");
}

/* Write the RTC, read it back. Seconds may tick between the calls, so
** they get a window; the calendar fields must come back exact.
*/
static void test_clock_date_roundtrip(void)
{
    static x16_date_time set;
    static x16_date_time got;

    set.year    = 126;                  /* 2026 */
    set.month   = 7;
    set.day     = 27;
    set.hours   = 12;
    set.minutes = 34;
    set.seconds = 5;
    set.jiffies = 0;
    set.weekday = 1;

    x16_clock_set_date_time(&set);
    x16_clock_get_date_time(&got);

    t_check(got.year == 126 && got.month == 7 && got.day == 27 &&
            got.hours == 12 && got.minutes == 34 &&
            got.seconds >= 5 && got.seconds <= 8,
            "CLOCK_DATE_ROUNDTRIP");
}

/* ------------------------------------------------------------------ */
/* i2c                                                                */
/* ------------------------------------------------------------------ */

/* SMC offsets $30-$32 are the firmware version -- stable, side-effect
** free. Two reads must agree and neither may NAK.
*/
static void test_i2c_smc_version(void)
{
    unsigned int v1 = x16_i2c_read_byte(X16_I2C_SMC, 0x30);
    unsigned int v2 = x16_i2c_read_byte(X16_I2C_SMC, 0x30);

    t_check(v1 != 0xFFFF && v1 == v2, "I2C_SMC_VERSION");
}

/* RTC NVRAM: write, read back, overwrite, read back. Both values must
** land, so a shim that drops the value or the offset cannot pass.
*/
static void test_i2c_nvram_rw(void)
{
    unsigned char ok1, ok2;
    unsigned int r1, r2;

    ok1 = x16_i2c_write_byte(X16_I2C_RTC, X16_I2C_RTC_NVRAM, 0xA5);
    r1  = x16_i2c_read_byte(X16_I2C_RTC, X16_I2C_RTC_NVRAM);
    ok2 = x16_i2c_write_byte(X16_I2C_RTC, X16_I2C_RTC_NVRAM, 0x5A);
    r2  = x16_i2c_read_byte(X16_I2C_RTC, X16_I2C_RTC_NVRAM);

    t_check(ok1 == 1 && ok2 == 1 && r1 == 0xA5 && r2 == 0x5A,
            "I2C_NVRAM_RW");
}

/* Batch read after positioning the device pointer. The same value sits
** in both candidate cells (see the header comment), so the expected
** bytes are the same whether or not the device auto-increments.
** The fixed-pointer mode must park both bytes on buf[0] and leave the
** sentinel at buf[1] alone.
*/
static void test_i2c_batch_read(void)
{
    static unsigned char buf[2];
    unsigned char ok, okf;

    x16_i2c_write_byte(X16_I2C_RTC, X16_I2C_RTC_NVRAM,     0x77);
    x16_i2c_write_byte(X16_I2C_RTC, X16_I2C_RTC_NVRAM + 1, 0x77);
    x16_i2c_read_byte(X16_I2C_RTC, X16_I2C_RTC_NVRAM);  /* position */

    buf[0] = buf[1] = 0xEE;
    ok = x16_i2c_batch_read(X16_I2C_RTC, buf, 2, 0);
    t_check(ok == 1 && buf[0] == 0x77 && buf[1] == 0x77,
            "I2C_BATCH_READ");

    x16_i2c_read_byte(X16_I2C_RTC, X16_I2C_RTC_NVRAM);  /* reposition */
    buf[0] = buf[1] = 0xEE;
    okf = x16_i2c_batch_read(X16_I2C_RTC, buf, 2, 1);
    t_check(okf == 1 && buf[0] == 0x77 && buf[1] == 0xEE,
            "I2C_BATCH_READ_FIXED");
}

/* A 2-byte batch write is {offset, value} on the wire; the value must
** be readable back through the single-byte path.
*/
static void test_i2c_batch_write(void)
{
    static unsigned char msg[2];
    unsigned int n, r;

    msg[0] = X16_I2C_RTC_NVRAM + 2;
    msg[1] = 0x3C;
    n = x16_i2c_batch_write(X16_I2C_RTC, msg, 2);
    r = x16_i2c_read_byte(X16_I2C_RTC, X16_I2C_RTC_NVRAM + 2);

    t_check(n == 2 && r == 0x3C, "I2C_BATCH_WRITE");
}

/* ------------------------------------------------------------------ */
/* graph + fb                                                         */
/* ------------------------------------------------------------------ */

/* graph_init must install the default driver and put VERA in its
** 320x240@8bpp bitmap state. fb_get_info is the driver's own answer;
** layer 0's config register is the independent hardware one (FB_init
** writes $07: bitmap, 8bpp). The KERNAL's screen-mode byte is NOT
** consulted -- GRAPH_INIT bypasses the screen editor and leaves it
** stale, which is why the restore below uses x16_screen_reset().
*/
static void test_graph_init(void)
{
    unsigned int w = 0, h = 0;
    unsigned char depth;

    x16_graph_init(0);
    x16_graph_set_colors(7, 2, 0);
    x16_graph_clear();
    depth = x16_fb_get_info(&w, &h);

    VERA.control = 0;
    t_check(w == 320 && h == 240 && depth == 8 &&
            VERA.layer0.config == 0x07, "GRAPH_INIT");
}

/* Filled rect through GRAPH, verified with vpeek. A GEOS filled shape
** is two-tone: stroke border (7), fill interior (2). Any of the four
** coordinate words misrouted moves an edge and a probe catches it.
*/
static void test_graph_rect(void)
{
    x16_graph_draw_rect(10, 20, 50, 30, 0, 1);

    t_check(vpeek(FB_VRAM(10, 20)) == 7 &&      /* top-left corner: border */
            vpeek(FB_VRAM(59, 49)) == 7 &&      /* bottom-right corner */
            vpeek(FB_VRAM(12, 22)) == 2 &&      /* interior: fill color */
            vpeek(FB_VRAM(60, 20)) == 0 &&      /* one past the right edge */
            vpeek(FB_VRAM(10, 50)) == 0,        /* one past the bottom */
            "GRAPH_RECT");
}

static void test_graph_line(void)
{
    x16_graph_draw_line(0, 100, 9, 100);

    t_check(vpeek(FB_VRAM(0, 100)) == 7 &&
            vpeek(FB_VRAM(5, 100)) == 7 &&
            vpeek(FB_VRAM(9, 100)) == 7 &&
            vpeek(FB_VRAM(10, 100)) == 0, "GRAPH_LINE");
}

/* A filled oval's interior is the FILL color (2 here); the outline is
** the stroke. GEOS inheritance -- see x16/graph.h.
*/
static void test_graph_oval(void)
{
    x16_graph_draw_oval(200, 50, 20, 10, 1);

    t_check(vpeek(FB_VRAM(210, 55)) == 2 &&     /* center: filled */
            vpeek(FB_VRAM(199, 55)) == 0,       /* left of the box */
            "GRAPH_OVAL");
}

/* Lift a slice out of the filled rect's corner and drop it on empty
** ground. The slice carries the rect's two-tone texture -- border 7
** along the top and left, fill 2 inside -- which pins BOTH source
** coordinates, not just "some color arrived". Moving down copies
** height+1 rows (ROM quirk, see x16/graph.h): rows 210-213.
*/
static void test_graph_move_rect(void)
{
    x16_graph_move_rect(10, 20, 200, 210, 8, 3);

    t_check(vpeek(FB_VRAM(200, 210)) == 7 &&    /* top row: border */
            vpeek(FB_VRAM(207, 210)) == 7 &&
            vpeek(FB_VRAM(200, 212)) == 7 &&    /* left column: border */
            vpeek(FB_VRAM(207, 212)) == 2 &&    /* interior: fill */
            vpeek(FB_VRAM(203, 213)) == 2 &&    /* the +1th row moved too */
            vpeek(FB_VRAM(208, 210)) == 0 &&    /* past the right edge */
            vpeek(FB_VRAM(200, 214)) == 0,      /* past the last row */
            "GRAPH_MOVE_RECT");
}

/* set_window must actually clip: a glyph at x=200 is outside a
** 100-wide window (put_char returns 0) and inside the full screen
** (returns 1, and the pen advances).
*/
static void test_graph_window_put_char(void)
{
    unsigned int x, y;
    unsigned char clipped, drawn;

    x16_graph_set_window(0, 0, 100, 100);
    x = 200; y = 180;
    clipped = x16_graph_put_char(&x, &y, 'A');

    x16_graph_set_window(0, 0, 0, 0);
    x = 200; y = 180;
    drawn = x16_graph_put_char(&x, &y, 'A');

    t_check(clipped == 0 && drawn == 1 && x > 200 && y == 180,
            "GRAPH_PUT_CHAR");
}

static void test_graph_char_size(void)
{
    static x16_char_size cs;
    unsigned char printable, control;

    cs.width = 0;
    printable = x16_graph_get_char_size('A', 0, &cs);

    cs.style = 0xAA;
    control = x16_graph_get_char_size(X16_CON_ATTR_UNDERLINE, 0, &cs);

    t_check(printable == 1 && control == 0 &&
            cs.style == X16_GRAPH_STYLE_UNDERLINE, "GRAPH_CHAR_SIZE");
}

/* An 8-pixel run down and back through the cursor machine, then one
** byte cross-checked against VRAM directly.
*/
static void test_fb_pixels_roundtrip(void)
{
    static const unsigned char src[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    static unsigned char dst[8];

    x16_fb_cursor_position(100, 100);
    x16_fb_set_pixels(src, 8);
    x16_fb_cursor_position(100, 100);
    x16_fb_get_pixels(dst, 8);

    t_check(memcmp(src, dst, 8) == 0 &&
            vpeek(FB_VRAM(103, 100)) == 4, "FB_PIXELS_ROUNDTRIP");
}

/* Pattern bits go MSB first: $F0 paints four pixels, spares four. */
static void test_fb_set_8_pixels(void)
{
    x16_fb_cursor_position(50, 50);
    x16_fb_set_8_pixels(0xF0, 9);

    t_check(vpeek(FB_VRAM(50, 50)) == 9 &&
            vpeek(FB_VRAM(53, 50)) == 9 &&
            vpeek(FB_VRAM(54, 50)) == 0 &&
            vpeek(FB_VRAM(57, 50)) == 0, "FB_SET_8_PIXELS");
}

/* $CC = %11001100 under a full mask: fg,fg,bg,bg,fg,fg,bg,bg. Swapping
** pattern and mask in the shim scrambles this sequence.
*/
static void test_fb_set_8_pixels_opaque(void)
{
    x16_fb_cursor_position(60, 60);
    x16_fb_set_8_pixels_opaque(0xCC, 0xFF, 3, 4);

    t_check(vpeek(FB_VRAM(60, 60)) == 3 &&
            vpeek(FB_VRAM(61, 60)) == 3 &&
            vpeek(FB_VRAM(62, 60)) == 4 &&
            vpeek(FB_VRAM(63, 60)) == 4 &&
            vpeek(FB_VRAM(64, 60)) == 3 &&
            vpeek(FB_VRAM(67, 60)) == 4, "FB_8_PIXELS_OPAQUE");
}

/* fill_pixels paints the run; move_pixels carries 10 of them to an
** empty scanline. r0-r4 all have to arrive for both probes to pass.
*/
static void test_fb_fill_move(void)
{
    x16_fb_cursor_position(0, 150);
    x16_fb_fill_pixels(20, 1, 6);
    x16_fb_move_pixels(0, 150, 100, 151, 10);

    t_check(vpeek(FB_VRAM(0, 150)) == 6 &&
            vpeek(FB_VRAM(19, 150)) == 6 &&
            vpeek(FB_VRAM(20, 150)) == 0 &&
            vpeek(FB_VRAM(100, 151)) == 6 &&
            vpeek(FB_VRAM(109, 151)) == 6 &&
            vpeek(FB_VRAM(110, 151)) == 0, "FB_FILL_MOVE");
}

/* The filter callback is C code called from inside the ROM's unrolled
** loop; the trampoline must keep the ROM's X/Y counters alive across
** it. Four pixels seeded by vpoke, incremented by the filter.
*/
static unsigned char __fastcall__ inc_filter(unsigned char c)
{
    return c + 1;
}

static void test_fb_filter(void)
{
    unsigned char i;

    for (i = 0; i < 4; ++i) {
        vpoke(0x11, FB_VRAM(i, 160));
    }
    x16_fb_cursor_position(0, 160);
    x16_fb_filter_pixels(4, inc_filter);

    t_check(vpeek(FB_VRAM(0, 160)) == 0x12 &&
            vpeek(FB_VRAM(3, 160)) == 0x12 &&
            vpeek(FB_VRAM(4, 160)) == 0x00, "FB_FILTER");
}

/* cursor_next_line advances from the POSITIONED point, not from where
** set_pixel's write left the VERA address.
*/
static void test_fb_next_line(void)
{
    x16_fb_cursor_position(5, 170);
    x16_fb_set_pixel(8);
    x16_fb_cursor_next_line(5);
    x16_fb_set_pixel(8);

    t_check(vpeek(FB_VRAM(5, 170)) == 8 &&
            vpeek(FB_VRAM(5, 171)) == 8, "FB_NEXT_LINE");
}

/* Two palette words through FB, read back at VERA's palette base. */
static void test_fb_palette(void)
{
    static const unsigned char pal[4] = { 0x34, 0x02, 0x21, 0x01 };

    x16_fb_set_palette(pal, 16, 2);

    t_check(vpeek(0x1FA00UL + 16 * 2)     == 0x34 &&
            vpeek(0x1FA00UL + 16 * 2 + 1) == 0x02 &&
            vpeek(0x1FA00UL + 17 * 2)     == 0x21 &&
            vpeek(0x1FA00UL + 17 * 2 + 1) == 0x01, "FB_PALETTE");
}

/* ------------------------------------------------------------------ */
/* console                                                            */
/* ------------------------------------------------------------------ */

/* The console draws through GRAPH, so "did anything land" is a VRAM
** scan of the top-left corner: capture it, print, compare.
*/
static unsigned char con_snap[64];

static void con_capture(unsigned char *buf)
{
    unsigned char x, y, i = 0;

    for (y = 0; y < 8; ++y) {
        for (x = 0; x < 8; ++x) {
            buf[i++] = vpeek(FB_VRAM(x + 2, y + 2));
        }
    }
}

static void test_con_put_char(void)
{
    static unsigned char after[64];

    x16_con_init(0, 0, 0, 0);
    x16_con_disable_paging();
    con_capture(con_snap);

    x16_con_put_char('H', 0);
    x16_con_put_char('i', 1);
    con_capture(after);

    t_check(memcmp(con_snap, after, 64) != 0, "CON_PUT_CHAR");
}

/* Line input, fed through the keyboard buffer: "X" + RETURN. The first
** con_get_char hands the line's first character back.
*/
static void test_con_get_char(void)
{
    unsigned char c;

    x16_kbd_put('X');
    x16_kbd_put(13);
    c = x16_con_get_char();

    t_check(c == 'X', "CON_GET_CHAR");
}

/* ------------------------------------------------------------------ */

void main(void)
{
    t_init();

    test_kbd_put_roundtrip();
    test_kbd_modifiers();
    test_kbd_keymap_roundtrip();
    test_kbd_keymap_bad();

    test_mse_get();
    test_mse_matches_mouse();

    test_clock_timer();
    test_clock_update();
    test_clock_date_roundtrip();

    test_i2c_smc_version();
    test_i2c_nvram_rw();
    test_i2c_batch_read();
    test_i2c_batch_write();

    test_graph_init();
    test_graph_rect();
    test_graph_line();
    test_graph_oval();
    test_graph_move_rect();
    test_graph_window_put_char();
    test_graph_char_size();

    test_fb_pixels_roundtrip();
    test_fb_set_8_pixels();
    test_fb_set_8_pixels_opaque();
    test_fb_fill_move();
    test_fb_filter();
    test_fb_next_line();
    test_fb_palette();

    test_con_put_char();
    test_con_get_char();

    x16_screen_reset();         /* full CINT: GRAPH_INIT leaves the
                                ** KERNAL's mode byte stale, so only a
                                ** reset reliably restores text */

    t_done();
}
