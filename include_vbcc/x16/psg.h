/* =====================================================================
 * x16clib :: x16/psg.h -- VERA PSG, 16 voices
 * =====================================================================
 * A voice is four bytes in VRAM at $1F9C0: frequency word, then
 * pan|volume, then waveform|width.
 *
 * The PSG lives in VERA's write-only region, so x16_psg_note_off()'s
 * read-modify-write only works on a voice this program has already set.
 * Call x16_psg_init() first.
 * =====================================================================
 */

#ifndef X16_PSG_H
#define X16_PSG_H

/* Frequency word for a pitch in Hz. One step is 25000000/512 / 2^17 Hz,
** so the word is hz * 2.68435456, scaled to 175922/2^16.
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
void x16_psg_init(void);

void x16_psg_set_freq(__reg("r0") unsigned char voice, __reg("r2/r3") unsigned int freq);

/* vol 0-63, pan an X16_PSG_PAN_* mask. */
void x16_psg_set_vol(__reg("r0") unsigned char voice, __reg("r2") unsigned char vol,
                     __reg("r4") unsigned char pan);

/* wave an X16_PSG_WAVE_* code, width the pulse width 0-63. */
void x16_psg_set_wave(__reg("r0") unsigned char voice, __reg("r2") unsigned char wave,
                      __reg("r4") unsigned char width);

void x16_psg_note_off(__reg("r0") unsigned char voice);

/* Point data port 0 at one of a voice's four bytes, for streaming. */
void x16_psg_voice_ptr(__reg("r0") unsigned char voice, __reg("r2") unsigned char offset);

/* A software AD-S-R envelope over the voice's volume.
**      x16_psg_set_freq(0, X16_PSG_HZ(880));
**      x16_psg_set_wave(0, X16_PSG_WAVE_PULSE, 32);
**      x16_psg_set_vol(0, 0, X16_PSG_PAN_BOTH);        // pan only
**      x16_psg_env_start(0, 60, 8, 20, 4);
**      for (;;) { x16_vsync_wait(); x16_psg_env_tick(); }
**
** x16_psg_env_tick() must be called once per frame. sustain = 0 releases
** at once; 255 holds until x16_psg_env_release(). release = 0 holds at the
** peak until x16_psg_env_stop(). (Five char args: release rides the C soft
** stack.) */
void x16_psg_env_start(__reg("r0") unsigned char voice,
                       __reg("r2") unsigned char peak,
                       __reg("r4") unsigned char attack,
                       __reg("r6") unsigned char sustain,
                       unsigned char release);

void x16_psg_env_release(__reg("a") unsigned char voice);
void x16_psg_env_stop(__reg("a") unsigned char voice);
void x16_psg_env_tick(void);

#endif /* X16_PSG_H */
