// =====================================================================
// x16clib :: x16/i2c.c -- the I2C bus
// =====================================================================
// The X16's I2C bus carries the System Management Controller (device
// $42), the real-time clock (device $6F) and its 32 bytes of
// battery-backed NVRAM. The KERNAL owns the bit-banging; these are
// wrappers over its four entry points.
//
// Reads answer $FFFF on a bus error rather than a plausible byte, so a
// caller can tell "the device said 0" from "the device did not answer".
// =====================================================================

#include <x16/i2c.h>

// Oscar64 loses an asm write to a C local, even a volatile one, so
// anything the assembly stores to lives at module scope.
volatile unsigned char v;
volatile unsigned char hi;
volatile unsigned char lo;

volatile unsigned char x16__i2_ok;

// ---------------------------------------------------------------------
// One byte from `device` at `offset`. Returns the byte, or 0xFFFF if
// the device did not acknowledge.
// ---------------------------------------------------------------------
unsigned int x16_i2c_read_byte(unsigned char device,
                               unsigned char offset) {    __asm {
        ldx device
        ldy offset
        jsr 0xfec6     // A = value, carry set on error (I2C_READ_BYTE)
        sta v
        lda #0
        rol                             // carry -> bit 0
        sta x16__i2_ok                  // 1 = the read FAILED
    }
    if (x16__i2_ok != 0) {
        return 0xFFFF;
    }
    return (unsigned int)v;
}

// ---------------------------------------------------------------------
// One byte to `device` at `offset`. Returns 1 on success, 0 on NAK.
// ---------------------------------------------------------------------
unsigned char x16_i2c_write_byte(unsigned char device,
                                 unsigned char offset,
                                 unsigned char value) {
    __asm {
        ldx device
        ldy offset
        lda value
        jsr 0xfec9                      // I2C_WRITE_BYTE
        lda #0
        rol                             // carry -> bit 0
        eor #1                          // ...inverted: 1 = success
        sta x16__i2_ok
    }
    return x16__i2_ok;
}

// ---------------------------------------------------------------------
// Read `count` bytes into `buf`. fixed != 0 re-reads one register
// instead of walking consecutive offsets. Returns 1 on success.
// ---------------------------------------------------------------------
unsigned char x16_i2c_batch_read(unsigned char device,
                                 void *buf,
                                 unsigned int count,
                                 unsigned char fixed) {
    __asm {
        lda buf
        sta 0x02                        // r0L
        lda buf+1
        sta 0x03                        // r0H
        lda count
        sta 0x04                        // r1L
        lda count+1
        sta 0x05                        // r1H
        ldx device
        lda fixed
        cmp #1                          // carry set iff fixed != 0
        jsr 0xfeb4                      // I2C_BATCH_READ
        lda #0
        rol
        eor #1                          // 1 = success
        sta x16__i2_ok
    }
    return x16__i2_ok;
}

// ---------------------------------------------------------------------
// Write `count` bytes from `buf`. Returns the number actually written,
// or 0xFFFF on a bus error.
// ---------------------------------------------------------------------
unsigned int x16_i2c_batch_write(unsigned char device,
                                 const void *buf,
                                 unsigned int count) {    __asm {
        lda buf
        sta 0x02                        // r0L
        lda buf+1
        sta 0x03                        // r0H
        lda count
        sta 0x04                        // r1L
        lda count+1
        sta 0x05                        // r1H
        ldx device
        jsr 0xfeb7   // r2 = bytes written (I2C_BATCH_WRITE)
        lda 0x06                        // r2L
        sta lo
        lda 0x07                        // r2H
        sta hi
        lda #0
        rol                             // carry -> bit 0: 1 = FAILED
        sta x16__i2_ok
    }
    if (x16__i2_ok != 0) {
        return 0xFFFF;
    }
    return (unsigned int)lo | ((unsigned int)hi << 8);
}
