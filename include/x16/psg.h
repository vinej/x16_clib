/* =====================================================================
 * x16clib :: x16/psg.h -- VERA PSG, 16 voices
 * =====================================================================
 * A voice is four bytes in VRAM at $1F9C0: frequency word, then
 * pan|volume, then waveform|width.
 *
 *      freq_word = Hz * 2.68435 (approximately)
 *
 * so A4 at 440 Hz is 1181. X16_PSG_HZ() does the conversion for
 * compile-time constants.
 *
 * The PSG lives in VERA's write-only region, so x16_psg_note_off()'s
 * read-modify-write only works on a voice this program has already set.
 * Call x16_psg_init() first.
 *
 * For music rather than sound effects, look at ZSMKit -- the community's
 * ZSM player -- which drives this hardware from a data stream.
 * =====================================================================
 */

#ifndef X16_PSG_H
#define X16_PSG_H

/* Frequency word for a pitch in Hz. One step is 25000000/512 / 2^17 Hz,
** so the word is hz * 2.68435456.
**
** cc65 has no floating point, so that multiplier is scaled to 175922/2^16
** and the shift is rounded rather than truncated. The result is within
** one step (0.37 Hz) of exact for every pitch up to 24 kHz, and the
** largest product still fits in 32 bits. A literal `hz` folds at compile
** time; a variable costs one 32-bit multiply.
**
**      X16_PSG_HZ(440) == 1181         X16_PSG_HZ(880) == 2362
*/
#define X16_PSG_HZ(hz) \
        ((unsigned int)(((unsigned long)(hz) * 175922UL + 32768UL) >> 16))

/* Panning, for x16_psg_set_vol(). Neither bit set is silence. */
#define X16_PSG_PAN_LEFT        0x40
#define X16_PSG_PAN_RIGHT       0x80
#define X16_PSG_PAN_BOTH        0xC0

/* Waveform, for x16_psg_set_wave(). */
#define X16_PSG_WAVE_PULSE      0x00
#define X16_PSG_WAVE_SAWTOOTH   0x40
#define X16_PSG_WAVE_TRIANGLE   0x80
#define X16_PSG_WAVE_NOISE      0xC0

/* Silence all 16 voices. */
void x16_psg_init (void);

/* voice is 0-15. */
void __fastcall__ x16_psg_set_freq (unsigned char voice, unsigned int freq);

/* vol 0-63, pan is an X16_PSG_PAN_* constant. */
void __fastcall__ x16_psg_set_vol (unsigned char voice, unsigned char vol,
                                   unsigned char pan);

/* wave is an X16_PSG_WAVE_* constant; width is the pulse width, or the
** XOR amount for the other waveforms, 0-63.
*/
void __fastcall__ x16_psg_set_wave (unsigned char voice, unsigned char wave,
                                    unsigned char width);

/* Volume to zero, panning kept. Safe from an ISR. */
void __fastcall__ x16_psg_note_off (unsigned char voice);

/* Point data port 0 at one register of a voice, on auto-increment.
** offset is 0-3.
*/
void __fastcall__ x16_psg_voice_ptr (unsigned char voice,
                                     unsigned char offset);

/* ---------------------------------------------------------------------
 * ASR envelopes -- the decay everybody hand-rolls in the frame loop.
 *
 * Attack ramps the volume up to `peak`, sustain holds it, release ramps
 * it back to silence. Set the voice's frequency, wave and pan first; the
 * envelope drives only the volume bits and leaves the panning alone.
 *
 *      x16_psg_set_freq(0, X16_PSG_HZ(880));
 *      x16_psg_set_wave(0, X16_PSG_WAVE_PULSE, 32);
 *      x16_psg_set_vol(0, 0, X16_PSG_PAN_BOTH);        // pan only
 *      x16_psg_env_start(0, 60, 8, 20, 4);
 *      ...
 *      for (;;) { x16_vsync_wait(); x16_psg_env_tick(); }
 *
 * x16_psg_env_tick() must be called once per frame. From a VSYNC
 * callback is fine -- the IRQ wrapper saves the zero page for you.
 * ------------------------------------------------------------------ */

/* attack = 0 jumps straight to the peak (audible immediately).
** sustain = 0 releases at once; 255 holds until x16_psg_env_release().
** release = 0 holds at the peak until x16_psg_env_stop().
*/
void __fastcall__ x16_psg_env_start (unsigned char voice,
                                     unsigned char peak,
                                     unsigned char attack,
                                     unsigned char sustain,
                                     unsigned char release);

/* Enter the release phase now. */
void __fastcall__ x16_psg_env_release (unsigned char voice);

/* Silence and disarm immediately. */
void __fastcall__ x16_psg_env_stop (unsigned char voice);

/* Advance every armed envelope one step. Once per frame. */
void x16_psg_env_tick (void);

#endif /* X16_PSG_H */
