/* =====================================================================
 * x16clib :: x16/zimodem.h -- ZiModem (ESP32 WiFi) over the serial card
 * =====================================================================
 * The WiFi half of the serial card is an ESP32 running ZiModem firmware,
 * driven as a Hayes-style modem: "AT..." command lines out, "OK\r\n"
 * back. This layer frames the commands and matches the replies over
 * <x16/serial.h>'s primitives; it is not the ESP32 firmware.
 *
 *      x16_zi_init(x16_ser_uart0(), X16_SER_BAUD_115200);
 *      x16_zi_cmd("atw\"myssid,password\"");    // join a network
 *      x16_zi_wait_ok();
 *      x16_zi_get_ip(ip);                       // "192.168.1.23"
 *
 * zi_init leaves the same UART selected that x16_ser_init() did, so the
 * x16_ser_* calls keep working alongside these -- stream data with them
 * once "atd" has a connection up.
 *
 * EVERY REPLY-READING CALL BLOCKS with no timeout, spinning on the UART
 * until the modem's answer arrives (only x16_zi_cmd, x16_zi_hexdecode
 * and x16_zi_delay never read). They are for a board that is really
 * there: x16_ser_detect() first. Command strings are sent as-is plus
 * CR/LF -- ZiModem is case-insensitive, but send 7-bit ASCII.
 * =====================================================================
 */

#ifndef X16_ZIMODEM_H
#define X16_ZIMODEM_H

/* Program the UART (as x16_ser_init does), wake the ESP32, abort any
** stream left running, apply the standard config (echo off, verbose
** result codes, stream mode) and wait for its "OK". Takes a couple of
** hundred ms in settle delays. Blocks.
*/
void x16_zi_init (unsigned int uart, unsigned int divisor);

/* ATZ: return the modem to its saved profile. Blocks. */
void x16_zi_reset (void);

/* Send one AT command line, CR/LF appended -- pure transmit, so follow
** with x16_zi_wait_ok() (or your own read) to consume the reply.
*/
void x16_zi_cmd (const char *cmd);

/* Read and discard the reply up to and including "OK\r\n". Blocks. */
void x16_zi_wait_ok (void);

/* Fetch the current IPv4 address into buf (>= 25 bytes) as a
** NUL-terminated dotted-quad. Blocks.
*/
void x16_zi_get_ip (char *buf);

/* Hex-mode file download: open a filename/URL, pull chunks until one
** comes back empty, close.
**
**      if (x16_zi_hex_open(url) == 0) {
**          while ((n = x16_zi_hex_chunk(buf)) != 0)
**              use(buf, n);
**          x16_zi_hex_close();
**      }
**
** open returns 0 when the transfer started, 1 if the file was not
** found. chunk decodes one line -- up to 44 raw bytes -- into buf and
** returns the count, 0 when the file is done. All three block.
*/
unsigned char x16_zi_hex_open (const char *name);
unsigned char x16_zi_hex_chunk (unsigned char *buf);
void x16_zi_hex_close (void);

/* The hex-mode payload decoder: `ndigits` (even) uppercase ASCII hex
** digits from src pack into ndigits/2 bytes at dest. Returns the byte
** count. Pure computation -- never touches the UART.
*/
unsigned char x16_zi_hexdecode (unsigned char *dest,
                                             const char *src,
                                             unsigned char ndigits);

/* A coarse busy-wait, ~40 ms per tick at 8 MHz -- self-contained, so it
** needs neither the jiffy IRQ nor the KERNAL.
*/
void x16_zi_delay (unsigned char ticks);

#endif /* X16_ZIMODEM_H */
