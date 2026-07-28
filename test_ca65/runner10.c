/* =====================================================================
 * x16clib :: test/runner10.c -- util/int16, util/int32, util/double
 * =====================================================================
 * Standalone suite (its own main; run via
 *     .\build_ca65.ps1 -Test -Source test_ca65\runner10.c).
 *
 * int16/int32: every public routine against C-computed expected values
 * -- the compiler's own 16/32-bit arithmetic is the oracle, which is
 * exactly the point of the parity layer. Division checks all four sign
 * combinations and the divide-by-zero contract (quotient = dividend,
 * *rem untouched).
 *
 * double: identities over exactly-representable values only (halves,
 * quarters, small integers -- 0.1 is NOT exact and never asserted
 * exact), IEEE-754 bit patterns pinned for a few knowns (the header
 * documents the format exactly), transcendentals at their exact points
 * plus tolerance checks elsewhere, and string I/O compared bytewise --
 * testlib.h pulls in <ascii_charmap.h>, so every literal in this file
 * is ASCII, matching the module's documented output bytes.
 * =====================================================================
 */

#include <string.h>

#include "testlib.h"

#include <x16/int16.h>
#include <x16/int32.h>
#include <x16/double.h>

/* --- double helpers ------------------------------------------------- */

static x16_double da, db, dc, dexp, deps;

/* DAC = parse(s); also leaves the value in d. */
static void dset (x16_double d, const char *s)
{
    x16_d_from_str(s, strlen(s));
    x16_d_store(d);
}

/* 1 if DAC == parse(want) exactly (by compare, so -0 == +0). */
static unsigned char d_is (const char *want)
{
    x16_d_store(dc);
    dset(dexp, want);
    x16_d_load(dc);
    return x16_d_cmp(dexp) == 0;
}

/* 1 if |DAC - parse(want)| < parse(eps). */
static unsigned char d_near (const char *want, const char *eps)
{
    x16_d_store(dc);
    dset(dexp, want);
    dset(deps, eps);
    x16_d_load(dc);
    x16_d_sub(dexp);
    x16_d_abs();
    return x16_d_cmp(deps) == -1;
}

/* 1 if d is {0,0,0,0,0,0,b6,b7} -- all the pinned patterns look so. */
static unsigned char d_bytes (const x16_double d,
                              unsigned char b6, unsigned char b7)
{
    unsigned char i;
    for (i = 0; i < 6; ++i) {
        if (d[i] != 0) {
            return 0;
        }
    }
    return d[6] == b6 && d[7] == b7;
}

/* --- int16 ----------------------------------------------------------- */

static void t_int16 (void)
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
    ** as a cast hex, because cc65 reads the literal -32768 as a negated
    ** 32768 -- which does not fit an int, so it promotes to long.
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

    t_check(strcmp(x16_i16_to_dec(65535u), "65535") == 0 &&
            strcmp(x16_i16_to_dec(0u), "0") == 0 &&
            strcmp(x16_i16_to_dec(10010u), "10010") == 0, "I16_TO_DEC");
    t_check(strcmp(x16_i16_to_dec_s((int)0x8000u), "-32768") == 0 &&
            strcmp(x16_i16_to_dec_s(32767), "32767") == 0 &&
            strcmp(x16_i16_to_dec_s(0), "0") == 0, "I16_TO_DEC_S");
}

/* --- int32 ----------------------------------------------------------- */

static void t_int32 (void)
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

    t_check(strcmp(x16_i32_to_dec(4294967295UL), "4294967295") == 0 &&
            strcmp(x16_i32_to_dec(0UL), "0") == 0 &&
            strcmp(x16_i32_to_dec(1000000UL), "1000000") == 0,
            "I32_TO_DEC");
}

/* --- double ---------------------------------------------------------- */

static void t_double (void)
{
    /* The header pins the format as little-endian IEEE-754 binary64:
    ** check known bit patterns from the module's own converters.
    */
    x16_d_from_s16(1);
    x16_d_store(da);
    t_check(d_bytes(da, 0xF0, 0x3F), "D_BITS_ONE");
    x16_d_from_s16(-2);
    x16_d_store(da);
    t_check(d_bytes(da, 0x00, 0xC0), "D_BITS_NEG2");
    dset(da, "0.5");
    t_check(d_bytes(da, 0xE0, 0x3F), "D_BITS_HALF");

    /* int <-> double roundtrips at the extremes (exact: 32 bits fit
    ** the 53-bit mantissa).
    */
    x16_d_from_s16((int)0x8000u);
    t_check(x16_d_to_s32() == -32768L, "D_S16_MIN");
    x16_d_from_s16(32767);
    t_check(x16_d_to_s32() == 32767L, "D_S16_MAX");
    x16_d_from_s32(2147483647L);
    t_check(x16_d_to_s32() == 2147483647L, "D_S32_MAX");
    x16_d_from_s32((long)0x80000000UL);
    t_check(x16_d_to_s32() == (long)0x80000000UL, "D_S32_MIN");

    /* from_s16 and from_str agree. */
    x16_d_from_s16((int)0x8000u);
    t_check(d_is("-32768"), "D_STR_S16_AGREE");

    /* to_s32 truncates toward zero; out of range clamps; NaN answers 0. */
    dset(da, "2.75");
    x16_d_load(da);
    t_check(x16_d_to_s32() == 2L, "D_TRUNC_POS");
    dset(da, "-2.75");
    x16_d_load(da);
    t_check(x16_d_to_s32() == -2L, "D_TRUNC_NEG");
    dset(da, "3000000000");
    x16_d_load(da);
    t_check(x16_d_to_s32() == 2147483647L, "D_CLAMP_POS");
    dset(da, "-3000000000");
    x16_d_load(da);
    t_check(x16_d_to_s32() == (long)0x80000000UL, "D_CLAMP_NEG");

    /* Arithmetic identities over exactly-representable values. */
    x16_d_from_s16(1);
    x16_d_store(da);
    x16_d_from_s16(2);
    x16_d_store(db);
    x16_d_load(da);
    x16_d_add(db);
    t_check(d_is("3"), "D_ADD_1_2");

    dset(da, "0.25");
    dset(db, "0.25");
    x16_d_load(da);
    x16_d_add(db);
    t_check(d_is("0.5"), "D_ADD_QUARTERS");

    x16_d_from_s16(3);
    x16_d_store(da);
    x16_d_from_s16(5);
    x16_d_store(db);
    x16_d_load(da);
    x16_d_sub(db);
    t_check(d_is("-2"), "D_SUB_3_5");

    dset(da, "1.5");
    x16_d_load(da);
    x16_d_mul(da);
    t_check(d_is("2.25"), "D_MUL_HALVES");

    x16_d_from_s16(7);
    x16_d_store(da);
    x16_d_from_s16(2);
    x16_d_store(db);
    x16_d_load(da);
    x16_d_div(db);
    x16_d_store(dc);
    t_check(d_is("3.5") &&
            (x16_d_load(dc), strcmp(x16_d_to_str(), "3.5") == 0),
            "D_DIV_7_2");

    /* Compare orderings, negatives and both zeroes included. */
    dset(da, "-1");
    dset(db, "1");
    x16_d_load(da);
    t_check(x16_d_cmp(db) == -1, "D_CMP_LT");
    x16_d_load(db);
    t_check(x16_d_cmp(da) == 1, "D_CMP_GT");
    dset(da, "-3");
    dset(db, "-2");
    x16_d_load(da);
    t_check(x16_d_cmp(db) == -1, "D_CMP_NEG");
    dset(da, "2.5");
    dset(db, "2.25");
    x16_d_load(da);
    t_check(x16_d_cmp(db) == 1, "D_CMP_FRAC");
    x16_d_from_s16(0);
    x16_d_store(da);
    dset(db, "-0");
    x16_d_load(da);
    t_check(x16_d_cmp(db) == 0, "D_CMP_ZEROSIGN");

    /* Negate and abs are sign-bit operations. */
    dset(da, "1.5");
    x16_d_load(da);
    x16_d_neg();
    x16_d_store(db);
    t_check(strcmp((x16_d_load(db), x16_d_to_str()), "-1.5") == 0,
            "D_NEG");
    x16_d_load(db);
    x16_d_abs();
    t_check(d_is("1.5"), "D_ABS");

    /* sqrt: exact at perfect squares; NaN below zero. */
    x16_d_from_s16(4);
    x16_d_sqrt();
    t_check(d_is("2"), "D_SQRT_4");
    dset(da, "2.25");
    x16_d_load(da);
    x16_d_sqrt();
    t_check(d_is("1.5"), "D_SQRT_225");
    x16_d_from_s16(-1);
    x16_d_sqrt();
    t_check(strcmp(x16_d_to_str(), "NAN") == 0, "D_SQRT_NEG");

    /* exp/ln/pow: exact points, then tolerance. */
    x16_d_from_s16(0);
    x16_d_exp();
    t_check(d_is("1"), "D_EXP_0");
    x16_d_from_s16(1);
    x16_d_ln();
    t_check(d_is("0"), "D_LN_1");
    x16_d_from_s16(1);
    x16_d_exp();
    t_check(d_near("2.718281828459045", "0.000000001"), "D_EXP_1");
    x16_d_from_s16(0);
    x16_d_store(db);
    x16_d_from_s16(5);
    x16_d_pow(db);
    t_check(d_is("1"), "D_POW_Y0");
    x16_d_from_s16(10);
    x16_d_store(db);
    x16_d_from_s16(2);
    x16_d_pow(db);
    t_check(d_near("1024", "0.000001"), "D_POW_2_10");

    /* Trig at the exact points, then tolerance. */
    x16_d_from_s16(0);
    x16_d_sin();
    t_check(d_is("0"), "D_SIN_0");
    x16_d_from_s16(0);
    x16_d_cos();
    t_check(d_is("1"), "D_COS_0");
    x16_d_from_s16(0);
    x16_d_tan();
    t_check(d_is("0"), "D_TAN_0");
    x16_d_from_s16(0);
    x16_d_atan();
    t_check(d_is("0"), "D_ATAN_0");
    dset(da, "1.5707963267948966");
    x16_d_load(da);
    x16_d_sin();
    t_check(d_near("1", "0.000000001"), "D_SIN_PIHALF");
    x16_d_from_s16(1);
    x16_d_atan();
    t_check(d_near("0.7853981633974483", "0.000000001"), "D_ATAN_1");

    /* Hyperbolics: exact at zero, tanh saturates. */
    x16_d_from_s16(0);
    x16_d_sinh();
    t_check(d_is("0"), "D_SINH_0");
    x16_d_from_s16(0);
    x16_d_cosh();
    t_check(d_is("1"), "D_COSH_0");
    x16_d_from_s16(0);
    x16_d_tanh();
    t_check(d_is("0"), "D_TANH_0");
    x16_d_from_s16(25);
    x16_d_tanh();
    t_check(d_is("1"), "D_TANH_SAT");
    x16_d_from_s16(-25);
    x16_d_tanh();
    t_check(d_is("-1"), "D_TANH_NSAT");

    /* Infinities and NaN, bytewise-ASCII strings. */
    x16_d_from_s16(0);
    x16_d_store(db);
    x16_d_from_s16(1);
    x16_d_div(db);
    x16_d_store(dc);
    t_check(strcmp(x16_d_to_str(), "INF") == 0, "D_INF_STR");
    x16_d_from_s16(1000);
    x16_d_store(da);
    x16_d_load(dc);
    t_check(x16_d_cmp(da) == 1, "D_INF_CMP");
    x16_d_from_s16(-1);
    x16_d_div(db);
    t_check(strcmp(x16_d_to_str(), "-INF") == 0, "D_NINF_STR");
    x16_d_from_s16(0);
    x16_d_div(db);
    x16_d_store(dc);
    t_check(strcmp(x16_d_to_str(), "NAN") == 0, "D_NAN_STR");
    x16_d_load(dc);
    t_check(x16_d_cmp(da) == 1 &&
            (x16_d_load(da), x16_d_cmp(dc)) == 1, "D_NAN_CMP");
    x16_d_load(dc);
    t_check(x16_d_to_s32() == 0L, "D_NAN_S32");

    /* String I/O roundtrips over exact values, compared bytewise. */
    dset(da, "1.5");
    x16_d_load(da);
    t_check(strcmp(x16_d_to_str(), "1.5") == 0, "D_STR_1P5");
    dset(da, "-0.03125");
    x16_d_load(da);
    t_check(strcmp(x16_d_to_str(), "-0.03125") == 0, "D_STR_NEG5TH");
    dset(da, "123456789");
    x16_d_load(da);
    t_check(strcmp(x16_d_to_str(), "123456789") == 0, "D_STR_INT");
    x16_d_from_s32(123456789L);
    t_check(strcmp(x16_d_to_str(), "123456789") == 0, "D_STR_S32");
    dset(da, "1E3");
    x16_d_load(da);
    t_check(strcmp(x16_d_to_str(), "1000") == 0, "D_STR_E3");
    dset(da, "2.5e-2");
    x16_d_load(da);
    t_check(strcmp(x16_d_to_str(), "0.025") == 0, "D_STR_SMALL_E");
    dset(da, "1E21");
    x16_d_load(da);
    t_check(strcmp(x16_d_to_str(), "1E+21") == 0, "D_STR_SCI");
}

void main (void)
{
    t_init();
    t_int16();
    t_int32();
    t_double();
    t_done();
}
