// =====================================================================
// x16clib :: x16/string.c -- NUL-terminated string toolkit
// =====================================================================
// The six ca65 string modules (string, case, ctype, find, slice,
// strsort) in one file, mirroring the one-header choice of x16/string.h.
// These are pure-CPU byte loops, so unlike the hardware modules they are
// PLAIN C: each function follows its src_ca65/string/*.s twin's control
// flow -- the byte-index loops carry the same 256-wrap semantics the
// asm's `iny / bne` had.
//
// Every compared byte is an explicit value, exactly as the ca65 port
// wrote them: KickC's default encoding is petscii_mixed, and a library
// must not depend on which #pragma encoding the including program has
// in effect. The PETSCII/ISO case and ctype bounds are FIXED BYTE
// RANGES from the upstream assembly, not reasoned from ASCII.
//
// One deliberate deviation: str_pattern_match. The ca65 matcher
// recurses on '*' (4 bytes of CPU stack per star); KickC's phi-call
// convention cannot recurse, so this is the standard iterative
// greedy-with-backtracking wildcard matcher -- same language ('?' one
// char, '*' any run including none), same verdicts.
// =====================================================================

#include <x16/string.h>

// ---------------------------------------------------------------------
// Fundamentals (src_ca65/string/string.s)
// ---------------------------------------------------------------------

unsigned char x16_str_length(const char *s) {
    unsigned char y = 0;
    for (;;) {
        if (s[y] == 0) {
            break;
        }
        ++y;
        if (y == 0) {                   // 256+ bytes without a NUL: report 0
            break;
        }
    }
    return y;
}

unsigned char x16_str_copy(char *target, const char *source) {
    unsigned char y = 0;
    unsigned char c;
    for (;;) {
        c = (unsigned char)source[y];
        target[y] = (char)c;            // copies the NUL too, then stops
        if (c == 0) {
            break;
        }
        ++y;
        if (y == 0) {
            break;
        }
    }
    return y;
}

unsigned char x16_str_ncopy(char *target, const char *source,
                            unsigned char maxlength) {
    unsigned char y = 0;
    unsigned char c;
    for (;;) {
        if (y == maxlength) {           // hit the cap
            target[y] = 0;              // terminate at the cap
            break;
        }
        c = (unsigned char)source[y];
        target[y] = (char)c;
        if (c == 0) {                   // copied the NUL
            break;
        }
        ++y;
    }
    return y;
}

unsigned char x16_str_append(char *target, const char *suffix) {
    unsigned char tlen = x16_str_length(target);
    char *t = target + tlen;            // the append point
    unsigned char y = 0;
    unsigned char c;
    for (;;) {
        c = (unsigned char)suffix[y];   // copy the suffix in
        t[y] = (char)c;
        if (c == 0) {
            break;
        }
        ++y;
        if (y == 0) {
            break;
        }
    }
    return (unsigned char)(y + tlen);   // result length = target + suffix
}

unsigned char x16_str_nappend(char *target, const char *suffix,
                              unsigned char maxlength) {
    unsigned char tlen = x16_str_length(target);
    unsigned char room;
    char *t;
    unsigned char y;
    unsigned char c;
    if (tlen >= maxlength) {            // no room at all: leave it be
        return tlen;
    }
    room = (unsigned char)(maxlength - tlen);
    t = target + tlen;
    y = 0;
    for (;;) {
        if (y == room) {                // stop at the room limit
            t[y] = 0;                   // terminate at the cap
            break;
        }
        c = (unsigned char)suffix[y];
        t[y] = (char)c;
        if (c == 0) {
            break;
        }
        ++y;
    }
    return (unsigned char)(y + tlen);
}

signed char x16_str_compare(const char *s1, const char *s2) {
    unsigned char y = 0;
    unsigned char c1;
    unsigned char c2;
    for (;;) {
        c1 = (unsigned char)s1[y];
        if (c1 == 0) {
            break;
        }
        c2 = (unsigned char)s2[y];
        if (c1 != c2) {
            if (c1 < c2) {
                return -1;
            }
            return 1;
        }
        ++y;
        if (y == 0) {                   // ran the whole page: equal
            return 0;
        }
    }
    if (s2[y] == 0) {                   // string1 ended; string2 too?
        return 0;
    }
    return -1;                          // string1 is the shorter -> before
}

unsigned char x16_str_hash(const char *s) {
    unsigned char h = 179;              // hash(-1) = 179, the seed
    unsigned char cy = 0;               // the asm's carry threads the rols
    unsigned char ncy;
    unsigned char y = 0;
    unsigned char c;
    for (;;) {
        c = (unsigned char)s[y];
        if (c == 0) {
            break;
        }
        ncy = (h & 0x80) != 0 ? (unsigned char)1 : (unsigned char)0;
        h = (unsigned char)((h << 1) | cy);   // rol: 9-bit rotate via cy
        cy = ncy;
        h = (unsigned char)(h ^ c);
        ++y;
        if (y == 0) {
            break;
        }
    }
    return h;
}

// ---------------------------------------------------------------------
// Case folding (src_ca65/string/case.s) -- PETSCII and ISO place the
// letters at different codes, so the two encodings genuinely swap:
// PETSCII "lower" is numerically ISO "upper". That is the charset,
// not a bug.
// ---------------------------------------------------------------------

unsigned char x16_str_lowerchar(unsigned char c) {
    c = (unsigned char)(c & 0x7f);
    if (c >= 97 && c < 123) {
        c = (unsigned char)(c & 0xdf);
    }
    return c;
}

unsigned char x16_str_lowerchar_iso(unsigned char c) {
    if (c >= 65 && c < 91) {
        c = (unsigned char)(c | 0x20);
    }
    return c;
}

unsigned char x16_str_upperchar(unsigned char c) {
    if (c >= 65 && c < 91) {
        c = (unsigned char)(c | 0x20);
    }
    return c;
}

unsigned char x16_str_upperchar_iso(unsigned char c) {
    if (c >= 97 && c < 123) {
        c = (unsigned char)(c & 0xdf);
    }
    return c;
}

unsigned char x16_str_lower(char *s) {
    char *p = s;
    unsigned char y = 0;
    unsigned char c;
    for (;;) {
        c = (unsigned char)p[y];
        if (c == 0) {
            break;
        }
        p[y] = (char)x16_str_lowerchar(c);
        ++y;
        if (y == 0) {
            break;
        }
    }
    return y;
}

unsigned char x16_str_lower_iso(char *s) {
    char *p = s;
    unsigned char y = 0;
    unsigned char c;
    for (;;) {
        c = (unsigned char)p[y];
        if (c == 0) {
            break;
        }
        p[y] = (char)x16_str_lowerchar_iso(c);
        ++y;
        if (y == 0) {
            break;
        }
    }
    return y;
}

unsigned char x16_str_upper(char *s) {
    char *p = s;
    unsigned char y = 0;
    unsigned char c;
    for (;;) {
        c = (unsigned char)p[y];
        if (c == 0) {
            break;
        }
        p[y] = (char)x16_str_upperchar(c);
        ++y;
        if (y == 0) {
            break;
        }
    }
    return y;
}

unsigned char x16_str_upper_iso(char *s) {
    char *p = s;
    unsigned char y = 0;
    unsigned char c;
    for (;;) {
        c = (unsigned char)p[y];
        if (c == 0) {
            break;
        }
        p[y] = (char)x16_str_upperchar_iso(c);
        ++y;
        if (y == 0) {
            break;
        }
    }
    return y;
}

signed char x16_str_compare_nocase(const char *s1, const char *s2) {
    unsigned char y = 0;
    unsigned char c1;
    unsigned char c2;
    for (;;) {
        c1 = (unsigned char)s1[y];
        if (c1 == 0) {
            break;
        }
        c1 = x16_str_lowerchar(c1);
        c2 = x16_str_lowerchar((unsigned char)s2[y]);
        if (c1 != c2) {
            if (c1 < c2) {
                return -1;
            }
            return 1;
        }
        ++y;
        if (y == 0) {
            return 0;
        }
    }
    if (s2[y] == 0) {
        return 0;
    }
    return -1;
}

signed char x16_str_compare_nocase_iso(const char *s1, const char *s2) {
    unsigned char y = 0;
    unsigned char c1;
    unsigned char c2;
    for (;;) {
        c1 = (unsigned char)s1[y];
        if (c1 == 0) {
            break;
        }
        c1 = x16_str_lowerchar_iso(c1);
        c2 = x16_str_lowerchar_iso((unsigned char)s2[y]);
        if (c1 != c2) {
            if (c1 < c2) {
                return -1;
            }
            return 1;
        }
        ++y;
        if (y == 0) {
            return 0;
        }
    }
    if (s2[y] == 0) {
        return 0;
    }
    return -1;
}

// ---------------------------------------------------------------------
// Classification (src_ca65/string/ctype.s) -- each answers 0 or 1.
// The bounds are the upstream assembly's explicit byte values.
// ---------------------------------------------------------------------

unsigned char x16_str_isdigit(unsigned char c) {
    if (c >= 0x30 && c <= 0x39) {       // '0'..'9'
        return 1;
    }
    return 0;
}

unsigned char x16_str_isxdigit(unsigned char c) {
    if (c >= 0x30 && c <= 0x39) {       // '0'..'9'
        return 1;
    }
    if (c >= 0x41 && c <= 0x46) {       // 65..70
        return 1;
    }
    if (c >= 0x61 && c <= 0x66) {       // 97..102
        return 1;
    }
    return 0;
}

unsigned char x16_str_islower(unsigned char c) {
    if (c >= 97 && c <= 122) {          // same in both encodings
        return 1;
    }
    return 0;
}

unsigned char x16_str_isupper(unsigned char c) {
    if (c >= 97 && c <= 122) {          // PETSCII upper case, and
        return 1;
    }
    if (c >= 193 && c <= 218) {         // ...its alias range
        return 1;
    }
    return 0;
}

unsigned char x16_str_isupper_iso(unsigned char c) {
    if (c >= 65 && c <= 90) {           // ISO 'A'..'Z'
        return 1;
    }
    return 0;
}

unsigned char x16_str_isletter(unsigned char c) {
    if (x16_str_islower(c)) {
        return 1;
    }
    return x16_str_isupper(c);
}

unsigned char x16_str_isletter_iso(unsigned char c) {
    if (x16_str_islower(c)) {
        return 1;
    }
    return x16_str_isupper_iso(c);
}

unsigned char x16_str_isspace(unsigned char c) {
    if (c == 32 || c == 13 || c == 9 || c == 10 || c == 141 || c == 160) {
        return 1;
    }
    return 0;
}

unsigned char x16_str_isprint(unsigned char c) {
    if (c >= 160) {                     // PETSCII printable: 32-127, 160-255
        return 1;
    }
    if (c >= 32 && c < 128) {
        return 1;
    }
    return 0;
}

unsigned char x16_str_isprint_iso(unsigned char c) {
    if (c >= 160) {                     // ISO printable: 32-126, 160-255
        return 1;
    }
    if (c >= 32 && c < 127) {
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------
// Searching (src_ca65/string/find.s)
// ---------------------------------------------------------------------

unsigned char x16_str_find(const char *s, unsigned char c) {
    unsigned char y = 0;
    unsigned char b;
    for (;;) {
        b = (unsigned char)s[y];
        if (b == 0) {
            return 255;
        }
        if (b == c) {
            return y;
        }
        ++y;
        if (y == 0) {
            return 255;
        }
    }
}

unsigned char x16_str_rfind(const char *s, unsigned char c) {
    unsigned char y = 0;
    while (s[y] != 0) {                 // measure the string
        ++y;
        if (y == 0) {
            break;
        }
    }
    if (y == 0) {                       // empty string
        return 255;
    }
    --y;                                // start at the last character
    for (;;) {
        if ((unsigned char)s[y] == c) {
            return y;
        }
        if (y == 0) {                   // walked past index 0
            return 255;
        }
        --y;
    }
}

unsigned char x16_str_contains(const char *s, unsigned char c) {
    if (x16_str_find(s, c) != 255) {
        return 1;
    }
    return 0;
}

unsigned char x16_str_find_eol(const char *s) {
    unsigned char y = 0;
    unsigned char b;
    for (;;) {
        b = (unsigned char)s[y];
        if (b == 0) {
            return 255;
        }
        if (b == 13 || b == 10) {
            return y;
        }
        ++y;
        if (y == 0) {
            return 255;
        }
    }
}

// The matcher's five indices live at module scope, not as locals. As
// locals they are mutually-dependent loop variables -- si and pi are
// each reassigned from star_si/star_pi inside the loop -- and KickC's
// SSA pass throws a bare NullPointerException on the resulting phis,
// with no file, line or hint. Reduced to a 30-line standalone case to
// confirm it is the cross-assignment and not this module.
__mem volatile unsigned char x16__pm_si;        // string index
__mem volatile unsigned char x16__pm_pi;        // pattern index
__mem volatile unsigned char x16__pm_star_pi;   // just past the last '*'
__mem volatile unsigned char x16__pm_star_si;   // what it has swallowed
__mem volatile unsigned char x16__pm_have_star;
__mem volatile unsigned char x16__pm_state;     // 2 walking, 1 done, 0 no

unsigned char x16_str_pattern_match(const char *s, const char *pattern) {
    unsigned char sc;
    unsigned char pc;

    x16__pm_si = 0;
    x16__pm_pi = 0;
    x16__pm_star_pi = 0;
    x16__pm_star_si = 0;
    x16__pm_have_star = 0;
    x16__pm_state = 2;

    while (x16__pm_state == 2) {
        sc = (unsigned char)s[x16__pm_si];
        if (sc == 0) {
            x16__pm_state = 1;          // string exhausted
        } else {
            pc = (unsigned char)pattern[x16__pm_pi];
            if (pc == 0x2a) {           // '*': remember it, match none yet
                ++x16__pm_pi;
                x16__pm_have_star = 1;
                x16__pm_star_pi = x16__pm_pi;
                x16__pm_star_si = x16__pm_si;
            } else if (pc == 0x3f || pc == sc) {
                ++x16__pm_pi;           // '?' matches any single character
                ++x16__pm_si;
            } else if (x16__pm_have_star != 0) {
                ++x16__pm_star_si;      // grow what the last '*' swallows
                x16__pm_si = x16__pm_star_si;
                x16__pm_pi = x16__pm_star_pi;
            } else {
                x16__pm_state = 0;
            }
        }
    }
    if (x16__pm_state == 0) {
        return 0;
    }
    // The string is exhausted: only '*'s may remain in the pattern.
    while ((unsigned char)pattern[x16__pm_pi] == 0x2a) {
        ++x16__pm_pi;
    }
    if (pattern[x16__pm_pi] == 0) {
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------
// Slicing and trimming (src_ca65/string/slice.s). Whitespace is space,
// TAB, CR, LF, shift-CR (141) and shift-space (160), the same set as
// x16_str_isspace.
// ---------------------------------------------------------------------

// whitespace test, private to the slice routines (the ca65 module
// carried its own copy too, so slicing does not drag ctype in)
unsigned char x16__slice_isws(unsigned char c) {
    if (c == 32 || c == 13 || c == 10 || c == 9 || c == 141 || c == 160) {
        return 1;
    }
    return 0;
}

void x16_str_left(char *target, const char *source, unsigned char length) {
    unsigned char y = length;
    target[y] = 0;                      // terminate the target at [length]
    while (y != 0) {
        --y;
        target[y] = source[y];
    }
}

void x16_str_right(char *target, const char *source, unsigned char length) {
    unsigned char len = 0;
    const char *src;
    unsigned char y;
    while (source[len] != 0) {          // measure the source
        ++len;
        if (len == 0) {
            break;
        }
    }
    src = source + (unsigned char)(len - length);   // source += total - length
    y = length;                         // then it is just a left-copy
    target[y] = 0;
    while (y != 0) {
        --y;
        target[y] = src[y];
    }
}

void x16_str_slice(char *target, const char *source,
                   unsigned char start, unsigned char length) {
    const char *src = source + start;
    unsigned char y = length;
    target[y] = 0;
    while (y != 0) {
        --y;
        target[y] = src[y];
    }
}

unsigned char x16_str_ltrim(char *s) {
    char *p = s;
    unsigned char y = 0;
    unsigned char k;
    unsigned char c;
    const char *src;
    for (;;) {
        c = (unsigned char)p[y];
        if (c == 0) {                   // ran off the end: all whitespace
            p[0] = 0;
            return 0;
        }
        if (x16__slice_isws(c) == 0) {
            break;
        }
        ++y;
        if (y == 0) {                   // pathological wrap: treat as no-lead
            break;
        }
    }
    if (y == 0) {                       // nothing to strip; count the length
        while (p[y] != 0) {
            ++y;
            if (y == 0) {
                break;
            }
        }
        return y;
    }
    src = p + y;                        // shift the rest down, in place
    k = 0;
    for (;;) {
        c = (unsigned char)src[k];
        p[k] = (char)c;
        if (c == 0) {
            break;
        }
        ++k;
        if (k == 0) {
            break;
        }
    }
    return k;
}

unsigned char x16_str_rtrim(char *s) {
    char *p = s;
    unsigned char y = 0;
    while (p[y] != 0) {                 // measure
        ++y;
        if (y == 0) {
            break;
        }
    }
    for (;;) {
        if (y == 0) {                   // empty, or every char was whitespace
            break;
        }
        --y;
        if (x16__slice_isws((unsigned char)p[y]) != 0) {
            continue;                   // whitespace: keep stepping back
        }
        ++y;                            // keep the last non-whitespace char
        break;
    }
    p[y] = 0;
    return y;
}

unsigned char x16_str_trim(char *s) {
    x16_str_rtrim(s);
    return x16_str_ltrim(s);
}

// ---------------------------------------------------------------------
// Sorting (src_ca65/string/strsort.s) -- insertion sort of a
// string-POINTER array, ascending by content through x16_str_compare.
// The strings never move; only the pointer array is permuted. The
// 2-byte elements are handled as words, exactly as the asm did.
// ---------------------------------------------------------------------

void x16_str_sort(const char **array, unsigned int count) {
    unsigned int *arr = (unsigned int *)array;
    unsigned int i;
    unsigned int j;
    unsigned int key;
    unsigned int aj;
    signed char v;
    if (count < 2) {                    // nothing to do
        return;
    }
    for (i = 1; i < count; ++i) {
        key = arr[i];                   // the pointer being inserted
        j = i - 1;
        for (;;) {
            aj = arr[j];
            v = x16_str_compare((char *)aj, (char *)key);
            if (v != 1) {               // arr[j] <= key: stop shifting
                arr[j + 1] = key;
                break;
            }
            arr[j + 1] = aj;            // arr[j+1] = arr[j]
            if (j == 0) {               // key belongs at arr[0]
                arr[0] = key;
                break;
            }
            --j;
        }
    }
}
