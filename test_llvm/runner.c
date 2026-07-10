/* =====================================================================
 * x16clib :: test_llvm/runner.c -- the llvm-mos regression suite
 * =====================================================================
 * Same discipline as the cc65 suite: drive the library one way, verify
 * through an INDEPENDENT path. Writes go through a library call on port
 * 0; every read back comes from the SDK's own vpeek(), which sets up its
 * own address. A bug in the address plumbing therefore cannot hide
 * behind itself.
 *
 * Test fixtures are poisoned with t_vpoke() rather than the SDK's
 * vpoke(), which is broken in llvm-mos-sdk v23.0.1 -- see testlib.h.
 *
 * The ABI tests matter more here than anywhere. llvm-mos passes argument
 * bytes left to right in A, X, __rc2, __rc3, ..., and a shim that reads
 * them in the wrong order does not crash -- it quietly does the wrong
 * thing. Each ABI test is built so that ANY transposition changes the
 * observable result.
 * ===================================================================== */

#include "testlib.h"
#include <cx16.h>
#include <x16/x16.h>

/* Far from anything the KERNAL or the tests themselves use. */
#define TESTVRAM    0x08000UL

/* ------------------------------------------------------------------ */
/* zero page                                                           */
/* ------------------------------------------------------------------ */

/* The linker places the scratch block; the value is not fixed. What must
** hold is that it landed inside the window commodore.ld leaves free,
** $26-$7F. Below $26 it would be sitting on an imaginary register.
*/
static void test_zp_base(void)
{
    unsigned char base = x16_zp_base();

    t_check(base >= 0x26 && base <= 0x7F - 15, "ZP_BASE");
}

/* ------------------------------------------------------------------ */
/* vera                                                                */
/* ------------------------------------------------------------------ */

static void test_vera_fill(void)
{
    unsigned char i, ok = 1;

    for (i = 0; i < 8; ++i) {
        t_vpoke(0x00, TESTVRAM + i);
    }
    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0xA5, 8);

    for (i = 0; i < 8; ++i) {
        if (vpeek(TESTVRAM + i) != 0xA5) { ok = 0; }
    }
    t_check(ok, "VERA_FILL");
}

/* Exactly `count` bytes and not one more. The cell past the end is
** poisoned so the check cannot pass by reading an already-zero cell.
*/
static void test_vera_fill_count(void)
{
    t_vpoke(0x5A, TESTVRAM + 4);
    t_vpoke(0x5A, TESTVRAM + 5);

    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0xC3, 5);

    t_check(vpeek(TESTVRAM + 4) == 0xC3 &&
            vpeek(TESTVRAM + 5) == 0x5A,
            "VERA_FILL_COUNT");
}

/* A count of zero writes nothing. The naive dex/bne loop writes 65536. */
static void test_vera_fill_zero(void)
{
    t_vpoke(0x77, TESTVRAM);

    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0xEE, 0);

    t_check(vpeek(TESTVRAM) == 0x77, "VERA_FILL_ZERO");
}

static void test_vera_copy(void)
{
    unsigned char i, ok = 1;

    for (i = 0; i < 6; ++i) {
        t_vpoke(0x10 + i, TESTVRAM + i);          /* source */
        t_vpoke(0x00,     TESTVRAM + 0x40 + i);   /* destination, cleared */
    }

    x16_vera_addr0(X16_INC_1, TESTVRAM);            /* read from  */
    x16_vera_addr1(X16_INC_1, TESTVRAM + 0x40);     /* write to   */
    x16_vera_copy(6);

    for (i = 0; i < 6; ++i) {
        if (vpeek(TESTVRAM + 0x40 + i) != (unsigned char)(0x10 + i)) { ok = 0; }
    }
    t_check(ok, "VERA_COPY");
}

/* The probe must answer one bit, not a version number. */
static void test_vera_has_fx(void)
{
    unsigned char fx = x16_vera_has_fx();

    t_check(fx == 0 || fx == 1, "VERA_HAS_FX");
}

/* ------------------------------------------------------------------ */
/* the ABI                                                             */
/* ------------------------------------------------------------------ */

/* x16_vera_addr0(inc, addr): A = inc, X = addr bits 0-7, __rc2 = 8-15,
** __rc3 = 16-23. The two address bytes differ and neither is zero, so
** transposing them lands the write somewhere else entirely.
*/
static void test_abi_addr0_argorder(void)
{
    const unsigned long addr = 0x18E7UL;   /* bank 0, lo $E7, hi $18 */

    t_vpoke(0x00, addr);
    x16_vera_addr0(X16_INC_1, addr);
    /* Write one byte through the port the library just pointed. */
    VERA.data0 = 0x9B;

    t_check(vpeek(addr) == 0x9B, "ABI_ADDR0_ARGORDER");
}

/* The bank bit lives in address bit 16, which arrives in __rc3. A shim
** that drops it writes to the same offset in bank 0.
*/
static void test_abi_addr0_bank(void)
{
    const unsigned long hi = 0x10100UL;    /* bank 1, offset $0100 */
    const unsigned long lo = 0x00100UL;    /* bank 0, same offset  */

    t_vpoke(0x11, lo);
    t_vpoke(0x22, hi);

    x16_vera_addr0(X16_INC_1, hi);
    VERA.data0 = 0xD4;

    t_check(vpeek(hi) == 0xD4 && vpeek(lo) == 0x11, "ABI_ADDR0_BANK");
}

/* x16_vera_fill(value, count): A = value, X = count lo, __rc2 = count hi.
** count = 0x0102 (258). Swapping lo and hi gives 0x0201 (513), which
** would run past the poison byte at +258.
*/
static void test_abi_fill_argorder(void)
{
    t_vpoke(0x3C, TESTVRAM + 258);            /* poison, just past the end */

    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0x6D, 0x0102);

    t_check(vpeek(TESTVRAM)       == 0x6D &&
            vpeek(TESTVRAM + 257) == 0x6D &&
            vpeek(TESTVRAM + 258) == 0x3C,
            "ABI_FILL_ARGORDER");
}

/* x16_vera_copy(count): A = count lo, X = count hi. Same shape: 0x0102
** bytes copied, the byte after the run left alone.
*/
static void test_abi_copy_argorder(void)
{
    x16_vera_addr0(X16_INC_0, TESTVRAM);            /* a constant source */
    t_vpoke(0x81, TESTVRAM);
    t_vpoke(0x19, TESTVRAM + 0x200 + 258);            /* poison */

    x16_vera_addr0(X16_INC_0, TESTVRAM);
    x16_vera_addr1(X16_INC_1, TESTVRAM + 0x200);
    x16_vera_copy(0x0102);

    t_check(vpeek(TESTVRAM + 0x200)       == 0x81 &&
            vpeek(TESTVRAM + 0x200 + 257) == 0x81 &&
            vpeek(TESTVRAM + 0x200 + 258) == 0x19,
            "ABI_COPY_ARGORDER");
}

/* A one-byte return arrives in A. Promote it to int and check the high
** byte is zero -- cc65 needed an explicit `ldx #0` for this; llvm-mos
** must synthesise the widening itself.
*/
static void test_abi_bool_return(void)
{
    int fx = x16_vera_has_fx();

    t_check(fx == 0 || fx == 1, "ABI_BOOL_RETURN");
}

/* ------------------------------------------------------------------ */
/* palette                                                             */
/* ------------------------------------------------------------------ */

#define PAL_LO(i)   vpeek(X16_VRAM_PALETTE + (unsigned long)(i) * 2)
#define PAL_HI(i)   vpeek(X16_VRAM_PALETTE + (unsigned long)(i) * 2 + 1)

/* An entry is two bytes: low = Green<<4 | Blue, high = Red. Index, low
** and high are all different and none is zero, so a shim that transposes
** any two of them is caught.
*/
static void test_pal_set(void)
{
    x16_pal_set(5, 0x0A73);

    t_check(PAL_LO(5) == 0x73 && PAL_HI(5) == 0x0A, "PAL_SET");
}

/* Index 128 is past the 8-bit halfway mark: index*2 carries into the
** address high byte. pal_set folds that carry into VERA_ADDR_M, and it is
** the sort of thing an argument shuffle can quietly break.
*/
static void test_pal_set_high_index(void)
{
    x16_pal_set(200, 0x0C41);

    t_check(PAL_LO(200) == 0x41 && PAL_HI(200) == 0x0C, "PAL_SET_HIGH_INDEX");
}

/* x16_pal_load(src, first, count): A/X = src, __rc2 = first, __rc3 = count.
** Three entries, all distinct, starting at an index whose low byte differs
** from both `first` and `count` -- so mixing up any pair shows.
*/
static void test_pal_load(void)
{
    static const unsigned int src[3] = { 0x0111, 0x0222, 0x0333 };
    unsigned char ok;

    x16_pal_load(src, 100, 3);

    ok = PAL_LO(100) == 0x11 && PAL_HI(100) == 0x01 &&
         PAL_LO(101) == 0x22 && PAL_HI(101) == 0x02 &&
         PAL_LO(102) == 0x33 && PAL_HI(102) == 0x03;

    t_check(ok, "PAL_LOAD");
}

/* A count of zero loads nothing. Without the guard the loop runs 256
** times and shreds the whole palette -- a bug fixed in the assembly
** library, and this is the test that keeps it fixed.
*/
static void test_pal_load_zero(void)
{
    static const unsigned int src[1] = { 0xFFFF };

    x16_pal_set(150, 0x0555);
    x16_pal_load(src, 150, 0);

    t_check(PAL_LO(150) == 0x55 && PAL_HI(150) == 0x05, "PAL_LOAD_ZERO");
}

/* ------------------------------------------------------------------ */
/* screen                                                              */
/* ------------------------------------------------------------------ */

/* The KERNAL's editor colour byte: foreground low nibble, background
** high. Read it directly -- an independent path from the library's write.
*/
#define KERNAL_COLOR   (*(volatile unsigned char *)0x0376)

/* x16_screen_color(fg, bg): A = fg, X = bg, and the internal routine
** already wants exactly that, so the entry point is a fall-through. If
** the two ever swap, this catches it: 3 and 12 are different, and neither
** nibble is symmetric.
*/
static void test_screen_color(void)
{
    unsigned char saved = KERNAL_COLOR;

    x16_screen_color(3, 12);
    t_check(KERNAL_COLOR == (unsigned char)((12 << 4) | 3), "SCREEN_COLOR");

    KERNAL_COLOR = saved;
}

/* locate() then get_cursor() round-trip. Row and column differ, and both
** are non-zero, so a transposed shim reports them the wrong way round.
** Two shims are under test at once: locate rotates A/X into X/Y through
** the stack, and get_cursor takes its second pointer from __rc2/__rc3.
*/
static void test_screen_cursor_roundtrip(void)
{
    unsigned char row = 0xEE, col = 0xEE;

    x16_screen_locate(7, 13);
    x16_screen_get_cursor(&row, &col);

    t_check(row == 7 && col == 13, "SCREEN_CURSOR_ROUNDTRIP");
}

/* Both out-params must be written. A shim that stores through one pointer
** twice would pass the round-trip above if the values happened to agree;
** here the sentinels differ from the answers and from each other.
*/
static void test_screen_get_cursor_both(void)
{
    unsigned char row = 0x5A, col = 0xA5;

    x16_screen_locate(2, 9);
    x16_screen_get_cursor(&row, &col);

    t_check(row == 2 && col == 9 && row != col, "SCREEN_GET_CURSOR_BOTH");
}

/* Mode 0 is the default text mode and is always supported; $FF is not a
** mode at all. The routine answers 1 / 0, not the KERNAL's carry.
*/
static void test_screen_mode(void)
{
    unsigned char ok  = x16_screen_set_mode(0);
    unsigned char bad = x16_screen_set_mode(0xFF);
    unsigned char now = x16_screen_get_mode();

    x16_screen_set_mode(0);
    t_check(ok == 1 && bad == 0 && now == 0, "SCREEN_MODE");
}

/* ------------------------------------------------------------------ */

int main(void)
{
    t_init();

    test_zp_base();

    test_vera_fill();
    test_vera_fill_count();
    test_vera_fill_zero();
    test_vera_copy();
    test_vera_has_fx();

    test_abi_addr0_argorder();
    test_abi_addr0_bank();
    test_abi_fill_argorder();
    test_abi_copy_argorder();
    test_abi_bool_return();

    test_pal_set();
    test_pal_set_high_index();
    test_pal_load();
    test_pal_load_zero();

    test_screen_color();
    test_screen_cursor_roundtrip();
    test_screen_get_cursor_both();
    test_screen_mode();

    t_done();
    return 0;
}
