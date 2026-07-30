/* =====================================================================
 * x16clib :: x16/string.h -- NUL-terminated string toolkit
 * =====================================================================
 * The library's string modules: fundamentals (length, copy, append,
 * compare, hash), case folding, character classification, searching,
 * slicing/trimming, and a pointer-array sort. Strings are
 * NUL-terminated, at most 255 characters plus the NUL, and nothing here
 * bounds-checks -- make your target buffers big enough.
 *
 * ENCODINGS. The copy/search/slice routines are pure memory ops and do
 * not care what the bytes mean. Case folding and the case-sensitive
 * predicates do: PETSCII and ISO place the letters at different codes,
 * so those come in pairs -- x16_str_upper() for PETSCII bytes,
 * x16_str_upper_iso() for ISO bytes -- and the two genuinely swap
 * directions (PETSCII "lower" is numerically ISO "upper"; that is the
 * charset, not a bug). Mind the same trap elsewhere: a default
 * #pragma encoding is petscii_mixed, so a C string literal may not hold
 * the bytes you typed -- build test data for the _iso routines from
 * explicit bytes, not literals.
 * =====================================================================
 */

#ifndef X16_STRING_H
#define X16_STRING_H


/* ---------------------------------------------------------------------
 * Fundamentals
 * ------------------------------------------------------------------ */

/* Length in characters, up to the first NUL. A run of 256+ bytes
** without a NUL reports 0.
*/
unsigned char x16_str_length (const char *s);

/* target = source, overwriting. Returns the length copied. */
unsigned char x16_str_copy (char *target, const char *source);

/* Copy at most maxlength characters, then NUL-terminate (always).
** Returns the length of the target string.
*/
unsigned char x16_str_ncopy (char *target, const char *source,
                             unsigned char maxlength);

/* target += suffix. Returns the length of the resulting string. */
unsigned char x16_str_append (char *target, const char *suffix);

/* Append, but never let the target exceed maxlength characters; the
** suffix is cut to fit, and if the target is already at or past the cap
** nothing changes. Returns the length of the resulting string.
*/
unsigned char x16_str_nappend (char *target, const char *suffix,
                               unsigned char maxlength);

/* Strict byte-order compare, for sorting: -1 if s1 < s2, 0 if equal,
** 1 if greater. A prefix sorts before its extension.
*/
signed char x16_str_compare (const char *s1, const char *s2);

/* An 8-bit rolling hash: hash(-1) = 179, then rol-and-XOR per
** character. Cheap identity check, not cryptography.
*/
unsigned char x16_str_hash (const char *s);

/* ---------------------------------------------------------------------
 * Case folding
 * ------------------------------------------------------------------ */

/* Fold one character; anything that is not a letter passes through. */
unsigned char x16_str_lowerchar (unsigned char c);
unsigned char x16_str_upperchar (unsigned char c);
unsigned char x16_str_lowerchar_iso (unsigned char c);
unsigned char x16_str_upperchar_iso (unsigned char c);

/* Fold a whole string in place. Returns its length. */
unsigned char x16_str_lower (char *s);
unsigned char x16_str_upper (char *s);
unsigned char x16_str_lower_iso (char *s);
unsigned char x16_str_upper_iso (char *s);

/* Case-insensitive compare: both sides folded, then -1/0/1 like
** x16_str_compare.
*/
signed char x16_str_compare_nocase (const char *s1, const char *s2);
signed char x16_str_compare_nocase_iso (const char *s1, const char *s2);

/* ---------------------------------------------------------------------
 * Classification -- each answers 0 or 1
 * ------------------------------------------------------------------ */

/* The same in both encodings: '0'-'9' (48-57); hex digits 0-9 plus
** 65-70 and 97-102; the byte range 97-122; space, TAB, CR, LF,
** shift-CR (141) and shift-space (160).
*/
unsigned char x16_str_isdigit (unsigned char c);
unsigned char x16_str_isxdigit (unsigned char c);
unsigned char x16_str_islower (unsigned char c);
unsigned char x16_str_isspace (unsigned char c);

/* Encoding-specific: PETSCII upper case lives at 97-122 and 193-218,
** ISO upper case at 65-90. A letter is lower or upper; printable is
** 32-127/160-255 in PETSCII, 32-126/160-255 in ISO.
*/
unsigned char x16_str_isupper (unsigned char c);
unsigned char x16_str_isupper_iso (unsigned char c);
unsigned char x16_str_isletter (unsigned char c);
unsigned char x16_str_isletter_iso (unsigned char c);
unsigned char x16_str_isprint (unsigned char c);
unsigned char x16_str_isprint_iso (unsigned char c);

/* ---------------------------------------------------------------------
 * Searching
 * ------------------------------------------------------------------ */

/* First index of c scanning left to right (find) or right to left
** (rfind); 255 when it is not there.
*/
unsigned char x16_str_find (const char *s, unsigned char c);
unsigned char x16_str_rfind (const char *s, unsigned char c);

/* 1 if c occurs in s. */
unsigned char x16_str_contains (const char *s, unsigned char c);

/* First index of a CR (13) or LF (10); 255 when the string has neither. */
unsigned char x16_str_find_eol (const char *s);

/* Wildcard match: '?' matches any single character, '*' any run
** including none; everything else matches itself, case-sensitively.
** Answers 1 on a match.
*/
unsigned char x16_str_pattern_match (const char *s, const char *pattern);

/* ---------------------------------------------------------------------
 * Slicing and trimming
 * ------------------------------------------------------------------ */

/* Copy the first / last `length` characters of source into target,
** NUL-terminated. `length` must not exceed the source length.
*/
void x16_str_left (char *target, const char *source, unsigned char length);
void x16_str_right (char *target, const char *source, unsigned char length);

/* Copy `length` characters starting at index `start`. The run must lie
** within the source.
*/
void x16_str_slice (char *target, const char *source,
                    unsigned char start, unsigned char length);

/* Drop whitespace -- the x16_str_isspace set -- from the left end, the
** right end, or both, in place. Return the new length.
*/
unsigned char x16_str_ltrim (char *s);
unsigned char x16_str_rtrim (char *s);
unsigned char x16_str_trim (char *s);

/* ---------------------------------------------------------------------
 * Sorting
 * ------------------------------------------------------------------ */

/* Sort an array of string pointers ascending by content, with
** x16_str_compare's ordering. The strings never move; only the pointer
** array is permuted. Insertion sort: fine for menu-sized arrays.
*/
void x16_str_sort (const char **array, unsigned int count);

/* pulls the implementation in with this header */
#pragma compile("string.c")

#endif
