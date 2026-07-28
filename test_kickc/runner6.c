/* =====================================================================
 * x16clib :: test_kickc/runner6.c -- the string library, standalone
 * =====================================================================
 * The sixth PRG of the suite: x16/string.c, mirroring ca65's runner4.c
 * check for check (37 of them). Run it with
 *
 *     .\build_kickc.ps1 -Test -Source test_kickc\runner6.c
 *
 * ENCODINGS, because every byte here is an assertion. testlib.h sets
 * #pragma encoding(ascii) globally, so every C literal in this file
 * compiles to its ASCII byte -- which is also its ISO byte. That makes
 * literals safe for the encoding-agnostic routines (copy, find,
 * sort...) and for nothing else: the PETSCII-flavoured case/ctype tests
 * build their inputs from explicit PETSCII bytes (0x41='a', 0xC1='A',
 * ...), and the ISO tests from explicit ISO bytes, so no encoding can
 * silently change what is being tested.
 *
 * KickC dialect notes vs the ca65 original: no function-static data
 * (test fixtures live at file scope), and every t_check condition is
 * wrapped in (expr) ? 1 : 0 because KickC bools do not convert.
 * =====================================================================
 */

#include "testlib.h"
#include <x16/x16.h>

/* ------------------------------------------------------------------ */

/* Plain byte-wise equality, independent of the library under test. */
unsigned char streq(const char *sa, const char *sb) {
    while (*sa != 0 && *sa == *sb) {
        ++sa;
        ++sb;
    }
    if (*sa == *sb) {
        return 1;
    }
    return 0;
}

/* n bytes of buf equal exp, byte-for-byte. */
unsigned char bytes_eq(const unsigned char *buf, const unsigned char *exp,
                       unsigned char n) {
    unsigned char i;
    for (i = 0; i < n; ++i) {
        if (buf[i] != exp[i]) {
            return 0;
        }
    }
    return 1;
}

char dst[24];

void poison(void) {
    unsigned char i;
    for (i = 0; i < 24; ++i) {
        dst[i] = 0xaa;
    }
    /* A zero early on keeps a runaway copy short. */
    dst[3] = 0;
}

/* ------------------------------------------------------------------ */
/* fundamentals                                                        */
/* ------------------------------------------------------------------ */

char csrc[] = "hello";

void test_str_length(void) {
    t_check((x16_str_length("commander") == 9 && x16_str_length("") == 0)
            ? 1 : 0, "STR_LENGTH");
}

/* THE mutation canary: if copy swaps source and target, the poisoned
** destination never receives "hello" and this goes red.
*/
void test_str_copy(void) {
    poison();
    t_check((x16_str_copy(dst, csrc) == 5
            && streq(dst, "hello") != 0 && dst[5] == 0) ? 1 : 0,
            "STR_COPY");
}

void test_str_ncopy(void) {
    unsigned char ok;

    poison();
    ok = (x16_str_ncopy(dst, "commander", 4) == 4
          && streq(dst, "comm") != 0) ? 1 : 0;

    poison();
    if (!(x16_str_ncopy(dst, "ab", 6) == 2 && streq(dst, "ab") != 0)) {
        ok = 0;
    }

    t_check(ok, "STR_NCOPY");
}

void test_str_append(void) {
    poison();
    x16_str_copy(dst, "foo");
    t_check((x16_str_append(dst, "bar") == 6 && streq(dst, "foobar") != 0)
            ? 1 : 0, "STR_APPEND");
}

void test_str_nappend(void) {
    unsigned char ok;

    poison();
    x16_str_copy(dst, "foo");

    /* Room for two of six suffix chars: cut to fit, NUL at the cap. */
    ok = (x16_str_nappend(dst, "barbaz", 5) == 5
          && streq(dst, "fooba") != 0) ? 1 : 0;

    /* Already at the cap: length reported, nothing changes. */
    if (!(x16_str_nappend(dst, "zz", 5) == 5 && streq(dst, "fooba") != 0)) {
        ok = 0;
    }

    t_check(ok, "STR_NAPPEND");
}

void test_str_compare(void) {
    t_check((x16_str_compare("abc", "abc") == 0
            && x16_str_compare("abc", "abd") == -1
            && x16_str_compare("abd", "abc") == 1
            && x16_str_compare("ab", "abc") == -1   /* prefix sorts first */
            && x16_str_compare("abc", "ab") == 1) ? 1 : 0,
            "STR_COMPARE");
}

void test_str_hash(void) {
    t_check((x16_str_hash("commander") == x16_str_hash("commander")
            && x16_str_hash("commander") != x16_str_hash("commandes")
            && x16_str_hash("") == 179) ? 1 : 0,   /* the seed, by spec */
            "STR_HASH");
}

/* ------------------------------------------------------------------ */
/* case folding -- inputs are explicit encoding bytes (see header)     */
/* ------------------------------------------------------------------ */

void test_case_char_pet(void) {
    /* PETSCII: lower-case letters at $41-$5A, upper case at $61-$7A
    ** with the $C1-$DA alias. Folding down lands on $41, up on $61.
    */
    t_check((x16_str_lowerchar(0xc1) == 0x41    /* 'A' -> 'a' */
            && x16_str_lowerchar(0x61) == 0x41  /* alias form too */
            && x16_str_lowerchar(0x41) == 0x41  /* already lower */
            && x16_str_lowerchar(0x31) == 0x31  /* digits untouched */
            && x16_str_upperchar(0x41) == 0x61  /* 'a' -> 'A' */
            && x16_str_upperchar(0x61) == 0x61
            && x16_str_upperchar(0x31) == 0x31) ? 1 : 0,
            "CASE_CHAR_PET");
}

void test_case_char_iso(void) {
    /* ISO bytes: the classic 65-90 / 97-122 ranges. */
    t_check((x16_str_lowerchar_iso(65) == 97
            && x16_str_lowerchar_iso(90) == 122
            && x16_str_lowerchar_iso(97) == 97
            && x16_str_lowerchar_iso(48) == 48
            && x16_str_upperchar_iso(97) == 65
            && x16_str_upperchar_iso(122) == 90
            && x16_str_upperchar_iso(65) == 65
            && x16_str_upperchar_iso(48) == 48) ? 1 : 0,
            "CASE_CHAR_ISO");
}

/* "HellO1" in PETSCII bytes: upper H, lower ell, upper O, digit. */
unsigned char case_pbuf[] = { 0xc8, 0x45, 0x4c, 0x4c, 0xcf, 0x31, 0 };
const unsigned char case_plo[] = { 0x48, 0x45, 0x4c, 0x4c, 0x4f, 0x31, 0 };
const unsigned char case_pup[] = { 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x31, 0 };

void test_case_str_pet(void) {
    unsigned char ok;

    ok = (x16_str_lower((char *)case_pbuf) == 6
          && bytes_eq(case_pbuf, case_plo, 7) != 0) ? 1 : 0;
    if (!(x16_str_upper((char *)case_pbuf) == 6
          && bytes_eq(case_pbuf, case_pup, 7) != 0)) {
        ok = 0;
    }

    t_check(ok, "CASE_STR_PET");
}

/* "Hello1" in ISO bytes. */
unsigned char case_ibuf[] = { 72, 101, 108, 108, 111, 49, 0 };
const unsigned char case_iup[] = { 72, 69, 76, 76, 79, 49, 0 };
const unsigned char case_ilo[] = { 104, 101, 108, 108, 111, 49, 0 };

void test_case_str_iso(void) {
    unsigned char ok;

    ok = (x16_str_upper_iso((char *)case_ibuf) == 6
          && bytes_eq(case_ibuf, case_iup, 7) != 0) ? 1 : 0;
    if (!(x16_str_lower_iso((char *)case_ibuf) == 6
          && bytes_eq(case_ibuf, case_ilo, 7) != 0)) {
        ok = 0;
    }

    t_check(ok, "CASE_STR_ISO");
}

/* "Ab" (upper A, lower b) vs "ab" -- equal once folded. */
const char nc_s1[] = { 0xc1, 0x42, 0 };
const char nc_s2[] = { 0x41, 0x62, 0 };
const char nc_a[]  = { 0x41, 0 };
const char nc_b[]  = { 0x42, 0 };
const char nc_ab[] = { 0x41, 0x42, 0 };

void test_cmp_nocase_pet(void) {
    t_check((x16_str_compare_nocase(nc_s1, nc_s2) == 0
            && x16_str_compare_nocase(nc_a, nc_b) == -1
            && x16_str_compare_nocase(nc_b, nc_a) == 1
            && x16_str_compare_nocase(nc_a, nc_ab) == -1) ? 1 : 0,
            "CMP_NOCASE_PET");
}

/* ISO bytes: "AB" vs "ab" equal folded; 'a'(97) < 'B'(66 -> 98). */
const char nci_s1[] = { 65, 66, 0 };
const char nci_s2[] = { 97, 98, 0 };
const char nci_a[]  = { 97, 0 };
const char nci_bu[] = { 66, 0 };

void test_cmp_nocase_iso(void) {
    t_check((x16_str_compare_nocase_iso(nci_s1, nci_s2) == 0
            && x16_str_compare_nocase_iso(nci_a, nci_bu) == -1
            && x16_str_compare_nocase_iso(nci_bu, nci_a) == 1) ? 1 : 0,
            "CMP_NOCASE_ISO");
}

/* ------------------------------------------------------------------ */
/* classification -- explicit bytes, true AND false cases each         */
/* ------------------------------------------------------------------ */

void test_ctype(void) {
    t_check((x16_str_isdigit(48) != 0 && x16_str_isdigit(57) != 0
            && x16_str_isdigit(47) == 0 && x16_str_isdigit(58) == 0
            && x16_str_isdigit(65) == 0) ? 1 : 0,
            "CT_ISDIGIT");

    /* 0-9 plus both letter ranges 65-70 and 97-102, either encoding. */
    t_check((x16_str_isxdigit(48) != 0 && x16_str_isxdigit(57) != 0
            && x16_str_isxdigit(65) != 0 && x16_str_isxdigit(70) != 0
            && x16_str_isxdigit(97) != 0 && x16_str_isxdigit(102) != 0
            && x16_str_isxdigit(64) == 0 && x16_str_isxdigit(71) == 0
            && x16_str_isxdigit(96) == 0 && x16_str_isxdigit(103) == 0)
            ? 1 : 0,
            "CT_ISXDIGIT");

    t_check((x16_str_islower(97) != 0 && x16_str_islower(122) != 0
            && x16_str_islower(96) == 0 && x16_str_islower(123) == 0
            && x16_str_islower(65) == 0) ? 1 : 0,
            "CT_ISLOWER");

    /* PETSCII upper case: 97-122 and the 193-218 alias range. */
    t_check((x16_str_isupper(97) != 0 && x16_str_isupper(122) != 0
            && x16_str_isupper(193) != 0 && x16_str_isupper(218) != 0
            && x16_str_isupper(96) == 0 && x16_str_isupper(130) == 0
            && x16_str_isupper(65) == 0 && x16_str_isupper(219) == 0)
            ? 1 : 0,
            "CT_ISUPPER");

    t_check((x16_str_isupper_iso(65) != 0 && x16_str_isupper_iso(90) != 0
            && x16_str_isupper_iso(64) == 0 && x16_str_isupper_iso(91) == 0
            && x16_str_isupper_iso(97) == 0) ? 1 : 0,
            "CT_ISUPPER_ISO");

    t_check((x16_str_isletter(97) != 0 && x16_str_isletter(122) != 0
            && x16_str_isletter(193) != 0 && x16_str_isletter(218) != 0
            && x16_str_isletter(48) == 0 && x16_str_isletter(64) == 0
            && x16_str_isletter(219) == 0) ? 1 : 0,
            "CT_ISLETTER");

    t_check((x16_str_isletter_iso(97) != 0 && x16_str_isletter_iso(65) != 0
            && x16_str_isletter_iso(90) != 0 && x16_str_isletter_iso(122) != 0
            && x16_str_isletter_iso(48) == 0 && x16_str_isletter_iso(91) == 0
            && x16_str_isletter_iso(64) == 0) ? 1 : 0,
            "CT_ISLETTER_ISO");

    t_check((x16_str_isspace(32) != 0 && x16_str_isspace(13) != 0
            && x16_str_isspace(10) != 0 && x16_str_isspace(9) != 0
            && x16_str_isspace(141) != 0 && x16_str_isspace(160) != 0
            && x16_str_isspace(33) == 0 && x16_str_isspace(0) == 0
            && x16_str_isspace(8) == 0) ? 1 : 0,
            "CT_ISSPACE");

    t_check((x16_str_isprint(32) != 0 && x16_str_isprint(127) != 0
            && x16_str_isprint(160) != 0 && x16_str_isprint(255) != 0
            && x16_str_isprint(31) == 0 && x16_str_isprint(128) == 0
            && x16_str_isprint(159) == 0) ? 1 : 0,
            "CT_ISPRINT");

    /* ISO stops at 126: DEL (127) is not printable there. */
    t_check((x16_str_isprint_iso(32) != 0 && x16_str_isprint_iso(126) != 0
            && x16_str_isprint_iso(160) != 0 && x16_str_isprint_iso(255) != 0
            && x16_str_isprint_iso(127) == 0 && x16_str_isprint_iso(31) == 0
            && x16_str_isprint_iso(128) == 0) ? 1 : 0,
            "CT_ISPRINT_ISO");
}

/* ------------------------------------------------------------------ */
/* searching                                                           */
/* ------------------------------------------------------------------ */

void test_str_find(void) {
    t_check((x16_str_find("commander", 'm') == 2
            && x16_str_find("commander", 'r') == 8
            && x16_str_find("commander", 'z') == 255
            && x16_str_find("", 'a') == 255) ? 1 : 0,
            "STR_FIND");
}

void test_str_rfind(void) {
    t_check((x16_str_rfind("commander", 'm') == 3
            && x16_str_rfind("commander", 'c') == 0
            && x16_str_rfind("commander", 'z') == 255
            && x16_str_rfind("", 'a') == 255) ? 1 : 0,
            "STR_RFIND");
}

void test_str_contains(void) {
    t_check((x16_str_contains("commander", 'd') == 1
            && x16_str_contains("commander", 'z') == 0
            && x16_str_contains("", 'a') == 0) ? 1 : 0,
            "STR_CONTAINS");
}

/* Explicit bytes so no encoding can touch the CR/LF. */
const char eol_crs[] = { 0x61, 0x62, 13, 0x63, 0 };
const char eol_lfs[] = { 0x61, 10, 0x62, 0 };

void test_str_find_eol(void) {
    t_check((x16_str_find_eol(eol_crs) == 2
            && x16_str_find_eol(eol_lfs) == 1
            && x16_str_find_eol("abc") == 255) ? 1 : 0,
            "STR_FIND_EOL");
}

void test_pattern_literal(void) {
    t_check((x16_str_pattern_match("hello", "hello") == 1
            && x16_str_pattern_match("hello", "world") == 0
            && x16_str_pattern_match("hello", "hell") == 0
            && x16_str_pattern_match("hell", "hello") == 0
            && x16_str_pattern_match("", "") == 1) ? 1 : 0,
            "PM_LITERAL");
}

void test_pattern_qmark(void) {
    /* '?' consumes exactly one character -- never zero. */
    t_check((x16_str_pattern_match("hello", "h?llo") == 1
            && x16_str_pattern_match("hello", "?????") == 1
            && x16_str_pattern_match("hllo", "h?llo") == 0
            && x16_str_pattern_match("", "?") == 0) ? 1 : 0,
            "PM_QMARK");
}

void test_pattern_star(void) {
    /* '*' matches any run including none, and multiples nest. */
    t_check((x16_str_pattern_match("", "*") == 1
            && x16_str_pattern_match("anything", "*") == 1
            && x16_str_pattern_match("hello", "h*o") == 1
            && x16_str_pattern_match("ho", "h*o") == 1
            && x16_str_pattern_match("hello", "*lo") == 1
            && x16_str_pattern_match("aXXbYYc", "a*b*c") == 1
            && x16_str_pattern_match("hello", "h*z") == 0) ? 1 : 0,
            "PM_STAR");
}

/* ------------------------------------------------------------------ */
/* slicing and trimming                                                */
/* ------------------------------------------------------------------ */

void test_str_left(void) {
    unsigned char ok;

    poison();
    x16_str_left(dst, "commander", 3);
    ok = streq(dst, "com");

    poison();
    x16_str_left(dst, "commander", 0);
    if (dst[0] != 0) {
        ok = 0;
    }

    t_check(ok, "STR_LEFT");
}

void test_str_right(void) {
    poison();
    x16_str_right(dst, "commander", 3);
    t_check(streq(dst, "der"), "STR_RIGHT");
}

void test_str_slice(void) {
    unsigned char ok;

    poison();
    x16_str_slice(dst, "commander", 3, 4);
    ok = streq(dst, "mand");

    poison();
    x16_str_slice(dst, "commander", 2, 0);
    if (dst[0] != 0) {
        ok = 0;
    }

    t_check(ok, "STR_SLICE");
}

/* Space and TAB in front, as explicit bytes. */
char trim_lead[] = { 32, 9, 0x61, 0x62, 0 };
char trim_none[] = { 0x61, 0 };
char trim_blank1[] = { 32, 32, 0 };

void test_str_ltrim(void) {
    unsigned char ok;

    ok = (x16_str_ltrim(trim_lead) == 2 && streq(trim_lead, "ab") != 0)
         ? 1 : 0;
    if (!(x16_str_ltrim(trim_none) == 1 && streq(trim_none, "a") != 0)) {
        ok = 0;
    }
    if (!(x16_str_ltrim(trim_blank1) == 0 && trim_blank1[0] == 0)) {
        ok = 0;
    }

    t_check(ok, "STR_LTRIM");
}

char trim_tail[] = { 0x61, 0x62, 32, 9, 13, 0 };
char trim_blank2[] = { 32, 32, 0 };

void test_str_rtrim(void) {
    unsigned char ok;

    ok = (x16_str_rtrim(trim_tail) == 2 && streq(trim_tail, "ab") != 0)
         ? 1 : 0;
    if (!(x16_str_rtrim(trim_blank2) == 0 && trim_blank2[0] == 0)) {
        ok = 0;
    }

    t_check(ok, "STR_RTRIM");
}

/* Shift-space (160) and shift-CR (141) count as whitespace too. */
char trim_both[] = { 160, 32, 0x61, 0x62, 141, 10, 0 };

void test_str_trim(void) {
    t_check((x16_str_trim(trim_both) == 2 && streq(trim_both, "ab") != 0)
            ? 1 : 0, "STR_TRIM");
}

/* ------------------------------------------------------------------ */
/* sorting                                                             */
/* ------------------------------------------------------------------ */

/* The duplicate lives in its own buffer, so a sort that merely
** compares pointers instead of contents cannot pass by accident.
*/
const char sort_apple2[] = "apple";
const char *sort_arr[] = { "melon", "apple", "banana", sort_apple2, "" };
const char *sort_one[] = { "banana" };

void test_str_sort(void) {
    unsigned char ok;

    x16_str_sort(sort_arr, 5);

    ok = (streq(sort_arr[0], "") != 0
          && streq(sort_arr[1], "apple") != 0
          && streq(sort_arr[2], "apple") != 0
          && streq(sort_arr[3], "banana") != 0
          && streq(sort_arr[4], "melon") != 0) ? 1 : 0;

    /* A 1-element array is already sorted; the guard must not walk it. */
    x16_str_sort(sort_one, 1);
    if (streq(sort_one[0], "banana") == 0) {
        ok = 0;
    }

    t_check(ok, "STR_SORT");
}

/* ------------------------------------------------------------------ */

int main(void) {
    t_init();

    test_str_length();
    test_str_copy();
    test_str_ncopy();
    test_str_append();
    test_str_nappend();
    test_str_compare();
    test_str_hash();

    test_case_char_pet();
    test_case_char_iso();
    test_case_str_pet();
    test_case_str_iso();
    test_cmp_nocase_pet();
    test_cmp_nocase_iso();

    test_ctype();

    test_str_find();
    test_str_rfind();
    test_str_contains();
    test_str_find_eol();
    test_pattern_literal();
    test_pattern_qmark();
    test_pattern_star();

    test_str_left();
    test_str_right();
    test_str_slice();
    test_str_ltrim();
    test_str_rtrim();
    test_str_trim();

    test_str_sort();

    t_done();
    return 0;
}
