/* =====================================================================
 * x16clib :: test_oscar64/runner12.c -- int16 and int32 arithmetic
 * =====================================================================
 * Standalone suite:
 *
 *      .\build_oscar64.ps1 -Test -Source test_oscar64\runner12.c
 *
 * Ported from the int16/int32 halves of test_ca65/runner10.c, check for
 * check. (Its double half arrives with that module.)
 *
 * Pure computation, so all of it runs headless. The values are chosen to
 * pin the edges the documentation promises rather than the easy middle:
 * wrapping addition, |-32768| staying put, arithmetic vs logical shift on
 * a negative, all four sign combinations of truncating division, a zero
 * divisor leaving *rem untouched, and sqrt at both ends of its range.
 *
 * String comparison is a local four-liner rather than <string.h>: the
 * only thing needed is equality, and the digits under test are ASCII
 * bytes the module wrote.
 * ===================================================================== */

#include "testlib.h"
#include <x16/int16.h>
#include <x16/int32.h>

/* 1 if the NUL-terminated strings match. */
static unsigned char t_streq(const char *a, const char *b)
{
    while (*a != 0 && *a == *b) {
        ++a;
        ++b;
    }
    if (*a == *b) {
        return 1;
    }
    return 0;
}

/* --- int16 ----------------------------------------------------------- */

static void t_int16(void)
{
    unsigned int uq, ur;
    int q, r;

    t_check(x16_i16_from_u8(200) == 200 &&
            x16_i16_from_u8(0) == 0, "I16_FROM_U8");
    t_check(x16_i16_from_s8(-56) == -56 &&
            x16_i16_from_s8(127) == 127, "I16_FROM_S8");

    t_check(x16_i16_add(12345, -345) == 12000, "I16_ADD");
    t_check(x16_i16_add(30000, 30000) == (int)(30000u + 30000u),
            "I16_ADD_WRAP");
    t_check(x16_i16_sub(-12345, 100) == -12445, "I16_SUB");

    t_check(x16_i16_neg(-32767) == 32767 &&
            x16_i16_neg(0) == 0, "I16_NEG");
    t_check(x16_i16_abs(-12345) == 12345 &&
            x16_i16_abs(32767) == 32767, "I16_ABS");
    /* -32768 has no positive counterpart: |x| wraps to itself. Spelled
    ** as a cast hex so the literal cannot promote to long.
    */
    t_check(x16_i16_abs((int)0x8000u) == (int)0x8000u, "I16_ABS_MIN");

    t_check(x16_i16_shl(0x4001) == (int)0x8002u, "I16_SHL");
    t_check(x16_i16_shr(0x8002u) == 0x4001u, "I16_SHR");
    t_check(x16_i16_asr((int)0x8002u) == (int)0xC001u &&
            x16_i16_asr(4096) == 2048, "I16_ASR");

    t_check(x16_i16_cmpu(1u, 2u) == -1 &&
            x16_i16_cmpu(2u, 2u) == 0 &&
            x16_i16_cmpu(0xFFFFu, 1u) == 1, "I16_CMPU");
    t_check(x16_i16_cmps(-1, 1) == -1 &&
            x16_i16_cmps(1, -1) == 1 &&
            x16_i16_cmps(-5, -5) == 0 &&
            x16_i16_cmps((int)0x8000u, 32767) == -1, "I16_CMPS");

    t_check(x16_i16_mul(1234, 567) == (int)(1234u * 567u), "I16_MUL");
    t_check(x16_i16_mul(-5, 7) == -35, "I16_MUL_NEG");

    ur = 0xBEEF;
    uq = x16_i16_divmod(50000u, 7u, &ur);
    t_check(uq == 50000u / 7u && ur == 50000u % 7u, "I16_DIVMOD");

    /* All four sign combinations: quotient truncates toward zero, the
    ** remainder follows the dividend -- C's own / and % semantics.
    */
    q = x16_i16_divmod_s(7, 2, &r);
    t_check(q == 3 && r == 1, "I16_DIVS_PP");
    q = x16_i16_divmod_s(-7, 2, &r);
    t_check(q == -3 && r == -1, "I16_DIVS_NP");
    q = x16_i16_divmod_s(7, -2, &r);
    t_check(q == -3 && r == 1, "I16_DIVS_PN");
    q = x16_i16_divmod_s(-7, -2, &r);
    t_check(q == 3 && r == -1, "I16_DIVS_NN");

    /* b == 0 changes nothing: a comes back, *rem stays untouched. */
    ur = 0xBEEF;
    uq = x16_i16_divmod(1234u, 0u, &ur);
    t_check(uq == 1234u && ur == 0xBEEF, "I16_DIV_ZERO");
    r = -77;
    q = x16_i16_divmod_s(-4321, 0, &r);
    t_check(q == -4321 && r == -77, "I16_DIVS_ZERO");

    t_check(x16_i16_sqrt(0u) == 0 &&
            x16_i16_sqrt(1u) == 1 &&
            x16_i16_sqrt(3u) == 1 &&
            x16_i16_sqrt(4u) == 2, "I16_SQRT_LO");
    t_check(x16_i16_sqrt(10000u) == 100 &&
            x16_i16_sqrt(65024u) == 254 &&
            x16_i16_sqrt(65025u) == 255 &&
            x16_i16_sqrt(65535u) == 255, "I16_SQRT_HI");

    t_check(t_streq(x16_i16_to_dec(65535u), "65535") &&
            t_streq(x16_i16_to_dec(0u), "0") &&
            t_streq(x16_i16_to_dec(10010u), "10010"), "I16_TO_DEC");
    t_check(t_streq(x16_i16_to_dec_s((int)0x8000u), "-32768") &&
            t_streq(x16_i16_to_dec_s(32767), "32767") &&
            t_streq(x16_i16_to_dec_s(0), "0"), "I16_TO_DEC_S");
}

/* --- int32 ----------------------------------------------------------- */

static void t_int32(void)
{
    unsigned long uq, ur;

    t_check(x16_i32_from_u16(0xFFFFu) == 65535L &&
            x16_i32_from_u16(0u) == 0L, "I32_FROM_U16");
    t_check(x16_i32_from_s16(-2) == -2L &&
            x16_i32_from_s16(32767) == 32767L, "I32_FROM_S16");
    t_check(x16_i32_to_s16(-70000L) == (int)(unsigned int)(-70000L & 0xFFFFL) &&
            x16_i32_to_s16(123456L) == (int)(unsigned int)(123456L & 0xFFFFL),
            "I32_TO_S16");

    t_check(x16_i32_add(123456789L, 987654321L) == 1111111110L, "I32_ADD");
    t_check(x16_i32_sub(100000L, 100001L) == -1L, "I32_SUB");

    t_check(x16_i32_neg(2147483647L) == -2147483647L &&
            x16_i32_neg(0L) == 0L, "I32_NEG");
    t_check(x16_i32_abs(-1000000L) == 1000000L &&
            x16_i32_abs((long)0x80000000UL) == (long)0x80000000UL,
            "I32_ABS");

    t_check(x16_i32_shl(0x40000001L) == (long)0x80000002UL, "I32_SHL");
    t_check(x16_i32_shr(0x80000002UL) == 0x40000001UL, "I32_SHR");
    t_check(x16_i32_asr((long)0x80000002UL) == (long)0xC0000001UL &&
            x16_i32_asr(0x10000000L) == 0x08000000L, "I32_ASR");

    t_check(x16_i32_cmpu(0x80000000UL, 1UL) == 1 &&
            x16_i32_cmpu(5UL, 5UL) == 0 &&
            x16_i32_cmpu(1UL, 0xFFFFFFFFUL) == -1, "I32_CMPU");
    t_check(x16_i32_cmps((long)0x80000000UL, 1L) == -1 &&
            x16_i32_cmps(1L, -1L) == 1 &&
            x16_i32_cmps(-9L, -9L) == 0, "I32_CMPS");

    t_check(x16_i32_mul(123456L, 789012L) ==
            (long)(123456UL * 789012UL), "I32_MUL");
    t_check(x16_i32_mul(-5000L, 7000L) == -35000000L, "I32_MUL_NEG");

    ur = 0xDEADBEEFUL;
    uq = x16_i32_divmod(1000000000UL, 7UL, &ur);
    t_check(uq == 142857142UL && ur == 6UL, "I32_DIVMOD");

    ur = 0xDEADBEEFUL;
    uq = x16_i32_divmod(424242UL, 0UL, &ur);
    t_check(uq == 424242UL && ur == 0xDEADBEEFUL, "I32_DIV_ZERO");

    t_check(t_streq(x16_i32_to_dec(4294967295UL), "4294967295") &&
            t_streq(x16_i32_to_dec(0UL), "0") &&
            t_streq(x16_i32_to_dec(1000000UL), "1000000"),
            "I32_TO_DEC");
}

int main(void)
{
    t_init();

    t_int16();
    t_int32();

    t_done();
    return 0;
}
