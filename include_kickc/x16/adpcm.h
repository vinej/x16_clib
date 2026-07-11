/* =====================================================================
 * x16clib :: x16/adpcm.h -- IMA ADPCM decoding (4:1 compression)
 * =====================================================================
 * The natural partner to the PCM streamer. IMA ADPCM stores 16-bit
 * samples as 4-bit deltas, so a second of 16-bit mono at 16 kHz is 8 KB
 * instead of 32 -- one RAM bank per second, streamable from disk.
 *
 * This is the canonical IMA/DVI algorithm, the one in WAV files, with
 * the LOW nibble of each byte first.
 *
 *      x16_adpcm_init();
 *      x16_adpcm_block(compressed, samples, sizeof compressed);
 *      x16_pcm_ctrl(X16_PCM_16BIT | X16_PCM_VOLUME(15));
 *      x16_pcm_stream_start(samples, sizeof samples, 64);
 *
 * Decoder state carries across calls, so a long sample can be decoded a
 * slice at a time -- which is what makes it stream.
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

#ifndef X16_ADPCM_H
#define X16_ADPCM_H

#include <x16/zpsafe.h>

/* Predictor 0, step index 0. */
void x16_adpcm_init (void);

/* An IMA WAV block header carries an initial predictor and step index.
** Set them before decoding that block's payload.
*/
void x16_adpcm_set_state (int predictor, unsigned char index);
int x16_adpcm_predictor (void);
unsigned char x16_adpcm_index (void);

/* Decode one 4-bit code to a signed 16-bit sample. */
int x16_adpcm_nibble (unsigned char code);

/* Decode `count` SOURCE bytes into `dst` as signed 16-bit little-endian
** samples: two samples, four bytes, out for every byte in.
*/
void x16_adpcm_block (const void *src, void *dst,
                                   unsigned int count);

#endif /* X16_ADPCM_H */
