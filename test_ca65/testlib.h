/* =====================================================================
 * x16clib :: test/testlib.h -- tiny on-target assertion harness
 * =====================================================================
 * Header-only; include it from exactly one translation unit.
 *
 * Reproduces the byte-for-byte output contract of the assembly library's
 * test/testlib.asm, so build.ps1's stdout watcher works unchanged:
 *
 *   "\rPASS <NAME>"   "\rFAIL <NAME>"   "\rSKIP <NAME>"
 *   "\rDONE <passes>/<total>[ SKIP <skips>]\r"      (counts in hex)
 *
 * Every result line begins with a CR so the PASS/FAIL token always
 * starts a line, whatever the test under check left on the current one
 * (a test may legitimately clear the screen or print into the tilemap).
 * build.ps1 anchors its regex to the line start, and that anchor is what
 * catches a test whose result never got reported.
 *
 * Names must be [A-Z0-9_] -- build.ps1's regex depends on it.
 *
 * Two deliberate choices:
 *
 *   cbm_k_bsout() rather than conio's cputc(). It is a bare CHROUT
 *   wrapper, so the bytes reaching the emulator are exactly the ones
 *   written here -- no newline rewriting, and no linking of conio.
 *
 *   <ascii_charmap.h> rather than the cx16 default. Without it cc65
 *   translates string literals to PETSCII, and "PASS" would leave as
 *   $D0 $C1 $D3 $D3 instead of the $50 $41 $53 $53 the watcher greps for.
 *   It must come before any literal in the translation unit.
 * =====================================================================
 */

#ifndef X16_TESTLIB_H
#define X16_TESTLIB_H

#include <ascii_charmap.h>
#include <cbm.h>

/* A phase that happens to skip nothing still defines t_skip. */
#pragma warn (unused-func, off)

static unsigned char t_passes;
static unsigned char t_total;
static unsigned char t_skips;

static void t_puts(const char *s)
{
    while (*s) {
        cbm_k_bsout(*s++);
    }
}

static void t_puthex(unsigned char v)
{
    static unsigned char n;

    n = v >> 4;
    cbm_k_bsout(n < 10 ? n + '0' : n + ('A' - 10));
    n = v & 0x0F;
    cbm_k_bsout(n < 10 ? n + '0' : n + ('A' - 10));
}

static void t_init(void)
{
    t_passes = t_total = t_skips = 0;
}

/* ok != 0 -> PASS. */
static void t_check(unsigned char ok, const char *name)
{
    ++t_total;
    cbm_k_bsout('\r');
    if (ok) {
        ++t_passes;
        t_puts("PASS ");
    } else {
        t_puts("FAIL ");
    }
    t_puts(name);
}

/* For a check the target machine genuinely cannot perform, as opposed to
** one that failed. Counted separately and excluded from the pass/total in
** DONE, so a skip can never be mistaken for a pass. Use it only where an
** independent oracle proved the capability is absent -- never to paper
** over a failure.
*/
static void t_skip(const char *name)
{
    ++t_skips;
    cbm_k_bsout('\r');
    t_puts("SKIP ");
    t_puts(name);
}

static void t_done(void)
{
    cbm_k_bsout('\r');
    t_puts("DONE ");
    t_puthex(t_passes);
    cbm_k_bsout('/');
    t_puthex(t_total);
    if (t_skips) {
        t_puts(" SKIP ");
        t_puthex(t_skips);
    }
    cbm_k_bsout('\r');
}

#endif /* X16_TESTLIB_H */
