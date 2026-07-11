/* =====================================================================
 * x16clib :: x16/psg.h -- VERA PSG (16 voices)
 * =====================================================================
 * The voices live in VRAM at $1F9C0, four bytes each:
 *   0  frequency 7:0
 *   1  frequency 15:8
 *   2  right(7) | left(6) | volume(5:0)
 *   3  waveform(7:6) | pulse width or XOR(5:0)
 *
 * That VRAM range is write-only. Reads return the last value the host
 * wrote, which is why x16_psg_note_off's read-modify-write only works
 * on a voice this program has already set.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** KickC build. The API is identical to the cc65 build's; what differs is
** the delivery. KickC has no linker and no archive format -- it compiles
** the whole program from source and strips what goes unused -- so the
** KickC port is a SOURCE distribution. Include this header; the matching
** implementation in src_kickc/x16/ is compiled in automatically when the
** library path points there:
**
**     kickc -p cx16 -a -I include_kickc -L src_kickc yourprog.c
** --------------------------------------------------------------------- */

#ifndef X16_PSG_H
#define X16_PSG_H

#include <x16/zpsafe.h>

/* Frequency word for a pitch in Hz. One step is 25000000/512 / 2^17 Hz,
** so the word is hz * 2.68435456, scaled to integer arithmetic: the
** multiplier is 175922/2^16 and the shift is rounded. Within one step
** (0.37 Hz) of exact for every pitch up to 24 kHz. A literal `hz` folds
** at compile time.
**
**      X16_PSG_HZ(440) == 1181         X16_PSG_HZ(880) == 2362
*/
#define X16_PSG_HZ(hz) \
        ((unsigned int)(((unsigned long)(hz) * 175922 + 32768) >> 16))

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
void x16_psg_set_freq (unsigned char voice, unsigned int freq);

/* vol 0-63, pan is an X16_PSG_PAN_* constant. */
void x16_psg_set_vol (unsigned char voice, unsigned char vol,
                      unsigned char pan);

/* wave is an X16_PSG_WAVE_* constant; width is the pulse width, or the
** XOR amount for the other waveforms, 0-63.
*/
void x16_psg_set_wave (unsigned char voice, unsigned char wave,
                       unsigned char width);

/* Volume to zero, panning kept. */
void x16_psg_note_off (unsigned char voice);

/* Point data port 0 at one register of a voice, on auto-increment.
** offset is 0-3.
*/
void x16_psg_voice_ptr (unsigned char voice, unsigned char offset);

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
void x16_psg_env_start (unsigned char voice, unsigned char peak,
                        unsigned char attack, unsigned char sustain,
                        unsigned char release);

/* Enter the release phase now. */
void x16_psg_env_release (unsigned char voice);

/* Silence and disarm immediately. */
void x16_psg_env_stop (unsigned char voice);

/* Advance every armed envelope one step. Once per frame. */
void x16_psg_env_tick (void);

#endif /* X16_PSG_H */
