// =====================================================================
// x16clib :: x16/bits.c -- bit and nibble helpers
// =====================================================================
// The same operations as src_ca65/util/bits.s, in plain C. The point of
// keeping them as functions at all is API parity: C and assembly
// callers of the other toolchains share one implementation, and
// x16_bit_put turns a flag into a set-or-clear without a branch at the
// call site.
// =====================================================================

#include <x16/bits.h>

void x16_bit_set(unsigned char *addr, unsigned char mask) {
    unsigned char v = *addr;
    v = (unsigned char)(v | mask);
    *addr = v;
}

void x16_bit_clr(unsigned char *addr, unsigned char mask) {
    unsigned char v = *addr;
    v = (unsigned char)(v & (unsigned char)(mask ^ 0xff));
    *addr = v;
}

void x16_bit_put(unsigned char *addr, unsigned char mask, unsigned char on) {
    if (on != 0) {
        x16_bit_set(addr, mask);
    } else {
        x16_bit_clr(addr, mask);
    }
}

unsigned char x16_bit_test(const unsigned char *addr, unsigned char mask) {
    unsigned char v = *addr;
    return (unsigned char)(v & mask);
}

unsigned char x16_hinib(unsigned char v) {
    return (unsigned char)(v >> 4);
}

unsigned char x16_lonib(unsigned char v) {
    return (unsigned char)(v & 0x0f);
}

unsigned char x16_catnib(unsigned char hi, unsigned char lo) {
    return (unsigned char)(((hi & 0x0f) << 4) | (lo & 0x0f));
}
