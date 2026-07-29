// =====================================================================
// x16clib :: x16/stack.c -- an 8 KB LIFO in a HIRAM bank
// =====================================================================
// A byte stack living in one banked-RAM page ($A000-$BFFF), the twin of
// x16/ringbuffer.h. Give it a bank with x16_stack_init() and the bank is
// yours to lose: nothing else may use it while the stack is live.
//
// It grows DOWNWARD from the top of the window, like the 6502's own, so
// `sp` doubles as the count of bytes still free.
//
// The bank is switched in around each access and switched back, so a
// caller's own bank selection survives.
//
// NOT SAFE ACROSS AN INTERRUPT -- see the note in ringbuffer.c.
//
// WRITTEN IN C, for the same reason ringbuffer.c is: the ca65 version
// shares helper routines between its entry points and KickC asm labels
// do not reach across function bodies. The index arithmetic is identical
// and so are the results.
// =====================================================================

#include <x16/stack.h>

#define X16_HST_TOP  8191U              // top offset of the bank window

// RAM_BANK ($00) is switched in asm; see the note in ringbuffer.c.

volatile unsigned char x16__hst_bank;    // the HIRAM bank we own
volatile unsigned char x16__hst_sav;    // the caller's RAM_BANK
volatile unsigned char x16__hst_v;      // the byte in transit
volatile unsigned int x16__hst_sp;      // next free slot, counting down

// ---------------------------------------------------------------------
// Claim `bank` and empty the stack.
// ---------------------------------------------------------------------
void x16_stack_init(unsigned char bank) {
    x16__hst_bank = bank;
    x16__hst_sp = X16_HST_TOP;
}

// ---------------------------------------------------------------------
// One byte on. Overruns silently when full -- test x16_stack_isfull()
// first if that matters.
// ---------------------------------------------------------------------
void x16_stack_push(unsigned char b) {
    unsigned char *w;
    x16__hst_v = b;
    __asm {
        lda 0x00                        // RAM_BANK
        sta x16__hst_sav
        lda x16__hst_bank
        sta 0x00
    }
    w = (unsigned char *)(0xA000U + x16__hst_sp);
    *w = x16__hst_v;
    __asm {
        lda x16__hst_sav
        sta 0x00                        // RAM_BANK
    }
    x16__hst_sp = x16__hst_sp - 1;
}

// ---------------------------------------------------------------------
// A 16-bit value. Pushed high byte first, so a popw -- which pops low
// then high -- reads back what was written.
// ---------------------------------------------------------------------
void x16_stack_pushw(unsigned int w) {
    x16_stack_push((unsigned char)(w >> 8));
    x16_stack_push((unsigned char)(w & 0xFF));
}

// ---------------------------------------------------------------------
// One byte off. Underruns silently when empty.
// ---------------------------------------------------------------------
unsigned char x16_stack_pop(void) {
    unsigned char *w;
    x16__hst_sp = x16__hst_sp + 1;
    __asm {
        lda 0x00                        // RAM_BANK
        sta x16__hst_sav
        lda x16__hst_bank
        sta 0x00
    }
    w = (unsigned char *)(0xA000U + x16__hst_sp);
    x16__hst_v = *w;
    __asm {
        lda x16__hst_sav
        sta 0x00                        // RAM_BANK
    }
    return x16__hst_v;
}

unsigned int x16_stack_popw(void) {
    unsigned char lo = x16_stack_pop();
    unsigned char hi = x16_stack_pop();
    return (unsigned int)lo | ((unsigned int)hi << 8);
}

// ---------------------------------------------------------------------
// How much is stacked, and how much room is left.
// ---------------------------------------------------------------------
unsigned int x16_stack_size(void) {
    return X16_HST_TOP - x16__hst_sp;
}

unsigned int x16_stack_free(void) {
    return x16__hst_sp;
}

unsigned char x16_stack_isempty(void) {
    if (x16__hst_sp == X16_HST_TOP) {
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------
// Full when the pointer has run off the bottom of the window. The ca65
// version's low-byte test for "0 or 1 byte free" was unreachable until
// upstream fixed it (x16_library v0.18.6); this reports full one push
// earlier, which is what the fix made it do.
// ---------------------------------------------------------------------
unsigned char x16_stack_isfull(void) {
    if (x16__hst_sp >= 0x2000 || x16__hst_sp <= 1) {
        return 1;
    }
    return 0;
}
