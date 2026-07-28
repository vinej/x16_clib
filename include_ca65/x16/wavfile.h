/* =====================================================================
 * x16clib :: x16/wavfile.h -- parse a WAV/RIFF header
 * =====================================================================
 * Reads a RIFF/WAVE header out of a memory buffer and publishes the
 * PCM format, so the numbers can go straight to the PCM layer in
 * x16/pcm.h. Parsing from RAM keeps this independent of how the file
 * was read (x16_fs_load(), MACPTR, a RAM bank...); the caller streams
 * the bulk sample data itself.
 *
 *      unsigned char buf[...];              // >= header + data start
 *      x16_wav_info  wav;
 *
 *      if (x16_wav_parse_header(buf)) {
 *          x16_wav_get_info(&wav);
 *          x16_pcm_rate(0);
 *          x16_pcm_ctrl(X16_PCM_VOLUME(15)
 *                       | (wav.channels == 2 ? X16_PCM_STEREO : 0)
 *                       | (wav.bits == 16 ? X16_PCM_16BIT : 0));
 *          x16_pcm_stream_start(buf + wav.data_off,
 *                               (unsigned int)wav.data_len, 64);
 *      }
 *
 * Only the header is validated: format code 1 is PCM, anything else
 * (IMA ADPCM is 17 -- see x16/adpcm.h) is reported, not rejected.
 * The buffer must hold everything up to and including the start of the
 * "data" chunk header; for a canonical WAV that is the first 44 bytes.
 * =====================================================================
 */

#ifndef X16_WAVFILE_H
#define X16_WAVFILE_H

/* The published header fields. Block-copied from the assembly module,
** so the order is load-bearing. Do not reorder.
*/
typedef struct {
    unsigned char format;       /* audio format code: 1 = integer PCM */
    unsigned char channels;     /* 1 = mono, 2 = stereo */
    unsigned long rate;         /* sample rate in Hz */
    unsigned char bits;         /* bits per sample: 8 or 16 */
    unsigned int  data_off;     /* byte offset of the samples in the buffer */
    unsigned long data_len;     /* sample data length in bytes */
} x16_wav_info;

/* Parse the header in `buf`. Returns 1 on success with the fields
** published, 0 if the buffer is not RIFF/WAVE, has a data chunk before
** its fmt chunk, or runs a kilobyte of chunks without finding data.
*/
unsigned char __fastcall__ x16_wav_parse_header (const void *buf);

/* What the last successful parse found. */
void __fastcall__ x16_wav_get_info (x16_wav_info *out);

/* Shortcuts for the two most-wanted fields. */
unsigned long x16_wav_rate (void);
unsigned long x16_wav_data_len (void);

#endif /* X16_WAVFILE_H */
