// =====================================================================
// x16clib :: x16/fileio.c -- the KERNAL channel verbs
// =====================================================================
// SETLFS/SETNAM/OPEN/CHKIN/CHKOUT/CHRIN/CHROUT/CLOSE, plus the
// named-open conveniences that do all of it in one call.
//
// The channel API is the general one: it reads and writes a byte at a
// time, works with any device, and is what you want for a SEQ file or a
// command channel. For loading a whole file at once use x16/load.h --
// it is far faster.
//
// A logical file number 0-127 is a normal file; 128-255 asks the KERNAL
// for its own buffer handling. Secondary 0 reads, 1 writes, 2-14 is a
// channel the DOS interprets, 15 is the command channel.
// =====================================================================

#include <x16/fileio.h>

__mem volatile unsigned char x16__fi_v;

// The five open_* arguments, staged where the shared helper can see them
// (the cc65 build used the library's X16_P block for exactly this).
__mem volatile unsigned char x16__fi_nlo;
__mem volatile unsigned char x16__fi_nhi;
__mem volatile unsigned char x16__fi_len;
__mem volatile unsigned char x16__fi_lfn;
__mem volatile unsigned char x16__fi_dev;
__mem volatile unsigned char x16__fi_sec;

// ---------------------------------------------------------------------
// SETLFS: the logical file, its device, and the secondary address.
// ---------------------------------------------------------------------
void x16_fio_set_lfs(unsigned char lfn, unsigned char device,
                     unsigned char secondary) {
    asm {
        lda lfn
        ldx device
        ldy secondary
        jsr $ffba /*SETLFS*/
    }
}

// ---------------------------------------------------------------------
// SETNAM: the filename for the next OPEN. A length of 0 means no name.
// ---------------------------------------------------------------------
void x16_fio_set_name(const char *name, unsigned char len) {
    asm {
        lda len
        ldx name                        // SETNAM wants X = low, Y = high
        ldy name+1
        jsr $ffbd /*SETNAM*/
    }
}

// ---------------------------------------------------------------------
// OPEN, on whatever set_lfs/set_name left behind. 0 on success, else
// the KERNAL error code.
// ---------------------------------------------------------------------
unsigned char x16_fio_open(void) {
    asm {
        jsr $ffc0 /*OPEN*/
        bcs fi_open_err
        lda #0
    fi_open_err:
        sta x16__fi_v
    }
    return x16__fi_v;
}

void x16_fio_close(unsigned char lfn) {
    asm {
        lda lfn
        jsr $ffc3 /*CLOSE*/
    }
}

// ---------------------------------------------------------------------
// Select a logical file for input / output. 0 on success.
// ---------------------------------------------------------------------
unsigned char x16_fio_chkin(unsigned char lfn) {
    asm {
        ldx lfn
        jsr $ffc6 /*CHKIN*/
        bcs fi_chkin_err
        lda #0
    fi_chkin_err:
        sta x16__fi_v
    }
    return x16__fi_v;
}

unsigned char x16_fio_chkout(unsigned char lfn) {
    asm {
        ldx lfn
        jsr $ffc9 /*CHKOUT*/
        bcs fi_chkout_err
        lda #0
    fi_chkout_err:
        sta x16__fi_v
    }
    return x16__fi_v;
}

// ---------------------------------------------------------------------
// Put the default channels back (screen out, keyboard in).
// ---------------------------------------------------------------------
void x16_fio_clrchn(void) {
    asm {
        jsr $ffcc /*CLRCHN*/
    }
}

// ---------------------------------------------------------------------
// One byte in / out on the selected channel.
// ---------------------------------------------------------------------
unsigned char x16_fio_chrin(void) {
    asm {
        jsr $ffcf /*CHRIN*/
        sta x16__fi_v
    }
    return x16__fi_v;
}

void x16_fio_chrout(unsigned char b) {
    asm {
        lda b
        jsr $ffd2 /*CHROUT*/
    }
}

// ---------------------------------------------------------------------
// The KERNAL status byte: bit 6 is EOF, bit 7 a device-not-present.
// Reading it CLEARS it, so read it once and keep the answer.
// ---------------------------------------------------------------------
unsigned char x16_fio_readst(void) {
    asm {
        jsr $ffb7 /*READST*/
        sta x16__fi_v
    }
    return x16__fi_v;
}

// ---------------------------------------------------------------------
// GETIN: one byte from the keyboard queue, or 0 if nothing waits.
// ---------------------------------------------------------------------
unsigned char x16_fio_getin(void) {
    asm {
        jsr $ffe4 /*GETIN*/
        sta x16__fi_v
    }
    return x16__fi_v;
}

// ---------------------------------------------------------------------
// Close every open file, or every file on one device.
// ---------------------------------------------------------------------
void x16_fio_close_all(void) {
    asm {
        jsr $ffe7 /*CLALL*/
    }
}

void x16_fio_close_device(unsigned char device) {
    asm {
        lda device
        jsr $ff4a /*CLOSE_ALL*/
    }
}

// ---------------------------------------------------------------------
// SETNAM + SETLFS + OPEN in one call, from the staged arguments.
// ---------------------------------------------------------------------
void x16__fi_setup(void) {
    asm {
        lda x16__fi_len
        ldx x16__fi_nlo
        ldy x16__fi_nhi
        jsr $ffbd /*SETNAM*/
        lda x16__fi_lfn
        ldx x16__fi_dev
        ldy x16__fi_sec
        jsr $ffba /*SETLFS*/
        jsr $ffc0 /*OPEN*/
        bcs fi_setup_err
        lda #0
    fi_setup_err:
        sta x16__fi_v
    }
}

void x16__fi_stage(const char *name, unsigned char len, unsigned char lfn,
                   unsigned char device, unsigned char secondary) {
    x16__fi_nlo = (unsigned char)((unsigned int)name & 0xFF);
    x16__fi_nhi = (unsigned char)((unsigned int)name >> 8);
    x16__fi_len = len;
    x16__fi_lfn = lfn;
    x16__fi_dev = device;
    x16__fi_sec = secondary;
}

unsigned char x16_fio_open_named(const char *name, unsigned char len,
                                 unsigned char lfn, unsigned char device,
                                 unsigned char secondary) {
    x16__fi_stage(name, len, lfn, device, secondary);
    x16__fi_setup();
    return x16__fi_v;
}

// ---------------------------------------------------------------------
// ...and then select the file for input / output, so the very next
// chrin/chrout talks to it.
// ---------------------------------------------------------------------
unsigned char x16_fio_open_read(const char *name, unsigned char len,
                                unsigned char lfn, unsigned char device,
                                unsigned char secondary) {
    x16__fi_stage(name, len, lfn, device, secondary);
    x16__fi_setup();
    if (x16__fi_v != 0) {
        return x16__fi_v;
    }
    return x16_fio_chkin(lfn);
}

unsigned char x16_fio_open_write(const char *name, unsigned char len,
                                 unsigned char lfn, unsigned char device,
                                 unsigned char secondary) {
    x16__fi_stage(name, len, lfn, device, secondary);
    x16__fi_setup();
    if (x16__fi_v != 0) {
        return x16__fi_v;
    }
    return x16_fio_chkout(lfn);
}

// ---------------------------------------------------------------------
// CLRCHN then CLOSE: the pair a named open needs on the way out.
// ---------------------------------------------------------------------
void x16_fio_close_named(unsigned char lfn) {
    asm {
        jsr $ffcc /*CLRCHN*/
        lda lfn
        jsr $ffc3 /*CLOSE*/
    }
}
