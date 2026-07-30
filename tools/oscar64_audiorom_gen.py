"""Emit src_oscar64/x16/audiorom.c.

57 wrappers that all do the same three things -- marshal into A/X/Y (and
sometimes carry), cross the ROM bank with JSRFAR, convert the answer --
so they are generated from a table. Hand-typing 57 three-byte JSRFAR
addresses is exactly the kind of transcription the machine should do:
the entry table below is the ROM's own $C000 + 3n layout, checked
against src_ca65/core/const_rom.inc.
"""
import io

BANK = 0x0A

# name -> ROM entry, from src_ca65/core/const_rom.inc
E = {
    'bas_fmfreq': 0xC000, 'bas_fmnote': 0xC003, 'bas_fmplaystring': 0xC006,
    'bas_fmvib': 0xC009, 'bas_playstringvoice': 0xC00C, 'bas_psgfreq': 0xC00F,
    'bas_psgnote': 0xC012, 'bas_psgwav': 0xC015, 'bas_psgplaystring': 0xC018,
    'notecon_bas2fm': 0xC01B, 'notecon_bas2midi': 0xC01E,
    'notecon_bas2psg': 0xC021, 'notecon_fm2bas': 0xC024,
    'notecon_fm2midi': 0xC027, 'notecon_fm2psg': 0xC02A,
    'notecon_freq2bas': 0xC02D, 'notecon_freq2fm': 0xC030,
    'notecon_freq2midi': 0xC033, 'notecon_freq2psg': 0xC036,
    'notecon_midi2bas': 0xC039, 'notecon_midi2fm': 0xC03C,
    'notecon_midi2psg': 0xC03F, 'notecon_psg2bas': 0xC042,
    'notecon_psg2fm': 0xC045, 'notecon_psg2midi': 0xC048,
    'psg_init': 0xC04B, 'psg_playfreq': 0xC04E, 'psg_read': 0xC051,
    'psg_setatten': 0xC054, 'psg_setfreq': 0xC057, 'psg_setpan': 0xC05A,
    'psg_setvol': 0xC05D, 'psg_write': 0xC060, 'ym_init': 0xC063,
    'ym_loaddefpatches': 0xC066, 'ym_loadpatch': 0xC069,
    'ym_loadpatchlfn': 0xC06C, 'ym_playdrum': 0xC06F, 'ym_playnote': 0xC072,
    'ym_setatten': 0xC075, 'ym_setdrum': 0xC078, 'ym_setnote': 0xC07B,
    'ym_setpan': 0xC07E, 'ym_read': 0xC081, 'ym_release': 0xC084,
    'ym_trigger': 0xC087, 'ym_write': 0xC08A, 'bas_fmchordstring': 0xC08D,
    'bas_psgchordstring': 0xC090, 'psg_getatten': 0xC093,
    'psg_getpan': 0xC096, 'ym_getatten': 0xC099, 'ym_getpan': 0xC09C,
    'audio_init': 0xC09F, 'psg_write_fast': 0xC0A2,
    'ym_get_chip_type': 0xC0A5,
}

# (c_name, rom_entry, return kind, [(c_type, c_param)], marshal asm lines)
#
# return kinds:  'carry' 0=ok/1=fail   'a' byte in A   'x' byte in X
#                'xy' word X=lo,Y=hi   'void'
W = [
    # --- BASIC-compatible FM/PSG helpers -----------------------------
    ('fmfreq', 'bas_fmfreq', 'carry',
     [('unsigned char', 'channel'), ('unsigned int', 'hz'),
      ('unsigned char', 'noretrigger')],
     ['lda noretrigger', 'cmp #1', 'lda channel', 'ldx hz', 'ldy hz+1']),
    ('fmnote', 'bas_fmnote', 'carry',
     [('unsigned char', 'channel'), ('unsigned char', 'octnote'),
      ('unsigned char', 'kf'), ('unsigned char', 'noretrigger')],
     ['lda noretrigger', 'cmp #1', 'lda channel', 'ldx octnote', 'ldy kf']),
    ('fmvib', 'bas_fmvib', 'carry',
     [('unsigned char', 'speed'), ('unsigned char', 'depth')],
     ['lda speed', 'ldx depth']),
    ('fmplaystring', 'bas_fmplaystring', 'void',
     [('const char *', 's'), ('unsigned char', 'len')],
     ['ldx s', 'ldy s+1', 'lda len']),
    ('psgplaystring', 'bas_psgplaystring', 'void',
     [('const char *', 's'), ('unsigned char', 'len')],
     ['ldx s', 'ldy s+1', 'lda len']),
    ('fmchordstring', 'bas_fmchordstring', 'void',
     [('const char *', 's'), ('unsigned char', 'len')],
     ['ldx s', 'ldy s+1', 'lda len']),
    ('psgchordstring', 'bas_psgchordstring', 'void',
     [('const char *', 's'), ('unsigned char', 'len')],
     ['ldx s', 'ldy s+1', 'lda len']),
    ('playstring_voice', 'bas_playstringvoice', 'void',
     [('unsigned char', 'voice')], ['lda voice']),
    ('psgfreq', 'bas_psgfreq', 'carry',
     [('unsigned char', 'voice'), ('unsigned int', 'hz')],
     ['lda voice', 'ldx hz', 'ldy hz+1']),
    ('psgnote', 'bas_psgnote', 'carry',
     [('unsigned char', 'voice'), ('unsigned char', 'octnote'),
      ('unsigned char', 'kf')],
     ['lda voice', 'ldx octnote', 'ldy kf']),
    ('psgwav', 'bas_psgwav', 'carry',
     [('unsigned char', 'voice'), ('unsigned char', 'waveform')],
     ['lda voice', 'ldx waveform']),

    # --- note conversions --------------------------------------------
    ('note_bas2fm', 'notecon_bas2fm', 'x',
     [('unsigned char', 'octnote')], ['ldx octnote']),
    ('note_bas2midi', 'notecon_bas2midi', 'x',
     [('unsigned char', 'octnote')], ['ldx octnote']),
    ('note_bas2psg', 'notecon_bas2psg', 'xy',
     [('unsigned char', 'octnote'), ('unsigned char', 'kf')],
     ['ldx octnote', 'ldy kf']),
    ('note_fm2bas', 'notecon_fm2bas', 'x',
     [('unsigned char', 'kc')], ['ldx kc']),
    ('note_fm2midi', 'notecon_fm2midi', 'x',
     [('unsigned char', 'kc')], ['ldx kc']),
    ('note_fm2psg', 'notecon_fm2psg', 'xy',
     [('unsigned char', 'kc'), ('unsigned char', 'kf')],
     ['ldx kc', 'ldy kf']),
    ('note_freq2bas', 'notecon_freq2bas', 'xy',
     [('unsigned int', 'hz')], ['ldx hz', 'ldy hz+1']),
    ('note_freq2fm', 'notecon_freq2fm', 'xy',
     [('unsigned int', 'hz')], ['ldx hz', 'ldy hz+1']),
    ('note_freq2midi', 'notecon_freq2midi', 'xy',
     [('unsigned int', 'hz')], ['ldx hz', 'ldy hz+1']),
    ('note_freq2psg', 'notecon_freq2psg', 'xy',
     [('unsigned int', 'hz')], ['ldx hz', 'ldy hz+1']),
    # the one conversion whose input rides A rather than X
    ('note_midi2bas', 'notecon_midi2bas', 'x',
     [('unsigned char', 'midinote')], ['lda midinote']),
    ('note_midi2fm', 'notecon_midi2fm', 'x',
     [('unsigned char', 'midinote')], ['ldx midinote']),
    ('note_midi2psg', 'notecon_midi2psg', 'xy',
     [('unsigned char', 'midinote'), ('unsigned char', 'kf')],
     ['ldx midinote', 'ldy kf']),
    ('note_psg2bas', 'notecon_psg2bas', 'xy',
     [('unsigned int', 'freq')], ['ldx freq', 'ldy freq+1']),
    ('note_psg2fm', 'notecon_psg2fm', 'xy',
     [('unsigned int', 'freq')], ['ldx freq', 'ldy freq+1']),
    ('note_psg2midi', 'notecon_psg2midi', 'xy',
     [('unsigned int', 'freq')], ['ldx freq', 'ldy freq+1']),

    # --- ROM PSG API -------------------------------------------------
    ('psg_init', 'psg_init', 'void', [], []),
    ('psg_playfreq', 'psg_playfreq', 'void',
     [('unsigned char', 'voice'), ('unsigned int', 'freq')],
     ['lda voice', 'ldx freq', 'ldy freq+1']),
    ('psg_read', 'psg_read', 'a',
     [('unsigned char', 'reg'), ('unsigned char', 'cooked')],
     ['lda cooked', 'cmp #1', 'ldx reg']),
    ('psg_setatten', 'psg_setatten', 'void',
     [('unsigned char', 'voice'), ('unsigned char', 'atten')],
     ['lda voice', 'ldx atten']),
    ('psg_setfreq', 'psg_setfreq', 'void',
     [('unsigned char', 'voice'), ('unsigned int', 'freq')],
     ['lda voice', 'ldx freq', 'ldy freq+1']),
    ('psg_setpan', 'psg_setpan', 'void',
     [('unsigned char', 'voice'), ('unsigned char', 'pan')],
     ['lda voice', 'ldx pan']),
    ('psg_setvol', 'psg_setvol', 'void',
     [('unsigned char', 'voice'), ('unsigned char', 'vol')],
     ['lda voice', 'ldx vol']),
    ('psg_write', 'psg_write', 'void',
     [('unsigned char', 'reg'), ('unsigned char', 'value')],
     ['ldx reg', 'lda value']),
    ('psg_write_fast', 'psg_write_fast', 'void',
     [('unsigned char', 'reg'), ('unsigned char', 'value')],
     ['ldx reg', 'lda value']),
    ('psg_getatten', 'psg_getatten', 'x',
     [('unsigned char', 'voice')], ['lda voice']),
    ('psg_getpan', 'psg_getpan', 'x',
     [('unsigned char', 'voice')], ['lda voice']),

    # --- ROM YM/FM API -----------------------------------------------
    ('ym_init', 'ym_init', 'carry', [], []),
    ('ym_loaddefpatches', 'ym_loaddefpatches', 'carry', [], []),
    # carry SET selects a ROM patch number in X
    ('ym_loadpatch', 'ym_loadpatch', 'carry',
     [('unsigned char', 'channel'), ('unsigned char', 'patch')],
     ['sec', 'lda channel', 'ldx patch']),
    # carry CLEAR selects a 26-byte patch in RAM at X/Y
    ('ym_loadpatch_ram', 'ym_loadpatch', 'carry',
     [('unsigned char', 'channel'), ('const void *', 'patch')],
     ['clc', 'lda channel', 'ldx patch', 'ldy patch+1']),
    ('ym_loadpatchlfn', 'ym_loadpatchlfn', 'carry',
     [('unsigned char', 'channel'), ('unsigned char', 'lfn')],
     ['lda channel', 'ldx lfn']),
    ('ym_playdrum', 'ym_playdrum', 'carry',
     [('unsigned char', 'channel'), ('unsigned char', 'midinote')],
     ['lda channel', 'ldx midinote']),
    ('ym_playnote', 'ym_playnote', 'carry',
     [('unsigned char', 'channel'), ('unsigned char', 'kc'),
      ('unsigned char', 'kf'), ('unsigned char', 'noretrigger')],
     ['lda noretrigger', 'cmp #1', 'lda channel', 'ldx kc', 'ldy kf']),
    ('ym_setatten', 'ym_setatten', 'carry',
     [('unsigned char', 'channel'), ('unsigned char', 'atten')],
     ['lda channel', 'ldx atten']),
    ('ym_setdrum', 'ym_setdrum', 'carry',
     [('unsigned char', 'channel'), ('unsigned char', 'midinote')],
     ['lda channel', 'ldx midinote']),
    ('ym_setnote', 'ym_setnote', 'carry',
     [('unsigned char', 'channel'), ('unsigned char', 'kc'),
      ('unsigned char', 'kf')],
     ['lda channel', 'ldx kc', 'ldy kf']),
    ('ym_setpan', 'ym_setpan', 'carry',
     [('unsigned char', 'channel'), ('unsigned char', 'pan')],
     ['lda channel', 'ldx pan']),
    ('ym_read', 'ym_read', 'a',
     [('unsigned char', 'reg'), ('unsigned char', 'cooked')],
     ['lda cooked', 'cmp #1', 'ldx reg']),
    ('ym_release', 'ym_release', 'carry',
     [('unsigned char', 'channel')], ['lda channel']),
    ('ym_trigger', 'ym_trigger', 'carry',
     [('unsigned char', 'channel'), ('unsigned char', 'noretrigger')],
     ['lda noretrigger', 'cmp #1', 'lda channel']),
    ('ym_write', 'ym_write', 'carry',
     [('unsigned char', 'reg'), ('unsigned char', 'value')],
     ['ldx reg', 'lda value']),
    ('ym_getatten', 'ym_getatten', 'x',
     [('unsigned char', 'channel')], ['lda channel']),
    ('ym_getpan', 'ym_getpan', 'x',
     [('unsigned char', 'channel')], ['lda channel']),
    ('audio_init', 'audio_init', 'carry', [], []),
    ('ym_get_chip_type', 'ym_get_chip_type', 'a', [], []),
]

RET_C = {'carry': 'unsigned char', 'a': 'unsigned char',
         'x': 'unsigned char', 'xy': 'unsigned int', 'void': 'void'}

HEADER = '''// =====================================================================
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
'''

def emit(w):
    name, entry, ret, params, marshal = w
    addr = E[entry]
    lo, hi = addr & 0xFF, addr >> 8
    decl_params = ', '.join('%s%s' % (t if t.endswith('*') else t + ' ', p)
                            for t, p in params) or 'void'
    lines = []
    lines.append('%s x16_ar_%s(%s) {' % (RET_C[ret], name, decl_params))
    lines.append('    __asm {')
    for m in marshal:
        lines.append('        %s' % m)
    lines.append('        jsr 0xff6e')
    lines.append('        byt 0x%02x, 0x%02x, 0x%02x                    '
                 '/* rom_%s, BANK_AUDIO */' % (lo, hi, BANK, entry))
    if ret == 'carry':
        lines.append('        lda #0')
        lines.append('        rol')
        lines.append('        sta x16__ar_v')
    elif ret == 'a':
        lines.append('        sta x16__ar_v')
    elif ret == 'x':
        lines.append('        stx x16__ar_v')
    elif ret == 'xy':
        lines.append('        stx x16__ar_lo')
        lines.append('        sty x16__ar_hi')
    lines.append('    }')
    if ret in ('carry', 'a', 'x'):
        lines.append('    return x16__ar_v;')
    elif ret == 'xy':
        lines.append('    return (unsigned int)x16__ar_lo'
                     ' | ((unsigned int)x16__ar_hi << 8);')
    lines.append('}')
    return '\n'.join(lines)


SECTIONS = [
    (0, '// ---------------------------------------------------------------------\n'
        '// BASIC-compatible FM/PSG utility and play-string calls.\n'
        '// ---------------------------------------------------------------------'),
    (11, '// ---------------------------------------------------------------------\n'
         '// Note conversions. These answer in X (or X/Y for a word) even on the\n'
         '// error path, where the ROM parks zeros and sets carry without always\n'
         '// writing A -- so the byte forms read X, not A.\n'
         '// ---------------------------------------------------------------------'),
    (27, '// ---------------------------------------------------------------------\n'
         '// The ROM PSG API. These keep the driver\'s shadows coherent; psg.c\n'
         '// does not.\n'
         '// ---------------------------------------------------------------------'),
    (38, '// ---------------------------------------------------------------------\n'
         '// The ROM YM/FM API.\n'
         '// ---------------------------------------------------------------------'),
]

out = [HEADER]
sec = dict(SECTIONS)
for i, w in enumerate(W):
    if i in sec:
        out.append('\n' + sec[i])
    out.append('\n' + emit(w))

io.open('src_oscar64/x16/audiorom.c', 'w', newline='').write('\n'.join(out) + '\n')
print('src_oscar64/x16/audiorom.c: %d wrappers' % len(W))
