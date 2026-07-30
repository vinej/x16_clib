/* =====================================================================
 * x16clib :: x16/audiorom.h -- the AUDIO ROM bank's API, wrapped
 * =====================================================================
 * The X16 ROM ships an audio driver in its own bank: FM patches, PSG
 * and YM volume/pan/attenuation shadows, note conversion tables, and
 * the BASIC FMPLAY/PSGPLAY engine. These wrappers cross into that bank
 * with the KERNAL's JSRFAR, so they are safe to call from anywhere.
 *
 * The ar_ layer and this library's own x16_psg_* / x16_ym_* modules
 * both drive the same hardware, but only the ROM keeps shadows of what
 * it wrote. Pick one layer per voice: raw x16_psg_* writes are
 * invisible to x16_ar_psg_read() and will not be re-applied by the
 * ROM's attenuation arithmetic.
 *
 *      x16_ar_audio_init();
 *      x16_ar_ym_playnote(0, x16_ar_note_midi2fm(69), 0, 0);
 *
 * Unless noted otherwise a return of 0 means success and 1 means the
 * ROM reported failure (a YM busy timeout, an out-of-range input).
 *
 * BLOCKING: x16_ar_fmplaystring() and x16_ar_psgplaystring() play the
 * whole string before returning, pacing themselves on the 60 Hz jiffy
 * clock -- which only ticks while the VSYNC interrupt is running. The
 * chordstring calls strike their notes and return immediately.
 * =====================================================================
 */

#ifndef X16_AUDIOROM_H
#define X16_AUDIOROM_H

/* x16_ar_ym_get_chip_type() results. */
#define X16_AR_YM_NONE          0
#define X16_AR_YM_OPP           1
#define X16_AR_YM_OPM           2
#define X16_AR_YM_UNKNOWN       3

/* Pan values for the setpan calls. */
#define X16_AR_PAN_OFF          0
#define X16_AR_PAN_LEFT         1
#define X16_AR_PAN_RIGHT        2
#define X16_AR_PAN_BOTH         3

/* ---------------------------------------------------------------------
 * One-call setup: YM init, PSG init, default FM patches on all eight
 * channels. Equivalent to ym_init + psg_init + ym_loaddefpatches.
 * ------------------------------------------------------------------ */
unsigned char x16_ar_audio_init (void);

/* ---------------------------------------------------------------------
 * BASIC-compatible helpers. `octnote` packs (octave << 4) | note with
 * note 1-12 (1 = C); note 0 releases the channel. `noretrigger` nonzero
 * changes pitch without restarting the envelope.
 * ------------------------------------------------------------------ */
unsigned char x16_ar_fmfreq (unsigned char channel,
                                          unsigned int hz,
                                          unsigned char noretrigger);
unsigned char x16_ar_fmnote (unsigned char channel,
                                          unsigned char octnote,
                                          unsigned char kf,
                                          unsigned char noretrigger);
unsigned char x16_ar_fmvib (unsigned char speed,
                                         unsigned char depth);
unsigned char x16_ar_psgfreq (unsigned char voice,
                                           unsigned int hz);
unsigned char x16_ar_psgnote (unsigned char voice,
                                           unsigned char octnote,
                                           unsigned char kf);
unsigned char x16_ar_psgwav (unsigned char voice,
                                          unsigned char waveform);

/* Play-string engine (FMPLAY/PSGPLAY syntax). The playstring calls
** BLOCK until the music ends -- see the header comment.
**
** CHARSET TRAP: the ROM parser matches note letters against $41-$5A,
** the codes BASIC strings use. A C string literal is encoded however the
** including program asked for -- so put `#pragma encoding(ascii)` in the
** file that writes the literal, or spell the notes in explicit bytes.
** Getting this wrong is silent: the parser simply matches nothing.
*/
void x16_ar_playstring_voice (unsigned char voice);
void x16_ar_fmplaystring (const char *s, unsigned char len);
void x16_ar_psgplaystring (const char *s, unsigned char len);
void x16_ar_fmchordstring (const char *s, unsigned char len);
void x16_ar_psgchordstring (const char *s, unsigned char len);

/* ---------------------------------------------------------------------
 * Note conversions, between four pitch spaces: BASIC oct/note bytes,
 * MIDI note numbers, YM KC/KF pairs and 17-bit-VERA PSG frequencies.
 * An out-of-range input converts to 0.
 *
 * The word-returning calls pack two bytes: low byte = the note/KC,
 * high byte = KF (the fractional semitone), except the psg-frequency
 * returns, which are one 16-bit number.
 * ------------------------------------------------------------------ */
/* KERNAL DEFECTS in the audio bank, found while testing this wrapper
** against x16-rom r49. The wrappers pass arguments through correctly;
** these are the ROM's own, so they are documented rather than papered
** over, and test_ca65/runner8.c pins each one:
**
**   notecon_midi2bas  indexes the bas2midi table instead of midi2bas,
**                     so x16_ar_note_midi2bas() returns the wrong note
**                     (MIDI 60 gives 59, not 65). bas2midi is fine.
**   psg_getatten      loads the value into .A, then RESTORE_BANK
**                     overwrites .A with the saved RAM bank, and only
**                     then copies to .X -- so x16_ar_psg_getatten()
**                     hands back the RAM bank current before the call.
**                     psg_getpan does its copy first and is correct.
**
** BASIC oct/note, while you are here, is octave*16 + note + 1: twelve
** notes then four unused codes per octave, so middle C is 65 and the
** codes in the gaps convert to 0.
*/
unsigned char x16_ar_note_bas2fm (unsigned char octnote);
unsigned char x16_ar_note_bas2midi (unsigned char octnote);
unsigned int x16_ar_note_bas2psg (unsigned char octnote,
                                               unsigned char kf);
unsigned char x16_ar_note_fm2bas (unsigned char kc);
unsigned char x16_ar_note_fm2midi (unsigned char kc);
unsigned int x16_ar_note_fm2psg (unsigned char kc,
                                              unsigned char kf);
unsigned int x16_ar_note_freq2bas (unsigned int hz);
unsigned int x16_ar_note_freq2fm (unsigned int hz);
unsigned int x16_ar_note_freq2midi (unsigned int hz);
unsigned int x16_ar_note_freq2psg (unsigned int hz);
unsigned char x16_ar_note_midi2bas (unsigned char midinote);
unsigned char x16_ar_note_midi2fm (unsigned char midinote);
unsigned int x16_ar_note_midi2psg (unsigned char midinote,
                                                unsigned char kf);
unsigned int x16_ar_note_psg2bas (unsigned int freq);
unsigned int x16_ar_note_psg2fm (unsigned int freq);
unsigned int x16_ar_note_psg2midi (unsigned int freq);

/* ---------------------------------------------------------------------
 * ROM PSG layer. Registers are the PSG's own map: 4 per voice --
 * 0 freq low, 1 freq high, 2 = L/R gate | volume, 3 = waveform | duty.
 * ------------------------------------------------------------------ */
void x16_ar_psg_init (void);
void x16_ar_psg_playfreq (unsigned char voice, unsigned int freq);
void x16_ar_psg_setfreq (unsigned char voice, unsigned int freq);
void x16_ar_psg_setvol (unsigned char voice, unsigned char vol);
void x16_ar_psg_setatten (unsigned char voice, unsigned char atten);
void x16_ar_psg_setpan (unsigned char voice, unsigned char pan);
unsigned char x16_ar_psg_getatten (unsigned char voice);
unsigned char x16_ar_psg_getpan (unsigned char voice);

/* Raw register access that still keeps the ROM's shadows coherent.
** `cooked` nonzero reads volumes with attenuation applied; zero reads
** back exactly what was written. x16_ar_psg_write_fast() assumes the
** caller already pointed VERA at the PSG -- it is the bare fast path.
*/
void x16_ar_psg_write (unsigned char reg, unsigned char value);
void x16_ar_psg_write_fast (unsigned char reg, unsigned char value);
unsigned char x16_ar_psg_read (unsigned char reg,
                                            unsigned char cooked);

/* ---------------------------------------------------------------------
 * ROM YM/FM layer. `kc` is the YM2151 key code, `kf` the key fraction.
 * ------------------------------------------------------------------ */
unsigned char x16_ar_ym_init (void);
unsigned char x16_ar_ym_loaddefpatches (void);

/* Load one of the 32 ROM patches, or a 26-byte patch image from RAM. */
unsigned char x16_ar_ym_loadpatch (unsigned char channel,
                                                unsigned char patch);
unsigned char x16_ar_ym_loadpatch_ram (unsigned char channel,
                                                    const void *patch);

/* Load a patch from an already-OPENed logical file. */
unsigned char x16_ar_ym_loadpatchlfn (unsigned char channel,
                                                   unsigned char lfn);

unsigned char x16_ar_ym_playnote (unsigned char channel,
                                               unsigned char kc,
                                               unsigned char kf,
                                               unsigned char noretrigger);
unsigned char x16_ar_ym_playdrum (unsigned char channel,
                                               unsigned char midinote);
unsigned char x16_ar_ym_setnote (unsigned char channel,
                                              unsigned char kc,
                                              unsigned char kf);
unsigned char x16_ar_ym_setdrum (unsigned char channel,
                                              unsigned char midinote);
unsigned char x16_ar_ym_trigger (unsigned char channel,
                                              unsigned char noretrigger);
unsigned char x16_ar_ym_release (unsigned char channel);
unsigned char x16_ar_ym_setatten (unsigned char channel,
                                               unsigned char atten);
unsigned char x16_ar_ym_setpan (unsigned char channel,
                                             unsigned char pan);
unsigned char x16_ar_ym_getatten (unsigned char channel);
unsigned char x16_ar_ym_getpan (unsigned char channel);

/* Raw YM register access through the ROM, which is what keeps its
** shadows honest -- x16_ym_write() in ym.h does not. `cooked` nonzero
** reads TL values with attenuation applied.
*/
unsigned char x16_ar_ym_write (unsigned char reg,
                                            unsigned char value);
unsigned char x16_ar_ym_read (unsigned char reg,
                                           unsigned char cooked);

/* Which chip the ROM detected at init: X16_AR_YM_*. */
unsigned char x16_ar_ym_get_chip_type (void);

/* pulls the implementation in with this header */
#pragma compile("audiorom.c")

#endif /* X16_AUDIOROM_H */
