// =====================================================================
// x16clib :: x16/audiorom.c -- the AUDIO ROM bank's API, wrapped
// =====================================================================
// Thin wrappers over the Commander X16 ROM audio bank (BANK_AUDIO, $0A).
// The entry points live at $C000+ INSIDE that bank -- not in the $FFxx
// KERNAL table -- so every call here crosses banks through the KERNAL's
// JSRFAR at $FF6E, whose convention is three inline bytes after the jsr:
//
//      jsr 0xff6e
//      byt <entry, >entry, 0x0a
//
// JSRFAR preserves the flags in BOTH directions, which this module needs
// in both: several ROM calls take carry as an INPUT, and nearly all of
// them report success or failure in it.
//
// The ROM driver keeps its own PSG/YM volume, pan, attenuation and patch
// shadows coherent. That is the whole reason to call the ROM rather than
// this library's psg.c and ym.c, which poke the hardware directly and
// know nothing of those shadows. Do not mix the two layers on one voice
// and expect the ROM's idea of its state to survive.
//
// x16_ar_fmplaystring/x16_ar_psgplaystring play their string TO THE END
// before returning, pacing on the 60 Hz jiffy clock -- which only
// advances while the VSYNC interrupt runs. Do not call them with
// interrupts off (or in the headless testbench): they never come back.
// The chordstring calls strike their notes and return at once.
//
// MARSHALLING. Oscar64 hands parameters to this code as named operands,
// so each wrapper loads A/X/Y from them directly. Two rules matter:
//
//   A carry INPUT is computed first, with `cmp #1` -- set iff the flag
//   byte is nonzero -- because lda/ldx/ldy leave carry alone. Loading the
//   value registers afterwards is therefore safe, and doing it the other
//   way round would destroy the flag.
//
//   Where the ROM wants a 16-bit value in X/Y, the low byte is `p` and
//   the high byte `p+1`: Oscar64 lays a C word out little-endian and the
//   asm operand names its first byte.
//
// GENERATED, and deliberately so: 57 wrappers that differ only in their
// arguments and their three-byte JSRFAR address. The generator and its
// entry table live in the commit that added this file; the table was
// checked against src_ca65/core/const_rom.inc. Edit a wrapper here
// freely -- nothing regenerates it -- but if you add one, copy the shape.
// =====================================================================

#include <x16/audiorom.h>

// asm -> C hand-off. One byte for the flag and byte returns, two for the
// word ones.
volatile unsigned char x16__ar_v;
volatile unsigned char x16__ar_lo;
volatile unsigned char x16__ar_hi;


// ---------------------------------------------------------------------
// BASIC-compatible FM/PSG utility and play-string calls.
// ---------------------------------------------------------------------

unsigned char x16_ar_fmfreq(unsigned char channel, unsigned int hz, unsigned char noretrigger) {
    __asm {
        lda noretrigger
        cmp #1
        lda channel
        ldx hz
        ldy hz+1
        jsr 0xff6e
        byt 0x00, 0xc0, 0x0a                    /* rom_bas_fmfreq, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_fmnote(unsigned char channel, unsigned char octnote, unsigned char kf, unsigned char noretrigger) {
    __asm {
        lda noretrigger
        cmp #1
        lda channel
        ldx octnote
        ldy kf
        jsr 0xff6e
        byt 0x03, 0xc0, 0x0a                    /* rom_bas_fmnote, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_fmvib(unsigned char speed, unsigned char depth) {
    __asm {
        lda speed
        ldx depth
        jsr 0xff6e
        byt 0x09, 0xc0, 0x0a                    /* rom_bas_fmvib, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

void x16_ar_fmplaystring(const char *s, unsigned char len) {
    __asm {
        ldx s
        ldy s+1
        lda len
        jsr 0xff6e
        byt 0x06, 0xc0, 0x0a                    /* rom_bas_fmplaystring, BANK_AUDIO */
    }
}

void x16_ar_psgplaystring(const char *s, unsigned char len) {
    __asm {
        ldx s
        ldy s+1
        lda len
        jsr 0xff6e
        byt 0x18, 0xc0, 0x0a                    /* rom_bas_psgplaystring, BANK_AUDIO */
    }
}

void x16_ar_fmchordstring(const char *s, unsigned char len) {
    __asm {
        ldx s
        ldy s+1
        lda len
        jsr 0xff6e
        byt 0x8d, 0xc0, 0x0a                    /* rom_bas_fmchordstring, BANK_AUDIO */
    }
}

void x16_ar_psgchordstring(const char *s, unsigned char len) {
    __asm {
        ldx s
        ldy s+1
        lda len
        jsr 0xff6e
        byt 0x90, 0xc0, 0x0a                    /* rom_bas_psgchordstring, BANK_AUDIO */
    }
}

void x16_ar_playstring_voice(unsigned char voice) {
    __asm {
        lda voice
        jsr 0xff6e
        byt 0x0c, 0xc0, 0x0a                    /* rom_bas_playstringvoice, BANK_AUDIO */
    }
}

unsigned char x16_ar_psgfreq(unsigned char voice, unsigned int hz) {
    __asm {
        lda voice
        ldx hz
        ldy hz+1
        jsr 0xff6e
        byt 0x0f, 0xc0, 0x0a                    /* rom_bas_psgfreq, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_psgnote(unsigned char voice, unsigned char octnote, unsigned char kf) {
    __asm {
        lda voice
        ldx octnote
        ldy kf
        jsr 0xff6e
        byt 0x12, 0xc0, 0x0a                    /* rom_bas_psgnote, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_psgwav(unsigned char voice, unsigned char waveform) {
    __asm {
        lda voice
        ldx waveform
        jsr 0xff6e
        byt 0x15, 0xc0, 0x0a                    /* rom_bas_psgwav, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

// ---------------------------------------------------------------------
// Note conversions. These answer in X (or X/Y for a word) even on the
// error path, where the ROM parks zeros and sets carry without always
// writing A -- so the byte forms read X, not A.
// ---------------------------------------------------------------------

unsigned char x16_ar_note_bas2fm(unsigned char octnote) {
    __asm {
        ldx octnote
        jsr 0xff6e
        byt 0x1b, 0xc0, 0x0a                    /* rom_notecon_bas2fm, BANK_AUDIO */
        stx x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_note_bas2midi(unsigned char octnote) {
    __asm {
        ldx octnote
        jsr 0xff6e
        byt 0x1e, 0xc0, 0x0a                    /* rom_notecon_bas2midi, BANK_AUDIO */
        stx x16__ar_v
    }
    return x16__ar_v;
}

unsigned int x16_ar_note_bas2psg(unsigned char octnote, unsigned char kf) {
    __asm {
        ldx octnote
        ldy kf
        jsr 0xff6e
        byt 0x21, 0xc0, 0x0a                    /* rom_notecon_bas2psg, BANK_AUDIO */
        stx x16__ar_lo
        sty x16__ar_hi
    }
    return (unsigned int)x16__ar_lo | ((unsigned int)x16__ar_hi << 8);
}

unsigned char x16_ar_note_fm2bas(unsigned char kc) {
    __asm {
        ldx kc
        jsr 0xff6e
        byt 0x24, 0xc0, 0x0a                    /* rom_notecon_fm2bas, BANK_AUDIO */
        stx x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_note_fm2midi(unsigned char kc) {
    __asm {
        ldx kc
        jsr 0xff6e
        byt 0x27, 0xc0, 0x0a                    /* rom_notecon_fm2midi, BANK_AUDIO */
        stx x16__ar_v
    }
    return x16__ar_v;
}

unsigned int x16_ar_note_fm2psg(unsigned char kc, unsigned char kf) {
    __asm {
        ldx kc
        ldy kf
        jsr 0xff6e
        byt 0x2a, 0xc0, 0x0a                    /* rom_notecon_fm2psg, BANK_AUDIO */
        stx x16__ar_lo
        sty x16__ar_hi
    }
    return (unsigned int)x16__ar_lo | ((unsigned int)x16__ar_hi << 8);
}

unsigned int x16_ar_note_freq2bas(unsigned int hz) {
    __asm {
        ldx hz
        ldy hz+1
        jsr 0xff6e
        byt 0x2d, 0xc0, 0x0a                    /* rom_notecon_freq2bas, BANK_AUDIO */
        stx x16__ar_lo
        sty x16__ar_hi
    }
    return (unsigned int)x16__ar_lo | ((unsigned int)x16__ar_hi << 8);
}

unsigned int x16_ar_note_freq2fm(unsigned int hz) {
    __asm {
        ldx hz
        ldy hz+1
        jsr 0xff6e
        byt 0x30, 0xc0, 0x0a                    /* rom_notecon_freq2fm, BANK_AUDIO */
        stx x16__ar_lo
        sty x16__ar_hi
    }
    return (unsigned int)x16__ar_lo | ((unsigned int)x16__ar_hi << 8);
}

unsigned int x16_ar_note_freq2midi(unsigned int hz) {
    __asm {
        ldx hz
        ldy hz+1
        jsr 0xff6e
        byt 0x33, 0xc0, 0x0a                    /* rom_notecon_freq2midi, BANK_AUDIO */
        stx x16__ar_lo
        sty x16__ar_hi
    }
    return (unsigned int)x16__ar_lo | ((unsigned int)x16__ar_hi << 8);
}

unsigned int x16_ar_note_freq2psg(unsigned int hz) {
    __asm {
        ldx hz
        ldy hz+1
        jsr 0xff6e
        byt 0x36, 0xc0, 0x0a                    /* rom_notecon_freq2psg, BANK_AUDIO */
        stx x16__ar_lo
        sty x16__ar_hi
    }
    return (unsigned int)x16__ar_lo | ((unsigned int)x16__ar_hi << 8);
}

unsigned char x16_ar_note_midi2bas(unsigned char midinote) {
    __asm {
        lda midinote
        jsr 0xff6e
        byt 0x39, 0xc0, 0x0a                    /* rom_notecon_midi2bas, BANK_AUDIO */
        stx x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_note_midi2fm(unsigned char midinote) {
    __asm {
        ldx midinote
        jsr 0xff6e
        byt 0x3c, 0xc0, 0x0a                    /* rom_notecon_midi2fm, BANK_AUDIO */
        stx x16__ar_v
    }
    return x16__ar_v;
}

unsigned int x16_ar_note_midi2psg(unsigned char midinote, unsigned char kf) {
    __asm {
        ldx midinote
        ldy kf
        jsr 0xff6e
        byt 0x3f, 0xc0, 0x0a                    /* rom_notecon_midi2psg, BANK_AUDIO */
        stx x16__ar_lo
        sty x16__ar_hi
    }
    return (unsigned int)x16__ar_lo | ((unsigned int)x16__ar_hi << 8);
}

unsigned int x16_ar_note_psg2bas(unsigned int freq) {
    __asm {
        ldx freq
        ldy freq+1
        jsr 0xff6e
        byt 0x42, 0xc0, 0x0a                    /* rom_notecon_psg2bas, BANK_AUDIO */
        stx x16__ar_lo
        sty x16__ar_hi
    }
    return (unsigned int)x16__ar_lo | ((unsigned int)x16__ar_hi << 8);
}

unsigned int x16_ar_note_psg2fm(unsigned int freq) {
    __asm {
        ldx freq
        ldy freq+1
        jsr 0xff6e
        byt 0x45, 0xc0, 0x0a                    /* rom_notecon_psg2fm, BANK_AUDIO */
        stx x16__ar_lo
        sty x16__ar_hi
    }
    return (unsigned int)x16__ar_lo | ((unsigned int)x16__ar_hi << 8);
}

unsigned int x16_ar_note_psg2midi(unsigned int freq) {
    __asm {
        ldx freq
        ldy freq+1
        jsr 0xff6e
        byt 0x48, 0xc0, 0x0a                    /* rom_notecon_psg2midi, BANK_AUDIO */
        stx x16__ar_lo
        sty x16__ar_hi
    }
    return (unsigned int)x16__ar_lo | ((unsigned int)x16__ar_hi << 8);
}

// ---------------------------------------------------------------------
// The ROM PSG API. These keep the driver's shadows coherent; psg.c
// does not.
// ---------------------------------------------------------------------

void x16_ar_psg_init(void) {
    __asm {
        jsr 0xff6e
        byt 0x4b, 0xc0, 0x0a                    /* rom_psg_init, BANK_AUDIO */
    }
}

void x16_ar_psg_playfreq(unsigned char voice, unsigned int freq) {
    __asm {
        lda voice
        ldx freq
        ldy freq+1
        jsr 0xff6e
        byt 0x4e, 0xc0, 0x0a                    /* rom_psg_playfreq, BANK_AUDIO */
    }
}

unsigned char x16_ar_psg_read(unsigned char reg, unsigned char cooked) {
    __asm {
        lda cooked
        cmp #1
        ldx reg
        jsr 0xff6e
        byt 0x51, 0xc0, 0x0a                    /* rom_psg_read, BANK_AUDIO */
        sta x16__ar_v
    }
    return x16__ar_v;
}

void x16_ar_psg_setatten(unsigned char voice, unsigned char atten) {
    __asm {
        lda voice
        ldx atten
        jsr 0xff6e
        byt 0x54, 0xc0, 0x0a                    /* rom_psg_setatten, BANK_AUDIO */
    }
}

void x16_ar_psg_setfreq(unsigned char voice, unsigned int freq) {
    __asm {
        lda voice
        ldx freq
        ldy freq+1
        jsr 0xff6e
        byt 0x57, 0xc0, 0x0a                    /* rom_psg_setfreq, BANK_AUDIO */
    }
}

void x16_ar_psg_setpan(unsigned char voice, unsigned char pan) {
    __asm {
        lda voice
        ldx pan
        jsr 0xff6e
        byt 0x5a, 0xc0, 0x0a                    /* rom_psg_setpan, BANK_AUDIO */
    }
}

void x16_ar_psg_setvol(unsigned char voice, unsigned char vol) {
    __asm {
        lda voice
        ldx vol
        jsr 0xff6e
        byt 0x5d, 0xc0, 0x0a                    /* rom_psg_setvol, BANK_AUDIO */
    }
}

void x16_ar_psg_write(unsigned char reg, unsigned char value) {
    __asm {
        ldx reg
        lda value
        jsr 0xff6e
        byt 0x60, 0xc0, 0x0a                    /* rom_psg_write, BANK_AUDIO */
    }
}

void x16_ar_psg_write_fast(unsigned char reg, unsigned char value) {
    __asm {
        ldx reg
        lda value
        jsr 0xff6e
        byt 0xa2, 0xc0, 0x0a                    /* rom_psg_write_fast, BANK_AUDIO */
    }
}

unsigned char x16_ar_psg_getatten(unsigned char voice) {
    __asm {
        lda voice
        jsr 0xff6e
        byt 0x93, 0xc0, 0x0a                    /* rom_psg_getatten, BANK_AUDIO */
        stx x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_psg_getpan(unsigned char voice) {
    __asm {
        lda voice
        jsr 0xff6e
        byt 0x96, 0xc0, 0x0a                    /* rom_psg_getpan, BANK_AUDIO */
        stx x16__ar_v
    }
    return x16__ar_v;
}

// ---------------------------------------------------------------------
// The ROM YM/FM API.
// ---------------------------------------------------------------------

unsigned char x16_ar_ym_init(void) {
    __asm {
        jsr 0xff6e
        byt 0x63, 0xc0, 0x0a                    /* rom_ym_init, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_loaddefpatches(void) {
    __asm {
        jsr 0xff6e
        byt 0x66, 0xc0, 0x0a                    /* rom_ym_loaddefpatches, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_loadpatch(unsigned char channel, unsigned char patch) {
    __asm {
        sec
        lda channel
        ldx patch
        jsr 0xff6e
        byt 0x69, 0xc0, 0x0a                    /* rom_ym_loadpatch, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_loadpatch_ram(unsigned char channel, const void *patch) {
    __asm {
        clc
        lda channel
        ldx patch
        ldy patch+1
        jsr 0xff6e
        byt 0x69, 0xc0, 0x0a                    /* rom_ym_loadpatch, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_loadpatchlfn(unsigned char channel, unsigned char lfn) {
    __asm {
        lda channel
        ldx lfn
        jsr 0xff6e
        byt 0x6c, 0xc0, 0x0a                    /* rom_ym_loadpatchlfn, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_playdrum(unsigned char channel, unsigned char midinote) {
    __asm {
        lda channel
        ldx midinote
        jsr 0xff6e
        byt 0x6f, 0xc0, 0x0a                    /* rom_ym_playdrum, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_playnote(unsigned char channel, unsigned char kc, unsigned char kf, unsigned char noretrigger) {
    __asm {
        lda noretrigger
        cmp #1
        lda channel
        ldx kc
        ldy kf
        jsr 0xff6e
        byt 0x72, 0xc0, 0x0a                    /* rom_ym_playnote, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_setatten(unsigned char channel, unsigned char atten) {
    __asm {
        lda channel
        ldx atten
        jsr 0xff6e
        byt 0x75, 0xc0, 0x0a                    /* rom_ym_setatten, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_setdrum(unsigned char channel, unsigned char midinote) {
    __asm {
        lda channel
        ldx midinote
        jsr 0xff6e
        byt 0x78, 0xc0, 0x0a                    /* rom_ym_setdrum, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_setnote(unsigned char channel, unsigned char kc, unsigned char kf) {
    __asm {
        lda channel
        ldx kc
        ldy kf
        jsr 0xff6e
        byt 0x7b, 0xc0, 0x0a                    /* rom_ym_setnote, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_setpan(unsigned char channel, unsigned char pan) {
    __asm {
        lda channel
        ldx pan
        jsr 0xff6e
        byt 0x7e, 0xc0, 0x0a                    /* rom_ym_setpan, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_read(unsigned char reg, unsigned char cooked) {
    __asm {
        lda cooked
        cmp #1
        ldx reg
        jsr 0xff6e
        byt 0x81, 0xc0, 0x0a                    /* rom_ym_read, BANK_AUDIO */
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_release(unsigned char channel) {
    __asm {
        lda channel
        jsr 0xff6e
        byt 0x84, 0xc0, 0x0a                    /* rom_ym_release, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_trigger(unsigned char channel, unsigned char noretrigger) {
    __asm {
        lda noretrigger
        cmp #1
        lda channel
        jsr 0xff6e
        byt 0x87, 0xc0, 0x0a                    /* rom_ym_trigger, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_write(unsigned char reg, unsigned char value) {
    __asm {
        ldx reg
        lda value
        jsr 0xff6e
        byt 0x8a, 0xc0, 0x0a                    /* rom_ym_write, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_getatten(unsigned char channel) {
    __asm {
        lda channel
        jsr 0xff6e
        byt 0x99, 0xc0, 0x0a                    /* rom_ym_getatten, BANK_AUDIO */
        stx x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_getpan(unsigned char channel) {
    __asm {
        lda channel
        jsr 0xff6e
        byt 0x9c, 0xc0, 0x0a                    /* rom_ym_getpan, BANK_AUDIO */
        stx x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_audio_init(void) {
    __asm {
        jsr 0xff6e
        byt 0x9f, 0xc0, 0x0a                    /* rom_audio_init, BANK_AUDIO */
        lda #0
        rol
        sta x16__ar_v
    }
    return x16__ar_v;
}

unsigned char x16_ar_ym_get_chip_type(void) {
    __asm {
        jsr 0xff6e
        byt 0xa5, 0xc0, 0x0a                    /* rom_ym_get_chip_type, BANK_AUDIO */
        sta x16__ar_v
    }
    return x16__ar_v;
}
