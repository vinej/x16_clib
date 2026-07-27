/* =====================================================================
 * x16clib :: x16/spi.h -- VERA SPI controller helpers
 * =====================================================================
 * VERA exposes an SPI master at two registers: writing DATA starts one
 * full-duplex byte transfer, BUSY stays set until the received byte can
 * be read back, and SELECT asserts chip-select. These are the raw
 * primitives -- select, clock speed, byte exchange -- for talking to
 * whatever hangs off that bus.
 *
 * What hangs off it on a stock X16 is the SD CARD, and the KERNAL's DOS
 * drives it through these same registers. Do not run transfers of your
 * own while a load or save is in flight, and leave the bus deselected
 * (x16_spi_deselect()) when you are done, or the next directory listing
 * will find the card mid-conversation.
 *
 * The usual SD dance: x16_spi_slow() + 80 clocks of x16_spi_read() with
 * the card DESELECTED, then select, command, response -- but that
 * protocol is the card's, not VERA's, and this header stops at bytes.
 *
 * Auto-TX is VERA's bulk-read accelerator: while it is on, every read of
 * DATA starts the next $FF transfer by itself. Turn it on, read DATA
 * once to prime, then x16_spi_autotx_read() streams at full clock.
 * =====================================================================
 */

#ifndef X16_SPI_H
#define X16_SPI_H

/* SPI_CTRL bits, as x16_spi_get_ctrl() returns them. BUSY is read-only:
** while it is set the transfer is still shifting.
*/
#define X16_SPI_SELECT   0x01   /* 1 asserts chip-select, 0 releases it */
#define X16_SPI_SLOWCLK  0x02   /* 1 = ~390 kHz, 0 = ~12.5 MHz */
#define X16_SPI_AUTOTX   0x04   /* reading DATA starts a $FF transfer */
#define X16_SPI_BUSY     0x80   /* read-only */

/* The raw control register, for saving/restoring around your own use. */
unsigned char x16_spi_get_ctrl (void);
void __fastcall__ x16_spi_set_ctrl (unsigned char bits);

/* Block until the active transfer finishes (BUSY clears). The byte
** routines below already wait; this is for hand-rolled sequences.
*/
void x16_spi_wait (void);

/* Chip-select, clock speed and Auto-TX, one bit each. */
void x16_spi_select (void);
void x16_spi_deselect (void);
void x16_spi_slow (void);        /* ~390 kHz: SD card initialisation */
void x16_spi_fast (void);        /* ~12.5 MHz */
void x16_spi_autotx_on (void);
void x16_spi_autotx_off (void);

/* Transmit `out`, wait, and return the byte that came back. */
unsigned char __fastcall__ x16_spi_transfer (unsigned char out);

/* Transmit `out` and wait; the received byte is discarded. */
void __fastcall__ x16_spi_write (unsigned char out);

/* Transmit $FF (the bus idle pattern), wait, return the received byte. */
unsigned char x16_spi_read (void);

/* Wait, then read DATA in Auto-TX mode -- the read itself starts the
** next $FF transfer, so a loop of these streams back-to-back.
*/
unsigned char x16_spi_autotx_read (void);

/* Bulk exchanges: `count` bytes into or out of RAM, one transfer each.
** Reads transmit $FF for every byte, as SD cards expect.
*/
void __fastcall__ x16_spi_read_bytes (void *dest, unsigned int count);
void __fastcall__ x16_spi_write_bytes (const void *src, unsigned int count);

#endif /* X16_SPI_H */
