// =====================================================================
// x16clib :: x16/ringbuffer.c -- an 8 KB FIFO in a HIRAM bank
// =====================================================================
// A byte queue living in one banked-RAM page ($A000-$BFFF), so it costs
// no low RAM at all. Give it a bank with x16_ring_init() and the bank is
// yours to lose: nothing else may use it while the queue is live.
//
// Capacity is 8191 bytes, one short of the window, because head and tail
// must stay distinguishable when the queue is full.
//
// The bank is switched in around each access and switched back, so a
// caller's own bank selection survives.
//
// NOT SAFE ACROSS AN INTERRUPT: a handler that pushes while a get is
// half-done will corrupt the indices. Bracket in sei/cli if you share
// one with an ISR.
//
// WRITTEN IN C, unlike most of this library. The ca65 version
// (src_ca65/storage/ringbuffer.s) threads the head and tail through
// zero-page pointer pairs and shares six helper routines between its
// entry points -- and asm labels do not reach across function
// bodies, so those helpers cannot be shared the same way. The
// arithmetic is plain 16-bit index work either way, the banked window is
// just a pointer, and the results are identical; bcd.c made the same
// trade for the same reason.
// =====================================================================

#include <x16/ringbuffer.h>

#define X16_RNG_CAP  8192U               // the bank window, $A000-$BFFF
#define X16_RNG_MAX  8191U               // ...one short, so full != empty

// RAM_BANK ($00) is switched in asm rather than a deref of a
// cast literal, and would make a second load block --
// which is fatal here (see input.c). bank.c does the same.

volatile unsigned char x16__rng_bank;    // the HIRAM bank we own
volatile unsigned char x16__rng_sav;    // the caller's RAM_BANK
volatile unsigned char x16__rng_v;      // the byte in transit
volatile unsigned int x16__rng_fill;     // bytes currently queued
volatile unsigned int x16__rng_head;     // where the next put goes
volatile unsigned int x16__rng_tail;     // one before the next get

// ---------------------------------------------------------------------
// Claim `bank` and empty the queue.
//
// The tail starts at the TOP of the window: the first get advances it,
// wrapping to 0, which is where the head began.
// ---------------------------------------------------------------------
void x16_ring_init(unsigned char bank) {
    x16__rng_bank = bank;
    x16__rng_fill = 0;
    x16__rng_head = 0;
    x16__rng_tail = X16_RNG_MAX;
}

// ---------------------------------------------------------------------
// One byte in. Overruns silently when full -- test x16_ring_isfull()
// first if that matters.
// ---------------------------------------------------------------------
void x16_ring_put(unsigned char b) {
    unsigned char *w;
    x16__rng_v = b;
    __asm {
        lda 0x00                        // RAM_BANK
        sta x16__rng_sav
        lda x16__rng_bank
        sta 0x00
    }
    w = (unsigned char *)(0xA000U + x16__rng_head);
    *w = x16__rng_v;
    __asm {
        lda x16__rng_sav
        sta 0x00                        // RAM_BANK
    }
    x16__rng_head = x16__rng_head + 1;
    if (x16__rng_head == X16_RNG_CAP) {
        x16__rng_head = 0;
    }
    x16__rng_fill = x16__rng_fill + 1;
}

// ---------------------------------------------------------------------
// A 16-bit value, low byte first -- so a getw reads back what a putw
// wrote.
// ---------------------------------------------------------------------
void x16_ring_putw(unsigned int w) {
    x16_ring_put((unsigned char)(w & 0xFF));
    x16_ring_put((unsigned char)(w >> 8));
}

// ---------------------------------------------------------------------
// One byte out. Underruns silently when empty.
// ---------------------------------------------------------------------
unsigned char x16_ring_get(void) {
    unsigned char *w;
    x16__rng_fill = x16__rng_fill - 1;
    x16__rng_tail = x16__rng_tail + 1;
    if (x16__rng_tail == X16_RNG_CAP) {
        x16__rng_tail = 0;
    }
    __asm {
        lda 0x00                        // RAM_BANK
        sta x16__rng_sav
        lda x16__rng_bank
        sta 0x00
    }
    w = (unsigned char *)(0xA000U + x16__rng_tail);
    x16__rng_v = *w;
    __asm {
        lda x16__rng_sav
        sta 0x00                        // RAM_BANK
    }
    return x16__rng_v;
}

unsigned int x16_ring_getw(void) {
    unsigned char lo = x16_ring_get();
    unsigned char hi = x16_ring_get();
    return (unsigned int)lo | ((unsigned int)hi << 8);
}

// ---------------------------------------------------------------------
// How much is queued, and how much room is left. size + free is always
// 8191.
// ---------------------------------------------------------------------
unsigned int x16_ring_size(void) {
    return x16__rng_fill;
}

unsigned int x16_ring_free(void) {
    return X16_RNG_MAX - x16__rng_fill;
}

unsigned char x16_ring_isempty(void) {
    if (x16__rng_fill == 0) {
        return 1;
    }
    return 0;
}

unsigned char x16_ring_isfull(void) {
    if (x16__rng_fill >= X16_RNG_MAX) {
        return 1;
    }
    return 0;
}
