/* =====================================================================
 * x16clib :: x16/ym.h -- YM2151 FM synthesiser
 * =====================================================================
 * Two ways in, and they do not mix freely:
 *
 *   x16_ym_write()  writes a chip register directly. Fast, and the only
 *                   route to the LFO and per-operator envelopes -- but the
 *                   ROM audio driver keeps RAM shadows of volume and pan,
 *                   and a raw write leaves those stale.
 *
 *   x16_ym_poke()   goes through the ROM driver, keeping its shadows
 *                   coherent. Use this if you also use the note API.
 *
 * Call x16_ym_init() before anything else; it returns 0 if the machine
 * has no YM2151.
 * =====================================================================
 */

#ifndef X16_YM_H
#define X16_YM_H

/* Panning, for x16_ym_pan(). */
#define X16_YM_PAN_OFF          0
#define X16_YM_PAN_LEFT         1
#define X16_YM_PAN_RIGHT        2
#define X16_YM_PAN_BOTH         3

/* Build the packed note x16_ym_note_bas() wants: octave 0-7, semitone
** 1-12 (1 = C). A note of 0 releases. */
#define X16_YM_NOTE(octave, semitone)   (((octave) << 4) | (semitone))
#define X16_YM_NOTE_RELEASE             0

/* Retrigger the envelope, or only bend the pitch. */
#define X16_YM_RETRIGGER        1
#define X16_YM_HOLD             0

/* Reset the chip and load the default instrument patches. Returns 0 if
** there is no YM2151. Must precede x16_ym_patch_*. */
unsigned char x16_ym_init(void);

/* Raw register write: complete access, stale shadows. Returns 0 if the
** chip stayed busy. */
unsigned char x16_ym_write(__reg("r0") unsigned char reg, __reg("r2") unsigned char value);

/* The same write through the ROM driver, keeping its shadows coherent. */
void x16_ym_poke(__reg("r0") unsigned char reg, __reg("r2") unsigned char value);

/* 1 while the chip is busy. Not an error. */
unsigned char x16_ym_busy(void);

/* channel is 0-7 throughout. patch is a ROM instrument index, 0-162. */
unsigned char x16_ym_patch_rom(__reg("r0") unsigned char channel, __reg("r2") unsigned char patch);

/* patch points at an instrument definition in RAM. */
unsigned char x16_ym_patch_ram(__reg("r0") unsigned char channel, __reg("r2/r3") const void *patch);

/* A packed note from X16_YM_NOTE(), 0 to release. */
unsigned char x16_ym_note_bas(__reg("r0") unsigned char channel,
                              __reg("r2") unsigned char note,
                              __reg("r4") unsigned char retrigger);

/* A raw YM2151 key code and key fraction (pitch bend). (Four char args:
** all fit in r0/r2/r4/r6.) */
void x16_ym_note(__reg("r0") unsigned char channel, __reg("r2") unsigned char kc,
                 __reg("r4") unsigned char kf, __reg("r6") unsigned char retrigger);

void x16_ym_release_note(__reg("a") unsigned char channel);

/* atten 0 is the patch's own volume; larger is quieter. */
unsigned char x16_ym_vol(__reg("r0") unsigned char channel, __reg("r2") unsigned char atten);

unsigned char x16_ym_pan(__reg("r0") unsigned char channel, __reg("r2") unsigned char pan);

/* Drum note 25-87. */
unsigned char x16_ym_drum(__reg("r0") unsigned char channel, __reg("r2") unsigned char note);

/* Read the ROM driver's shadows. */
unsigned char x16_ym_get_pan(__reg("a") unsigned char channel);
unsigned char x16_ym_get_vol(__reg("a") unsigned char channel);

#endif /* X16_YM_H */
