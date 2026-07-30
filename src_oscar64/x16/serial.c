// =====================================================================
// x16clib :: x16/serial.c -- the serial / WiFi card UARTs
// =====================================================================
// Up to two 16C550-style UARTs sit on 8-byte boundaries in the expansion
// I/O window; the standard card populates $9F60 (UART 0) and $9F68.
//
// The register file, as an offset from the UART's base:
//   0  RHR/THR   receive / transmit holding    (DLL when DLAB=1)
//   1  IER       interrupt enable              (DLM when DLAB=1)
//   2  IIR/FCR   read: interrupt id / write: FIFO control
//   3  LCR       line control (word size, parity, stop, DLAB)
//   4  MCR       modem control (DTR/RTS/loop/auto-flow)
//   5  LSR       line status (DR, THRE, errors)
//   6  MSR       modem status
//   7  SCR       scratch (no hardware effect -- used to fingerprint)
//
// x16_ser_init programs 8N1 + FIFOs + auto-flow and leaves interrupts
// OFF: this module POLLS, so it never goes near irq.c's CINV chain and is
// safe alongside it. It remembers the UART it was handed; every other
// call talks to that one.
//
// The base is held in a VOLATILE pointer, and that is load-bearing: the
// status polls below re-read LSR on every pass, and a non-volatile read
// is exactly what an optimiser may hoist out of the loop. Reads here have
// hardware side effects too -- reading RHR pops the RX FIFO, reading LSR
// clears the sticky error bits -- so each one has to happen exactly as
// often as it is written.
// =====================================================================

#include <x16/serial.h>

// --- register offsets ------------------------------------------------
#define SER_RHR 0                       // = THR on write, = DLL when DLAB set
#define SER_IER 1                       // = DLM when DLAB set
#define SER_FCR 2                       // write: FIFO control (reads IIR)
#define SER_LCR 3
#define SER_MCR 4
#define SER_LSR 5
#define SER_MSR 6
#define SER_SCR 7

// --- LSR bits --------------------------------------------------------
#define SER_LSR_DR   0x01               // a received byte is ready
#define SER_LSR_THRE 0x20               // transmit holding register is empty

#define SER_SCAN_FIRST 0x9F60           // first candidate UART base
#define SER_SCAN_LAST  0x9FF8           // last candidate UART base
#define SER_SCAN_STEP  8                // UARTs sit on 8-byte boundaries

static volatile unsigned char *ser_p;   // the UART init last selected
static unsigned int ser_u0;             // detect results (0 = none)
static unsigned int ser_u1;

volatile unsigned char x16__se_f;       // the caller's processor status

// ---------------------------------------------------------------------
// Fingerprint one candidate base: 1 if a UART answered, 0 if not.
//
// Three registers whose behaviour a UART is required to have and bare bus
// is not: the top nibble of IER always reads 0, the top two bits of MCR
// always read 0, and the scratch register holds whatever you put in it.
// Two different scratch patterns make a floating bus very unlikely to
// answer by accident.
// ---------------------------------------------------------------------
static unsigned char ser_probe(volatile unsigned char *p) {
    p[SER_IER] = 0xF0;
    if ((p[SER_IER] & 0xF0) != 0) {
        return 0;
    }
    p[SER_IER] = 0;

    p[SER_MCR] = 0xFF;
    if (p[SER_MCR] != 0x3F) {
        p[SER_MCR] = 0;                 // leave MCR clean before bailing
        return 0;
    }
    p[SER_MCR] = 0;

    p[SER_SCR] = 0xA5;
    if (p[SER_SCR] != 0xA5) {
        return 0;
    }
    p[SER_SCR] = 0x5A;
    if (p[SER_SCR] != 0x5A) {
        return 0;
    }
    return 1;
}

// ---------------------------------------------------------------------
// Scan the window for UARTs, with interrupts held off so no handler sees
// one mid-fingerprint.
//
// The two asm blocks are each stack-balanced -- php/pla saves the status
// into a byte, pha/plp puts it back -- because Oscar64 is free to use the
// hardware stack for its own temporaries in the C between them. A php in
// one block and a plp in another would not survive that.
// ---------------------------------------------------------------------
unsigned char x16_ser_detect(void) {
    unsigned int base;
    unsigned char found = 0;

    ser_u0 = 0;
    ser_u1 = 0;

    __asm {
        php
        pla
        sta x16__se_f
        sei
    }

    for (base = SER_SCAN_FIRST; base <= SER_SCAN_LAST; base += SER_SCAN_STEP) {
        if (ser_probe((volatile unsigned char *)base)) {
            if (ser_u0 == 0) {
                ser_u0 = base;
            } else {
                ser_u1 = base;
                break;                  // two is all the card can have
            }
        }
    }

    __asm {
        lda x16__se_f
        pha
        plp
    }

    if (ser_u0 != 0) {
        ++found;
    }
    if (ser_u1 != 0) {
        ++found;
    }
    return found;
}

unsigned int x16_ser_uart0(void) {
    return ser_u0;
}

unsigned int x16_ser_uart1(void) {
    return ser_u1;
}

// ---------------------------------------------------------------------
// Program a UART for 8N1, FIFOs on, auto-flow, no interrupts, and make it
// the current one.
// ---------------------------------------------------------------------
void x16_ser_init(unsigned int base, unsigned int divisor) {
    volatile unsigned char *p = (volatile unsigned char *)base;

    ser_p = p;

    p[SER_LCR] = 0x80;                  // DLAB = 1 to reach the divisor latch
    p[SER_RHR] = (unsigned char)divisor;                // DLL
    p[SER_IER] = (unsigned char)(divisor >> 8);         // DLM
    p[SER_LCR] = 0x03;                  // 8 bits, no parity, 1 stop, DLAB = 0
    p[SER_FCR] = 0x87;                  // FIFO enable + reset both, trigger 8
    p[SER_MCR] = 0x27;                  // DTR+RTS, auto-flow, OUT2 (ZiModem)
    p[SER_IER] = 0x00;                  // no interrupts: this module polls
}

// ---------------------------------------------------------------------
// Is a received byte waiting?
// ---------------------------------------------------------------------
unsigned char x16_ser_avail(void) {
    if ((ser_p[SER_LSR] & SER_LSR_DR) != 0) {
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------
// One byte in, without blocking: the byte, or -1 if the RX FIFO is empty.
// ---------------------------------------------------------------------
int x16_ser_get(void) {
    if ((ser_p[SER_LSR] & SER_LSR_DR) == 0) {
        return -1;
    }
    return (int)ser_p[SER_RHR];          // this read pops the RX FIFO
}

// ---------------------------------------------------------------------
// One byte in, blocking. Only sane once something is really connected.
// ---------------------------------------------------------------------
unsigned char x16_ser_get_wait(void) {
    while ((ser_p[SER_LSR] & SER_LSR_DR) == 0) {
    }
    return ser_p[SER_RHR];
}

// ---------------------------------------------------------------------
// One byte out, waiting for room in the transmit FIFO.
// ---------------------------------------------------------------------
void x16_ser_put(unsigned char b) {
    while ((ser_p[SER_LSR] & SER_LSR_THRE) == 0) {
    }
    ser_p[SER_RHR] = b;                 // offset 0 on write is THR
}

// ---------------------------------------------------------------------
// A NUL-terminated string, and a counted binary-safe run.
//
// ca65's ser_puts walks the string with a Y index and so stops at 256
// bytes; this one does not, which is the same behaviour for every string
// that is actually NUL-terminated inside 256 bytes.
// ---------------------------------------------------------------------
void x16_ser_puts(const char *s) {
    while (*s != 0) {
        x16_ser_put((unsigned char)*s);
        ++s;
    }
}

void x16_ser_write(const void *data, unsigned char len) {
    const unsigned char *p = (const unsigned char *)data;

    // A do/while, so len == 0 sends 256 bytes as documented.
    do {
        x16_ser_put(*p);
        ++p;
    } while (--len != 0);
}

// ---------------------------------------------------------------------
// Read until the needle has gone past, or max bytes are stored. The
// needle is included in what is stored. Returns the count stored.
// ---------------------------------------------------------------------
unsigned int x16_ser_read_until(char *buf, unsigned int max,
                                const char *match) {
    const char *needle = match;         // the moving cursor
    unsigned int stored = 0;
    unsigned char c;

    while (stored < max) {
        c = x16_ser_get_wait();
        *buf = (char)c;
        ++buf;
        ++stored;

        if ((char)c == *needle) {
            ++needle;
            if (*needle == 0) {         // whole needle matched
                break;
            }
        } else {
            needle = match;             // mismatch: start the needle over
        }
    }
    return stored;
}

// ---------------------------------------------------------------------
// The same match, with the bytes thrown away.
// ---------------------------------------------------------------------
void x16_ser_discard_until(const char *match) {
    const char *needle = match;

    for (;;) {
        if ((char)x16_ser_get_wait() == *needle) {
            ++needle;
            if (*needle == 0) {
                return;
            }
        } else {
            needle = match;
        }
    }
}
