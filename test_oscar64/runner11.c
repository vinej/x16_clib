/* =====================================================================
 * x16clib :: test_oscar64/runner11.c -- wavfile, zsm, vdc
 * =====================================================================
 * Standalone suite:
 *
 *      .\build_oscar64.ps1 -Test -Source test_oscar64\runner11.c
 *
 * Ported from test_ca65/runner8.c, matching its wavfile / zsm / vdc
 * coverage check for check. (runner8's audiorom half arrives with that
 * module, in its own suite.)
 *
 * All three are testable headlessly for the same reason: none of them
 * needs a device to answer. The WAV parser reads a header this file
 * builds field by field, so every test can corrupt exactly one thing;
 * the ZSM player walks a stream built here and reports its own state;
 * and VERA's display composer registers read back what was written, so
 * the arithmetic in set_active() can be checked against the raw block
 * rather than only round-tripped.
 *
 * String literals are ASCII because testlib.h sets #pragma
 * encoding(ascii) globally -- the same reason ca65's runner8 can write
 * "RIFF" and get RIFF.
 * ===================================================================== */

#include "testlib.h"
#include <x16/wavfile.h>
#include <x16/zsm.h>
#include <x16/vdc.h>

/* --- wavfile --------------------------------------------------------- */

/* A canonical 44-byte PCM header, built field by field. All multi-byte
** fields little-endian.
*/
static unsigned char wav[64];

static void wav_zero(void)
{
    unsigned char i;

    for (i = 0; i < 64; ++i) {
        wav[i] = 0;
    }
}

static void wav_put(unsigned int off, const char *s)
{
    while (*s) {
        wav[off] = (unsigned char)*s;
        ++off;
        ++s;
    }
}

static void wav_dw(unsigned int off, unsigned long v)
{
    wav[off]     = (unsigned char)(v & 0xFF);
    wav[off + 1] = (unsigned char)((v >> 8) & 0xFF);
    wav[off + 2] = (unsigned char)((v >> 16) & 0xFF);
    wav[off + 3] = (unsigned char)((v >> 24) & 0xFF);
}

static void wav_w(unsigned int off, unsigned int v)
{
    wav[off]     = (unsigned char)(v & 0xFF);
    wav[off + 1] = (unsigned char)(v >> 8);
}

static void wav_build(void)
{
    wav_zero();
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

static void t_wavfile(void)
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

    /* Byte for byte, not merely equal. Oscar64 1.32.272 miscompiled the
    ** obvious OR-chain form of the parser's 32-bit read and dropped the
    ** LOW byte of every value -- 22050 came back as $5600, which an
    ** "== 22050" check catches but a sloppier one might not. wavfile.c
    ** pins the working form; this is what proves it still works.
    */
    t_check((unsigned char)(info.rate & 0xFF) == 0x22 &&
            (unsigned char)((info.rate >> 8) & 0xFF) == 0x56 &&
            (unsigned char)((info.rate >> 16) & 0xFF) == 0x00 &&
            (unsigned char)((info.rate >> 24) & 0xFF) == 0x00 &&
            (unsigned char)(info.data_len & 0xFF) == 0x08 &&
            (unsigned char)((info.data_len >> 8) & 0xFF) == 0x00,
            "WAV_LONG_BYTES");

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

static void zsm_build(void)
{
    unsigned char i;

    for (i = 0; i < 32; ++i) {
        zsm[i] = 0;
    }
    zsm[0] = 'z';
    zsm[1] = 'm';
    zsm[2] = 1;                 /* version 1 */
    zsm[12] = 60;               /* tick rate low: 60 Hz */
    zsm[13] = 0;
    /* stream at +16: one PSG write (reg 0 = voice 0 freq low), then EOF.
    ** Commands: $00-$3F is a PSG write (the byte IS the register, one
    ** value byte follows), $40 is EXTCMD, $41-$7F a YM pair count,
    ** $80 is EOF, $81-$FF a delay.
    */
    zsm[16] = 0x00;             /* PSG register 0... */
    zsm[17] = 0x42;             /* ...= $42 */
    zsm[18] = 0x80;             /* EOF */
}

static void t_zsm(void)
{
    unsigned char ok;

    zsm_build();
    ok = x16_zsm_init(zsm);
    t_check(ok == X16_ZSM_ERR_NONE, "ZSM_INIT");
    t_check(x16_zsm_lasterr() == 0, "ZSM_LASTERR_OK");
    t_check(x16_zsm_get_tickrate() == 60, "ZSM_TICKRATE");

    /* Bad magic and a too-new version must both fail, and lasterr must
    ** distinguish them.
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

static void t_vdc(void)
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

int main(void)
{
    t_init();

    t_wavfile();
    t_zsm();
    t_vdc();                    /* last: it reprograms the display */

    t_done();
    return 0;
}
