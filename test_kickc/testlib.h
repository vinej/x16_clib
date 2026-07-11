/* =====================================================================
 * x16clib :: test_kickc/testlib.h -- tiny on-target assertion harness
 * =====================================================================
 * Header-only; include it from exactly one translation unit.
 *
 * Reproduces the byte-for-byte output contract of the other suites'
 * testlibs, so build_kickc.ps1's stdout watcher works unchanged:
 *
 *   "\rPASS <NAME>"   "\rFAIL <NAME>"   "\rSKIP <NAME>"
 *   "\rDONE <passes>/<total>[ SKIP <skips>]\r"      (counts in hex)
 *
 * Every result line begins with a CR so the PASS/FAIL token always
 * starts a line, whatever the test under check left on the current one.
 * build_kickc.ps1 anchors its regex to the line start, and that anchor
 * is what catches a test whose result never got reported.
 *
 * Names must be [A-Z0-9_] -- the watcher's regex depends on it.
 *
 * Two deliberate choices:
 *
 *   A bare CHROUT ($FFD2) wrapper rather than KickC's conio, so the
 *   bytes reaching the emulator are exactly the ones written here --
 *   no newline rewriting, and no conio dragged into the PRG.
 *
 *   #pragma encoding(ascii) rather than KickC's petscii_mixed default.
 *   Without it "PASS" would leave as $D0 $C1 $D3 $D3 instead of the
 *   $50 $41 $53 $53 the watcher greps for. The pragma is global, so it
 *   also keeps the test data's char literals at their ASCII values.
 * =====================================================================
 */

#ifndef X16_TESTLIB_H
#define X16_TESTLIB_H

#pragma encoding(ascii)

char t_passes;
char t_total;
char t_skips;

void t_chrout(char c) {
    asm { lda c jsr $ffd2 }
}

void t_puts(const char *s) {
    while (*s) {
        t_chrout(*s);
        s++;
    }
}

void t_puthex(char v) {
    char n = v >> 4;
    t_chrout(n < 10 ? n + '0' : n + 0x37);
    n = v & 0x0f;
    t_chrout(n < 10 ? n + '0' : n + 0x37);
}

void t_init(void) {
    t_passes = 0;
    t_total = 0;
    t_skips = 0;
}

/* ok != 0 -> PASS. */
void t_check(char ok, const char *name) {
    t_total++;
    t_chrout(13);
    if (ok) {
        t_passes++;
        t_puts("PASS ");
    } else {
        t_puts("FAIL ");
    }
    t_puts(name);
}

/* A skip is neither a pass nor part of the total: it cannot be mistaken
** for a pass, and the DONE tally still has to balance.
*/
void t_skip(const char *name) {
    t_skips++;
    t_chrout(13);
    t_puts("SKIP ");
    t_puts(name);
}

void t_done(void) {
    t_chrout(13);
    t_puts("DONE ");
    t_puthex(t_passes);
    t_chrout('/');
    t_puthex(t_total);
    if (t_skips) {
        t_puts(" SKIP ");
        t_puthex(t_skips);
    }
    t_chrout(13);
}

#endif /* X16_TESTLIB_H */
