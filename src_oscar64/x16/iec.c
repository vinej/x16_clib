// =====================================================================
// x16clib :: x16/iec.c -- raw IEC bus control
// =====================================================================
// The bus underneath the file API: LISTEN/TALK a device, open a
// secondary channel, push or pull bytes one at a time, and the two block
// movers (MACPTR/MCIOUT) that hand a whole buffer to the drive in one
// call.
//
// Most programs want x16/fileio.h or x16/load.h instead. Reach for this
// when you need the protocol itself -- a DOS command channel, a
// fastloader handshake, or a device that is not a disk.
//
// The channel helpers spell out the command bases the DOS expects:
// $60 for a data channel, $E0 to close, $F0 to open.
// =====================================================================

#include <x16/iec.h>

volatile unsigned char x16__ie_v;
volatile unsigned char x16__ie_lo;
volatile unsigned char x16__ie_hi;

// ---------------------------------------------------------------------
// Address a device for input or output. Everything after this goes to
// (or comes from) that device until an unlisten/untalk.
// ---------------------------------------------------------------------
void x16_iec_listen(unsigned char device) {
    __asm {
        lda device
        jsr 0xffb1                      // LISTEN
    }
}

void x16_iec_talk(unsigned char device) {
    __asm {
        lda device
        jsr 0xffb4                      // TALK
    }
}

// ---------------------------------------------------------------------
// The secondary address, after a listen (SECOND) or a talk (TKSA).
// ---------------------------------------------------------------------
void x16_iec_second(unsigned char cmd) {
    __asm {
        lda cmd
        jsr 0xff93                      // SECOND
    }
}

void x16_iec_tksa(unsigned char cmd) {
    __asm {
        lda cmd
        jsr 0xff96                      // TKSA
    }
}

// ---------------------------------------------------------------------
// One byte out to a listening device, or one byte in from a talker.
// ---------------------------------------------------------------------
void x16_iec_ciout(unsigned char b) {
    __asm {
        lda b
        jsr 0xffa8                      // CIOUT
    }
}

unsigned char x16_iec_acptr(void) {
    __asm {
        jsr 0xffa5                      // ACPTR
        sta x16__ie_v
    }
    return x16__ie_v;
}

// ---------------------------------------------------------------------
// Release the bus.
// ---------------------------------------------------------------------
void x16_iec_unlisten(void) {
    __asm {
        jsr 0xffae                      // UNLSN
    }
}

void x16_iec_untalk(void) {
    __asm {
        jsr 0xffab                      // UNTLK
    }
}

// ---------------------------------------------------------------------
// The serial timeout flag, and the KERNAL status byte.
// ---------------------------------------------------------------------
void x16_iec_set_timeout(unsigned char t) {
    __asm {
        lda t
        jsr 0xffa2                      // SETTMO
    }
}

unsigned char x16_iec_readst(void) {
    __asm {
        jsr 0xffb7                      // READST
        sta x16__ie_v
    }
    return x16__ie_v;
}

// ---------------------------------------------------------------------
// Pull `count` bytes from a talking device into `dest`, in one call.
// Returns the number actually transferred, or -1 if the device does not
// implement MACPTR.
//
// Carry IN must be CLEAR: that means "advance the pointer". Set, it
// pins the address for port I/O -- and it arrives here as whatever the
// caller left behind, so it is cleared explicitly.
// ---------------------------------------------------------------------
int x16_iec_macptr(unsigned char count, void *dest) {
    __asm {
        lda count
        ldx dest                        // MACPTR wants the buffer in X/Y
        ldy dest+1
        clc
        jsr 0xff44            // out: X = low, Y = high, C = error (MACPTR)
        bcs ie_mac_bad
        stx x16__ie_lo
        sty x16__ie_hi
        lda #0
        sta x16__ie_v
        jmp ie_mac_done
    ie_mac_bad:
        lda #1
        sta x16__ie_v
    ie_mac_done:
    }
    if (x16__ie_v != 0) {
        return -1;
    }
    return (int)((unsigned int)x16__ie_lo | ((unsigned int)x16__ie_hi << 8));
}

// ---------------------------------------------------------------------
// Push `count` bytes from `src` to a listening device. Returns the
// number sent, or -1 if the device does not implement MCIOUT.
// ---------------------------------------------------------------------
int x16_iec_mciout(unsigned char count, const void *src) {
    __asm {
        lda count
        ldx src
        ldy src+1
        clc                             // advance, as in macptr above
        jsr 0xfeb1                      // MCIOUT
        bcs ie_mci_bad
        stx x16__ie_lo
        sty x16__ie_hi
        lda #0
        sta x16__ie_v
        jmp ie_mci_done
    ie_mci_bad:
        lda #1
        sta x16__ie_v
    ie_mci_done:
    }
    if (x16__ie_v != 0) {
        return -1;
    }
    return (int)((unsigned int)x16__ie_lo | ((unsigned int)x16__ie_hi << 8));
}

// ---------------------------------------------------------------------
// The four channel conveniences: LISTEN or TALK the device, then send
// the secondary with the DOS command base already added.
// ---------------------------------------------------------------------
void x16_iec_open_channel(unsigned char device, unsigned char secondary) {
    __asm {
        lda device
        jsr 0xffb1                      // LISTEN
        lda secondary
        ora #0xf0                        // IEC_CMD_OPEN
        jsr 0xff93                      // SECOND
    }
}

void x16_iec_data_channel(unsigned char device, unsigned char secondary) {
    __asm {
        lda device
        jsr 0xffb1                      // LISTEN
        lda secondary
        ora #0x60                        // IEC_CMD_DATA
        jsr 0xff93                      // SECOND
    }
}

void x16_iec_close_channel(unsigned char device, unsigned char secondary) {
    __asm {
        lda device
        jsr 0xffb1                      // LISTEN
        lda secondary
        ora #0xe0                        // IEC_CMD_CLOSE
        jsr 0xff93                      // SECOND
    }
}

void x16_iec_talk_channel(unsigned char device, unsigned char secondary) {
    __asm {
        lda device
        jsr 0xffb4                      // TALK
        lda secondary
        ora #0x60                        // IEC_CMD_DATA
        jsr 0xff96                      // TKSA
    }
}
