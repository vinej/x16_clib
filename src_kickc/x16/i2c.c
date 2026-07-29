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

__mem volatile unsigned char x16__i2_ok;

// ---------------------------------------------------------------------
// One byte from `device` at `offset`. Returns the byte, or 0xFFFF if
// the device did not acknowledge.
// ---------------------------------------------------------------------
unsigned int x16_i2c_read_byte(__mem unsigned char device,
                               __mem unsigned char offset) {
    __mem unsigned char v;
    asm {
        ldx device
        ldy offset
        jsr $fec6 /*I2C_READ_BYTE*/     // A = value, carry set on error
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
unsigned char x16_i2c_write_byte(__mem unsigned char device,
                                 __mem unsigned char offset,
                                 __mem unsigned char value) {
    asm {
        ldx device
        ldy offset
        lda value
        jsr $fec9 /*I2C_WRITE_BYTE*/
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
unsigned char x16_i2c_batch_read(__mem unsigned char device,
                                 void *buf,
                                 __mem unsigned int count,
                                 __mem unsigned char fixed) {
    asm {
        lda buf
        sta $02 /*r0L*/
        lda buf+1
        sta $03 /*r0H*/
        lda count
        sta $04 /*r1L*/
        lda count+1
        sta $05 /*r1H*/
        ldx device
        lda fixed
        cmp #1                          // carry set iff fixed != 0
        jsr $feb4 /*I2C_BATCH_READ*/
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
unsigned int x16_i2c_batch_write(__mem unsigned char device,
                                 const void *buf,
                                 __mem unsigned int count) {
    __mem unsigned char lo;
    __mem unsigned char hi;
    asm {
        lda buf
        sta $02 /*r0L*/
        lda buf+1
        sta $03 /*r0H*/
        lda count
        sta $04 /*r1L*/
        lda count+1
        sta $05 /*r1H*/
        ldx device
        jsr $feb7 /*I2C_BATCH_WRITE*/   // r2 = bytes written
        lda $06 /*r2L*/
        sta lo
        lda $07 /*r2H*/
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
