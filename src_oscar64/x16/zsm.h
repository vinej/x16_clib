/* =====================================================================
 * x16clib :: x16/zsm.h -- compact ZSM stream player
 * =====================================================================
 * Plays ZSM revision 1 music streams (the Commander X16 community's
 * standard tracker export: PSG writes, YM2151 writes, delays, a loop
 * point) resident in normal 16-bit address space.
 *
 *      if (x16_zsm_init(song) == 0) {
 *          for (;;) {
 *              ...once per tick -- x16_zsm_get_tickrate() Hz...
 *              if (!(x16_zsm_tick() & X16_ZSM_ACTIVE)) break;
 *          }
 *      }
 *
 * The player advances only when x16_zsm_tick() is called: hook it to a
 * VSYNC handler's flag or a timer yourself. Tick from the MAIN LOOP --
 * the PCM streamer it can start runs off the AFLOW interrupt.
 *
 * PCM: EXTCMD channel-0 commands set VERA's AUDIO_CTRL/AUDIO_RATE, and
 * command 2 triggers instruments from the file's optional PCM table
 * (memory-resident samples up to 64 KB offsets/lengths) through the
 * AFLOW streamer of x16/pcm.h. A file whose PCM table is present but
 * unsupported fails x16_zsm_init() with X16_ZSM_ERR_PCM.
 * =====================================================================
 */

#ifndef X16_ZSM_H
#define X16_ZSM_H

/* x16_zsm_init() results, also remembered for x16_zsm_lasterr(). */
#define X16_ZSM_ERR_NONE        0
#define X16_ZSM_ERR_MAGIC       1   /* not a ZSM file */
#define X16_ZSM_ERR_VERSION     2   /* a revision newer than 1 */
#define X16_ZSM_ERR_RANGE       3   /* loop/PCM offset needs >16 bits */
#define X16_ZSM_ERR_PCM         4   /* PCM table present but unusable */

/* Status bits from x16_zsm_status() and x16_zsm_tick(). */
#define X16_ZSM_ACTIVE          0x01
#define X16_ZSM_LOOP            0x02
#define X16_ZSM_EOF             0x04

/* Initialize from a ZSM file image (16-byte header first). Returns 0
** and starts in the playing state, or an X16_ZSM_ERR_* code. Only
** 16-bit loop offsets are supported.
*/
unsigned char x16_zsm_init (const void *header);

/* Why the last x16_zsm_init() failed -- X16_ZSM_ERR_NONE after one
** that worked. The init already returns this code; this keeps it
** readable afterwards.
*/
unsigned char x16_zsm_lasterr (void);

/* Initialize from a raw headerless command stream. `loop` is the
** address to rewind to at EOF, or NULL to just stop. The tick rate
** defaults to 60.
*/
void x16_zsm_init_stream (const void *stream, const void *loop);

/* Pause / resume / restart. x16_zsm_stop() also stops any PCM stream
** and silences the DAC; what the PSG and YM are holding keeps
** sounding -- silence those with their own APIs if you need to.
*/
void x16_zsm_play (void);
void x16_zsm_stop (void);
void x16_zsm_rewind (void);

/* The header's tick rate in Hz (usually 60). */
unsigned int x16_zsm_get_tickrate (void);

/* X16_ZSM_* bits; x16_zsm_tick() advances playback by one tick first.
** A finished, non-looping stream reads X16_ZSM_EOF and stays inactive.
*/
unsigned char x16_zsm_status (void);
unsigned char x16_zsm_tick (void);

/* 1 if the loaded file carries a usable PCM instrument table. */
unsigned char x16_zsm_pcm_present (void);

/* Manually fire a PCM instrument from that table (the stream's EXTCMD
** command 2 does the same). Out-of-range indexes and unsupported
** samples are silently ignored.
*/
void x16_zsm_pcm_trigger (unsigned char instrument);

/* pulls the implementation in with this header */
#pragma compile("zsm.c")

#endif /* X16_ZSM_H */
