/* =====================================================================
 * x16clib :: test_oscar64/runner15.c -- 64-bit software floating point
 * =====================================================================
 * Standalone suite:
 *
 *      .\build_oscar64.ps1 -Test -Source test_oscar64\runner15.c
 *
 * The checks that matter first, ported from the double half of
 * test_ca65/runner10.c. This tree reaches the arithmetic through a
 * dispatch prologue -- one entry point, a selector byte, an RTS-trick
 * vector -- because a label inside a global __asm block is invisible to
 * other asm blocks. That the module COMPILES says nothing about whether
 * the dispatch lands on the right routine with the arguments intact,
 * which is exactly what these check.
 *
 * The bit patterns are the module's own contract: a binary64 1.0 is
 * $3FF0000000000000, so little-endian its last two bytes are $F0 $3F.
 * ===================================================================== */

#include "testlib.h"
#include <x16/double.h>

static x16_double da, db;

/* 1 if d is {0,0,0,0,0,0,b6,b7} -- every pinned pattern looks so. */
static unsigned char d_bytes(const x16_double d, unsigned char b6,
                             unsigned char b7)
{
    unsigned char i;

    for (i = 0; i < 6; ++i) {
        if (d[i] != 0) {
            return 0;
        }
    }
    if (d[6] == b6 && d[7] == b7) {
        return 1;
    }
    return 0;
}

int main(void)
{
    t_init();

    /* from_s16 -> store: the dispatch carried A/X and the store carried
    ** A/Y. 1.0 is $3FF0000000000000. */
    x16_d_from_s16(1);
    x16_d_store(da);
    t_check(d_bytes(da, 0xF0, 0x3F), "D_BITS_ONE");

    x16_d_from_s16(-2);
    x16_d_store(da);
    t_check(d_bytes(da, 0x00, 0xC0), "D_BITS_NEG2");

    /* Round trip through the integer conversions. */
    x16_d_from_s16(12345);
    t_check(x16_d_to_s32() == 12345L, "D_S16_ROUNDTRIP");

    x16_d_from_s32(-1000000L);
    t_check(x16_d_to_s32() == -1000000L, "D_S32_ROUNDTRIP");

    /* Arithmetic against a memory operand: 1 + 2 = 3. */
    x16_d_from_s16(2);
    x16_d_store(db);
    x16_d_from_s16(1);
    x16_d_add(db);
    t_check(x16_d_to_s32() == 3L, "D_ADD_1_2");

    x16_d_from_s16(20);
    x16_d_store(db);
    x16_d_from_s16(7);
    x16_d_sub(db);
    t_check(x16_d_to_s32() == -13L, "D_SUB");

    x16_d_from_s16(6);
    x16_d_store(db);
    x16_d_from_s16(7);
    x16_d_mul(db);
    t_check(x16_d_to_s32() == 42L, "D_MUL");

    x16_d_from_s16(5);
    x16_d_store(db);
    x16_d_from_s16(100);
    x16_d_div(db);
    t_check(x16_d_to_s32() == 20L, "D_DIV");

    /* Compare: -1, 0, 1. */
    x16_d_from_s16(5);
    x16_d_store(db);
    x16_d_from_s16(3);
    t_check(x16_d_cmp(db) == -1, "D_CMP_LT");
    x16_d_from_s16(9);
    t_check(x16_d_cmp(db) == 1, "D_CMP_GT");
    x16_d_from_s16(5);
    t_check(x16_d_cmp(db) == 0, "D_CMP_EQ");

    /* neg / abs, which take no arguments at all. */
    x16_d_from_s16(7);
    x16_d_neg();
    t_check(x16_d_to_s32() == -7L, "D_NEG");
    x16_d_abs();
    t_check(x16_d_to_s32() == 7L, "D_ABS");

    /* A transcendental: sqrt(144) = 12. */
    x16_d_from_s16(144);
    x16_d_sqrt();
    t_check(x16_d_to_s32() == 12L, "D_SQRT");

    t_done();
    return 0;
}
