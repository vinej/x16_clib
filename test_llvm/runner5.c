/* =====================================================================
 * x16clib :: test_llvm/runner5.c -- UTIL modules: sort, bcd, bits,
 *                                   number, tscrunch
 * =====================================================================
 * The llvm-mos port of the ca65 suite's runner5.c: same checks, same
 * names, so the two builds' results line up. Build and run with
 *
 *      .\build_llvm.ps1 -Test -Source test_llvm\runner5.c
 *
 * Same harness contract as the main suite: testlib.h prints the
 * PASS/FAIL/DONE lines build_llvm.ps1's stdout watcher greps for.
 *
 * The TSCrunch fixtures are the upstream x16_library's own: the packed
 * bytes are the literal output of Antonio Savona's `tscrunch` tool over
 * the phrase and RLE payloads (test_ca65/runner.asm in x16_library),
 * so this checks our decoder against the real encoder, not against a
 * hand-built stream.
 * =====================================================================
 */

#include "testlib.h"
#include <x16/sort.h>
#include <x16/bcd.h>
#include <x16/bits.h>
#include <x16/number.h>
#include <x16/tscrunch.h>

/* ------------------------------------------------------------------ */
/* helpers                                                            */
/* ------------------------------------------------------------------ */

static unsigned char str_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == *b;
}

static unsigned char slen(const char *s)
{
    unsigned char n = 0;

    while (*s++) {
        ++n;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* sort                                                               */
/* ------------------------------------------------------------------ */

static void test_sort_u8(void)
{
    static unsigned char arr[] = { 5, 1, 4, 1, 5, 9, 2, 6, 0, 255, 7, 7 };
    static const unsigned char want[] = { 0, 1, 1, 2, 4, 5, 5, 6, 7, 7, 9, 255 };
    unsigned char i, ok = 1;

    x16_sort_u8(arr, 12);
    for (i = 0; i < 12; ++i) {
        if (arr[i] != want[i]) ok = 0;
    }
    t_check(ok, "SORT_U8");
}

/* count 0 and count 1 must leave the array alone. */
static void test_sort_tiny(void)
{
    static unsigned char arr[] = { 9, 3 };
    unsigned char ok = 1;

    x16_sort_u8(arr, 0);
    if (arr[0] != 9 || arr[1] != 3) ok = 0;
    x16_sort_u8(arr, 1);
    if (arr[0] != 9 || arr[1] != 3) ok = 0;
    x16_sort_u8(arr, 2);
    if (arr[0] != 3 || arr[1] != 9) ok = 0;
    t_check(ok, "SORT_TINY");
}

static void test_sort_s8(void)
{
    static signed char arr[] = { -1, 3, -128, 127, 0, -1, 5, -100 };
    static const signed char want[] = { -128, -100, -1, -1, 0, 3, 5, 127 };
    unsigned char i, ok = 1;

    x16_sort_s8(arr, 8);
    for (i = 0; i < 8; ++i) {
        if (arr[i] != want[i]) ok = 0;
    }
    t_check(ok, "SORT_S8");
}

static void test_sort_u16(void)
{
    static unsigned int arr[] = { 0x1234, 0x0034, 0xFFFF, 0x1234,
                                  0x0100, 0x00FF, 0x0000, 0x8000 };
    static const unsigned int want[] = { 0x0000, 0x0034, 0x00FF, 0x0100,
                                         0x1234, 0x1234, 0x8000, 0xFFFF };
    unsigned char i, ok = 1;

    x16_sort_u16(arr, 8);
    for (i = 0; i < 8; ++i) {
        if (arr[i] != want[i]) ok = 0;
    }
    t_check(ok, "SORT_U16");
}

static void test_sort_s16(void)
{
    /* -32767 - 1: a literal -32768 is a signed long in C89 */
    static int arr[] = { -1, 256, -256, 32767, -32767 - 1, 0, -1, 1 };
    static const int want[] = { -32767 - 1, -256, -1, -1, 0, 1, 256, 32767 };
    unsigned char i, ok = 1;

    x16_sort_s16(arr, 8);
    for (i = 0; i < 8; ++i) {
        if (arr[i] != want[i]) ok = 0;
    }
    t_check(ok, "SORT_S16");
}

/* Descending order via a C comparator: proves x16_sort() really drives
** the order through the callback, not through a built-in compare.
*/
static unsigned char cmp_desc(const void *a, const void *b)
{
    /* A sorts after B when A < B: descending. */
    return *(const unsigned int *)a < *(const unsigned int *)b;
}

static void test_sort_cmp(void)
{
    static unsigned int arr[] = { 3, 500, 500, 2, 65535U, 0, 10 };
    static const unsigned int want[] = { 65535U, 500, 500, 10, 3, 2, 0 };
    unsigned char i, ok = 1;

    x16_sort(arr, 7, cmp_desc);
    for (i = 0; i < 7; ++i) {
        if (arr[i] != want[i]) ok = 0;
    }
    t_check(ok, "SORT_CMP");
}

/* ------------------------------------------------------------------ */
/* bcd                                                                */
/* ------------------------------------------------------------------ */

static void test_bcd_add8(void)
{
    unsigned char a, ok = 1;

    a = 0x45;
    if (x16_bcd_add8(&a, 0x38) != 0 || a != 0x83) ok = 0;
    a = 0x99;                           /* the digit carry ripples out */
    if (x16_bcd_add8(&a, 0x01) != 1 || a != 0x00) ok = 0;
    t_check(ok, "BCD_ADD8");
}

static void test_bcd_add16(void)
{
    unsigned int a;
    unsigned char ok = 1;

    a = 0x0999;                         /* carry across the byte seam */
    if (x16_bcd_add16(&a, 0x0001) != 0 || a != 0x1000) ok = 0;
    a = 0x9999;
    if (x16_bcd_add16(&a, 0x0001) != 1 || a != 0x0000) ok = 0;
    a = 0x0987;
    if (x16_bcd_add16(&a, 0x1111) != 0 || a != 0x2098) ok = 0;
    t_check(ok, "BCD_ADD16");
}

static void test_bcd_add32(void)
{
    unsigned long a;
    unsigned char ok = 1;

    a = 0x09999999UL;                   /* carry across all four bytes */
    if (x16_bcd_add32(&a, 0x00000001UL) != 0 || a != 0x10000000UL) ok = 0;
    a = 0x99999999UL;
    if (x16_bcd_add32(&a, 0x00000001UL) != 1 || a != 0x00000000UL) ok = 0;
    t_check(ok, "BCD_ADD32");
}

static void test_bcd_sub8(void)
{
    unsigned char a, ok = 1;

    a = 0x42;
    if (x16_bcd_sub8(&a, 0x13) != 0 || a != 0x29) ok = 0;
    a = 0x00;                           /* borrow: wraps to 99 */
    if (x16_bcd_sub8(&a, 0x01) != 1 || a != 0x99) ok = 0;
    t_check(ok, "BCD_SUB8");
}

static void test_bcd_sub16(void)
{
    unsigned int a;
    unsigned char ok = 1;

    a = 0x1000;                         /* borrow across the byte seam */
    if (x16_bcd_sub16(&a, 0x0001) != 0 || a != 0x0999) ok = 0;
    a = 0x0000;
    if (x16_bcd_sub16(&a, 0x0001) != 1 || a != 0x9999) ok = 0;
    t_check(ok, "BCD_SUB16");
}

static void test_bcd_sub32(void)
{
    unsigned long a;
    unsigned char ok = 1;

    a = 0x10000000UL;
    if (x16_bcd_sub32(&a, 0x00000001UL) != 0 || a != 0x09999999UL) ok = 0;
    a = 0x00000000UL;
    if (x16_bcd_sub32(&a, 0x00000001UL) != 1 || a != 0x99999999UL) ok = 0;
    t_check(ok, "BCD_SUB32");
}

static void test_bcd_addto(void)
{
    static unsigned char buf[4];
    unsigned char ok = 1;

    buf[0] = 0x87; buf[1] = 0x09; buf[2] = 0x00; buf[3] = 0x00;
    if (x16_bcd_addto(buf, 0x00001111UL) != 0) ok = 0;
    if (buf[0] != 0x98 || buf[1] != 0x20 || buf[2] != 0x00 ||
        buf[3] != 0x00) ok = 0;

    buf[0] = buf[1] = buf[2] = buf[3] = 0x99;
    if (x16_bcd_addto(buf, 0x00000001UL) != 1) ok = 0;
    if (buf[0] | buf[1] | buf[2] | buf[3]) ok = 0;
    t_check(ok, "BCD_ADDTO");
}

static void test_bcd_subfrom(void)
{
    static unsigned char buf[4];
    unsigned char ok = 1;

    buf[0] = 0x98; buf[1] = 0x20; buf[2] = 0x00; buf[3] = 0x00;
    if (x16_bcd_subfrom(buf, 0x00001111UL) != 0) ok = 0;
    if (buf[0] != 0x87 || buf[1] != 0x09 || buf[2] != 0x00 ||
        buf[3] != 0x00) ok = 0;

    buf[0] = buf[1] = buf[2] = buf[3] = 0x00;
    if (x16_bcd_subfrom(buf, 0x00000001UL) != 1) ok = 0;
    if (buf[0] != 0x99 || buf[1] != 0x99 || buf[2] != 0x99 ||
        buf[3] != 0x99) ok = 0;
    t_check(ok, "BCD_SUBFROM");
}

/* ------------------------------------------------------------------ */
/* bits                                                               */
/* ------------------------------------------------------------------ */

static void test_bit_set(void)
{
    static unsigned char v;
    unsigned char ok = 1;

    v = 0x40;
    x16_bit_set(&v, 0x81);
    if (v != 0xC1) ok = 0;
    x16_bit_set(&v, 0x81);              /* idempotent */
    if (v != 0xC1) ok = 0;
    t_check(ok, "BIT_SET");
}

static void test_bit_clr(void)
{
    static unsigned char v;
    unsigned char ok = 1;

    v = 0xC1;
    x16_bit_clr(&v, 0x80);
    if (v != 0x41) ok = 0;
    x16_bit_clr(&v, 0x3E);              /* none of these bits set: no-op */
    if (v != 0x41) ok = 0;
    t_check(ok, "BIT_CLR");
}

static void test_bit_test(void)
{
    static unsigned char v;
    unsigned char ok = 1;

    v = 0xC1;
    if (x16_bit_test(&v, 0x01) != 0x01) ok = 0;     /* set bit */
    if (x16_bit_test(&v, 0x3E) != 0x00) ok = 0;     /* clear bits */
    if (x16_bit_test(&v, 0xC0) != 0xC0) ok = 0;     /* the masked value */
    t_check(ok, "BIT_TEST");
}

static void test_bit_put(void)
{
    static unsigned char v;
    unsigned char ok = 1;

    v = 0x40;
    x16_bit_put(&v, 0x0F, 1);
    if (v != 0x4F) ok = 0;
    x16_bit_put(&v, 0x0A, 0);
    if (v != 0x45) ok = 0;
    x16_bit_put(&v, 0x80, 0xFF);        /* any nonzero flag sets */
    if (v != 0xC5) ok = 0;
    t_check(ok, "BIT_PUT");
}

static void test_nibbles(void)
{
    unsigned char ok = 1;

    if (x16_hinib(0xAB) != 0x0A) ok = 0;
    if (x16_lonib(0xAB) != 0x0B) ok = 0;
    if (x16_hinib(0x0F) != 0x00) ok = 0;
    if (x16_catnib(0x0A, 0x0B) != 0xAB) ok = 0;
    if (x16_catnib(0xFA, 0xCB) != 0xAB) ok = 0;     /* masks its inputs */
    t_check(ok, "NIBBLES");
}

/* ------------------------------------------------------------------ */
/* number                                                             */
/* ------------------------------------------------------------------ */

static void test_u8_to_dec(void)
{
    unsigned char ok = 1;

    if (!str_eq(x16_u8_to_dec(0), "0")) ok = 0;
    if (!str_eq(x16_u8_to_dec(7), "7")) ok = 0;
    if (!str_eq(x16_u8_to_dec(255), "255")) ok = 0;
    t_check(ok, "U8_TO_DEC");
}

static void test_u16_to_dec(void)
{
    unsigned char ok = 1;

    if (!str_eq(x16_u16_to_dec(0), "0")) ok = 0;
    if (!str_eq(x16_u16_to_dec(9), "9")) ok = 0;
    if (!str_eq(x16_u16_to_dec(10), "10")) ok = 0;
    if (!str_eq(x16_u16_to_dec(999), "999")) ok = 0;
    if (!str_eq(x16_u16_to_dec(1000), "1000")) ok = 0;
    if (!str_eq(x16_u16_to_dec(65535U), "65535")) ok = 0;
    t_check(ok, "U16_TO_DEC");
}

static void test_s8_to_dec(void)
{
    unsigned char ok = 1;

    if (!str_eq(x16_s8_to_dec(-128), "-128")) ok = 0;
    if (!str_eq(x16_s8_to_dec(-1), "-1")) ok = 0;
    if (!str_eq(x16_s8_to_dec(0), "0")) ok = 0;
    if (!str_eq(x16_s8_to_dec(127), "127")) ok = 0;
    t_check(ok, "S8_TO_DEC");
}

static void test_s16_to_dec(void)
{
    unsigned char ok = 1;

    if (!str_eq(x16_s16_to_dec(-32767 - 1), "-32768")) ok = 0;
    if (!str_eq(x16_s16_to_dec(32767), "32767")) ok = 0;
    if (!str_eq(x16_s16_to_dec(-1), "-1")) ok = 0;
    if (!str_eq(x16_s16_to_dec(-100), "-100")) ok = 0;
    t_check(ok, "S16_TO_DEC");
}

static void test_u8_to_hex(void)
{
    unsigned char ok = 1;

    if (!str_eq(x16_u8_to_hex(0x00), "00")) ok = 0;
    if (!str_eq(x16_u8_to_hex(0x0F), "0F")) ok = 0;
    if (!str_eq(x16_u8_to_hex(0xA5), "A5")) ok = 0;
    if (!str_eq(x16_u8_to_hex(0xFF), "FF")) ok = 0;
    t_check(ok, "U8_TO_HEX");
}

static void test_u16_to_hex(void)
{
    unsigned char ok = 1;

    if (!str_eq(x16_u16_to_hex(0x0000), "0000")) ok = 0;
    if (!str_eq(x16_u16_to_hex(0xBEEF), "BEEF")) ok = 0;
    if (!str_eq(x16_u16_to_hex(0x00FF), "00FF")) ok = 0;
    t_check(ok, "U16_TO_HEX");
}

static void test_u8_to_bin(void)
{
    unsigned char ok = 1;

    if (!str_eq(x16_u8_to_bin(0xA5), "10100101")) ok = 0;
    if (!str_eq(x16_u8_to_bin(0x00), "00000000")) ok = 0;
    if (!str_eq(x16_u8_to_bin(0xFF), "11111111")) ok = 0;
    t_check(ok, "U8_TO_BIN");
}

static void test_u16_to_bin(void)
{
    unsigned char ok = 1;

    if (!str_eq(x16_u16_to_bin(0x8001), "1000000000000001")) ok = 0;
    if (!str_eq(x16_u16_to_bin(0x0000), "0000000000000000")) ok = 0;
    t_check(ok, "U16_TO_BIN");
}

static void test_dec_to_u16(void)
{
    unsigned int val;
    unsigned char ok = 1;

    val = 0;
    if (x16_dec_to_u16("65535", 5, &val) != 1 || val != 65535U) ok = 0;
    if (x16_dec_to_u16("0", 1, &val) != 1 || val != 0) ok = 0;
    if (x16_dec_to_u16("12345", 5, &val) != 1 || val != 12345U) ok = 0;
    if (x16_dec_to_u16("00042", 5, &val) != 1 || val != 42) ok = 0;
    t_check(ok, "DEC_TO_U16");
}

static void test_dec_to_u16_bad(void)
{
    unsigned int val;
    unsigned char ok = 1;

    val = 0x1234;
    if (x16_dec_to_u16("12X4", 4, &val) != 0) ok = 0;
    if (val != 0x1234) ok = 0;          /* untouched on failure */
    if (x16_dec_to_u16("65536", 5, &val) != 0) ok = 0;
    if (x16_dec_to_u16("99999", 5, &val) != 0) ok = 0;
    if (val != 0x1234) ok = 0;
    t_check(ok, "DEC_TO_U16_BAD");
}

static void test_dec_roundtrip(void)
{
    static const unsigned int vals[] = { 0, 9, 100, 4660, 32768U, 65535U };
    unsigned int back;
    unsigned char i, ok = 1;
    char *s;

    for (i = 0; i < 6; ++i) {
        s = x16_u16_to_dec(vals[i]);
        back = ~vals[i];
        if (x16_dec_to_u16(s, slen(s), &back) != 1) ok = 0;
        if (back != vals[i]) ok = 0;
    }
    t_check(ok, "DEC_ROUNDTRIP");
}

/* ------------------------------------------------------------------ */
/* tscrunch                                                           */
/* ------------------------------------------------------------------ */

/* tscrunch payload.bin payload.tsc -- the phrase four times over. */
static const unsigned char tsc_packed[] = {
    0x3f, 0x19, 0x58, 0x31, 0x36, 0x4c, 0x49, 0x42, 0x2d, 0x44, 0x45, 0x43,
    0x4f, 0x4d, 0x50, 0x52, 0x45, 0x53, 0x53, 0x2d, 0x54, 0x45, 0x53, 0x54,
    0x21, 0x21, 0x58, 0xfe, 0x18, 0xcc, 0xe8, 0x7f, 0x20
};

static const char tsc_phrase[] = "X16LIB-DECOMPRESS-TEST!!";

static unsigned char tsc_out[97];

static void test_tsc(void)
{
    unsigned char *end;
    unsigned char i, r, ok = 1;

    tsc_out[96] = 0x77;                 /* guard, one past the output */
    end = x16_tsc_decompress(tsc_packed, tsc_out);

    if (end != tsc_out + 96 || tsc_out[96] != 0x77) {
        t_check(0, "TSC");
        return;
    }
    for (r = 0; r < 4; ++r) {
        for (i = 0; i < 24; ++i) {
            if (tsc_out[r * 24 + i] != (unsigned char)tsc_phrase[i]) ok = 0;
        }
    }
    t_check(ok, "TSC");
}

/* tscrunch rle.bin rle.tsc -- the 196-byte RLE torture: 40 zeros,
** "RLE-EDGE", 90 x $55 (crossing the one-token zero-run length),
** 50 zeros, and the text again as a far match.
*/
static const unsigned char tsc_rpacked[] = {
    0x31, 0xcf, 0x00, 0x08, 0x52, 0x4c, 0x45, 0x2d, 0x45, 0x44, 0x47, 0x45,
    0xff, 0x55, 0xb3, 0x55, 0x81, 0x9e, 0x94, 0x20
};

static const char tsc_redge[] = "RLE-EDGE";

static unsigned char tsc_rout[197];

static void test_tsc_rle(void)
{
    unsigned char *end;
    unsigned char i, ok = 1;

    tsc_rout[196] = 0x77;
    end = x16_tsc_decompress(tsc_rpacked, tsc_rout);

    if (end != tsc_rout + 196 || tsc_rout[196] != 0x77) {
        t_check(0, "TSC_RLE");
        return;
    }
    for (i = 0; i < 40; ++i) {
        if (tsc_rout[i] != 0x00) ok = 0;
    }
    for (i = 0; i < 8; ++i) {
        if (tsc_rout[40 + i] != (unsigned char)tsc_redge[i]) ok = 0;
    }
    for (i = 0; i < 90; ++i) {
        if (tsc_rout[48 + i] != 0x55) ok = 0;
    }
    for (i = 0; i < 50; ++i) {
        if (tsc_rout[138 + i] != 0x00) ok = 0;
    }
    for (i = 0; i < 8; ++i) {
        if (tsc_rout[188 + i] != (unsigned char)tsc_redge[i]) ok = 0;
    }
    t_check(ok, "TSC_RLE");
}

/* ------------------------------------------------------------------ */

int main(void)
{
    t_init();

    test_sort_u8();
    test_sort_tiny();
    test_sort_s8();
    test_sort_u16();
    test_sort_s16();
    test_sort_cmp();

    test_bcd_add8();
    test_bcd_add16();
    test_bcd_add32();
    test_bcd_sub8();
    test_bcd_sub16();
    test_bcd_sub32();
    test_bcd_addto();
    test_bcd_subfrom();

    test_bit_set();
    test_bit_clr();
    test_bit_test();
    test_bit_put();
    test_nibbles();

    test_u8_to_dec();
    test_u16_to_dec();
    test_s8_to_dec();
    test_s16_to_dec();
    test_u8_to_hex();
    test_u16_to_hex();
    test_u8_to_bin();
    test_u16_to_bin();
    test_dec_to_u16();
    test_dec_to_u16_bad();
    test_dec_roundtrip();

    test_tsc();
    test_tsc_rle();

    t_done();
    return 0;
}
