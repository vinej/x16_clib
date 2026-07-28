/* =====================================================================
 * x16clib :: test_vbcc/runner4.c -- the string library, standalone
 * =====================================================================
 * The vbcc port of the ca65 suite's runner4.c: same checks, same names,
 * so the builds' results line up. Run it with
 *
 *     .\build_vbcc.ps1 -Test -Source test_vbcc\runner4.c
 *
 * CHARMAPS, because every byte here is an assertion. vc's +x16 config
 * compiles with -cbmascii, so every C string and char literal in THIS
 * file is stored as its PETSCII byte ('a' is $41, 'A' is $C1). That is
 * fine for the encoding-AGNOSTIC routines (copy, find, sort...), whose
 * tests only need the literal bytes to be self-consistent -- and for
 * nothing else: the PETSCII-flavoured case/ctype tests build their
 * inputs from explicit PETSCII bytes, the ISO tests from explicit ISO
 * bytes, so the charmap cannot silently change what is being tested.
 * Where the ca65 runner spelled letters as explicit ASCII bytes NEXT TO
 * a literal comparand (the trim tests), this port uses 'a'/'b' literals
 * instead, so buffer and comparand pass through the same charmap.
 *
 * The ABI checks follow runner.c's principle: each test is built so the
 * answer changes if a shim reads an argument from the wrong register --
 * the copy test in particular goes red if source and target swap.
 * =====================================================================
 */

#include "testlib.h"
#include <x16/string.h>

/* ------------------------------------------------------------------ */

/* Plain byte-wise equality, independent of the library under test. */
static unsigned char streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

/* n bytes of buf equal exp, byte-for-byte. */
static unsigned char bytes_eq(const unsigned char *buf,
                              const unsigned char *exp, unsigned char n)
{
    unsigned char i;

    for (i = 0; i < n; ++i) {
        if (buf[i] != exp[i]) {
            return 0;
        }
    }
    return 1;
}

static char dst[24];

static void poison(void)
{
    unsigned char i;

    for (i = 0; i < sizeof dst; ++i) {
        dst[i] = 0xAA;
    }
    /* A zero early on keeps a mutated shim's runaway copy short. */
    dst[3] = 0;
}

/* ------------------------------------------------------------------ */
/* fundamentals                                                        */
/* ------------------------------------------------------------------ */

static void test_str_length(void)
{
    t_check(x16_str_length("commander") == 9 && x16_str_length("") == 0,
            "STR_LENGTH");
}

/* THE mutation canary: if the copy shim swaps source and target, the
** poisoned destination never receives "hello" and this goes red.
*/
static void test_str_copy(void)
{
    static char csrc[] = "hello";

    poison();
    t_check(x16_str_copy(dst, csrc) == 5
            && streq(dst, "hello") && dst[5] == 0,
            "STR_COPY");
}

static void test_str_ncopy(void)
{
    unsigned char ok;

    poison();
    ok = x16_str_ncopy(dst, "commander", 4) == 4 && streq(dst, "comm");

    poison();
    ok = ok && x16_str_ncopy(dst, "ab", 6) == 2 && streq(dst, "ab");

    t_check(ok, "STR_NCOPY");
}

static void test_str_append(void)
{
    poison();
    x16_str_copy(dst, "foo");
    t_check(x16_str_append(dst, "bar") == 6 && streq(dst, "foobar"),
            "STR_APPEND");
}

static void test_str_nappend(void)
{
    unsigned char ok;

    poison();
    x16_str_copy(dst, "foo");

    /* Room for two of six suffix chars: cut to fit, NUL at the cap. */
    ok = x16_str_nappend(dst, "barbaz", 5) == 5 && streq(dst, "fooba");

    /* Already at the cap: length reported, nothing changes. */
    ok = ok && x16_str_nappend(dst, "zz", 5) == 5 && streq(dst, "fooba");

    t_check(ok, "STR_NAPPEND");
}

static void test_str_compare(void)
{
    t_check(x16_str_compare("abc", "abc") == 0
            && x16_str_compare("abc", "abd") == -1
            && x16_str_compare("abd", "abc") == 1
            && x16_str_compare("ab", "abc") == -1   /* prefix sorts first */
            && x16_str_compare("abc", "ab") == 1,
            "STR_COMPARE");
}

static void test_str_hash(void)
{
    t_check(x16_str_hash("commander") == x16_str_hash("commander")
            && x16_str_hash("commander") != x16_str_hash("commandes")
            && x16_str_hash("") == 179,             /* the seed, by spec */
            "STR_HASH");
}

/* ------------------------------------------------------------------ */
/* case folding -- inputs are explicit encoding bytes (see header)     */
/* ------------------------------------------------------------------ */

static void test_case_char_pet(void)
{
    /* PETSCII: lower-case letters at $41-$5A, upper case at $61-$7A
    ** with the $C1-$DA alias. Folding down lands on $41, up on $61.
    */
    t_check(x16_str_lowerchar(0xC1) == 0x41     /* 'A' -> 'a' */
            && x16_str_lowerchar(0x61) == 0x41  /* alias form too */
            && x16_str_lowerchar(0x41) == 0x41  /* already lower */
            && x16_str_lowerchar(0x31) == 0x31  /* digits untouched */
            && x16_str_upperchar(0x41) == 0x61  /* 'a' -> 'A' */
            && x16_str_upperchar(0x61) == 0x61
            && x16_str_upperchar(0x31) == 0x31,
            "CASE_CHAR_PET");
}

static void test_case_char_iso(void)
{
    /* ISO bytes: the classic 65-90 / 97-122 ranges. */
    t_check(x16_str_lowerchar_iso(65) == 97
            && x16_str_lowerchar_iso(90) == 122
            && x16_str_lowerchar_iso(97) == 97
            && x16_str_lowerchar_iso(48) == 48
            && x16_str_upperchar_iso(97) == 65
            && x16_str_upperchar_iso(122) == 90
            && x16_str_upperchar_iso(65) == 65
            && x16_str_upperchar_iso(48) == 48,
            "CASE_CHAR_ISO");
}

static void test_case_str_pet(void)
{
    /* "HellO1" in PETSCII bytes: upper H, lower ell, upper O, digit. */
    static unsigned char pbuf[] = { 0xC8, 0x45, 0x4C, 0x4C, 0xCF, 0x31, 0 };
    static const unsigned char lo[] = { 0x48, 0x45, 0x4C, 0x4C, 0x4F, 0x31, 0 };
    static const unsigned char up[] = { 0x68, 0x65, 0x6C, 0x6C, 0x6F, 0x31, 0 };
    unsigned char ok;

    ok = x16_str_lower((char *)pbuf) == 6 && bytes_eq(pbuf, lo, 7);
    ok = ok && x16_str_upper((char *)pbuf) == 6 && bytes_eq(pbuf, up, 7);

    t_check(ok, "CASE_STR_PET");
}

static void test_case_str_iso(void)
{
    /* "Hello1" in ISO bytes. */
    static unsigned char ibuf[] = { 72, 101, 108, 108, 111, 49, 0 };
    static const unsigned char up[] = { 72, 69, 76, 76, 79, 49, 0 };
    static const unsigned char lo[] = { 104, 101, 108, 108, 111, 49, 0 };
    unsigned char ok;

    ok = x16_str_upper_iso((char *)ibuf) == 6 && bytes_eq(ibuf, up, 7);
    ok = ok && x16_str_lower_iso((char *)ibuf) == 6 && bytes_eq(ibuf, lo, 7);

    t_check(ok, "CASE_STR_ISO");
}

static void test_cmp_nocase_pet(void)
{
    /* "Ab" (upper A, lower b) vs "ab" -- equal once folded. */
    static const char s1[] = { 0xC1, 0x42, 0 };
    static const char s2[] = { 0x41, 0x62, 0 };
    static const char a[]  = { 0x41, 0 };
    static const char b[]  = { 0x42, 0 };
    static const char ab[] = { 0x41, 0x42, 0 };

    t_check(x16_str_compare_nocase(s1, s2) == 0
            && x16_str_compare_nocase(a, b) == -1
            && x16_str_compare_nocase(b, a) == 1
            && x16_str_compare_nocase(a, ab) == -1, /* prefix sorts first */
            "CMP_NOCASE_PET");
}

static void test_cmp_nocase_iso(void)
{
    /* ISO bytes: "AB" vs "ab" equal folded; 'a'(97) < 'B'(66 -> 98). */
    static const char s1[] = { 65, 66, 0 };
    static const char s2[] = { 97, 98, 0 };
    static const char a[]  = { 97, 0 };
    static const char bu[] = { 66, 0 };

    t_check(x16_str_compare_nocase_iso(s1, s2) == 0
            && x16_str_compare_nocase_iso(a, bu) == -1
            && x16_str_compare_nocase_iso(bu, a) == 1,
            "CMP_NOCASE_ISO");
}

/* ------------------------------------------------------------------ */
/* classification -- explicit bytes, true AND false cases each         */
/* ------------------------------------------------------------------ */

static void test_ctype(void)
{
    t_check(x16_str_isdigit(48) && x16_str_isdigit(57)
            && !x16_str_isdigit(47) && !x16_str_isdigit(58)
            && !x16_str_isdigit(65),
            "CT_ISDIGIT");

    /* 0-9 plus both letter ranges 65-70 and 97-102, either encoding. */
    t_check(x16_str_isxdigit(48) && x16_str_isxdigit(57)
            && x16_str_isxdigit(65) && x16_str_isxdigit(70)
            && x16_str_isxdigit(97) && x16_str_isxdigit(102)
            && !x16_str_isxdigit(64) && !x16_str_isxdigit(71)
            && !x16_str_isxdigit(96) && !x16_str_isxdigit(103),
            "CT_ISXDIGIT");

    t_check(x16_str_islower(97) && x16_str_islower(122)
            && !x16_str_islower(96) && !x16_str_islower(123)
            && !x16_str_islower(65),
            "CT_ISLOWER");

    /* PETSCII upper case: 97-122 and the 193-218 alias range. */
    t_check(x16_str_isupper(97) && x16_str_isupper(122)
            && x16_str_isupper(193) && x16_str_isupper(218)
            && !x16_str_isupper(96) && !x16_str_isupper(130)
            && !x16_str_isupper(65) && !x16_str_isupper(219),
            "CT_ISUPPER");

    t_check(x16_str_isupper_iso(65) && x16_str_isupper_iso(90)
            && !x16_str_isupper_iso(64) && !x16_str_isupper_iso(91)
            && !x16_str_isupper_iso(97),
            "CT_ISUPPER_ISO");

    t_check(x16_str_isletter(97) && x16_str_isletter(122)
            && x16_str_isletter(193) && x16_str_isletter(218)
            && !x16_str_isletter(48) && !x16_str_isletter(64)
            && !x16_str_isletter(219),
            "CT_ISLETTER");

    t_check(x16_str_isletter_iso(97) && x16_str_isletter_iso(65)
            && x16_str_isletter_iso(90) && x16_str_isletter_iso(122)
            && !x16_str_isletter_iso(48) && !x16_str_isletter_iso(91)
            && !x16_str_isletter_iso(64),
            "CT_ISLETTER_ISO");

    t_check(x16_str_isspace(32) && x16_str_isspace(13)
            && x16_str_isspace(10) && x16_str_isspace(9)
            && x16_str_isspace(141) && x16_str_isspace(160)
            && !x16_str_isspace(33) && !x16_str_isspace(0)
            && !x16_str_isspace(8),
            "CT_ISSPACE");

    t_check(x16_str_isprint(32) && x16_str_isprint(127)
            && x16_str_isprint(160) && x16_str_isprint(255)
            && !x16_str_isprint(31) && !x16_str_isprint(128)
            && !x16_str_isprint(159),
            "CT_ISPRINT");

    /* ISO stops at 126: DEL (127) is not printable there. */
    t_check(x16_str_isprint_iso(32) && x16_str_isprint_iso(126)
            && x16_str_isprint_iso(160) && x16_str_isprint_iso(255)
            && !x16_str_isprint_iso(127) && !x16_str_isprint_iso(31)
            && !x16_str_isprint_iso(128),
            "CT_ISPRINT_ISO");
}

/* ------------------------------------------------------------------ */
/* searching                                                           */
/* ------------------------------------------------------------------ */

static void test_str_find(void)
{
    t_check(x16_str_find("commander", 'm') == 2
            && x16_str_find("commander", 'r') == 8
            && x16_str_find("commander", 'z') == 255
            && x16_str_find("", 'a') == 255,
            "STR_FIND");
}

static void test_str_rfind(void)
{
    t_check(x16_str_rfind("commander", 'm') == 3
            && x16_str_rfind("commander", 'c') == 0
            && x16_str_rfind("commander", 'z') == 255
            && x16_str_rfind("", 'a') == 255,
            "STR_RFIND");
}

static void test_str_contains(void)
{
    t_check(x16_str_contains("commander", 'd') == 1
            && x16_str_contains("commander", 'z') == 0
            && x16_str_contains("", 'a') == 0,
            "STR_CONTAINS");
}

static void test_str_find_eol(void)
{
    /* Explicit bytes so no charmap can touch the CR/LF. */
    static const char crs[] = { 0x61, 0x62, 13, 0x63, 0 };
    static const char lfs[] = { 0x61, 10, 0x62, 0 };

    t_check(x16_str_find_eol(crs) == 2
            && x16_str_find_eol(lfs) == 1
            && x16_str_find_eol("abc") == 255,
            "STR_FIND_EOL");
}

static void test_pattern_literal(void)
{
    t_check(x16_str_pattern_match("hello", "hello") == 1
            && x16_str_pattern_match("hello", "world") == 0
            && x16_str_pattern_match("hello", "hell") == 0
            && x16_str_pattern_match("hell", "hello") == 0
            && x16_str_pattern_match("", "") == 1,
            "PM_LITERAL");
}

static void test_pattern_qmark(void)
{
    /* '?' consumes exactly one character -- never zero. */
    t_check(x16_str_pattern_match("hello", "h?llo") == 1
            && x16_str_pattern_match("hello", "?????") == 1
            && x16_str_pattern_match("hllo", "h?llo") == 0
            && x16_str_pattern_match("", "?") == 0,
            "PM_QMARK");
}

static void test_pattern_star(void)
{
    /* '*' matches any run including none, and multiples nest. */
    t_check(x16_str_pattern_match("", "*") == 1
            && x16_str_pattern_match("anything", "*") == 1
            && x16_str_pattern_match("hello", "h*o") == 1
            && x16_str_pattern_match("ho", "h*o") == 1
            && x16_str_pattern_match("hello", "*lo") == 1
            && x16_str_pattern_match("aXXbYYc", "a*b*c") == 1
            && x16_str_pattern_match("hello", "h*z") == 0,
            "PM_STAR");
}

/* ------------------------------------------------------------------ */
/* slicing and trimming                                                */
/* ------------------------------------------------------------------ */

static void test_str_left(void)
{
    unsigned char ok;

    poison();
    x16_str_left(dst, "commander", 3);
    ok = streq(dst, "com");

    poison();
    x16_str_left(dst, "commander", 0);
    ok = ok && dst[0] == 0;

    t_check(ok, "STR_LEFT");
}

static void test_str_right(void)
{
    poison();
    x16_str_right(dst, "commander", 3);
    t_check(streq(dst, "der"), "STR_RIGHT");
}

static void test_str_slice(void)
{
    unsigned char ok;

    poison();
    x16_str_slice(dst, "commander", 3, 4);
    ok = streq(dst, "mand");

    poison();
    x16_str_slice(dst, "commander", 2, 0);
    ok = ok && dst[0] == 0;

    t_check(ok, "STR_SLICE");
}

static void test_str_ltrim(void)
{
    /* Space and TAB in front as explicit bytes; the letters as 'a'/'b'
    ** literals so they match the "ab" comparand under -cbmascii. */
    static char lead[] = { 32, 9, 'a', 'b', 0 };
    static char none[] = { 'a', 0 };
    static char blank[] = { 32, 32, 0 };
    unsigned char ok;

    ok = x16_str_ltrim(lead) == 2 && streq(lead, "ab");
    ok = ok && x16_str_ltrim(none) == 1 && streq(none, "a");
    ok = ok && x16_str_ltrim(blank) == 0 && blank[0] == 0;

    t_check(ok, "STR_LTRIM");
}

static void test_str_rtrim(void)
{
    static char tail[] = { 'a', 'b', 32, 9, 13, 0 };
    static char blank[] = { 32, 32, 0 };
    unsigned char ok;

    ok = x16_str_rtrim(tail) == 2 && streq(tail, "ab");
    ok = ok && x16_str_rtrim(blank) == 0 && blank[0] == 0;

    t_check(ok, "STR_RTRIM");
}

static void test_str_trim(void)
{
    /* Shift-space (160) and shift-CR (141) count as whitespace too. */
    static char both[] = { 160, 32, 'a', 'b', 141, 10, 0 };

    t_check(x16_str_trim(both) == 2 && streq(both, "ab"), "STR_TRIM");
}

/* ------------------------------------------------------------------ */
/* sorting                                                             */
/* ------------------------------------------------------------------ */

static void test_str_sort(void)
{
    /* The duplicate lives in its own buffer, so a sort that merely
    ** compares pointers instead of contents cannot pass by accident.
    */
    static const char apple2[] = "apple";
    static const char *arr[] = { "melon", "apple", "banana", apple2, "" };
    static const char *one[] = { "banana" };
    unsigned char ok;

    x16_str_sort(arr, 5);

    ok = streq(arr[0], "")
         && streq(arr[1], "apple") && streq(arr[2], "apple")
         && streq(arr[3], "banana") && streq(arr[4], "melon");

    /* A 1-element array is already sorted; the guard must not walk it. */
    x16_str_sort(one, 1);
    ok = ok && streq(one[0], "banana");

    t_check(ok, "STR_SORT");
}

/* ------------------------------------------------------------------ */

int main(void)
{
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
