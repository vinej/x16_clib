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
// after the jsr: a 16-bit entry address, then the bank byte. KickC's
// asm blocks have .byte (and only .byte), so each jsrfar is
//
//      jsr $ff6e
//      .byte <entry, >entry, bank
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
unsigned char x16_ym_write(__mem unsigned char reg, __mem unsigned char value) {
    __mem char r;
    asm {
        php
        sei
        ldy #128 /*YM_TIMEOUT*/
    yw_wait:
        dey
        bmi yw_timeout
        bit $9f41 /*YM_DATA*/
        bmi yw_wait                     // busy
        ldx reg
        stx $9f40 /*YM_REG*/
        nop                             // settling time between select
        nop                             // and data
        nop
        lda value
        sta $9f41 /*YM_DATA*/
        plp
        lda #1
        sta r
        bra yw_done
    yw_timeout:
        plp
        stz r
    yw_done:
    }
    return r;
}

unsigned char x16_ym_busy(void) {
    __mem char r;
    asm {
        lda $9f41 /*YM_DATA*/
        asl                             // bit 7 into carry
        lda #0
        rol
        sta r                           // busy is not an error
    }
    return r;
}

// Reset the chip and load the default patch set. The carry that
// matters is the LAST call's -- rts does not touch flags.
unsigned char x16_ym_init(void) {
    __mem char r;
    asm {
        jsr yi_t1
        jsr yi_t2
        lda #0
        rol                             // carry set = failure
        eor #1
        sta r
        bra yi_done
    yi_t1:
        jsr $ff6e /*JSRFAR*/
        .byte $9f, $c0, $0a             // rom_audio_init, BANK_AUDIO
        rts
    yi_t2:
        jsr $ff6e /*JSRFAR*/
        .byte $66, $c0, $0a             // rom_ym_loaddefpatches
        rts
    yi_done:
    }
    return r;
}

// Through the ROM driver: A = value, X = register (its convention).
void x16_ym_poke(__mem unsigned char reg, __mem unsigned char value) {
    asm {
        ldx reg
        lda value
        jsr $ff6e /*JSRFAR*/
        .byte $8a, $c0, $0a             // rom_ym_write, BANK_AUDIO
    }
}

// Carry SET selects the ROM patch table.
unsigned char x16_ym_patch_rom(__mem unsigned char channel, __mem unsigned char patch) {
    __mem char r;
    asm {
        sec                             // ROM patch table
        ldx patch
        lda channel
        jsr $ff6e /*JSRFAR*/
        .byte $69, $c0, $0a             // rom_ym_loadpatch, BANK_AUDIO
        lda #0
        rol
        eor #1
        sta r
    }
    return r;
}

// Carry CLEAR selects a patch in RAM, addressed by X/Y.
unsigned char x16_ym_patch_ram(__mem unsigned char channel, const void *patch) {
    __mem char r;
    asm {
        clc                             // patch in RAM
        ldx patch
        ldy patch+1
        lda channel
        jsr $ff6e /*JSRFAR*/
        .byte $69, $c0, $0a             // rom_ym_loadpatch, BANK_AUDIO
        lda #0
        rol
        eor #1
        sta r
    }
    return r;
}

// Raw key code and key fraction. retrigger != 0 restarts the envelope
// (carry CLEAR to the ROM); 0 only changes the pitch.
void x16_ym_note(__mem unsigned char channel, __mem unsigned char kc,
                 __mem unsigned char kf, __mem unsigned char retrigger) {
    asm {
        lda retrigger
        beq yn_hold
        clc                             // retrigger the envelope
        bra yn_call
    yn_hold:
        sec                             // change pitch only
    yn_call:
        ldx kc
        ldy kf
        lda channel
        jsr $ff6e /*JSRFAR*/
        .byte $72, $c0, $0a             // rom_ym_playnote, BANK_AUDIO
    }
}

unsigned char x16_ym_note_bas(__mem unsigned char channel, __mem unsigned char note,
                              __mem unsigned char retrigger) {
    __mem char r;
    asm {
        lda retrigger
        beq yb_hold
        clc
        bra yb_call
    yb_hold:
        sec
    yb_call:
        ldx note
        lda channel
        jsr $ff6e /*JSRFAR*/
        .byte $03, $c0, $0a             // rom_bas_fmnote, BANK_AUDIO
        lda #0
        rol                             // carry set = failure
        eor #1
        sta r
    }
    return r;
}

void x16_ym_release_note(__mem unsigned char channel) {
    asm {
        lda channel
        jsr $ff6e /*JSRFAR*/
        .byte $84, $c0, $0a             // rom_ym_release, BANK_AUDIO
    }
}

unsigned char x16_ym_vol(__mem unsigned char channel, __mem unsigned char atten) {
    __mem char r;
    asm {
        ldx atten
        lda channel
        jsr $ff6e /*JSRFAR*/
        .byte $75, $c0, $0a             // rom_ym_setatten, BANK_AUDIO
        lda #0
        rol
        eor #1
        sta r
    }
    return r;
}

unsigned char x16_ym_pan(__mem unsigned char channel, __mem unsigned char pan) {
    __mem char r;
    asm {
        ldx pan
        lda channel
        jsr $ff6e /*JSRFAR*/
        .byte $7e, $c0, $0a             // rom_ym_setpan, BANK_AUDIO
        lda #0
        rol
        eor #1
        sta r
    }
    return r;
}

unsigned char x16_ym_drum(__mem unsigned char channel, __mem unsigned char note) {
    __mem char r;
    asm {
        ldx note
        lda channel
        jsr $ff6e /*JSRFAR*/
        .byte $6f, $c0, $0a             // rom_ym_playdrum, BANK_AUDIO
        lda #0
        rol
        eor #1
        sta r
    }
    return r;
}

// The ROM driver returns its shadow in X.
unsigned char x16_ym_get_pan(__mem unsigned char channel) {
    __mem char r;
    asm {
        lda channel
        jsr $ff6e /*JSRFAR*/
        .byte $9c, $c0, $0a             // rom_ym_getpan, BANK_AUDIO
        stx r
    }
    return r;
}

unsigned char x16_ym_get_vol(__mem unsigned char channel) {
    __mem char r;
    asm {
        lda channel
        jsr $ff6e /*JSRFAR*/
        .byte $99, $c0, $0a             // rom_ym_getatten, BANK_AUDIO
        stx r
    }
    return r;
}
