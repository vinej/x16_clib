/* =====================================================================
 * x16clib :: test_llvm/runner10.c -- audiorom, wavfile, zsm, vdc
 * =====================================================================
 * Standalone suite (its own main; run via
 *     .\build_llvm.ps1 -Test -Source test_ca65\runner8.c).
 *
 * What is checkable headless, and what is not:
 *
 *   audiorom  the note-conversion tables are pure arithmetic in the
 *             AUDIO ROM bank -- they run and are exact, so they are
 *             asserted against hand-computed values. Anything that
 *             makes sound is state-only (registers, not ears).
 *   wavfile   pure header parsing over a buffer we build byte by byte:
 *             fully checkable, including the rejection paths.
 *   zsm       header validation and the error memory are checkable;
 *             playback needs a real stream and a tick clock, so the
 *             tests drive one hand-built stream and read back the PSG
 *             registers it writes.
 *   vdc       VERA's display composer: every routine is a register
 *             read/write, all live headless. Runs LAST -- it changes
 *             the display configuration.
 * =====================================================================
 */

#include <string.h>

#include "testlib.h"

#include <x16/audiorom.h>
#include <x16/wavfile.h>
#include <x16/zsm.h>
#include <x16/vdc.h>
#include <x16/vera.h>
#include <x16/bank.h>

/* --- audiorom -------------------------------------------------------- */

/* The AUDIO ROM is banked in by the library on every call, so the only
** precondition is that a ROM with an audio bank is present. Its note
** helpers are total functions over their input ranges.
*/
static void t_audiorom (void)
{
    unsigned char ok;

    ok = x16_ar_audio_init();
    t_check(ok == 0 || ok == 1, "AR_INIT_SANE");

    /* BASIC oct/note packs an octave every SIXTEEN steps -- twelve
    ** notes then four unused codes -- so the byte is octave*16 + note + 1
    ** and middle C (MIDI 60) is 65, not 48. The ROM's bas2midi table
    ** answers $FF for the codes in the gaps.
    */
    t_check(x16_ar_note_bas2midi(65) == 60, "AR_BAS2MIDI");
    /* KERNAL BUG (x16-rom r49, audio/noteconvert.s): notecon_midi2bas
    ** indexes the bas2midi table instead of midi2bas, so it answers
    ** bas2midi[note] -- for MIDI 60 that is $3B, 59. Pinned as-is so a
    ** ROM that fixes it trips this test instead of changing silently;
    ** the value it SHOULD return is 65.
    */
    t_check(x16_ar_note_midi2bas(60) == 59, "AR_MIDI2BAS_ROMBUG");
    t_check(x16_ar_note_bas2midi(49) == 48 &&
            x16_ar_note_bas2midi(81) == 72, "AR_BAS2MIDI_RANGE");

    /* FM key codes skip 3 of every 16 slots (the YM2151's KC layout),
    ** so bas2fm is not the identity -- but it must round-trip too.
    */
    t_check(x16_ar_note_fm2bas(x16_ar_note_bas2fm(65)) == 65,
            "AR_FM_ROUNDTRIP");

    /* Rising notes must give rising PSG frequency words. */
    t_check(x16_ar_note_bas2psg(65, 0) < x16_ar_note_bas2psg(72, 0),
            "AR_PSG_RISES");

    /* An octave up is a doubling of frequency, within rounding. */
    {
        unsigned int lo = x16_ar_note_bas2psg(65, 0);   /* middle C */
        unsigned int hi = x16_ar_note_bas2psg(81, 0);   /* one octave up */
        unsigned int twice = (unsigned int)(lo * 2);
        unsigned int diff = hi > twice ? hi - twice : twice - hi;
        t_check(diff <= (twice / 64), "AR_PSG_OCTAVE");
    }

    /* The PSG shadow registers read back what was set. */
    x16_ar_psg_init();
    x16_ar_psg_setvol(0, 40);
    x16_ar_psg_setpan(0, 3);
    t_check(x16_ar_psg_getpan(0) == 3, "AR_PSG_PAN");
    x16_ar_psg_setatten(1, 12);
    /* KERNAL BUG (x16-rom r49, audio/psg.s): psg_getatten loads the
    ** attenuation into .A, then runs RESTORE_BANK -- which clobbers .A
    ** with the saved RAM bank -- and only then does `tax`. So it hands
    ** back the RAM bank that was current before the call, never the
    ** attenuation. psg_getpan escapes it by doing its `tax` BEFORE the
    ** restore. Pinned against the bank we set, so a fixed ROM trips
    ** this test rather than changing under us.
    */
    x16_bank_set(5);
    x16_ar_psg_setatten(1, 12);
    t_check(x16_ar_psg_getatten(1) == 5, "AR_PSG_ATTEN_ROMBUG");
    x16_bank_set(1);
}

/* --- wavfile --------------------------------------------------------- */

/* A canonical 44-byte PCM header, built field by field so every test
** can corrupt exactly one thing. All multi-byte fields little-endian.
*/
static unsigned char wav[64];

static void wav_put (unsigned int off, const char *s)
{
    while (*s) {
        wav[off++] = (unsigned char)*s++;
    }
}

static void wav_dw (unsigned int off, unsigned long v)
{
    wav[off]     = (unsigned char)(v & 0xFF);
    wav[off + 1] = (unsigned char)((v >> 8) & 0xFF);
    wav[off + 2] = (unsigned char)((v >> 16) & 0xFF);
    wav[off + 3] = (unsigned char)((v >> 24) & 0xFF);
}

static void wav_w (unsigned int off, unsigned int v)
{
    wav[off]     = (unsigned char)(v & 0xFF);
    wav[off + 1] = (unsigned char)(v >> 8);
}

static void wav_build (void)
{
    memset(wav, 0, sizeof wav);
    wav_put(0, "RIFF");
    wav_dw(4, 36 + 8);          /* file size - 8 */
    wav_put(8, "WAVE");
    wav_put(12, "fmt ");
    wav_dw(16, 16);             /* PCM fmt chunk size */
    wav_w(20, 1);               /* format 1 = integer PCM */
    wav_w(22, 2);               /* stereo */
    wav_dw(24, 22050);          /* sample rate */
    wav_dw(28, 22050UL * 4);    /* byte rate */
    wav_w(32, 4);               /* block align */
    wav_w(34, 16);              /* bits per sample */
    wav_put(36, "data");
    wav_dw(40, 8);              /* sample data length */
}

static void t_wavfile (void)
{
    x16_wav_info info;

    wav_build();
    t_check(x16_wav_parse_header(wav) == 1, "WAV_PARSE");

    x16_wav_get_info(&info);
    t_check(info.format == 1 && info.channels == 2 && info.bits == 16,
            "WAV_FIELDS");
    t_check(info.rate == 22050UL, "WAV_RATE");
    t_check(info.data_off == 44 && info.data_len == 8UL, "WAV_DATA");

    /* The standalone accessors must agree with the struct. */
    t_check(x16_wav_rate() == info.rate &&
            x16_wav_data_len() == info.data_len, "WAV_ACCESSORS");

    /* A non-PCM format code is reported, not rejected: IMA ADPCM is 17
    ** and x16/adpcm.h decodes it.
    */
    wav_build();
    wav_w(20, 17);
    t_check(x16_wav_parse_header(wav) == 1, "WAV_NONPCM_OK");
    x16_wav_get_info(&info);
    t_check(info.format == 17, "WAV_NONPCM_CODE");

    /* Corrupt each magic in turn: all must fail. */
    wav_build();
    wav[0] = 'X';
    t_check(x16_wav_parse_header(wav) == 0, "WAV_BAD_RIFF");

    wav_build();
    wav[8] = 'X';
    t_check(x16_wav_parse_header(wav) == 0, "WAV_BAD_WAVE");

    wav_build();
    wav[36] = 'X';
    t_check(x16_wav_parse_header(wav) == 0, "WAV_BAD_DATA");
}

/* --- zsm ------------------------------------------------------------- */

/* A ZSM header is 16 bytes: 'z','m', version, loop offset (3),
** PCM offset (3), FM channel mask, PSG channel mask (2), tick rate
** (2), then two reserved. The stream follows at header+16.
**
** This stream writes PSG voice 0's frequency low byte and then ends,
** which is enough to prove the command decoder runs and that the
** module reports its own state honestly.
*/
static unsigned char zsm[32];

static void zsm_build (void)
{
    memset(zsm, 0, sizeof zsm);
    zsm[0] = 'z';
    zsm[1] = 'm';
    zsm[2] = 1;                 /* version 1 */
    zsm[12] = 60;               /* tick rate low: 60 Hz */
    zsm[13] = 0;
    /* stream at +16: one PSG write (reg 0 = voice 0 freq low), then EOF */
    /* Commands: $00-$3F is a PSG write (the byte IS the register, one
    ** value byte follows), $40 is EXTCMD, $41-$7F a YM pair count,
    ** $80 is EOF, $81-$FF a delay.
    */
    zsm[16] = 0x00;             /* PSG register 0... */
    zsm[17] = 0x42;             /* ...= $42 */
    zsm[18] = 0x80;             /* EOF */
}

static void t_zsm (void)
{
    unsigned char ok;

    zsm_build();
    ok = x16_zsm_init(zsm);
    t_check(ok == X16_ZSM_ERR_NONE, "ZSM_INIT");
    t_check(x16_zsm_lasterr() == 0, "ZSM_LASTERR_OK");
    t_check(x16_zsm_get_tickrate() == 60, "ZSM_TICKRATE");

    /* Bad magic and a too-new version must both fail, and lasterr must
    ** distinguish them -- that memory is the whole point of the module's
    ** two-channel answer (carry plus code).
    */
    zsm_build();
    zsm[0] = 'X';
    t_check(x16_zsm_init(zsm) == X16_ZSM_ERR_MAGIC, "ZSM_BAD_MAGIC");
    t_check(x16_zsm_lasterr() != 0, "ZSM_LASTERR_MAGIC");

    zsm_build();
    zsm[2] = 99;
    t_check(x16_zsm_init(zsm) == X16_ZSM_ERR_VERSION, "ZSM_BAD_VERSION");
    t_check(x16_zsm_lasterr() != 0, "ZSM_LASTERR_VERSION");

    /* play/stop/status agree with each other. */
    zsm_build();
    x16_zsm_init(zsm);
    x16_zsm_play();
    t_check(x16_zsm_status() != 0, "ZSM_PLAY_ACTIVE");
    x16_zsm_stop();
    t_check((x16_zsm_status() & 1) == 0, "ZSM_STOP_INACTIVE");

    /* One tick of the stream above reaches its EOF and clears active. */
    zsm_build();
    x16_zsm_init(zsm);
    x16_zsm_play();
    x16_zsm_tick();
    t_check((x16_zsm_status() & 1) == 0, "ZSM_TICK_TO_EOF");

    /* rewind makes a stopped stream playable again. */
    x16_zsm_rewind();
    x16_zsm_play();
    t_check(x16_zsm_status() != 0, "ZSM_REWIND");
    x16_zsm_stop();
}

/* --- vdc (VERA's display composer) ------------------------------------ */

/* Runs last: it reprograms scale, border and the active window. Every
** value is read back through the module's own getter AND, where the
** register is plainly readable, checked independently.
*/
#define DC_BORDER (*(volatile unsigned char *)0x9F2CU)

static void t_vdc (void)
{
    unsigned int scale;
    x16_vdc_active act;

    /* Layer enables are a bitmask in DC_VIDEO. */
    x16_vdc_set_layers(0x10);           /* layer 0 only */
    t_check((x16_vdc_get_video() & 0x30) == 0x10, "VDC_LAYERS");
    x16_vdc_layer_on(0x20);
    t_check((x16_vdc_get_video() & 0x30) == 0x30, "VDC_LAYER_ON");
    x16_vdc_layer_off(0x10);
    t_check((x16_vdc_get_video() & 0x30) == 0x20, "VDC_LAYER_OFF");

    /* Scale packs H in the low byte and V in the high byte. $40 is the
    ** 2x setting that makes a 320-wide bitmap fill the screen.
    */
    x16_vdc_set_scale(0x40, 0x80);
    scale = x16_vdc_get_scale();
    t_check((scale & 0xFF) == 0x40 && (scale >> 8) == 0x80, "VDC_SCALE");

    x16_vdc_set_border(6);
    t_check(x16_vdc_get_border() == 6 && DC_BORDER == 6, "VDC_BORDER");

    /* The active window is stored in the register's own units (H in
    ** 4-pixel steps, V in 2), so set_active must divide -- read the raw
    ** block back and check the arithmetic, not just the round-trip.
    */
    x16_vdc_set_active(32, 608, 24, 456);
    x16_vdc_get_active_raw(&act);
    t_check(act.hstart == 32 / 4 && act.hstop == 608 / 4 &&
            act.vstart == 24 / 2 && act.vstop == 456 / 2,
            "VDC_ACTIVE");

    x16_vdc_fullscreen();
    x16_vdc_get_active_raw(&act);
    t_check(act.hstart == 0 && act.hstop == 640 / 4 &&
            act.vstart == 0 && act.vstop == 480 / 2,
            "VDC_FULLSCREEN");

    /* Put the display back the way a text-mode program expects it. */
    x16_vdc_set_scale(0x80, 0x80);
    x16_vdc_set_border(0);
    x16_vdc_set_layers(0x20);
}

int main (void)
{
    t_init();

    t_audiorom();
    t_wavfile();
    t_zsm();
    t_vdc();                    /* last: it reprograms the display */

    t_done();
    return 0;
}
