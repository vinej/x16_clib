; =====================================================================
; x16clib :: comms/i2c.s -- the I2C bus: SMC, RTC, and friends
; =====================================================================
; Thin wrappers over the KERNAL's I2C jump-table calls. The carry
; follows the ROM convention -- set means NAK/error -- and the shims
; fold it into the return value, since C cannot see flags.
;
; The two interesting devices are on x16/i2c.h as constants:
;       X16_I2C_SMC $42   system management controller
;       X16_I2C_RTC $6F   real-time clock, with NVRAM at offsets $20-$5F
;
; Batch calls stream through the KERNAL virtual registers: r0 = buffer,
; r1 = count. A batch WRITE's first buffer byte is the device's register
; offset -- that is the I2C wire format, not a library invention.
;
; BE CAREFUL what you write: SMC offset $01 is the power switch.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

        zpage	r0
        zpage	r2
        zpage	r3
        zpage	r4
        zpage	r5
        zpage	r6


        global	_x16_i2c_read_byte
        global	_x16_i2c_write_byte
        global	_x16_i2c_batch_read
        global	_x16_i2c_batch_write

        section text

; ---------------------------------------------------------------------
; unsigned int __fastcall__ x16_i2c_read_byte(unsigned char device,
;                                             unsigned char offset)
;   returns the byte (0-255), or 0xFFFF on NAK
; ---------------------------------------------------------------------
_x16_i2c_read_byte:
        ldx     r0                      ; device
        ldy     r2                      ; offset
        jsr     I2C_READ_BYTE           ; A = value, carry set on error
        bcs     .err
        ldx     #0
        rts
.err:
        lda     #$FF
        tax
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_i2c_write_byte(unsigned char device,
;                                               unsigned char offset,
;                                               unsigned char value)
;   returns 1 on success, 0 on NAK
; ---------------------------------------------------------------------
_x16_i2c_write_byte:
        ldx     r0                      ; device
        ldy     r2                      ; offset
        lda     r4                      ; value
        jsr     I2C_WRITE_BYTE
        lda     #0
        ldx     #0
        bcs     .fail
        lda     #1
.fail:
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_i2c_batch_read(unsigned char device,
;                                               void *buf,
;                                               unsigned int count,
;                                               unsigned char fixed)
;   returns 1 on success, 0 on error
;
; Reads `count` bytes from the device's current internal offset --
; position it first, e.g. with an x16_i2c_read_byte. `fixed` nonzero
; parks every byte at buf[0] instead of advancing through the buffer
; (the KERNAL's carry-in flag), which is how you stream into a port.
; ---------------------------------------------------------------------
_x16_i2c_batch_read:
        ldx     r0                      ; device, read before buf buries it
        lda     r6                      ; fixed
        cmp     #1                      ; carry set iff fixed != 0
        lda     r2                      ; buf and count arrived one KERNAL
        sta     r0L                     ; register high, because `device`
        lda     r3                      ; took r0. Shift both down; lda and
        sta     r0H                     ; sta leave the carry alone.
        lda     r4
        sta     r1L                     ; count
        lda     r5
        sta     r1H
        jsr     I2C_BATCH_READ
        lda     #0
        ldx     #0
        bcs     .fail
        lda     #1
.fail:
        rts

; ---------------------------------------------------------------------
; unsigned int __fastcall__ x16_i2c_batch_write(unsigned char device,
;                                               const void *buf,
;                                               unsigned int count)
;   returns the number of bytes written, or 0xFFFF on error
;
; buf[0] must be the register offset; the data follows. So writing one
; register takes a 2-byte buffer, exactly like x16_i2c_write_byte.
; ---------------------------------------------------------------------
_x16_i2c_batch_write:
        ldx     r0                      ; device, read before buf buries it
        lda     r2                      ; the same downward shift as
        sta     r0L                     ; x16_i2c_batch_read
        lda     r3
        sta     r0H
        lda     r4
        sta     r1L                     ; count
        lda     r5
        sta     r1H
        jsr     I2C_BATCH_WRITE         ; r2 = bytes written
        bcs     .err
        lda     r2L
        ldx     r2H
        rts
.err:
        lda     #$FF
        tax
        rts
