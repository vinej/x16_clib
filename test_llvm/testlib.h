/* =====================================================================
 * x16clib :: test_llvm/testlib.h -- the on-machine test harness
 * =====================================================================
 * Same output contract as the cc65 suite's testlib.h, so build_llvm.ps1
 * and build_ca65.ps1 can share a watcher:
 *
 *      CR "PASS NAME"      CR "FAIL NAME"      CR "SKIP NAME"
 *      CR "DONE hh/hh" CR          -- hex, passes/total, skips excluded
 *
 * WHY NOT printf: llvm-mos's printf needs all ninety bytes of this
 * target's zero page, and the library's scratch block wants sixteen of
 * them. Linking both overflows the `zp` region and the link fails. So
 * everything here goes out through CHROUT, which needs none.
 *
 * Characters are ASCII. llvm-mos does no charmap translation, and the
 * emulator's -echo prints the raw byte, so 'A' arrives as 'A'.
 * ===================================================================== */

#ifndef X16_TESTLIB_H
#define X16_TESTLIB_H

#include <cbm.h>
#include <cx16.h>

/* ---------------------------------------------------------------------
** DO NOT USE THE SDK's vpoke(). It is broken in llvm-mos-sdk v23.0.1.
**
** vpoke(data, addr) reads the address out of __rc2/__rc3/__rc4, but the
** compiler passes the second argument starting in X -- so it takes
** addr's bits 8-15 as ADDR_L, bits 16-23 as ADDR_M, and garbage as
** ADDR_H. Every write lands at addr >> 8. Verified on the machine:
** vpoke(0xAB, 0x08000) stores at $00080, and vpoke(0xCD, 0x01234) at
** $00012. Compare cx16/lib/vpoke.s against vpeek.s, which correctly
** reads A, X, __rc2 -- that one is fine, and the tests rely on it as
** their independent read-back path.
**
** So the suite writes through t_vpoke() below and reads through the
** SDK's vpeek(). The two are still independent implementations, which is
** what the read-back principle actually requires.
** ------------------------------------------------------------------- */
static void t_vpoke (unsigned char value, unsigned long addr)
{
    VERA.control    = 0;                                /* ADDRSEL 0 */
    VERA.address    = (unsigned int)(addr & 0xFFFFUL);  /* ADDR_L, ADDR_M */
    VERA.address_hi = (unsigned char)((addr >> 16) & 1);/* bank, increment 0 */
    VERA.data0      = value;
}

static unsigned char t_passes;
static unsigned char t_total;

static void t_put (char c) { cbm_k_chrout((unsigned char)c); }

static void t_puts (const char *s) { while (*s) t_put(*s++); }

static void t_hex (unsigned char b)
{
    static const char digits[] = "0123456789ABCDEF";
    t_put(digits[b >> 4]);
    t_put(digits[b & 0x0F]);
}

static void t_init (void)
{
    t_passes = 0;
    t_total  = 0;
}

/* A result line begins with CR rather than ending with one, so that a
** test which prints diagnostics of its own cannot leave them stranded on
** the same line as the verdict.
*/
static void t_check (unsigned char ok, const char *name)
{
    ++t_total;
    t_put('\r');
    if (ok) {
        ++t_passes;
        t_puts("PASS ");
    } else {
        t_puts("FAIL ");
    }
    t_puts(name);
}

/* Skips are excluded from the total, so they can never be mistaken for
** passes when the watcher compares the PASS count against DONE.
*/
static void t_skip (const char *name)
{
    t_put('\r');
    t_puts("SKIP ");
    t_puts(name);
}

static void t_done (void)
{
    t_put('\r');
    t_puts("DONE ");
    t_hex(t_passes);
    t_put('/');
    t_hex(t_total);
    t_put('\r');
}

#endif /* X16_TESTLIB_H */
