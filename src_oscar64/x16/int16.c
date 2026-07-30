// =====================================================================
// x16clib :: x16/int16.c -- 16-bit integer arithmetic
// =====================================================================
// The parity layer over C's own 16-bit int, plus the three composites
// that genuinely add something: one-division divmod, integer sqrt, and
// decimal rendering without printf.
//
// Two conventions worth stating once:
//
//   The arithmetic entries do their work on UNSIGNED values and cast
//   back. Two's complement makes add/sub/neg/mul identical either way,
//   and going through unsigned means signed overflow -- which is
//   undefined in C, and which callers of a 16-bit library hit all the
//   time -- cannot become undefined behaviour here. It is how
//   x16_i16_abs(-32768) stays -32768 rather than trapping.
//
//   Digits are written as $30 + n, never '0' + n. A character literal
//   depends on the including program's encoding pragma; the byte does
//   not, and this string goes to files and screens that want ASCII.
// =====================================================================

#include <x16/int16.h>

// "-32768" plus a NUL is seven bytes; built from the end, so eight
// leaves room for the sign in front of five digits.
static char i16_buf[8];

// ---------------------------------------------------------------------
// Widening.
// ---------------------------------------------------------------------
int x16_i16_from_u8(unsigned char v) {
    return (int)(unsigned int)v;
}

int x16_i16_from_s8(signed char v) {
    return (int)v;
}

// ---------------------------------------------------------------------
// Add, subtract, negate, absolute value, multiply.
// ---------------------------------------------------------------------
int x16_i16_add(int a, int b) {
    return (int)((unsigned int)a + (unsigned int)b);
}

int x16_i16_sub(int a, int b) {
    return (int)((unsigned int)a - (unsigned int)b);
}

int x16_i16_neg(int a) {
    return (int)(0U - (unsigned int)a);
}

// -32768 has no positive counterpart in 16 bits, so it comes back
// unchanged rather than wrapping to something meaningless.
int x16_i16_abs(int a) {
    if (a < 0) {
        return (int)(0U - (unsigned int)a);
    }
    return a;
}

int x16_i16_mul(int a, int b) {
    return (int)((unsigned int)a * (unsigned int)b);
}

// ---------------------------------------------------------------------
// The three single-bit shifts.
// ---------------------------------------------------------------------
int x16_i16_shl(int a) {
    return (int)((unsigned int)a << 1);
}

unsigned int x16_i16_shr(unsigned int a) {
    return a >> 1;
}

// Sign fill, spelled out: the logical shift, with the sign bit put back.
// C leaves >> on a negative value implementation-defined, so this does
// not ask.
int x16_i16_asr(int a) {
    unsigned int u = (unsigned int)a;

    return (int)((u >> 1) | (u & 0x8000U));
}

// ---------------------------------------------------------------------
// Comparisons: -1, 0, 1.
// ---------------------------------------------------------------------
signed char x16_i16_cmpu(unsigned int a, unsigned int b) {
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

signed char x16_i16_cmps(int a, int b) {
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------
// One division, both results. A zero divisor is not an error and not a
// crash: nothing is written and `a` comes back.
// ---------------------------------------------------------------------
unsigned int x16_i16_divmod(unsigned int a, unsigned int b,
                            unsigned int *rem) {
    if (b == 0) {
        return a;                       // *rem deliberately untouched
    }
    *rem = a % b;
    return a / b;
}

// Truncated toward zero, remainder taking the DIVIDEND's sign: -7/2 is
// -3 remainder -1. Done on magnitudes and re-signed, rather than trusting
// the compiler's signed division to round the same way.
int x16_i16_divmod_s(int a, int b, int *rem) {
    unsigned int ua, ub, q, r;
    unsigned char neg_q, neg_r;

    if (b == 0) {
        return a;                       // *rem deliberately untouched
    }

    neg_r = 0;
    if (a < 0) {
        ua = 0U - (unsigned int)a;
        neg_r = 1;
    } else {
        ua = (unsigned int)a;
    }
    if (b < 0) {
        ub = 0U - (unsigned int)b;
    } else {
        ub = (unsigned int)b;
    }
    neg_q = 0;
    if ((a < 0) != (b < 0)) {
        neg_q = 1;
    }

    q = ua / ub;
    r = ua % ub;
    if (neg_q) {
        q = 0U - q;
    }
    if (neg_r) {
        r = 0U - r;
    }
    *rem = (int)r;
    return (int)q;
}

// ---------------------------------------------------------------------
// floor(sqrt(v)) by the classic two-bits-at-a-time method: no division,
// no table, and the answer never exceeds 255 because 255*255 < 65536.
// ---------------------------------------------------------------------
unsigned char x16_i16_sqrt(unsigned int v) {
    unsigned int bit = 0x4000U;         // the top odd power of four
    unsigned int res = 0;

    while (bit > v) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (v >= res + bit) {
            v -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (unsigned char)res;
}

// ---------------------------------------------------------------------
// Decimal, built from the least significant digit backwards, which is
// what makes "no leading zeros" free. The returned pointer is into the
// module buffer, at the first digit.
// ---------------------------------------------------------------------
char *x16_i16_to_dec(unsigned int v) {
    char *p = i16_buf + 7;

    *p = 0;
    do {
        --p;
        *p = (char)(0x30 + (v % 10));
        v /= 10;
    } while (v != 0);
    return p;
}

char *x16_i16_to_dec_s(int v) {
    unsigned int mag;
    char *p;

    if (v < 0) {
        mag = 0U - (unsigned int)v;     // -32768 becomes 32768, not 0
    } else {
        mag = (unsigned int)v;
    }
    p = x16_i16_to_dec(mag);
    if (v < 0) {
        --p;
        *p = 0x2D;                      // '-'
    }
    return p;
}
