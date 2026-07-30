// =====================================================================
// x16clib :: x16/spi.c -- VERA SPI controller helpers
// =====================================================================
// VERA's SPI controller is two registers: DATA ($9F3E) and CTRL ($9F3F).
// Writing DATA starts one full-duplex transfer; BUSY (CTRL bit 7) stays
// set until the received byte can be read back from DATA. SELECT (bit 0)
// asserts chip-select.
//
// The SD card hangs off this bus and the KERNAL's DOS drives it through
// these same two registers -- do not run transfers of your own while a
// KERNAL load or save is in flight.
//
// Every register touch is in __asm rather than through a C pointer. The
// BUSY spin in particular must re-read CTRL on every pass, and a plain
// `*((char *)0x9F3F)` read is exactly the thing an optimiser is entitled
// to hoist out of the loop. The bulk loops above them are plain C: they
// only call the byte primitives, so there is nothing to hoist.
// =====================================================================

#include <x16/spi.h>

volatile unsigned char x16__sp_v;

// ---------------------------------------------------------------------
// The raw control register.
// ---------------------------------------------------------------------
unsigned char x16_spi_get_ctrl(void) {
    __asm {
        lda 0x9f3f                      // VERA_SPI_CTRL
        sta x16__sp_v
    }
    return x16__sp_v;
}

void x16_spi_set_ctrl(unsigned char bits) {
    __asm {
        lda bits
        sta 0x9f3f
    }
}

// ---------------------------------------------------------------------
// Wait for the active transfer to finish. BUSY is bit 7, so `bit` copies
// it straight into N.
// ---------------------------------------------------------------------
void x16_spi_wait(void) {
    __asm {
    sp_w:
        bit 0x9f3f
        bmi sp_w
    }
}

// ---------------------------------------------------------------------
// Chip-select, clock speed and Auto-TX: one bit each, read-modify-write.
// ---------------------------------------------------------------------
void x16_spi_select(void) {
    __asm {
        lda 0x9f3f
        ora #0x01                       // X16_SPI_SELECT
        sta 0x9f3f
    }
}

void x16_spi_deselect(void) {
    __asm {
        lda 0x9f3f
        and #0xfe
        sta 0x9f3f
    }
}

void x16_spi_slow(void) {
    __asm {
        lda 0x9f3f
        ora #0x02                       // X16_SPI_SLOWCLK
        sta 0x9f3f
    }
}

void x16_spi_fast(void) {
    __asm {
        lda 0x9f3f
        and #0xfd
        sta 0x9f3f
    }
}

void x16_spi_autotx_on(void) {
    __asm {
        lda 0x9f3f
        ora #0x04                       // X16_SPI_AUTOTX
        sta 0x9f3f
    }
}

void x16_spi_autotx_off(void) {
    __asm {
        lda 0x9f3f
        and #0xfb
        sta 0x9f3f
    }
}

// ---------------------------------------------------------------------
// One byte out, one byte back. The wait is spelled out in each of these
// rather than calling x16_spi_wait(), so a transfer is one asm block
// with no call in the middle of the handshake.
// ---------------------------------------------------------------------
unsigned char x16_spi_transfer(unsigned char out) {
    __asm {
        lda out
        sta 0x9f3e                      // writing DATA starts the transfer
    sp_x:
        bit 0x9f3f
        bmi sp_x
        lda 0x9f3e
        sta x16__sp_v
    }
    return x16__sp_v;
}

void x16_spi_write(unsigned char out) {
    __asm {
        lda out
        sta 0x9f3e
    sp_p:
        bit 0x9f3f
        bmi sp_p
    }
}

unsigned char x16_spi_read(void) {
    __asm {
        lda #0xff                       // the bus idle pattern
        sta 0x9f3e
    sp_r:
        bit 0x9f3f
        bmi sp_r
        lda 0x9f3e
        sta x16__sp_v
    }
    return x16__sp_v;
}

// ---------------------------------------------------------------------
// In Auto-TX mode the READ of DATA starts the next $FF transfer, so a
// loop of these streams back-to-back with no write in between.
// ---------------------------------------------------------------------
unsigned char x16_spi_autotx_read(void) {
    __asm {
    sp_a:
        bit 0x9f3f
        bmi sp_a
        lda 0x9f3e
        sta x16__sp_v
    }
    return x16__sp_v;
}

// ---------------------------------------------------------------------
// Bulk exchanges. A count of 0 transfers nothing.
// ---------------------------------------------------------------------
void x16_spi_read_bytes(void *dest, unsigned int count) {
    unsigned char *p = (unsigned char *)dest;

    while (count != 0) {
        *p = x16_spi_read();
        ++p;
        --count;
    }
}

void x16_spi_write_bytes(const void *src, unsigned int count) {
    const unsigned char *p = (const unsigned char *)src;

    while (count != 0) {
        x16_spi_write(*p);
        ++p;
        --count;
    }
}
