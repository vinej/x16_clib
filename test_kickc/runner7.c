/* =====================================================================
 * x16clib :: test_kickc/runner7.c -- UTIL modules: sort, bcd, bits,
 *                                    number, tscrunch
 * =====================================================================
 * The seventh PRG of the suite, mirroring ca65's runner5.c check for
 * check (32 of them). Run it with
 *
 *      .\build_kickc.ps1 -Test -Source test_kickc\runner7.c
 *
 * The TSCrunch fixtures are the upstream x16_library's own: the packed
 * bytes are the literal output of Antonio Savona's `tscrunch` tool over
 * the phrase and RLE payloads, so this checks our decoder against the
 * real encoder, not against a hand-built stream.
 *
 * KickC dialect notes vs the ca65 original: no function-static data
 * (fixtures live at file scope), every t_check condition is wrapped in
 * (expr) ? 1 : 0, and the comparator dereferences through same-width
 * locals (a direct *pa < *pb compare has no ASM fragment in 0.8.6).
 * =====================================================================
 */

#include "testlib.h"
#include <x16/x16.h>

/* ------------------------------------------------------------------ */
/* helpers                                                            */
/* ------------------------------------------------------------------ */

unsigned char str_eq(const char *sa, const char *sb) {
    while (*sa != 0 && *sb != 0) {
        if (*sa != *sb) {
            return 0;
        }
        ++sa;
        ++sb;
    }
    if (*sa == *sb) {
        return 1;
    }
    return 0;
}

unsigned char slen(const char *s) {
    unsigned char n = 0;
    while (*s != 0) {
        ++n;
        ++s;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* sort                                                               */
/* ------------------------------------------------------------------ */

unsigned char su8_arr[] = { 5, 1, 4, 1, 5, 9, 2, 6, 0, 255, 7, 7 };
const unsigned char su8_want[] = { 0, 1, 1, 2, 4, 5, 5, 6, 7, 7, 9, 255 };

void test_sort_u8(void) {
    unsigned char i;
    unsigned char ok = 1;
    unsigned char got, exp;

    x16_sort_u8(su8_arr, 12);
    for (i = 0; i < 12; ++i) {
        got = su8_arr[i];
        exp = su8_want[i];
        if (got != exp) {
            ok = 0;
        }
    }
    t_check(ok, "SORT_U8");
}

/* count 0 and count 1 must leave the array alone. */
unsigned char stiny_arr[] = { 9, 3 };

void test_sort_tiny(void) {
    unsigned char ok = 1;

    x16_sort_u8(stiny_arr, 0);
    if (stiny_arr[0] != 9 || stiny_arr[1] != 3) {
        ok = 0;
    }
    x16_sort_u8(stiny_arr, 1);
    if (stiny_arr[0] != 9 || stiny_arr[1] != 3) {
        ok = 0;
    }
    x16_sort_u8(stiny_arr, 2);
    if (stiny_arr[0] != 3 || stiny_arr[1] != 9) {
        ok = 0;
    }
    t_check(ok, "SORT_TINY");
}

signed char ss8_arr[] = { -1, 3, -128, 127, 0, -1, 5, -100 };
const signed char ss8_want[] = { -128, -100, -1, -1, 0, 3, 5, 127 };

void test_sort_s8(void) {
    unsigned char i;
    unsigned char ok = 1;
    signed char got, exp;

    x16_sort_s8(ss8_arr, 8);
    for (i = 0; i < 8; ++i) {
        got = ss8_arr[i];
        exp = ss8_want[i];
        if (got != exp) {
            ok = 0;
        }
    }
    t_check(ok, "SORT_S8");
}

unsigned int su16_arr[] = { 0x1234, 0x0034, 0xffff, 0x1234,
                            0x0100, 0x00ff, 0x0000, 0x8000 };
const unsigned int su16_want[] = { 0x0000, 0x0034, 0x00ff, 0x0100,
                                   0x1234, 0x1234, 0x8000, 0xffff };

void test_sort_u16(void) {
    unsigned char i;
    unsigned char ok = 1;
    unsigned int got, exp;

    x16_sort_u16(su16_arr, 8);
    for (i = 0; i < 8; ++i) {
        got = su16_arr[i];
        exp = su16_want[i];
        if (got != exp) {
            ok = 0;
        }
    }
    t_check(ok, "SORT_U16");
}

/* -32767 - 1: a literal -32768 is out of int range on its own */
int ss16_arr[] = { -1, 256, -256, 32767, -32767 - 1, 0, -1, 1 };
const int ss16_want[] = { -32767 - 1, -256, -1, -1, 0, 1, 256, 32767 };

void test_sort_s16(void) {
    unsigned char i;
    unsigned char ok = 1;
    int got, exp;

    x16_sort_s16(ss16_arr, 8);
    for (i = 0; i < 8; ++i) {
        got = ss16_arr[i];
        exp = ss16_want[i];
        if (got != exp) {
            ok = 0;
        }
    }
    t_check(ok, "SORT_S16");
}

/* Descending order via a C comparator: proves x16_sort() really drives
** the order through the callback, not through a built-in compare.
*/
unsigned char cmp_desc(const void *pa, const void *pb) {
    /* A sorts after B when A < B: descending. */
    unsigned int aw = *(unsigned int *)pa;
    unsigned int bw = *(unsigned int *)pb;
    if (aw < bw) {
        return 1;
    }
    return 0;
}

unsigned int scmp_arr[] = { 3, 500, 500, 2, 65535u, 0, 10 };
const unsigned int scmp_want[] = { 65535u, 500, 500, 10, 3, 2, 0 };

void test_sort_cmp(void) {
    unsigned char i;
    unsigned char ok = 1;
    unsigned int got, exp;

    x16_sort(scmp_arr, 7, cmp_desc);
    for (i = 0; i < 7; ++i) {
        got = scmp_arr[i];
        exp = scmp_want[i];
        if (got != exp) {
            ok = 0;
        }
    }
    t_check(ok, "SORT_CMP");
}

/* ------------------------------------------------------------------ */
/* bcd                                                                */
/* ------------------------------------------------------------------ */

void test_bcd_add8(void) {
    unsigned char v8;
    unsigned char ok = 1;

    v8 = 0x45;
    if (x16_bcd_add8(&v8, 0x38) != 0 || v8 != 0x83) {
        ok = 0;
    }
    v8 = 0x99;                          /* the digit carry ripples out */
    if (x16_bcd_add8(&v8, 0x01) != 1 || v8 != 0x00) {
        ok = 0;
    }
    t_check(ok, "BCD_ADD8");
}

void test_bcd_add16(void) {
    unsigned int v16;
    unsigned char ok = 1;

    v16 = 0x0999;                       /* carry across the byte seam */
    if (x16_bcd_add16(&v16, 0x0001) != 0 || v16 != 0x1000) {
        ok = 0;
    }
    v16 = 0x9999;
    if (x16_bcd_add16(&v16, 0x0001) != 1 || v16 != 0x0000) {
        ok = 0;
    }
    v16 = 0x0987;
    if (x16_bcd_add16(&v16, 0x1111) != 0 || v16 != 0x2098) {
        ok = 0;
    }
    t_check(ok, "BCD_ADD16");
}

void test_bcd_add32(void) {
    unsigned long v32;
    unsigned char ok = 1;

    v32 = 0x09999999;                   /* carry across all four bytes */
    if (x16_bcd_add32(&v32, 0x00000001) != 0 || v32 != 0x10000000) {
        ok = 0;
    }
    v32 = 0x99999999;
    if (x16_bcd_add32(&v32, 0x00000001) != 1 || v32 != 0x00000000) {
        ok = 0;
    }
    t_check(ok, "BCD_ADD32");
}

void test_bcd_sub8(void) {
    unsigned char v8;
    unsigned char ok = 1;

    v8 = 0x42;
    if (x16_bcd_sub8(&v8, 0x13) != 0 || v8 != 0x29) {
        ok = 0;
    }
    v8 = 0x00;                          /* borrow: wraps to 99 */
    if (x16_bcd_sub8(&v8, 0x01) != 1 || v8 != 0x99) {
        ok = 0;
    }
    t_check(ok, "BCD_SUB8");
}

void test_bcd_sub16(void) {
    unsigned int v16;
    unsigned char ok = 1;

    v16 = 0x1000;                       /* borrow across the byte seam */
    if (x16_bcd_sub16(&v16, 0x0001) != 0 || v16 != 0x0999) {
        ok = 0;
    }
    v16 = 0x0000;
    if (x16_bcd_sub16(&v16, 0x0001) != 1 || v16 != 0x9999) {
        ok = 0;
    }
    t_check(ok, "BCD_SUB16");
}

void test_bcd_sub32(void) {
    unsigned long v32;
    unsigned char ok = 1;

    v32 = 0x10000000;
    if (x16_bcd_sub32(&v32, 0x00000001) != 0 || v32 != 0x09999999) {
        ok = 0;
    }
    v32 = 0x00000000;
    if (x16_bcd_sub32(&v32, 0x00000001) != 1 || v32 != 0x99999999) {
        ok = 0;
    }
    t_check(ok, "BCD_SUB32");
}

unsigned char bcd_buf[4];

void test_bcd_addto(void) {
    unsigned char ok = 1;

    bcd_buf[0] = 0x87; bcd_buf[1] = 0x09; bcd_buf[2] = 0x00; bcd_buf[3] = 0x00;
    if (x16_bcd_addto(bcd_buf, 0x00001111) != 0) {
        ok = 0;
    }
    if (bcd_buf[0] != 0x98 || bcd_buf[1] != 0x20 || bcd_buf[2] != 0x00 ||
        bcd_buf[3] != 0x00) {
        ok = 0;
    }

    bcd_buf[0] = 0x99; bcd_buf[1] = 0x99; bcd_buf[2] = 0x99; bcd_buf[3] = 0x99;
    if (x16_bcd_addto(bcd_buf, 0x00000001) != 1) {
        ok = 0;
    }
    if (bcd_buf[0] != 0 || bcd_buf[1] != 0 || bcd_buf[2] != 0 ||
        bcd_buf[3] != 0) {
        ok = 0;
    }
    t_check(ok, "BCD_ADDTO");
}

void test_bcd_subfrom(void) {
    unsigned char ok = 1;

    bcd_buf[0] = 0x98; bcd_buf[1] = 0x20; bcd_buf[2] = 0x00; bcd_buf[3] = 0x00;
    if (x16_bcd_subfrom(bcd_buf, 0x00001111) != 0) {
        ok = 0;
    }
    if (bcd_buf[0] != 0x87 || bcd_buf[1] != 0x09 || bcd_buf[2] != 0x00 ||
        bcd_buf[3] != 0x00) {
        ok = 0;
    }

    bcd_buf[0] = 0x00; bcd_buf[1] = 0x00; bcd_buf[2] = 0x00; bcd_buf[3] = 0x00;
    if (x16_bcd_subfrom(bcd_buf, 0x00000001) != 1) {
        ok = 0;
    }
    if (bcd_buf[0] != 0x99 || bcd_buf[1] != 0x99 || bcd_buf[2] != 0x99 ||
        bcd_buf[3] != 0x99) {
        ok = 0;
    }
    t_check(ok, "BCD_SUBFROM");
}

/* ------------------------------------------------------------------ */
/* bits                                                               */
/* ------------------------------------------------------------------ */

unsigned char bit_v;

void test_bit_set(void) {
    unsigned char ok = 1;

    bit_v = 0x40;
    x16_bit_set(&bit_v, 0x81);
    if (bit_v != 0xc1) {
        ok = 0;
    }
    x16_bit_set(&bit_v, 0x81);          /* idempotent */
    if (bit_v != 0xc1) {
        ok = 0;
    }
    t_check(ok, "BIT_SET");
}

void test_bit_clr(void) {
    unsigned char ok = 1;

    bit_v = 0xc1;
    x16_bit_clr(&bit_v, 0x80);
    if (bit_v != 0x41) {
        ok = 0;
    }
    x16_bit_clr(&bit_v, 0x3e);          /* none of these bits set: no-op */
    if (bit_v != 0x41) {
        ok = 0;
    }
    t_check(ok, "BIT_CLR");
}

void test_bit_test(void) {
    unsigned char ok = 1;

    bit_v = 0xc1;
    if (x16_bit_test(&bit_v, 0x01) != 0x01) {       /* set bit */
        ok = 0;
    }
    if (x16_bit_test(&bit_v, 0x3e) != 0x00) {       /* clear bits */
        ok = 0;
    }
    if (x16_bit_test(&bit_v, 0xc0) != 0xc0) {       /* the masked value */
        ok = 0;
    }
    t_check(ok, "BIT_TEST");
}

void test_bit_put(void) {
    unsigned char ok = 1;

    bit_v = 0x40;
    x16_bit_put(&bit_v, 0x0f, 1);
    if (bit_v != 0x4f) {
        ok = 0;
    }
    x16_bit_put(&bit_v, 0x0a, 0);
    if (bit_v != 0x45) {
        ok = 0;
    }
    x16_bit_put(&bit_v, 0x80, 0xff);    /* any nonzero flag sets */
    if (bit_v != 0xc5) {
        ok = 0;
    }
    t_check(ok, "BIT_PUT");
}

void test_nibbles(void) {
    unsigned char ok = 1;

    if (x16_hinib(0xab) != 0x0a) {
        ok = 0;
    }
    if (x16_lonib(0xab) != 0x0b) {
        ok = 0;
    }
    if (x16_hinib(0x0f) != 0x00) {
        ok = 0;
    }
    if (x16_catnib(0x0a, 0x0b) != 0xab) {
        ok = 0;
    }
    if (x16_catnib(0xfa, 0xcb) != 0xab) {           /* masks its inputs */
        ok = 0;
    }
    t_check(ok, "NIBBLES");
}

/* ------------------------------------------------------------------ */
/* number                                                             */
/* ------------------------------------------------------------------ */

void test_u8_to_dec(void) {
    unsigned char ok = 1;

    if (str_eq(x16_u8_to_dec(0), "0") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u8_to_dec(7), "7") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u8_to_dec(255), "255") == 0) {
        ok = 0;
    }
    t_check(ok, "U8_TO_DEC");
}

void test_u16_to_dec(void) {
    unsigned char ok = 1;

    if (str_eq(x16_u16_to_dec(0), "0") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u16_to_dec(9), "9") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u16_to_dec(10), "10") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u16_to_dec(999), "999") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u16_to_dec(1000), "1000") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u16_to_dec(65535u), "65535") == 0) {
        ok = 0;
    }
    t_check(ok, "U16_TO_DEC");
}

void test_s8_to_dec(void) {
    unsigned char ok = 1;

    if (str_eq(x16_s8_to_dec(-128), "-128") == 0) {
        ok = 0;
    }
    if (str_eq(x16_s8_to_dec(-1), "-1") == 0) {
        ok = 0;
    }
    if (str_eq(x16_s8_to_dec(0), "0") == 0) {
        ok = 0;
    }
    if (str_eq(x16_s8_to_dec(127), "127") == 0) {
        ok = 0;
    }
    t_check(ok, "S8_TO_DEC");
}

void test_s16_to_dec(void) {
    unsigned char ok = 1;

    if (str_eq(x16_s16_to_dec(-32767 - 1), "-32768") == 0) {
        ok = 0;
    }
    if (str_eq(x16_s16_to_dec(32767), "32767") == 0) {
        ok = 0;
    }
    if (str_eq(x16_s16_to_dec(-1), "-1") == 0) {
        ok = 0;
    }
    if (str_eq(x16_s16_to_dec(-100), "-100") == 0) {
        ok = 0;
    }
    t_check(ok, "S16_TO_DEC");
}

void test_u8_to_hex(void) {
    unsigned char ok = 1;

    if (str_eq(x16_u8_to_hex(0x00), "00") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u8_to_hex(0x0f), "0F") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u8_to_hex(0xa5), "A5") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u8_to_hex(0xff), "FF") == 0) {
        ok = 0;
    }
    t_check(ok, "U8_TO_HEX");
}

void test_u16_to_hex(void) {
    unsigned char ok = 1;

    if (str_eq(x16_u16_to_hex(0x0000), "0000") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u16_to_hex(0xbeef), "BEEF") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u16_to_hex(0x00ff), "00FF") == 0) {
        ok = 0;
    }
    t_check(ok, "U16_TO_HEX");
}

void test_u8_to_bin(void) {
    unsigned char ok = 1;

    if (str_eq(x16_u8_to_bin(0xa5), "10100101") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u8_to_bin(0x00), "00000000") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u8_to_bin(0xff), "11111111") == 0) {
        ok = 0;
    }
    t_check(ok, "U8_TO_BIN");
}

void test_u16_to_bin(void) {
    unsigned char ok = 1;

    if (str_eq(x16_u16_to_bin(0x8001), "1000000000000001") == 0) {
        ok = 0;
    }
    if (str_eq(x16_u16_to_bin(0x0000), "0000000000000000") == 0) {
        ok = 0;
    }
    t_check(ok, "U16_TO_BIN");
}

void test_dec_to_u16(void) {
    unsigned int val;
    unsigned char ok = 1;

    val = 0;
    if (x16_dec_to_u16("65535", 5, &val) != 1 || val != 65535u) {
        ok = 0;
    }
    if (x16_dec_to_u16("0", 1, &val) != 1 || val != 0) {
        ok = 0;
    }
    if (x16_dec_to_u16("12345", 5, &val) != 1 || val != 12345u) {
        ok = 0;
    }
    if (x16_dec_to_u16("00042", 5, &val) != 1 || val != 42) {
        ok = 0;
    }
    t_check(ok, "DEC_TO_U16");
}

void test_dec_to_u16_bad(void) {
    unsigned int val;
    unsigned char ok = 1;

    val = 0x1234;
    if (x16_dec_to_u16("12X4", 4, &val) != 0) {
        ok = 0;
    }
    if (val != 0x1234) {                /* untouched on failure */
        ok = 0;
    }
    if (x16_dec_to_u16("65536", 5, &val) != 0) {
        ok = 0;
    }
    if (x16_dec_to_u16("99999", 5, &val) != 0) {
        ok = 0;
    }
    if (val != 0x1234) {
        ok = 0;
    }
    t_check(ok, "DEC_TO_U16_BAD");
}

const unsigned int rt_vals[] = { 0, 9, 100, 4660, 32768u, 65535u };

void test_dec_roundtrip(void) {
    unsigned int back;
    unsigned int want;
    unsigned char i;
    unsigned char ok = 1;
    char *s;

    for (i = 0; i < 6; ++i) {
        want = rt_vals[i];
        s = x16_u16_to_dec(want);
        back = ~want;
        if (x16_dec_to_u16(s, slen(s), &back) != 1) {
            ok = 0;
        }
        if (back != want) {
            ok = 0;
        }
    }
    t_check(ok, "DEC_ROUNDTRIP");
}

/* ------------------------------------------------------------------ */
/* tscrunch                                                           */
/* ------------------------------------------------------------------ */

/* tscrunch payload.bin payload.tsc -- the phrase four times over. */
const unsigned char tsc_packed[] = {
    0x3f, 0x19, 0x58, 0x31, 0x36, 0x4c, 0x49, 0x42, 0x2d, 0x44, 0x45, 0x43,
    0x4f, 0x4d, 0x50, 0x52, 0x45, 0x53, 0x53, 0x2d, 0x54, 0x45, 0x53, 0x54,
    0x21, 0x21, 0x58, 0xfe, 0x18, 0xcc, 0xe8, 0x7f, 0x20
};

const char tsc_phrase[] = "X16LIB-DECOMPRESS-TEST!!";

unsigned char tsc_out[97];

void test_tsc(void) {
    unsigned char *end;
    unsigned char i;
    unsigned char k;
    unsigned char r;
    unsigned char ok = 1;

    tsc_out[96] = 0x77;                 /* guard, one past the output */
    end = (unsigned char *)x16_tsc_decompress(tsc_packed, tsc_out);

    if (end != tsc_out + 96 || tsc_out[96] != 0x77) {
        t_check(0, "TSC");
        return;
    }
    r = 0;                              /* base index of each repeat */
    for (i = 0; i < 4; ++i) {
        for (k = 0; k < 24; ++k) {
            if (tsc_out[r + k] != (unsigned char)tsc_phrase[k]) {
                ok = 0;
            }
        }
        r += 24;
    }
    t_check(ok, "TSC");
}

/* tscrunch rle.bin rle.tsc -- the 196-byte RLE torture: 40 zeros,
** "RLE-EDGE", 90 x $55 (crossing the one-token zero-run length),
** 50 zeros, and the text again as a far match.
*/
const unsigned char tsc_rpacked[] = {
    0x31, 0xcf, 0x00, 0x08, 0x52, 0x4c, 0x45, 0x2d, 0x45, 0x44, 0x47, 0x45,
    0xff, 0x55, 0xb3, 0x55, 0x81, 0x9e, 0x94, 0x20
};

const char tsc_redge[] = "RLE-EDGE";

unsigned char tsc_rout[197];

void test_tsc_rle(void) {
    unsigned char *end;
    unsigned char i;
    unsigned char ok = 1;

    tsc_rout[196] = 0x77;
    end = (unsigned char *)x16_tsc_decompress(tsc_rpacked, tsc_rout);

    if (end != tsc_rout + 196 || tsc_rout[196] != 0x77) {
        t_check(0, "TSC_RLE");
        return;
    }
    for (i = 0; i < 40; ++i) {
        if (tsc_rout[i] != 0x00) {
            ok = 0;
        }
    }
    for (i = 0; i < 8; ++i) {
        if (tsc_rout[40 + i] != (unsigned char)tsc_redge[i]) {
            ok = 0;
        }
    }
    for (i = 0; i < 90; ++i) {
        if (tsc_rout[48 + i] != 0x55) {
            ok = 0;
        }
    }
    for (i = 0; i < 50; ++i) {
        if (tsc_rout[138 + i] != 0x00) {
            ok = 0;
        }
    }
    for (i = 0; i < 8; ++i) {
        if (tsc_rout[188 + i] != (unsigned char)tsc_redge[i]) {
            ok = 0;
        }
    }
    t_check(ok, "TSC_RLE");
}

/* ------------------------------------------------------------------ */

int main(void) {
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
