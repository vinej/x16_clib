/* =====================================================================
 * x16clib :: x16/serial.h -- the serial / WiFi card UARTs
 * =====================================================================
 * The X16 serial / WiFi card carries up to two 16C550-style UARTs in the
 * expansion I/O window ($9F60-$9FF8, on 8-byte boundaries; the standard
 * card populates $9F60 and $9F68). The WiFi half is an ESP32 running
 * ZiModem firmware, driven as an AT-command modem over UART 0 -- that
 * protocol lives in <x16/zimodem.h>; this header is the bytes.
 *
 *      if (x16_ser_detect()) {
 *          x16_ser_init(x16_ser_uart0(), X16_SER_BAUD_9600);
 *          x16_ser_puts("hello\r\n");
 *          while ((c = x16_ser_get()) == -1)
 *              ;                        // poll (bound this yourself!)
 *      }
 *
 * x16_ser_init() programs 8N1, FIFOs on, auto-flow -- and NO interrupts:
 * this module polls, so it composes with <x16/irq.h> without touching
 * it. The UART handed to init becomes "the current one" for every other
 * call; call init again to point them elsewhere.
 *
 * The blocking calls (x16_ser_get_wait, x16_ser_put and everything built
 * on them) spin on the UART's status register with no timeout. They are
 * for a card that is really there -- detect first.
 * =====================================================================
 */

#ifndef X16_SERIAL_H
#define X16_SERIAL_H

/* Baud-rate divisors for x16_ser_init(), from the card's 14.7456 MHz
** clock: divisor = 14745600 / (16 * baud).
*/
#define X16_SER_BAUD_921600     0x0001
#define X16_SER_BAUD_460800     0x0002
#define X16_SER_BAUD_230400     0x0004
#define X16_SER_BAUD_115200     0x0008
#define X16_SER_BAUD_57600      0x0010
#define X16_SER_BAUD_38400      0x0018
#define X16_SER_BAUD_28800      0x0020
#define X16_SER_BAUD_19200      0x0030
#define X16_SER_BAUD_14400      0x0040
#define X16_SER_BAUD_9600       0x0060
#define X16_SER_BAUD_4800       0x00C0
#define X16_SER_BAUD_2400       0x0180
#define X16_SER_BAUD_1200       0x0300
#define X16_SER_BAUD_600        0x0600
#define X16_SER_BAUD_300        0x0C00

/* Scan the expansion window for UART chips. Returns how many answered
** (0, 1 or 2); their base addresses come back from x16_ser_uart0() and
** x16_ser_uart1() (0 = none). The probe fingerprints registers a 16C550
** must have -- a floating bus does not answer it by accident.
*/
unsigned char x16_ser_detect (void);
unsigned int x16_ser_uart0 (void);
unsigned int x16_ser_uart1 (void);

/* Program `base` for 8N1, FIFOs, auto-flow, no interrupts, and make it
** the current UART. `divisor` is an X16_SER_BAUD_* constant.
*/
void __fastcall__ x16_ser_init (unsigned int base, unsigned int divisor);

/* 1 if a received byte is waiting, else 0. Never blocks. */
unsigned char x16_ser_avail (void);

/* Read one byte without blocking: the byte, or -1 if none was waiting. */
int x16_ser_get (void);

/* Read one byte, spinning until one arrives. Hardware only. */
unsigned char x16_ser_get_wait (void);

/* Send one byte, waiting for room in the transmit FIFO. */
void __fastcall__ x16_ser_put (unsigned char b);

/* Send a NUL-terminated string / a counted (binary-safe) run of bytes.
** A len of 0 means 256.
*/
void __fastcall__ x16_ser_puts (const char *s);
void __fastcall__ x16_ser_write (const void *data, unsigned char len);

/* Read into buf until the NUL-terminated needle `match` has gone past,
** or `max` bytes are stored. The needle is included in the buffer.
** Returns the byte count actually stored. Blocks between bytes.
*/
unsigned int __fastcall__ x16_ser_read_until (char *buf, unsigned int max,
                                              const char *match);

/* Read and discard bytes until `match` has gone past. Blocks. */
void __fastcall__ x16_ser_discard_until (const char *match);

#endif /* X16_SERIAL_H */
