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

#ifndef X16_PCM_H
#define X16_PCM_H

/* Control byte for x16_pcm_ctrl(). */
#define X16_PCM_VOLUME(v)       ((v) & 0x0F)    /* 0-15 */
#define X16_PCM_STEREO          0x10
#define X16_PCM_16BIT           0x20
#define X16_PCM_RESET           0x80

/* 128 is full speed, 48828 Hz. 0 stops playback. */
#define X16_PCM_RATE_MAX        128

void x16_pcm_ctrl (unsigned char ctrl);

/* Returns the rate actually written: `rate` clamped to 128. The hardware
** register cannot be read back, so this return value is the only way to
** see what landed there.
*/
unsigned char x16_pcm_rate (unsigned char rate);

/* Clear the FIFO, keeping the current format and volume. */
void x16_pcm_reset (void);

unsigned char x16_pcm_full (void);
unsigned char x16_pcm_empty (void);

/* One sample. The hardware drops it if the FIFO is full. Safe from an
** ISR: it touches no shared scratch.
*/
void x16_pcm_put (unsigned char sample);

/* Push a block. Does not throttle -- meant for priming an empty FIFO
** with up to 4 KB. Bytes written past a full FIFO are discarded, so pace
** a longer stream yourself with x16_pcm_full(), or use the streamer below.
*/
void x16_pcm_write (const void *src, unsigned int count);

/* ---------------------------------------------------------------------
 * AFLOW-driven streaming.
 *
 * x16_pcm_write() cannot play anything longer than the FIFO's 4 KB.
 * Streaming works the way the hardware intends: VERA raises AFLOW when
 * the FIFO drops below a quarter full, and the interrupt refills it.
 *
 *      x16_pcm_ctrl(X16_PCM_VOLUME(15));
 *      x16_pcm_stream_start(samples, sizeof samples, 64);
 *      while (x16_pcm_stream_active()) { ...do other work... }
 *
 * The FIFO is primed before the DAC starts, so playback cannot underrun
 * at t=0. Requires enabled interrupts; the CINV hook installs itself.
 *
 * Note that x16_irq_remove() stops a stream: with the handler unhooked,
 * AFLOW has nothing to acknowledge it and would assert the IRQ line
 * forever.
 * ------------------------------------------------------------------ */

/* `rate` is 0-128, as for x16_pcm_rate(); 0 primes without playing. Set
** the format and volume with x16_pcm_ctrl() first.
*/
void x16_pcm_stream_start (const void *data, unsigned int count,
                                        unsigned char rate);

/* Stop refilling. Whatever is already queued keeps playing; call
** x16_pcm_reset() for immediate silence.
*/
void x16_pcm_stream_stop (void);

/* 1 while data remains to hand over. It reaches 0 once the last byte is
** in the FIFO -- which may still be playing.
*/
unsigned char x16_pcm_stream_active (void);

/* pulls the implementation in with this header */
#pragma compile("pcm.c")

#endif /* X16_PCM_H */
