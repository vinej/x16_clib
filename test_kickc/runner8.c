/* =====================================================================
 * x16clib :: test_kickc/runner8.c -- KERNAL wrapper modules
 * =====================================================================
 * Standalone suite for the seven upstream wrapper modules: keyboard,
 * mouse, clock, i2c, and the graph/fb/console drawing family.
 *
 *      .\build_kickc.ps1 -Test -Source test_ca65\runner7.c
 *
 * Independent-path principle throughout: draw through the ROM's FB/GRAPH
 * cursor machine, verify with cc65's t_vpeek(0, (unsigned int)()) -- a VRAM address path the
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

#include <x16/zpsafe.h>
#include "testlib.h"
#include <x16/input.h>
#include <x16/keyboard.h>
#include <x16/mouse.h>
#include <x16/clock.h>
#include <x16/i2c.h>
#include <x16/graph.h>
#include <x16/fb.h>
#include <x16/console.h>
#include <x16/screen.h>

/* The independent VRAM path, written by hand here exactly as the other
** KickC runners do, so a bug in the library cannot hide behind itself.
** Every coordinate this suite touches is below 64 KB: bank 0 covers all
** of them. */
void t_vsetaddr(unsigned char bank, unsigned int addr) {
    asm {
        lda #$01
        trb $9f25
        lda addr
        sta $9f20
        lda addr+1
        sta $9f21
        lda bank
        and #$01
        sta $9f22
    }
}

unsigned char t_vpeek(unsigned char bank, unsigned int addr) {
    char r;
    t_vsetaddr(bank, addr);
    asm { lda $9f23 sta r }
    return r;
}

void t_vpoke(unsigned char bank, unsigned int addr, unsigned char v) {
    t_vsetaddr(bank, addr);
    asm { lda v sta $9f23 }
}

/* cc65's <cx16.h> gives a struct view of VERA. KickC has none, and it
** reaches hardware through asm rather than a pointer deref, so the one
** register this suite reads gets a three-line accessor. */
unsigned char t_l0config(void) {
    char r;
    asm { lda $9f2d sta r }
    return r;
}

/* FB_VRAM addresses run past 64 KB -- 239*320+319 is 76799 -- so the
** bank matters. KickC has no 32-bit arithmetic to spare here, so the
** split is done in 16-bit steps with explicit carry detection.
*/
unsigned char t_fbbank;

unsigned int t_fboff(unsigned int px, unsigned int py) {
    unsigned int a;
    unsigned int b;
    unsigned int s;
    a = py << 8;                        /* py*256, py<=239 -> <=61184 */
    b = py << 6;                        /* py*64,  py<=239 -> <=15296 */
    t_fbbank = 0;
    s = a + b;
    if (s < a) { t_fbbank = 1; }        /* py*320 carried past 16 bits */
    a = s + px;
    if (a < s) { t_fbbank = t_fbbank + 1; }
    return a;
}

/* Split into two statements: C does not fix the order in which a call's
** arguments are evaluated, and t_fbbank is only valid after t_fboff. */
unsigned char t_fbpeek(unsigned int px, unsigned int py) {
    unsigned int off;
    off = t_fboff(px, py);
    return t_vpeek(t_fbbank, off);
}

void t_fbpoke(unsigned int px, unsigned int py, unsigned char v) {
    unsigned int off;
    off = t_fboff(px, py);
    t_vpoke(t_fbbank, off, v);
}

/* KickC has no <string.h>. */
char t_strcmp(const char *a, const char *b) {
    unsigned int i;
    for (i = 0; ; ++i) {
        if (a[i] != b[i]) { return 1; }
        if (a[i] == 0) { return 0; }
    }
}

char t_memcmp(const unsigned char *a, const unsigned char *b, unsigned int n) {
    unsigned int i;
    for (i = 0; i < n; ++i) {
        if (a[i] != b[i]) { return 1; }
    }
    return 0;
}

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

    { char ok_ = 1;
        if (!(X16_KEY_COUNT(p) == 2)) { ok_ = 0; }
        if (!(X16_KEY_CHAR(p) == 'A')) { ok_ = 0; }
        if (!(c1 == 'A')) { ok_ = 0; }
        if (!(c2 == 'B')) { ok_ = 0; }
        t_check(ok_, "KBD_PUT_ROUNDTRIP"); }
}

/* Nothing is held down under the emulator; the held-key bits must all
** read clear. CAPS is a latch, so it is left out of the assertion.
*/
static void test_kbd_modifiers(void)
{
    unsigned char m = x16_kbd_get_modifiers();

    t_check((char)((m & (X16_KBD_MOD_SHIFT | X16_KBD_MOD_ALT | X16_KBD_MOD_CTRL))
            == 0), "KBD_MODIFIERS");
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

    { char ok_ = 1;
        if (!(ok == 1)) { ok_ = 0; }
        if (!(idx0 == idx1)) { ok_ = 0; }
        if (!(t_strcmp(km0, km1) == 0)) { ok_ = 0; }
        t_check(ok_, "KBD_KEYMAP_ROUNDTRIP"); }
}

/* An unknown layout must fail AND leave the current one active. */
static void test_kbd_keymap_bad(void)
{
    unsigned char ok;

    x16_kbd_get_keymap(km0);
    ok = x16_kbd_set_keymap("zz-zz");
    x16_kbd_get_keymap(km1);

    { char ok_ = 1;
        if (!(ok == 0)) { ok_ = 0; }
        if (!(t_strcmp(km0, km1) == 0)) { ok_ = 0; }
        t_check(ok_, "KBD_KEYMAP_BAD"); }
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

    { char ok_ = 1;
        if (!(x <= 639)) { ok_ = 0; }
        if (!(y <= 479)) { ok_ = 0; }
        if (!(b == 0)) { ok_ = 0; }
        if (!(wheel == 0)) { ok_ = 0; }
        t_check(ok_, "MSE_GET"); }
}

/* The new mse_get and the existing mouse_get must tell one story. */
static void test_mse_matches_mouse(void)
{
    unsigned int x1, y1, x2, y2;
    unsigned char b1, b2;

    b2 = x16_mouse_get(&x2, &y2);
    x16_mse_get(&x1, &y1, &b1);

    { char ok_ = 1;
        if (!(x1 == x2)) { ok_ = 0; }
        if (!(y1 == y2)) { ok_ = 0; }
        if (!(b1 == b2)) { ok_ = 0; }
        t_check(ok_, "MSE_MATCHES_MOUSE"); }
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
    unsigned char *tb;

    x16_clock_set_timer(0x123456UL);
    t = x16_clock_get_timer();

    /* KickC has no 32-bit compare fragment, so the delta is checked
    ** bytewise: the counter only ever creeps, so the top two bytes must
    ** be untouched and the low byte within 60 of where it was set
    ** (0x56 + 60 = 0x92, well clear of a wrap). */
    tb = (unsigned char *)&t;
    { char ok_ = 1;
        if (!(tb[2] == 0x12)) { ok_ = 0; }
        if (!(tb[1] == 0x34)) { ok_ = 0; }
        if (!(tb[0] >= 0x56)) { ok_ = 0; }
        if (!(tb[0] - 0x56 < 60)) { ok_ = 0; }
        t_check(ok_, "CLOCK_TIMER"); }
}

/* UDTIM by hand: three ticks are three jiffies, exactly, when no IRQ
** races us -- allow a few more when one does.
*/
static void test_clock_update(void)
{
    unsigned long t;
    unsigned char *tb;

    x16_clock_set_timer(100UL);
    x16_clock_update();
    x16_clock_update();
    x16_clock_update();
    t = x16_clock_get_timer();

    /* Bytewise again (no 32-bit compare): 103..113 all live in the low
    ** byte with the rest zero. */
    tb = (unsigned char *)&t;
    { char ok_ = 1;
        if (!(tb[3] == 0)) { ok_ = 0; }
        if (!(tb[2] == 0)) { ok_ = 0; }
        if (!(tb[1] == 0)) { ok_ = 0; }
        if (!(tb[0] >= 103)) { ok_ = 0; }
        if (!(tb[0] <= 113)) { ok_ = 0; }
        t_check(ok_, "CLOCK_UPDATE"); }
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

    { char ok_ = 1;
        if (!(got.year == 126)) { ok_ = 0; }
        if (!(got.month == 7)) { ok_ = 0; }
        if (!(got.day == 27)) { ok_ = 0; }
        if (!(got.hours == 12)) { ok_ = 0; }
        if (!(got.minutes == 34)) { ok_ = 0; }
        if (!(got.seconds >= 5)) { ok_ = 0; }
        if (!(got.seconds <= 8)) { ok_ = 0; }
        t_check(ok_, "CLOCK_DATE_ROUNDTRIP"); }
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

    { char ok_ = 1;
        if (!(v1 != 0xFFFF)) { ok_ = 0; }
        if (!(v1 == v2)) { ok_ = 0; }
        t_check(ok_, "I2C_SMC_VERSION"); }
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

    { char ok_ = 1;
        if (!(ok1 == 1)) { ok_ = 0; }
        if (!(ok2 == 1)) { ok_ = 0; }
        if (!(r1 == 0xA5)) { ok_ = 0; }
        if (!(r2 == 0x5A)) { ok_ = 0; }
        t_check(ok_, "I2C_NVRAM_RW"); }
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
    { char ok_ = 1;
        if (!(ok == 1)) { ok_ = 0; }
        if (!(buf[0] == 0x77)) { ok_ = 0; }
        if (!(buf[1] == 0x77)) { ok_ = 0; }
        t_check(ok_, "I2C_BATCH_READ"); }

    x16_i2c_read_byte(X16_I2C_RTC, X16_I2C_RTC_NVRAM);  /* reposition */
    buf[0] = buf[1] = 0xEE;
    okf = x16_i2c_batch_read(X16_I2C_RTC, buf, 2, 1);
    { char ok_ = 1;
        if (!(okf == 1)) { ok_ = 0; }
        if (!(buf[0] == 0x77)) { ok_ = 0; }
        if (!(buf[1] == 0xEE)) { ok_ = 0; }
        t_check(ok_, "I2C_BATCH_READ_FIXED"); }
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

    { char ok_ = 1;
        if (!(n == 2)) { ok_ = 0; }
        if (!(r == 0x3C)) { ok_ = 0; }
        t_check(ok_, "I2C_BATCH_WRITE"); }
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

    
    { char ok_ = 1;
        if (!(w == 320)) { ok_ = 0; }
        if (!(h == 240)) { ok_ = 0; }
        if (!(depth == 8)) { ok_ = 0; }
        if (!(t_l0config() == 0x07)) { ok_ = 0; }
        t_check(ok_, "GRAPH_INIT"); }
}

/* Filled rect through GRAPH, verified with vpeek. A GEOS filled shape
** is two-tone: stroke border (7), fill interior (2). Any of the four
** coordinate words misrouted moves an edge and a probe catches it.
*/
static void test_graph_rect(void)
{
    x16_graph_draw_rect(10, 20, 50, 30, 0, 1);

    { char ok_ = 1;
        if (!(t_fbpeek(10, 20) == 7)) { ok_ = 0; }
        if (!(t_fbpeek(59, 49) == 7)) { ok_ = 0; }
        if (!(t_fbpeek(12, 22) == 2)) { ok_ = 0; }
        if (!(t_fbpeek(60, 20) == 0)) { ok_ = 0; }
        if (!(t_fbpeek(10, 50) == 0)) { ok_ = 0; }
        t_check(ok_, /* one past the bottom */
            "GRAPH_RECT"); }
}

static void test_graph_line(void)
{
    x16_graph_draw_line(0, 100, 9, 100);

    { char ok_ = 1;
        if (!(t_fbpeek(0, 100) == 7)) { ok_ = 0; }
        if (!(t_fbpeek(5, 100) == 7)) { ok_ = 0; }
        if (!(t_fbpeek(9, 100) == 7)) { ok_ = 0; }
        if (!(t_fbpeek(10, 100) == 0)) { ok_ = 0; }
        t_check(ok_, "GRAPH_LINE"); }
}

/* A filled oval's interior is the FILL color (2 here); the outline is
** the stroke. GEOS inheritance -- see x16/graph.h.
*/
static void test_graph_oval(void)
{
    x16_graph_draw_oval(200, 50, 20, 10, 1);

    { char ok_ = 1;
        if (!(t_fbpeek(210, 55) == 2)) { ok_ = 0; }
        if (!(t_fbpeek(199, 55) == 0)) { ok_ = 0; }
        t_check(ok_, /* left of the box */
            "GRAPH_OVAL"); }
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

    { char ok_ = 1;
        if (!(t_fbpeek(200, 210) == 7)) { ok_ = 0; }
        if (!(t_fbpeek(207, 210) == 7)) { ok_ = 0; }
        if (!(t_fbpeek(200, 212) == 7)) { ok_ = 0; }
        if (!(t_fbpeek(207, 212) == 2)) { ok_ = 0; }
        if (!(t_fbpeek(203, 213) == 2)) { ok_ = 0; }
        if (!(t_fbpeek(208, 210) == 0)) { ok_ = 0; }
        if (!(t_fbpeek(200, 214) == 0)) { ok_ = 0; }
        t_check(ok_, /* past the last row */
            "GRAPH_MOVE_RECT"); }
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

    { char ok_ = 1;
        if (!(clipped == 0)) { ok_ = 0; }
        if (!(drawn == 1)) { ok_ = 0; }
        if (!(x > 200)) { ok_ = 0; }
        if (!(y == 180)) { ok_ = 0; }
        t_check(ok_, "GRAPH_PUT_CHAR"); }
}

static void test_graph_char_size(void)
{
    static x16_char_size cs;
    unsigned char printable, control;

    cs.width = 0;
    printable = x16_graph_get_char_size('A', 0, &cs);

    cs.style = 0xAA;
    control = x16_graph_get_char_size(X16_CON_ATTR_UNDERLINE, 0, &cs);

    { char ok_ = 1;
        if (!(printable == 1)) { ok_ = 0; }
        if (!(control == 0)) { ok_ = 0; }
        if (!(cs.style == X16_GRAPH_STYLE_UNDERLINE)) { ok_ = 0; }
        t_check(ok_, "GRAPH_CHAR_SIZE"); }
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

    { char ok_ = 1;
        if (!(t_memcmp(src, dst, 8) == 0)) { ok_ = 0; }
        if (!(t_fbpeek(103, 100) == 4)) { ok_ = 0; }
        t_check(ok_, "FB_PIXELS_ROUNDTRIP"); }
}

/* Pattern bits go MSB first: $F0 paints four pixels, spares four. */
static void test_fb_set_8_pixels(void)
{
    x16_fb_cursor_position(50, 50);
    x16_fb_set_8_pixels(0xF0, 9);

    { char ok_ = 1;
        if (!(t_fbpeek(50, 50) == 9)) { ok_ = 0; }
        if (!(t_fbpeek(53, 50) == 9)) { ok_ = 0; }
        if (!(t_fbpeek(54, 50) == 0)) { ok_ = 0; }
        if (!(t_fbpeek(57, 50) == 0)) { ok_ = 0; }
        t_check(ok_, "FB_SET_8_PIXELS"); }
}

/* $CC = %11001100 under a full mask: fg,fg,bg,bg,fg,fg,bg,bg. Swapping
** pattern and mask in the shim scrambles this sequence.
*/
static void test_fb_set_8_pixels_opaque(void)
{
    x16_fb_cursor_position(60, 60);
    x16_fb_set_8_pixels_opaque(0xCC, 0xFF, 3, 4);

    { char ok_ = 1;
        if (!(t_fbpeek(60, 60) == 3)) { ok_ = 0; }
        if (!(t_fbpeek(61, 60) == 3)) { ok_ = 0; }
        if (!(t_fbpeek(62, 60) == 4)) { ok_ = 0; }
        if (!(t_fbpeek(63, 60) == 4)) { ok_ = 0; }
        if (!(t_fbpeek(64, 60) == 3)) { ok_ = 0; }
        if (!(t_fbpeek(67, 60) == 4)) { ok_ = 0; }
        t_check(ok_, "FB_8_PIXELS_OPAQUE"); }
}

/* fill_pixels paints the run; move_pixels carries 10 of them to an
** empty scanline. r0-r4 all have to arrive for both probes to pass.
*/
static void test_fb_fill_move(void)
{
    x16_fb_cursor_position(0, 150);
    x16_fb_fill_pixels(20, 1, 6);
    x16_fb_move_pixels(0, 150, 100, 151, 10);

    { char ok_ = 1;
        if (!(t_fbpeek(0, 150) == 6)) { ok_ = 0; }
        if (!(t_fbpeek(19, 150) == 6)) { ok_ = 0; }
        if (!(t_fbpeek(20, 150) == 0)) { ok_ = 0; }
        if (!(t_fbpeek(100, 151) == 6)) { ok_ = 0; }
        if (!(t_fbpeek(109, 151) == 6)) { ok_ = 0; }
        if (!(t_fbpeek(110, 151) == 0)) { ok_ = 0; }
        t_check(ok_, "FB_FILL_MOVE"); }
}

/* The filter callback is C code called from inside the ROM's unrolled
** loop; the trampoline must keep the ROM's X/Y counters alive across
** it. Four pixels seeded by vpoke, incremented by the filter.
*/
static unsigned char inc_filter(unsigned char c)
{
    return c + 1;
}

static void test_fb_filter(void)
{
    unsigned char i;

    for (i = 0; i < 4; ++i) {
        t_fbpoke(i, 160, 0x11);
    }
    x16_fb_cursor_position(0, 160);
    x16_fb_filter_pixels(4, inc_filter);

    { char ok_ = 1;
        if (!(t_fbpeek(0, 160) == 0x12)) { ok_ = 0; }
        if (!(t_fbpeek(3, 160) == 0x12)) { ok_ = 0; }
        if (!(t_fbpeek(4, 160) == 0x00)) { ok_ = 0; }
        t_check(ok_, "FB_FILTER"); }
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

    { char ok_ = 1;
        if (!(t_fbpeek(5, 170) == 8)) { ok_ = 0; }
        if (!(t_fbpeek(5, 171) == 8)) { ok_ = 0; }
        t_check(ok_, "FB_NEXT_LINE"); }
}

/* Two palette words through FB, read back at VERA's palette base. */
static void test_fb_palette(void)
{
    static const unsigned char pal[4] = { 0x34, 0x02, 0x21, 0x01 };

    x16_fb_set_palette(pal, 16, 2);

    { char ok_ = 1;
        if (!(t_vpeek(1, 0xFA00 + 16 * 2)     == 0x34)) { ok_ = 0; }
        if (!(t_vpeek(1, 0xFA00 + 16 * 2 + 1) == 0x02)) { ok_ = 0; }
        if (!(t_vpeek(1, 0xFA00 + 17 * 2)     == 0x21)) { ok_ = 0; }
        if (!(t_vpeek(1, 0xFA00 + 17 * 2 + 1) == 0x01)) { ok_ = 0; }
        t_check(ok_, "FB_PALETTE"); }
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
    /* unsigned int, not char: KickC has no fragment for widening a
    ** char + int expression into a call's int parameter. */
    unsigned int px, py;
    unsigned char i = 0;

    for (py = 2; py < 10; ++py) {
        for (px = 2; px < 10; ++px) {
            buf[i] = t_fbpeek(px, py);
            i = i + 1;
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

    t_check((char)(t_memcmp(con_snap, after, 64) != 0), "CON_PUT_CHAR");
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

    t_check((char)(c == 'X'), "CON_GET_CHAR");
}

/* ------------------------------------------------------------------ */

int main(void)
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
    return 0;
}
