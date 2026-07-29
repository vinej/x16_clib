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

__mem volatile unsigned char x16__ie_v;
__mem volatile unsigned char x16__ie_lo;
__mem volatile unsigned char x16__ie_hi;

// ---------------------------------------------------------------------
// Address a device for input or output. Everything after this goes to
// (or comes from) that device until an unlisten/untalk.
// ---------------------------------------------------------------------
void x16_iec_listen(unsigned char device) {
    asm {
        lda device
        jsr $ffb1 /*LISTEN*/
    }
}

void x16_iec_talk(unsigned char device) {
    asm {
        lda device
        jsr $ffb4 /*TALK*/
    }
}

// ---------------------------------------------------------------------
// The secondary address, after a listen (SECOND) or a talk (TKSA).
// ---------------------------------------------------------------------
void x16_iec_second(unsigned char cmd) {
    asm {
        lda cmd
        jsr $ff93 /*SECOND*/
    }
}

void x16_iec_tksa(unsigned char cmd) {
    asm {
        lda cmd
        jsr $ff96 /*TKSA*/
    }
}

// ---------------------------------------------------------------------
// One byte out to a listening device, or one byte in from a talker.
// ---------------------------------------------------------------------
void x16_iec_ciout(unsigned char b) {
    asm {
        lda b
        jsr $ffa8 /*CIOUT*/
    }
}

unsigned char x16_iec_acptr(void) {
    asm {
        jsr $ffa5 /*ACPTR*/
        sta x16__ie_v
    }
    return x16__ie_v;
}

// ---------------------------------------------------------------------
// Release the bus.
// ---------------------------------------------------------------------
void x16_iec_unlisten(void) {
    asm {
        jsr $ffae /*UNLSN*/
    }
}

void x16_iec_untalk(void) {
    asm {
        jsr $ffab /*UNTLK*/
    }
}

// ---------------------------------------------------------------------
// The serial timeout flag, and the KERNAL status byte.
// ---------------------------------------------------------------------
void x16_iec_set_timeout(unsigned char t) {
    asm {
        lda t
        jsr $ffa2 /*SETTMO*/
    }
}

unsigned char x16_iec_readst(void) {
    asm {
        jsr $ffb7 /*READST*/
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
    asm {
        lda count
        ldx dest                        // MACPTR wants the buffer in X/Y
        ldy dest+1
        clc
        jsr $ff44 /*MACPTR*/            // out: X = low, Y = high, C = error
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
    asm {
        lda count
        ldx src
        ldy src+1
        clc                             // advance, as in macptr above
        jsr $feb1 /*MCIOUT*/
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
    asm {
        lda device
        jsr $ffb1 /*LISTEN*/
        lda secondary
        ora #$f0                        // IEC_CMD_OPEN
        jsr $ff93 /*SECOND*/
    }
}

void x16_iec_data_channel(unsigned char device, unsigned char secondary) {
    asm {
        lda device
        jsr $ffb1 /*LISTEN*/
        lda secondary
        ora #$60                        // IEC_CMD_DATA
        jsr $ff93 /*SECOND*/
    }
}

void x16_iec_close_channel(unsigned char device, unsigned char secondary) {
    asm {
        lda device
        jsr $ffb1 /*LISTEN*/
        lda secondary
        ora #$e0                        // IEC_CMD_CLOSE
        jsr $ff93 /*SECOND*/
    }
}

void x16_iec_talk_channel(unsigned char device, unsigned char secondary) {
    asm {
        lda device
        jsr $ffb4 /*TALK*/
        lda secondary
        ora #$60                        // IEC_CMD_DATA
        jsr $ff96 /*TKSA*/
    }
}
