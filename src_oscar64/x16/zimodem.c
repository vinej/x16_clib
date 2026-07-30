// =====================================================================
// x16clib :: x16/zimodem.c -- ZiModem (ESP32 WiFi) over the serial card
// =====================================================================
// A Hayes-style modem conversation on top of x16/serial.h: AT lines out,
// "OK\r\n" back. Everything here frames commands and matches replies; the
// firmware doing the WiFi is the ESP32's.
//
// EVERY AT STRING IS SPELLED IN EXPLICIT BYTES, exactly as the ca65 port
// does. These go out to firmware that wants 7-bit ASCII, and a C string
// literal is at the mercy of whatever character mapping the toolchain
// applies -- `cmp #'B'` assembling to a PETSCII byte is a bug this
// library has already been bitten by. Bytes cannot be mistranslated.
// =====================================================================

#include <x16/zimodem.h>
#include <x16/serial.h>

// "ate0q0v1x1f0r1s45=3&p0&k3" -- echo off, quiet off, verbose, extended
// result codes, flow control, stream mode
static const char zi_cfg[] = {
    0x61, 0x74, 0x65, 0x30, 0x71, 0x30, 0x76, 0x31, 0x78, 0x31, 0x66, 0x30,
    0x72, 0x31, 0x73, 0x34, 0x35, 0x3D, 0x33, 0x26, 0x70, 0x30, 0x26, 0x6B,
    0x33, 0x00
};
static const char zi_atz[]   = { 0x61, 0x74, 0x7A, 0x00 };               // "atz"
static const char zi_ati2[]  = { 0x61, 0x74, 0x69, 0x32, 0x00 };         // "ati2"
static const char zi_ats45[] = { 0x61, 0x74, 0x73, 0x34, 0x35,
                                 0x3D, 0x31, 0x00 };                    // "ats45=1"
static const char zi_atg[]   = { 0x61, 0x74, 0x26, 0x67, 0x22, 0x00 };   // at&g"
static const char zi_qcrlf[] = { 0x22, 0x0D, 0x0A, 0x00 };               // " CR LF
static const char zi_crlf[]  = { 0x0D, 0x0A, 0x00 };
static const char zi_ok[]    = { 0x4F, 0x4B, 0x0D, 0x0A, 0x00 };         // "OK" CR LF
static const char zi_rrerr[] = { 0x52, 0x52, 0x4F, 0x52,
                                 0x0D, 0x0A, 0x00 };                     // "RROR" CR LF

static char zi_linebuf[90];             // one hex line, read before decoding

volatile unsigned char x16__zi_t;       // x16_zi_delay's tick counter

// ---------------------------------------------------------------------
// A coarse busy-wait so the ESP32 can keep up: 65536 inner steps per
// tick, about 40 ms at 8 MHz. Self-contained -- no jiffy IRQ, no KERNAL
// -- so it works in any context, including with interrupts off.
//
// In asm because a C loop with no side effects is exactly what an
// optimiser is entitled to delete. X carries the middle count rather
// than ca65's `inc a`, which is a 65C02 opcode this target does not have.
// ---------------------------------------------------------------------
void x16_zi_delay(unsigned char ticks) {
    __asm {
        lda ticks
        beq zd_done
        sta x16__zi_t
    zd_tick:
        ldx #0
    zd_mid:
        ldy #0
    zd_inner:
        iny
        bne zd_inner                    // 256 inner steps
        inx
        bne zd_mid                      // 256 middle steps -> 65536 inner
        dec x16__zi_t
        bne zd_tick
    zd_done:
    }
}

// ---------------------------------------------------------------------
// Send one AT command line. Pure transmit: it does NOT read the reply.
// ---------------------------------------------------------------------
void x16_zi_cmd(const char *cmd) {
    x16_ser_puts(cmd);
    x16_ser_puts(zi_crlf);
}

// ---------------------------------------------------------------------
// Read and discard the reply up to and including "OK\r\n".
// ---------------------------------------------------------------------
void x16_zi_wait_ok(void) {
    x16_ser_discard_until(zi_ok);
}

// ---------------------------------------------------------------------
// Put the ESP32 into a known command state: program the UART, let the
// board settle, CTRL-C any stream left running, then the standard config.
// ---------------------------------------------------------------------
void x16_zi_init(unsigned int uart, unsigned int divisor) {
    x16_ser_init(uart, divisor);
    x16_zi_delay(4);                    // the ESP32 may still be booting
    x16_ser_put(0x03);                  // CTRL-C: abort any prior stream
    x16_zi_delay(2);
    x16_zi_cmd(zi_cfg);
    x16_zi_wait_ok();
}

// ---------------------------------------------------------------------
// ATZ, back to the saved profile.
// ---------------------------------------------------------------------
void x16_zi_reset(void) {
    x16_ser_put(0x03);
    x16_zi_delay(2);
    x16_zi_cmd(zi_atz);
    x16_zi_wait_ok();
}

// ---------------------------------------------------------------------
// ATI2 prints the IP and then OK. Read the lot into the caller's buffer,
// then cut the string at the first character at or below space, leaving
// just the dotted-quad.
// ---------------------------------------------------------------------
void x16_zi_get_ip(char *buf) {
    unsigned char i;

    x16_zi_cmd(zi_ati2);
    x16_ser_read_until(buf, 24, zi_ok);

    for (i = 0; ; ++i) {
        if ((unsigned char)buf[i] < 0x21) {      // ' ' + 1
            buf[i] = 0;
            return;
        }
    }
}

// ---------------------------------------------------------------------
// Begin a hex-mode download: enable hex transfer, ask for the file, and
// eat the "[..header..]" line. 0 = started, 1 = not found.
// ---------------------------------------------------------------------
unsigned char x16_zi_hex_open(const char *name) {
    x16_zi_cmd(zi_ats45);               // ats45=1 : enable hex-mode transfer
    x16_zi_wait_ok();

    x16_ser_puts(zi_atg);               // at&g"
    x16_ser_puts(name);
    x16_ser_puts(zi_qcrlf);             // " CR LF

    if (x16_ser_get_wait() != 0x5B) {   // '[' opens the header
        x16_ser_discard_until(zi_rrerr);        // drain the "ERROR" line
        return 1;
    }
    x16_ser_discard_until(zi_crlf);     // skip the rest of the header
    return 0;
}

// ---------------------------------------------------------------------
// One payload line -> up to 44 raw bytes. 0 means the file is done, which
// the firmware signals with a bare "OK\r\n" where a data line would be.
// ---------------------------------------------------------------------
unsigned char x16_zi_hex_chunk(unsigned char *buf) {
    unsigned int n = x16_ser_read_until(zi_linebuf, 90, zi_crlf);

    if (n == 4 && (unsigned char)zi_linebuf[0] == 0x4F) {       // 'O' of OK
        return 0;
    }
    // digits = the line length minus its CR/LF
    return x16_zi_hexdecode(buf, zi_linebuf, (unsigned char)(n - 2));
}

// ---------------------------------------------------------------------
// Swallow the trailing OK after the payload.
// ---------------------------------------------------------------------
void x16_zi_hex_close(void) {
    x16_zi_wait_ok();
}

// ---------------------------------------------------------------------
// One uppercase ASCII hex digit -> its 0..15 value. 'A' is 0x41, so
// 0x41 - 0x30 = 17, and folding anything >= 10 down by 7 lands it on 10.
// ---------------------------------------------------------------------
static unsigned char zi_nib(unsigned char c) {
    c -= 0x30;                          // ASCII '0'
    if (c >= 10) {
        c -= 7;
    }
    return c;
}

// ---------------------------------------------------------------------
// A run of hex digits -> packed bytes. Returns the byte count written.
// The one piece of ZiModem logic with an independent oracle, so the test
// suite drives it directly.
// ---------------------------------------------------------------------
unsigned char x16_zi_hexdecode(unsigned char *dest, const char *src,
                               unsigned char ndigits) {
    unsigned char produced = 0;

    while (ndigits != 0) {
        *dest = (unsigned char)((zi_nib((unsigned char)src[0]) << 4) |
                                 zi_nib((unsigned char)src[1]));
        ++dest;
        src += 2;
        ++produced;
        ndigits -= 2;
    }
    return produced;
}
