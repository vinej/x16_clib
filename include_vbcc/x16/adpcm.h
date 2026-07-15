/* =====================================================================
 * x16clib :: x16/adpcm.h -- IMA ADPCM decoding (4:1 compression)
 * =====================================================================
 * The natural partner to the PCM streamer. IMA ADPCM stores 16-bit
 * samples as 4-bit deltas, so a second of 16-bit mono at 16 kHz is 8 KB
 * instead of 32 -- one RAM bank per second, streamable from disk.
 *
 * This is the canonical IMA/DVI algorithm, the one in WAV files, with the
 * LOW nibble of each byte first.
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

#ifndef X16_ADPCM_H
#define X16_ADPCM_H

/* Predictor 0, step index 0. */
void x16_adpcm_init(void);

/* An IMA WAV block header carries an initial predictor and step index.
** Set them before decoding that block's payload. */
void x16_adpcm_set_state(__reg("r0/r1") int predictor, __reg("r2") unsigned char index);
int x16_adpcm_predictor(void);
unsigned char x16_adpcm_index(void);

/* Decode one 4-bit code to a signed 16-bit sample. */
int x16_adpcm_nibble(__reg("a") unsigned char code);

/* Decode `count` SOURCE bytes into `dst` as signed 16-bit little-endian
** samples: two samples, four bytes, out for every byte in. */
void x16_adpcm_block(__reg("r0/r1") const void *src, __reg("r2/r3") void *dst,
                     __reg("r4/r5") unsigned int count);

#endif /* X16_ADPCM_H */
