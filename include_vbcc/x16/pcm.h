/* =====================================================================
 * x16clib :: x16/pcm.h -- VERA PCM audio, a 4 KB FIFO
 * =====================================================================
 * Samples are two's-complement signed.
 *
 * Order matters at startup: set the rate to 0, prime the FIFO, then set
 * the real rate. Starting playback on an empty FIFO underruns at once.
 *
 *      x16_pcm_rate(0);
 *      x16_pcm_ctrl(X16_PCM_VOLUME(15));
 *      x16_pcm_write(samples, sizeof samples);
 *      x16_pcm_rate(64);
 * =====================================================================
 */

#ifndef X16_PCM_H
#define X16_PCM_H

/* Control byte for x16_pcm_ctrl(). */
#define X16_PCM_VOLUME(v)       ((v) & 0x0F)    /* 0-15 */
#define X16_PCM_STEREO          0x10
#define X16_PCM_16BIT           0x20
#define X16_PCM_RESET           0x80

/* 128 is full speed, 48828 Hz. 0 stops playback. */
#define X16_PCM_RATE_MAX        128

void x16_pcm_ctrl(__reg("a") unsigned char ctrl);

/* Returns the rate actually written: `rate` clamped to 128. */
unsigned char x16_pcm_rate(__reg("a") unsigned char rate);

/* Clear the FIFO, keeping the current format and volume. */
void x16_pcm_reset(void);

unsigned char x16_pcm_full(void);
unsigned char x16_pcm_empty(void);

/* One sample. The hardware drops it if the FIFO is full. Safe from an
** ISR: it touches no shared scratch. */
void x16_pcm_put(__reg("a") unsigned char sample);

/* Push a block. Does not throttle -- meant for priming an empty FIFO with
** up to 4 KB. Bytes past a full FIFO are discarded. */
void x16_pcm_write(__reg("r0/r1") const void *src, __reg("r2/r3") unsigned int count);

/* ---------------------------------------------------------------------
 * AFLOW-driven streaming. VERA raises AFLOW when the FIFO drops below a
 * quarter full, and the interrupt refills it.
 *
 *      x16_pcm_ctrl(X16_PCM_VOLUME(15));
 *      x16_pcm_stream_start(samples, sizeof samples, 64);
 *      while (x16_pcm_stream_active()) { ...do other work... }
 *
 * Requires enabled interrupts; the CINV hook installs itself. Note that
 * x16_irq_remove() stops a stream.
 * ------------------------------------------------------------------ */

/* `rate` is 0-128, as for x16_pcm_rate(); 0 primes without playing. */
void x16_pcm_stream_start(__reg("r0/r1") const void *data, __reg("r2/r3") unsigned int count,
                          __reg("r4") unsigned char rate);

/* Stop refilling. Whatever is already queued keeps playing. */
void x16_pcm_stream_stop(void);

/* 1 while data remains to hand over. */
unsigned char x16_pcm_stream_active(void);

#endif /* X16_PCM_H */
