/* =====================================================================
 * x16clib :: test_vbcc/testlib.h -- the on-machine test harness
 * =====================================================================
 * Same output contract as the cc65 and llvm-mos suites, so build_vbcc.ps1
 * shares their watcher:
 *
 *      CR "PASS NAME"      CR "FAIL NAME"      CR "SKIP NAME"
 *      CR "DONE hh/hh" CR          -- hex, passes/total, skips excluded
 *
 * Output goes through the KERNAL's CHROUT ($FFD2), not vbcc's printf.
 * printf drags in a large slice of libvc and, more to the point, CHROUT
 * is the same independent path the other suites use, so the harness looks
 * identical across toolchains.
 *
 * THE PETSCII CHARMAP. vc's +x16 config compiles with -cbmascii, so the
 * compiler stores C string and char literals in PETSCII: "PASS" is laid
 * down as $D0$C1$D3$D3, and '_' as $A4 (digits, space, '/' and CR are the
 * same byte in both encodings). The emulator's -echo reports whatever
 * byte CHROUT was given, so those PETSCII codes would reach the ASCII
 * watcher in build_vbcc.ps1 unchanged and never match "PASS"/"DONE".
 *
 * t_put() therefore maps the alphabet back to ASCII on its way out --
 * 'A'..'Z' ($C1..$DA) minus $80, and '_' ($A4) to $5F. That is the whole
 * fix, and it lives at the one point every protocol byte and every test
 * name passes through, so the on-stdout stream is plain ASCII exactly as
 * in the cc65 and llvm-mos suites.
 *
 * The VERA read-back path (t_vpoke / t_vpeek) is a hand-written pair that
 * talks to the VERA data ports directly. It is deliberately independent
 * of any library routine: a write done through a library call is read
 * back here, so a bug in the address plumbing cannot hide behind itself.
 * ===================================================================== */

#ifndef X16_TESTLIB_H
#define X16_TESTLIB_H

/* CHROUT: character out through the KERNAL, A = byte. Declared as an
** inline-asm function so no C library is pulled in for it. */
void t_put_raw(__reg("a") unsigned char c) = "\tjsr\t$ffd2";


#define VERA_ADDR_L   (*(volatile unsigned char *)0x9F20)
#define VERA_ADDR_M   (*(volatile unsigned char *)0x9F21)
#define VERA_ADDR_H   (*(volatile unsigned char *)0x9F22)
#define VERA_DATA0    (*(volatile unsigned char *)0x9F23)
#define VERA_CTRL     (*(volatile unsigned char *)0x9F25)

/* Poison / read a VERA byte on data port 0, increment 0, bank per bit 16.
** Independent of the library's own VERA plumbing. */
static void t_vpoke(unsigned char value, unsigned long addr)
{
    VERA_CTRL   = 0;                                    /* ADDRSEL 0 */
    VERA_ADDR_L = (unsigned char)(addr & 0xFF);
    VERA_ADDR_M = (unsigned char)((addr >> 8) & 0xFF);
    VERA_ADDR_H = (unsigned char)((addr >> 16) & 1);    /* bank, inc 0 */
    VERA_DATA0  = value;
}

static unsigned char t_vpeek(unsigned long addr)
{
    VERA_CTRL   = 0;
    VERA_ADDR_L = (unsigned char)(addr & 0xFF);
    VERA_ADDR_M = (unsigned char)((addr >> 8) & 0xFF);
    VERA_ADDR_H = (unsigned char)((addr >> 16) & 1);
    return VERA_DATA0;
}

static unsigned char t_passes;
static unsigned char t_total;

/* Emit one byte of the protocol, undoing the compiler's PETSCII charmap.
**
** vc's +x16 config compiles with -cbmascii, so a C string literal is
** stored in PETSCII, not ASCII: 'A'..'Z' land at $C1..$DA and '_' at $A4
** (digits, space, '/', CR are the same in both). The emulator's -echo
** reports the raw byte, so those PETSCII codes would reach the ASCII
** watcher in build_vbcc.ps1 verbatim and never match. Map the alphabet
** back to ASCII here, at the single point every protocol byte passes
** through, so the on-stdout bytes are the plain "PASS"/"DONE" the watcher
** expects -- and the harness reads identically to the cc65/llvm suites.
*/
static void t_put(char c)
{
    unsigned char b = (unsigned char)c;
    if (b >= 0xC1 && b <= 0xDA) b -= 0x80;      /* PETSCII A-Z -> ASCII */
    else if (b == 0xA4)         b  = 0x5F;      /* PETSCII '_'  -> ASCII */
    t_put_raw(b);
}

static void t_puts(const char *s) { while (*s) t_put(*s++); }

/* End the current record. The wire byte must be CR ($0D): the KERNAL
** treats CR as carriage-return-plus-newline, and the watcher splits
** records on it. We send $0D through t_put_raw directly rather than
** t_put('\r'), because -cbmascii SWAPS CR and LF in char literals --
** '\r' compiles to byte 10 ($0A) and '\n' to byte 13 ($0D) -- so a
** literal '\r' would put $0A on the wire and never break the line. */
static void t_cr(void) { t_put_raw(0x0D); }

static void t_hex(unsigned char b)
{
    static const char digits[] = "0123456789ABCDEF";
    t_put(digits[b >> 4]);
    t_put(digits[b & 0x0F]);
}

static void t_init(void)
{
    t_passes = 0;
    t_total  = 0;
}

/* A result line begins with CR rather than ending with one, so a test
** that prints diagnostics of its own cannot strand them on the verdict
** line. */
static void t_check(unsigned char ok, const char *name)
{
    ++t_total;
    t_cr();
    if (ok) {
        ++t_passes;
        t_puts("PASS ");
    } else {
        t_puts("FAIL ");
    }
    t_puts(name);
}

/* Skips are excluded from the total, so they can never be mistaken for
** passes when the watcher compares the PASS count against DONE. */
static void t_skip(const char *name)
{
    t_cr();
    t_puts("SKIP ");
    t_puts(name);
}

static void t_done(void)
{
    t_cr();
    t_puts("DONE ");
    t_hex(t_passes);
    t_put('/');
    t_hex(t_total);
    t_cr();
}

#endif /* X16_TESTLIB_H */
