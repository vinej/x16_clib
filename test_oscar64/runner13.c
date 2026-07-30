/* =====================================================================
 * x16clib :: test_oscar64/runner13.c -- the AUDIO ROM bank's API
 * =====================================================================
 * Standalone suite:
 *
 *      .\build_oscar64.ps1 -Test -Source test_oscar64\runner13.c
 *
 * Ported from the audiorom half of test_ca65/runner8.c, check for check.
 *
 * The ROM bank is banked in by the library on every call, so the only
 * precondition is a ROM with an audio bank -- which the emulator has.
 * The note helpers are total functions over their input ranges, so they
 * are checkable against arithmetic the test does for itself: an octave
 * up must double the PSG frequency, rising notes must give rising
 * frequency words, and the FM key-code mapping must round-trip.
 *
 * TWO KERNAL BUGS ARE PINNED HERE rather than worked around. Both are
 * real defects in x16-rom r49, and both are asserted at their WRONG
 * values on purpose: if a later ROM fixes them, these tests fail and
 * say so, instead of the library quietly changing behaviour underneath
 * a caller. Each one names the ROM source file it lives in.
 *
 * The play-string calls are absent on purpose: x16_ar_fmplaystring()
 * and x16_ar_psgplaystring() block until the music ends, pacing on the
 * jiffy clock, and would never return in the headless testbench.
 * ===================================================================== */

#include "testlib.h"
#include <x16/audiorom.h>
#include <x16/bank.h>

static void t_audiorom(void)
{
    unsigned char ok;
    unsigned int lo, hi, twice, diff;

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
    lo = x16_ar_note_bas2psg(65, 0);            /* middle C */
    hi = x16_ar_note_bas2psg(81, 0);            /* one octave up */
    twice = (unsigned int)(lo * 2);
    if (hi > twice) {
        diff = hi - twice;
    } else {
        diff = twice - hi;
    }
    t_check(diff <= (twice / 64), "AR_PSG_OCTAVE");

    /* The PSG shadow registers read back what was set. */
    x16_ar_psg_init();
    x16_ar_psg_setvol(0, 40);
    x16_ar_psg_setpan(0, 3);
    t_check(x16_ar_psg_getpan(0) == 3, "AR_PSG_PAN");

    /* KERNAL BUG (x16-rom r49, audio/psg.s): psg_getatten loads the
    ** attenuation into .A, then runs RESTORE_BANK -- which clobbers .A
    ** with the saved RAM bank -- and only then does `tax`. So it hands
    ** back the RAM bank that was current before the call, never the
    ** attenuation. psg_getpan escapes it by doing its `tax` BEFORE the
    ** restore. Pinned against the bank we set, so a fixed ROM trips
    ** this test rather than changing under us.
    */
    x16_ar_psg_setatten(1, 12);
    x16_bank_set(5);
    x16_ar_psg_setatten(1, 12);
    t_check(x16_ar_psg_getatten(1) == 5, "AR_PSG_ATTEN_ROMBUG");
    x16_bank_set(1);
}

int main(void)
{
    t_init();

    t_audiorom();

    t_done();
    return 0;
}
