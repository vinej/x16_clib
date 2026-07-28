// =====================================================================
// x16clib :: x16/bcd.c -- packed-BCD add and subtract
// =====================================================================
// The ca65 version (src_ca65/util/bcd.s) runs the 65C02's decimal mode;
// KickC-compiled C cannot set the D flag mid-expression, so this is the
// same arithmetic done BYTEWISE in C: per byte, add/subtract each
// nibble with decimal adjust and thread the carry (the asm's sed/adc
// chain, spelled out). For valid packed BCD the results are
// bit-identical, including the wrapped digits and the carry/borrow
// returns. No decimal mode also means nothing here can upset a custom
// interrupt handler's binary arithmetic.
//
// The 32-bit forms stage the operand through a 4-byte buffer with one
// dword store -- KickC has no runtime 32-bit shifts, and the buffer
// mirrors the asm's bcd_b anyway.
// =====================================================================

#include <x16/bcd.h>

// The threaded digit carry: carry-out for the adds, borrow for the
// subtracts (the asm's processor carry, made a byte).
volatile unsigned char x16__bcd_cy;

// The staged 32-bit operand, low byte first (the asm's bcd_b).
volatile unsigned char x16__bcd_b4[4];

// ---------------------------------------------------------------------
// One byte of packed BCD: r = av + bv + carry-in, decimal adjusted.
// x16__bcd_cy is the carry in and out.
// ---------------------------------------------------------------------
unsigned char x16__bcd_add_byte(unsigned char av, unsigned char bv) {
    unsigned char lo = (unsigned char)((av & 0x0f) + (bv & 0x0f) + x16__bcd_cy);
    unsigned char hi = (unsigned char)((av >> 4) + (bv >> 4));
    if (lo > 9) {
        lo = (unsigned char)(lo + 6);
    }
    if (lo > 0x0f) {                    // the low digit carried
        hi = (unsigned char)(hi + 1);
        lo = (unsigned char)(lo & 0x0f);
    }
    if (hi > 9) {
        hi = (unsigned char)((hi + 6) & 0x0f);
        x16__bcd_cy = 1;
    } else {
        x16__bcd_cy = 0;
    }
    return (unsigned char)((hi << 4) | lo);
}

// r = av - bv - borrow-in, decimal adjusted; x16__bcd_cy is the borrow.
unsigned char x16__bcd_sub_byte(unsigned char av, unsigned char bv) {
    unsigned char lo_a = (unsigned char)(av & 0x0f);
    unsigned char lo_b = (unsigned char)((bv & 0x0f) + x16__bcd_cy);
    unsigned char hi_a = (unsigned char)(av >> 4);
    unsigned char hi_b = (unsigned char)(bv >> 4);
    unsigned char lo;
    unsigned char hi;
    if (lo_a >= lo_b) {
        lo = (unsigned char)(lo_a - lo_b);
    } else {
        lo = (unsigned char)((unsigned char)(lo_a + 10) - lo_b);
        hi_b = (unsigned char)(hi_b + 1);
    }
    if (hi_a >= hi_b) {
        hi = (unsigned char)(hi_a - hi_b);
        x16__bcd_cy = 0;
    } else {
        hi = (unsigned char)((unsigned char)(hi_a + 10) - hi_b);
        x16__bcd_cy = 1;
    }
    return (unsigned char)((hi << 4) | lo);
}

// bcd_b4 = the 32-bit operand's four bytes, low first (no shifts).
void x16__bcd_stage32(unsigned long b) {
    unsigned long *p = (unsigned long *)x16__bcd_b4;
    *p = b;
}

// *p (4 bytes, low first) += / -= the staged operand.
unsigned char x16__bcd_add4(unsigned char *p) {
    unsigned char i;
    x16__bcd_cy = 0;
    for (i = 0; i < 4; ++i) {
        p[i] = x16__bcd_add_byte(p[i], x16__bcd_b4[i]);
    }
    return x16__bcd_cy;
}

unsigned char x16__bcd_sub4(unsigned char *p) {
    unsigned char i;
    x16__bcd_cy = 0;
    for (i = 0; i < 4; ++i) {
        p[i] = x16__bcd_sub_byte(p[i], x16__bcd_b4[i]);
    }
    return x16__bcd_cy;
}

// ---------------------------------------------------------------------
// The public entries. Add returns 1 if the sum overflowed the width,
// sub returns 1 on borrow; *a keeps the wrapped digits either way.
// ---------------------------------------------------------------------

unsigned char x16_bcd_add8(unsigned char *a, unsigned char b) {
    x16__bcd_cy = 0;
    *a = x16__bcd_add_byte(*a, b);
    return x16__bcd_cy;
}

unsigned char x16_bcd_sub8(unsigned char *a, unsigned char b) {
    x16__bcd_cy = 0;
    *a = x16__bcd_sub_byte(*a, b);
    return x16__bcd_cy;
}

unsigned char x16_bcd_add16(unsigned int *a, unsigned int b) {
    unsigned int av = *a;
    unsigned char lo;
    unsigned char hi;
    x16__bcd_cy = 0;
    lo = x16__bcd_add_byte((unsigned char)av, (unsigned char)b);
    hi = x16__bcd_add_byte((unsigned char)(av >> 8), (unsigned char)(b >> 8));
    *a = ((unsigned int)hi << 8) | (unsigned int)lo;
    return x16__bcd_cy;
}

unsigned char x16_bcd_sub16(unsigned int *a, unsigned int b) {
    unsigned int av = *a;
    unsigned char lo;
    unsigned char hi;
    x16__bcd_cy = 0;
    lo = x16__bcd_sub_byte((unsigned char)av, (unsigned char)b);
    hi = x16__bcd_sub_byte((unsigned char)(av >> 8), (unsigned char)(b >> 8));
    *a = ((unsigned int)hi << 8) | (unsigned int)lo;
    return x16__bcd_cy;
}

unsigned char x16_bcd_add32(unsigned long *a, unsigned long b) {
    x16__bcd_stage32(b);
    return x16__bcd_add4((unsigned char *)a);
}

unsigned char x16_bcd_sub32(unsigned long *a, unsigned long b) {
    x16__bcd_stage32(b);
    return x16__bcd_sub4((unsigned char *)a);
}

unsigned char x16_bcd_addto(unsigned char *value, unsigned long b) {
    x16__bcd_stage32(b);
    return x16__bcd_add4(value);
}

unsigned char x16_bcd_subfrom(unsigned char *value, unsigned long b) {
    x16__bcd_stage32(b);
    return x16__bcd_sub4(value);
}
