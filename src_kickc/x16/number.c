// =====================================================================
// x16clib :: x16/number.c -- number formatting and parsing
// =====================================================================
// The same routines as src_ca65/util/number.s, in plain C: decimal by
// repeated subtraction against a powers-of-ten table (KickC has no
// runtime division to lean on, and neither did the asm), fixed-width
// hex and binary, and the overflow-checked decimal parser.
//
// ALL CONVERSIONS SHARE ONE MODULE BUFFER, exactly like the asm: the
// returned pointer aims into it and the next call overwrites it. The
// bytes are ASCII, written as explicit values ($30-$39, $41-$46, $2D)
// so no #pragma encoding in the including program can bend them.
// =====================================================================

#include <x16/number.h>

// The shared conversion buffer: 16 binary digits plus the terminator.
__mem volatile char x16__num_buf[17];

const unsigned int x16__num_pow10[5] = { 10000, 1000, 100, 10, 1 };

char *x16_u16_to_dec(unsigned int v) {
    unsigned char started = 0;          // past the leading zeros yet?
    unsigned char len = 0;
    unsigned char x;
    unsigned char d;
    unsigned int p;
    for (x = 0; x < 5; ++x) {
        d = 0x30;                       // '0'
        p = x16__num_pow10[x];
        while (v >= p) {                // repeated subtraction
            v -= p;
            ++d;
        }
        if (d != 0x30 || started != 0 || x == 4) {
            started = 1;                // a non-zero digit always prints,
            x16__num_buf[len] = (char)d;    // and the units digit always
            ++len;
        }
    }
    x16__num_buf[len] = 0;
    return (char *)x16__num_buf;
}

char *x16_u8_to_dec(unsigned char v) {
    return x16_u16_to_dec((unsigned int)v);
}

char *x16_s16_to_dec(int v) {
    unsigned int m;
    unsigned char len;
    unsigned char k;
    if (v >= 0) {
        return x16_u16_to_dec((unsigned int)v);
    }
    m = (unsigned int)v;                // two's-complement magnitude,
    m = (m ^ 0xFFFF) + 1;               // safe for -32768. XOR, not ~:
                                        // KickC has no fragment for a
                                        // 16-bit bitwise NOT.
    x16_u16_to_dec(m);                  // format the magnitude
    len = 0;
    while (x16__num_buf[len] != 0) {
        ++len;
    }
    k = len;                            // shift right by one for the sign
    for (;;) {
        x16__num_buf[k + 1] = x16__num_buf[k];
        if (k == 0) {
            break;
        }
        --k;
    }
    x16__num_buf[0] = 0x2d;             // '-'
    return (char *)x16__num_buf;
}

char *x16_s8_to_dec(signed char v) {
    return x16_s16_to_dec((int)v);
}

// one hex digit, ASCII: 0-9 -> $30.., 10-15 -> $41.. ('A'-10 = $37)
unsigned char x16__num_digit(unsigned char n) {
    n = (unsigned char)(n & 0x0f);
    if (n < 10) {
        return (unsigned char)(n + 0x30);
    }
    return (unsigned char)(n + 0x37);
}

char *x16_u8_to_hex(unsigned char v) {
    x16__num_buf[0] = (char)x16__num_digit((unsigned char)(v >> 4));
    x16__num_buf[1] = (char)x16__num_digit(v);
    x16__num_buf[2] = 0;
    return (char *)x16__num_buf;
}

char *x16_u16_to_hex(unsigned int v) {
    unsigned char hi = (unsigned char)(v >> 8);
    unsigned char lo = (unsigned char)v;
    x16__num_buf[0] = (char)x16__num_digit((unsigned char)(hi >> 4));
    x16__num_buf[1] = (char)x16__num_digit(hi);
    x16__num_buf[2] = (char)x16__num_digit((unsigned char)(lo >> 4));
    x16__num_buf[3] = (char)x16__num_digit(lo);
    x16__num_buf[4] = 0;
    return (char *)x16__num_buf;
}

char *x16_u8_to_bin(unsigned char v) {
    unsigned char y;
    for (y = 0; y < 8; ++y) {           // MSB first
        x16__num_buf[y] = (v & 0x80) != 0 ? (char)0x31 : (char)0x30;
        v = (unsigned char)(v << 1);
    }
    x16__num_buf[8] = 0;
    return (char *)x16__num_buf;
}

char *x16_u16_to_bin(unsigned int v) {
    unsigned char y;
    for (y = 0; y < 16; ++y) {
        x16__num_buf[y] = (v & 0x8000) != 0 ? (char)0x31 : (char)0x30;
        v = v << 1;
    }
    x16__num_buf[16] = 0;
    return (char *)x16__num_buf;
}

unsigned char x16_dec_to_u16(const char *s, unsigned char len,
                             unsigned int *value) {
    unsigned int v = 0;
    unsigned int t;
    unsigned char y;
    unsigned char d;
    for (y = 0; y < len; ++y) {
        d = (unsigned char)((unsigned char)s[y] - 0x30);
        if (d >= 10) {                  // a non-digit (wraps below '0' too)
            return 0;
        }
        // v = v * 10 + d, refusing anything past 65535 -- the asm
        // watched the carry out of each shift; the bound is the same.
        if (v > 6553) {
            return 0;
        }
        if (v == 6553 && d > 5) {
            return 0;
        }
        t = v;                          // *10 by shift-add, like the asm
        v = v << 1;                     // *2
        v = v << 1;                     // *4
        v = v + t;                      // *5
        v = v << 1;                     // *10
        v = v + (unsigned int)d;
    }
    *value = v;                         // untouched on any failure above
    return 1;
}
