/* =====================================================================
 * x16clib :: x16/i2c.h -- the I2C bus: SMC, RTC, and friends
 * =====================================================================
 * The X16's system devices hang off one I2C bus: the SMC (power,
 * keyboard, mouse) at $42 and the RTC at $6F, whose offsets $20-$5F are
 * 64 bytes of battery-backed NVRAM -- free save-game/settings storage.
 *
 * Errors are NAKs. The byte read returns 0xFFFF for one, the writes
 * return 0, so a missing device is detectable rather than fatal.
 *
 * BE CAREFUL WHAT YOU WRITE. SMC offset $01 is the power switch: a
 * stray x16_i2c_write_byte(X16_I2C_SMC, 0x01, 0) turns the machine off.
 * =====================================================================
 */

#ifndef X16_I2C_H
#define X16_I2C_H

#include <x16/zpsafe.h>

/* 7-bit device addresses. */
#define X16_I2C_SMC     0x42
#define X16_I2C_RTC     0x6F

/* The first NVRAM offset in the RTC, and how many bytes follow. */
#define X16_I2C_RTC_NVRAM       0x20
#define X16_I2C_RTC_NVRAM_LEN   0x40

/* One byte from a device register. Returns 0-255, or 0xFFFF on NAK. */
unsigned int x16_i2c_read_byte (unsigned char device,
                                             unsigned char offset);

/* One byte to a device register. Returns 1 on success, 0 on NAK. */
unsigned char x16_i2c_write_byte (unsigned char device,
                                               unsigned char offset,
                                               unsigned char value);

/* Read `count` bytes from the device's CURRENT internal offset --
** position it first, e.g. with an x16_i2c_read_byte of the offset
** before the ones you want. `fixed` 0 fills the buffer normally;
** nonzero parks every byte at buf[0] (stream into a port). Returns 1
** on success, 0 on error.
*/
unsigned char x16_i2c_batch_read (unsigned char device,
                                               void *buf,
                                               unsigned int count,
                                               unsigned char fixed);

/* Write `count` bytes in one transaction. buf[0] is the register
** offset, the data follows -- the I2C wire format. Returns the number
** of bytes written, or 0xFFFF on error.
*/
unsigned int x16_i2c_batch_write (unsigned char device,
                                               const void *buf,
                                               unsigned int count);

#endif /* X16_I2C_H */
