/* =====================================================================
 * x16clib :: x16/string.h -- NUL-terminated string toolkit (vbcc)
 * =====================================================================
 * The assembly library's string modules: fundamentals (length, copy,
 * append, compare, hash), case folding, character classification,
 * searching, slicing/trimming, and a pointer-array sort. Strings are
 * NUL-terminated, at most 255 characters plus the NUL, and nothing here
 * bounds-checks -- make your target buffers big enough.
 *
 * vbcc's own <string.h> overlaps some of this; these match the assembly
 * library's exact semantics (lengths in a byte, capped copies that
 * always NUL-terminate, compare answering -1/0/1) and share no C
 * runtime code.
 *
 * ENCODINGS. The copy/search/slice routines are pure memory ops and do
 * not care what the bytes mean. Case folding and the case-sensitive
 * predicates do: PETSCII and ISO place the letters at different codes,
 * so those come in pairs -- x16_str_upper() for PETSCII bytes,
 * x16_str_upper_iso() for ISO bytes -- and the two genuinely swap
 * directions (PETSCII "lower" is numerically ISO "upper"; that is the
 * charset, not a bug). Mind vbcc's side of the same trap: the +x16
 * config compiles with -cbmascii, so C string and char literals are
 * stored in PETSCII -- feed the _iso routines bytes you built yourself,
 * not literals.
 * =====================================================================
 */

#ifndef X16_STRING_H
#define X16_STRING_H

/* ---------------------------------------------------------------------
 * Fundamentals (string/string.s)
 * ------------------------------------------------------------------ */

/* Length in characters, up to the first NUL. A run of 256+ bytes
** without a NUL reports 0. */
unsigned char x16_str_length(__reg("r0/r1") const char *s);

/* target = source, overwriting. Returns the length copied. */
unsigned char x16_str_copy(__reg("r0/r1") char *target,
                           __reg("r2/r3") const char *source);

/* Copy at most maxlength characters, then NUL-terminate (always).
** Returns the length of the target string. */
unsigned char x16_str_ncopy(__reg("r0/r1") char *target,
                            __reg("r2/r3") const char *source,
                            __reg("r4") unsigned char maxlength);

/* target += suffix. Returns the length of the resulting string. */
unsigned char x16_str_append(__reg("r0/r1") char *target,
                             __reg("r2/r3") const char *suffix);

/* Append, but never let the target exceed maxlength characters; the
** suffix is cut to fit, and if the target is already at or past the cap
** nothing changes. Returns the length of the resulting string. */
unsigned char x16_str_nappend(__reg("r0/r1") char *target,
                              __reg("r2/r3") const char *suffix,
                              __reg("r4") unsigned char maxlength);

/* Strict byte-order compare, for sorting: -1 if s1 < s2, 0 if equal,
** 1 if greater. A prefix sorts before its extension. */
signed char x16_str_compare(__reg("r0/r1") const char *s1,
                            __reg("r2/r3") const char *s2);

/* An 8-bit rolling hash: hash(-1) = 179, then rol-and-XOR per
** character. Cheap identity check, not cryptography. */
unsigned char x16_str_hash(__reg("r0/r1") const char *s);

/* ---------------------------------------------------------------------
 * Case folding (string/case.s)
 * ------------------------------------------------------------------ */

/* Fold one character; anything that is not a letter passes through. */
unsigned char x16_str_lowerchar(__reg("a") unsigned char c);
unsigned char x16_str_upperchar(__reg("a") unsigned char c);
unsigned char x16_str_lowerchar_iso(__reg("a") unsigned char c);
unsigned char x16_str_upperchar_iso(__reg("a") unsigned char c);

/* Fold a whole string in place. Returns its length. */
unsigned char x16_str_lower(__reg("r0/r1") char *s);
unsigned char x16_str_upper(__reg("r0/r1") char *s);
unsigned char x16_str_lower_iso(__reg("r0/r1") char *s);
unsigned char x16_str_upper_iso(__reg("r0/r1") char *s);

/* Case-insensitive compare: both sides folded, then -1/0/1 like
** x16_str_compare. */
signed char x16_str_compare_nocase(__reg("r0/r1") const char *s1,
                                   __reg("r2/r3") const char *s2);
signed char x16_str_compare_nocase_iso(__reg("r0/r1") const char *s1,
                                       __reg("r2/r3") const char *s2);

/* ---------------------------------------------------------------------
 * Classification (string/ctype.s) -- each answers 0 or 1
 * ------------------------------------------------------------------ */

/* The same in both encodings: '0'-'9' (48-57); hex digits 0-9 plus
** 65-70 and 97-102; the byte range 97-122; space, TAB, CR, LF,
** shift-CR (141) and shift-space (160). */
unsigned char x16_str_isdigit(__reg("a") unsigned char c);
unsigned char x16_str_isxdigit(__reg("a") unsigned char c);
unsigned char x16_str_islower(__reg("a") unsigned char c);
unsigned char x16_str_isspace(__reg("a") unsigned char c);

/* Encoding-specific: PETSCII upper case lives at 97-122 and 193-218,
** ISO upper case at 65-90. A letter is lower or upper; printable is
** 32-127/160-255 in PETSCII, 32-126/160-255 in ISO. */
unsigned char x16_str_isupper(__reg("a") unsigned char c);
unsigned char x16_str_isupper_iso(__reg("a") unsigned char c);
unsigned char x16_str_isletter(__reg("a") unsigned char c);
unsigned char x16_str_isletter_iso(__reg("a") unsigned char c);
unsigned char x16_str_isprint(__reg("a") unsigned char c);
unsigned char x16_str_isprint_iso(__reg("a") unsigned char c);

/* ---------------------------------------------------------------------
 * Searching (string/find.s)
 * ------------------------------------------------------------------ */

/* First index of c scanning left to right (find) or right to left
** (rfind); 255 when it is not there. */
unsigned char x16_str_find(__reg("r0/r1") const char *s,
                           __reg("r2") unsigned char c);
unsigned char x16_str_rfind(__reg("r0/r1") const char *s,
                            __reg("r2") unsigned char c);

/* 1 if c occurs in s. */
unsigned char x16_str_contains(__reg("r0/r1") const char *s,
                               __reg("r2") unsigned char c);

/* First index of a CR (13) or LF (10); 255 when the string has neither. */
unsigned char x16_str_find_eol(__reg("r0/r1") const char *s);

/* Wildcard match: '?' matches any single character, '*' any run
** including none; everything else matches itself, case-sensitively.
** Answers 1 on a match. Each '*' in the pattern costs 4 bytes of CPU
** stack while matching. */
unsigned char x16_str_pattern_match(__reg("r0/r1") const char *s,
                                    __reg("r2/r3") const char *pattern);

/* ---------------------------------------------------------------------
 * Slicing and trimming (string/slice.s)
 * ------------------------------------------------------------------ */

/* Copy the first / last `length` characters of source into target,
** NUL-terminated. `length` must not exceed the source length. */
void x16_str_left(__reg("r0/r1") char *target,
                  __reg("r2/r3") const char *source,
                  __reg("r4") unsigned char length);
void x16_str_right(__reg("r0/r1") char *target,
                   __reg("r2/r3") const char *source,
                   __reg("r4") unsigned char length);

/* Copy `length` characters starting at index `start`. The run must lie
** within the source. */
void x16_str_slice(__reg("r0/r1") char *target,
                   __reg("r2/r3") const char *source,
                   __reg("r4") unsigned char start,
                   __reg("r6") unsigned char length);

/* Drop whitespace -- the x16_str_isspace set -- from the left end, the
** right end, or both, in place. Return the new length. */
unsigned char x16_str_ltrim(__reg("r0/r1") char *s);
unsigned char x16_str_rtrim(__reg("r0/r1") char *s);
unsigned char x16_str_trim(__reg("r0/r1") char *s);

/* ---------------------------------------------------------------------
 * Sorting (string/strsort.s)
 * ------------------------------------------------------------------ */

/* Sort an array of string pointers ascending by content, with
** x16_str_compare's ordering. The strings never move; only the pointer
** array is permuted. Insertion sort: fine for menu-sized arrays. */
void x16_str_sort(__reg("r0/r1") const char **array,
                  __reg("r2/r3") unsigned int count);

#endif /* X16_STRING_H */
