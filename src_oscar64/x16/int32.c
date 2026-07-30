// =====================================================================
// x16clib :: x16/int32.c -- 32-bit integer arithmetic
// =====================================================================
// The same shape as int16.c one size up: a parity layer over C's long,
// plus the two composites that pay for themselves -- a divmod that
// divides once instead of twice, and decimal without printf.
//
// The arithmetic goes through UNSIGNED and casts back. Two's complement
// makes add/sub/neg/mul identical either way, and it keeps signed
// overflow -- undefined in C -- out of the picture, which is how
// x16_i32_abs() can leave -2147483648 alone instead of trapping.
//
// Digits are $30 + n rather than '0' + n: a character literal depends on
// the including program's encoding pragma, a byte does not.
// =====================================================================

#include <x16/int32.h>

// "4294967295" plus a NUL is eleven bytes; twelve, built from the end.
static char i32_buf[12];

// ---------------------------------------------------------------------
// Widening and narrowing.
// ---------------------------------------------------------------------
long x16_i32_from_u16(unsigned int v) {
    return (long)(unsigned long)v;
}

long x16_i32_from_s16(int v) {
    return (long)v;
}

int x16_i32_to_s16(long v) {
    return (int)(unsigned int)((unsigned long)v & 0xFFFFUL);
}

// ---------------------------------------------------------------------
// Add, subtract, negate, absolute value, multiply.
// ---------------------------------------------------------------------
long x16_i32_add(long a, long b) {
    return (long)((unsigned long)a + (unsigned long)b);
}

long x16_i32_sub(long a, long b) {
    return (long)((unsigned long)a - (unsigned long)b);
}

long x16_i32_neg(long a) {
    return (long)(0UL - (unsigned long)a);
}

// -2147483648 has no positive counterpart in 32 bits, so it comes back
// unchanged.
long x16_i32_abs(long a) {
    if (a < 0) {
        return (long)(0UL - (unsigned long)a);
    }
    return a;
}

long x16_i32_mul(long a, long b) {
    return (long)((unsigned long)a * (unsigned long)b);
}

// ---------------------------------------------------------------------
// The three single-bit shifts.
// ---------------------------------------------------------------------
long x16_i32_shl(long a) {
    return (long)((unsigned long)a << 1);
}

unsigned long x16_i32_shr(unsigned long a) {
    return a >> 1;
}

// Sign fill, spelled out rather than left to the implementation.
long x16_i32_asr(long a) {
    unsigned long u = (unsigned long)a;

    return (long)((u >> 1) | (u & 0x80000000UL));
}

// ---------------------------------------------------------------------
// Comparisons: -1, 0, 1.
// ---------------------------------------------------------------------
signed char x16_i32_cmpu(unsigned long a, unsigned long b) {
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

signed char x16_i32_cmps(long a, long b) {
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------
// One division, both results. A zero divisor writes nothing and hands
// back `a`.
// ---------------------------------------------------------------------
unsigned long x16_i32_divmod(unsigned long a, unsigned long b,
                             unsigned long *rem) {
    if (b == 0) {
        return a;                       // *rem deliberately untouched
    }
    *rem = a % b;
    return a / b;
}

// ---------------------------------------------------------------------
// Decimal, least significant digit first into the end of the buffer, so
// "no leading zeros" costs nothing. The pointer returned is the first
// digit.
// ---------------------------------------------------------------------
char *x16_i32_to_dec(unsigned long v) {
    char *p = i32_buf + 11;

    *p = 0;
    do {
        --p;
        *p = (char)(0x30 + (unsigned char)(v % 10));
        v /= 10;
    } while (v != 0);
    return p;
}
