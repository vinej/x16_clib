// =====================================================================
// x16clib :: x16/ym.c -- YM2151 FM synthesiser
// =====================================================================
// The chip is at YM_REG ($9F40) and YM_DATA ($9F41). Note: NOT $9FE0.
//
// ---------------------------------------------------------------------
// ***  THE CHANNEL GOES IN .A, NOT .X.  ***
//
// Every ROM-driver entry below takes the FM channel (0-7) in .A and its
// payload in .X. That is the opposite of the register-level ym_write,
// and the opposite of what you would guess. Get it backwards and the
// chip plays a valid-looking note on the wrong channel: it fails
// silently, not loudly.
//
// The ROM driver lives in BANK_AUDIO ($0A) at $C000+, reached through
// the KERNAL's JSRFAR ($FF6E), whose calling convention is inline DATA
// after the jsr: a 16-bit entry address, then the bank byte. Oscar64's
// asm blocks express data with `byt`, so each jsrfar is
//
//      jsr 0xff6e
//      byt <entry, >entry, bank
//
// JSRFAR restores the callee's processor status on the way out, so the
// carry survives in BOTH directions: it is passed in (the patch-table
// select, the retrigger flag) and read back out as an error. Loads
// (lda/ldx/ldy) do not touch the carry, which is why the sequences
// below may set it before loading the registers.
// =====================================================================

#include <x16/ym.h>

// ---------------------------------------------------------------------
// Raw register write. The busy flag must be clear before touching
// YM_REG, and the chip needs settling time between the register select
// and the data write. Wrapped in sei so an interrupt cannot land
// between the two halves and leave a half-issued write behind.
// Returns 1 on success, 0 if the chip stayed busy (128 spins).
// ---------------------------------------------------------------------
unsigned char x16_ym_write(unsigned char reg, unsigned char value) {
    return __asm {
        php
        sei
        ldy #128                        /* YM_TIMEOUT */
    yw_wait:
        dey
        bmi yw_timeout
        bit 0x9f41                      /* YM_DATA */
        bmi yw_wait                     /* busy */
        ldx reg
        stx 0x9f40                      /* YM_REG */
        nop                             /* settling time between select */
        nop                             /* and data */
        nop
        lda value
        sta 0x9f41                      /* YM_DATA */
        plp
        lda #1
        sta accu
        jmp yw_done
    yw_timeout:
        plp
        lda #0
        sta accu
    yw_done:
    };
}

unsigned char x16_ym_busy(void) {
    return __asm {
        lda 0x9f41                      /* YM_DATA */
        asl                             /* bit 7 into carry */
        lda #0
        rol
        sta accu                           /* busy is not an error */
    };
}

// Reset the chip and load the default patch set. The carry that
// matters is the LAST call's -- rts does not touch flags.
unsigned char x16_ym_init(void) {
    return __asm {
        jsr yi_t1
        jsr yi_t2
        lda #0
        rol                             /* carry set = failure */
        eor #1
        sta accu
        jmp yi_done
    yi_t1:
        jsr 0xff6e                      /* JSRFAR */
        byt 0x9f, 0xc0, 0x0a             /* rom_audio_init, BANK_AUDIO */
        rts
    yi_t2:
        jsr 0xff6e                      /* JSRFAR */
        byt 0x66, 0xc0, 0x0a             /* rom_ym_loaddefpatches */
        rts
    yi_done:
    };
}

// Through the ROM driver: A = value, X = register (its convention).
void x16_ym_poke(unsigned char reg, unsigned char value) {
    __asm {
        ldx reg
        lda value
        jsr 0xff6e                      /* JSRFAR */
        byt 0x8a, 0xc0, 0x0a             /* rom_ym_write, BANK_AUDIO */
    }
}

// Carry SET selects the ROM patch table.
unsigned char x16_ym_patch_rom(unsigned char channel, unsigned char patch) {
    return __asm {
        sec                             /* ROM patch table */
        ldx patch
        lda channel
        jsr 0xff6e                      /* JSRFAR */
        byt 0x69, 0xc0, 0x0a             /* rom_ym_loadpatch, BANK_AUDIO */
        lda #0
        rol
        eor #1
        sta accu
    };
}

// Carry CLEAR selects a patch in RAM, addressed by X/Y.
unsigned char x16_ym_patch_ram(unsigned char channel, const void *patch) {
    return __asm {
        clc                             /* patch in RAM */
        ldx patch
        ldy patch+1
        lda channel
        jsr 0xff6e                      /* JSRFAR */
        byt 0x69, 0xc0, 0x0a             /* rom_ym_loadpatch, BANK_AUDIO */
        lda #0
        rol
        eor #1
        sta accu
    };
}

// Raw key code and key fraction. retrigger != 0 restarts the envelope
// (carry CLEAR to the ROM); 0 only changes the pitch.
void x16_ym_note(unsigned char channel, unsigned char kc,
                 unsigned char kf, unsigned char retrigger) {
    __asm {
        lda retrigger
        beq yn_hold
        clc                             /* retrigger the envelope */
        jmp yn_call
    yn_hold:
        sec                             /* change pitch only */
    yn_call:
        ldx kc
        ldy kf
        lda channel
        jsr 0xff6e                      /* JSRFAR */
        byt 0x72, 0xc0, 0x0a             /* rom_ym_playnote, BANK_AUDIO */
    }
}

unsigned char x16_ym_note_bas(unsigned char channel, unsigned char note,
                              unsigned char retrigger) {
    return __asm {
        lda retrigger
        beq yb_hold
        clc
        jmp yb_call
    yb_hold:
        sec
    yb_call:
        ldx note
        lda channel
        jsr 0xff6e                      /* JSRFAR */
        byt 0x03, 0xc0, 0x0a             /* rom_bas_fmnote, BANK_AUDIO */
        lda #0
        rol                             /* carry set = failure */
        eor #1
        sta accu
    };
}

void x16_ym_release_note(unsigned char channel) {
    __asm {
        lda channel
        jsr 0xff6e                      /* JSRFAR */
        byt 0x84, 0xc0, 0x0a             /* rom_ym_release, BANK_AUDIO */
    }
}

unsigned char x16_ym_vol(unsigned char channel, unsigned char atten) {
    return __asm {
        ldx atten
        lda channel
        jsr 0xff6e                      /* JSRFAR */
        byt 0x75, 0xc0, 0x0a             /* rom_ym_setatten, BANK_AUDIO */
        lda #0
        rol
        eor #1
        sta accu
    };
}

unsigned char x16_ym_pan(unsigned char channel, unsigned char pan) {
    return __asm {
        ldx pan
        lda channel
        jsr 0xff6e                      /* JSRFAR */
        byt 0x7e, 0xc0, 0x0a             /* rom_ym_setpan, BANK_AUDIO */
        lda #0
        rol
        eor #1
        sta accu
    };
}

unsigned char x16_ym_drum(unsigned char channel, unsigned char note) {
    return __asm {
        ldx note
        lda channel
        jsr 0xff6e                      /* JSRFAR */
        byt 0x6f, 0xc0, 0x0a             /* rom_ym_playdrum, BANK_AUDIO */
        lda #0
        rol
        eor #1
        sta accu
    };
}

// The ROM driver returns its shadow in X.
unsigned char x16_ym_get_pan(unsigned char channel) {
    return __asm {
        lda channel
        jsr 0xff6e                      /* JSRFAR */
        byt 0x9c, 0xc0, 0x0a             /* rom_ym_getpan, BANK_AUDIO */
        stx accu
    };
}

unsigned char x16_ym_get_vol(unsigned char channel) {
    return __asm {
        lda channel
        jsr 0xff6e                      /* JSRFAR */
        byt 0x99, 0xc0, 0x0a             /* rom_ym_getatten, BANK_AUDIO */
        stx accu
    };
}
