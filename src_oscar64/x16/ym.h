/* =====================================================================
 * x16clib :: x16/ym.h -- YM2151 FM synthesiser
 * =====================================================================
 * Two ways in, and they do not mix freely:
 *
 *   x16_ym_write  writes a chip register directly. Fast, complete
 *                 access to everything (LFO, per-operator envelopes) --
 *                 but the ROM audio driver keeps RAM shadows of volume
 *                 and pan, and a raw write leaves those stale.
 *
 *   x16_ym_poke   goes through the ROM driver, keeping its shadows
 *                 coherent. Use this if you also use the note API.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** Oscar64 build. The API is identical to the cc65 build's; what differs
** is the delivery. Oscar64 compiles the whole program at once and strips
** what goes unused, so this port is a SOURCE distribution: headers and
** implementations sit side by side in src_oscar64/x16/, and the
** `#pragma compile` at the bottom of this header pulls the matching .c
** in automatically:
**
**     oscar64 -tm=x16 -n -i=src_oscar64 -o=YOURPROG.PRG yourprog.c
** --------------------------------------------------------------------- */

#ifndef X16_YM_H
#define X16_YM_H

/* Panning, for x16_ym_pan(). */
#define X16_YM_PAN_OFF          0
#define X16_YM_PAN_LEFT         1
#define X16_YM_PAN_RIGHT        2
#define X16_YM_PAN_BOTH         3

/* Build the packed note x16_ym_note_bas() wants: octave 0-7, semitone
** 1-12 (1 = C). A note of 0 releases.
*/
#define X16_YM_NOTE(octave, semitone)   (((octave) << 4) | (semitone))
#define X16_YM_NOTE_RELEASE             0

/* Retrigger the envelope, or only bend the pitch. */
#define X16_YM_RETRIGGER        1
#define X16_YM_HOLD             0

/* Reset the chip and load the default instrument patches. Returns 0 if
** there is no YM2151. Must precede x16_ym_patch_*.
*/
unsigned char x16_ym_init (void);

/* Raw register write: complete access, stale shadows. Returns 0 if the
** chip stayed busy.
*/
unsigned char x16_ym_write (unsigned char reg, unsigned char value);

/* The same write through the ROM driver, keeping its shadows coherent. */
void x16_ym_poke (unsigned char reg, unsigned char value);

/* 1 while the chip is busy. Not an error. */
unsigned char x16_ym_busy (void);

/* channel is 0-7 throughout. */

/* patch is a ROM instrument index, 0-162. */
unsigned char x16_ym_patch_rom (unsigned char channel, unsigned char patch);

/* patch points at an instrument definition in RAM. */
unsigned char x16_ym_patch_ram (unsigned char channel, const void *patch);

/* A packed note from X16_YM_NOTE(), 0 to release. This is the one for
** playing tunes: the ROM converts it to a key code.
*/
unsigned char x16_ym_note_bas (unsigned char channel, unsigned char note,
                               unsigned char retrigger);

/* A raw YM2151 key code and key fraction (pitch bend). */
void x16_ym_note (unsigned char channel, unsigned char kc,
                  unsigned char kf, unsigned char retrigger);

void x16_ym_release_note (unsigned char channel);

/* atten 0 is the patch's own volume; larger is quieter. */
unsigned char x16_ym_vol (unsigned char channel, unsigned char atten);

unsigned char x16_ym_pan (unsigned char channel, unsigned char pan);

/* Drum note 25-87. */
unsigned char x16_ym_drum (unsigned char channel, unsigned char note);

/* Read the ROM driver's shadows. These agree with the chip only if you
** have been writing through x16_ym_poke / _vol / _pan rather than the
** raw x16_ym_write.
*/
unsigned char x16_ym_get_pan (unsigned char channel);
unsigned char x16_ym_get_vol (unsigned char channel);

/* pulls the implementation in with this header */
#pragma compile("ym.c")

#endif /* X16_YM_H */
