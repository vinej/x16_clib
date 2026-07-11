// =====================================================================
// x16clib :: x16/pcm.c -- VERA PCM audio (4 KB FIFO)
// =====================================================================
// AUDIO_CTRL ($9F3B): bit 7 read = FIFO full, bit 6 read = FIFO empty,
//   bit 5 = 16-bit, bit 4 = stereo, bits 3:0 = volume. Writing a 1 to
//   bit 7 resets the FIFO.
// AUDIO_RATE ($9F3C): 0 stops playback, 128 = 48828 Hz; above 128 is
//   invalid. Not readable, which is why x16_pcm_rate returns the value
//   it actually wrote.
// AUDIO_DATA ($9F3D): each write pushes one byte; writes are silently
//   dropped when the FIFO is full.
//
// Same code as src_ca65/audio/pcm.s. Set the rate to 0, prime the FIFO,
// then set the real rate: starting on an empty FIFO underruns at once.
// =====================================================================

#include <x16/pcm.h>

// x16_pcm_write walks its `src` parameter directly -- Oscar64 keeps
// pointer parameters in zero page.

void x16_pcm_ctrl(unsigned char ctrl) {
    __asm {
        lda ctrl
        sta 0x9f3b                      /* VERA_AUDIO_CTRL */
    }
}

// Returns the rate actually written: `rate` clamped to 128.
unsigned char x16_pcm_rate(unsigned char rate) {
    return __asm {
        lda rate
        cmp #129
        bcc pr_ok
        lda #128                        /* anything above 128 is invalid */
    pr_ok:
        sta 0x9f3c                      /* VERA_AUDIO_RATE */
        sta accu
    };
}

// Clear the FIFO, keeping the current format and volume.
void x16_pcm_reset(void) {
    __asm {
        lda 0x9f3b                      /* VERA_AUDIO_CTRL */
        and #0x3f                        /* 16BIT | STEREO | volume */
        ora #0x80                        /* FIFO_RESET */
        sta 0x9f3b                      /* VERA_AUDIO_CTRL */
    }
}

// 1 if the FIFO cannot take another byte.
unsigned char x16_pcm_full(void) {
    return __asm {
        lda 0x9f3b                      /* VERA_AUDIO_CTRL */
        asl                             /* bit 7 into carry */
        lda #0
        rol
        sta accu
    };
}

// 1 if the FIFO has run dry.
unsigned char x16_pcm_empty(void) {
    return __asm {
        lda 0x9f3b                      /* VERA_AUDIO_CTRL */
        and #0x40                       /* PCM_FIFO_EMPTY */
        beq pe_no
        lda #1
    pe_no:
        sta accu
    };
}

// Dropped by the hardware if the FIFO is full. Safe from an ISR.
void x16_pcm_put(unsigned char sample) {
    __asm {
        lda sample
        sta 0x9f3d                      /* VERA_AUDIO_DATA */
    }
}

// ---------------------------------------------------------------------
// Does not throttle: intended for priming an empty FIFO with up to
// 4 KB. Bytes written past a full FIFO are discarded by the hardware,
// so pace a longer stream yourself with x16_pcm_full() -- or use
// x16_pcm_stream_start().
// ---------------------------------------------------------------------
void x16_pcm_write(const void *src, unsigned int count) {
    __asm {
        ldy #0
    pw_loop:
        lda count
        ora count+1
        beq pw_done                     /* count exhausted */
        lda (src),y
        sta 0x9f3d                      /* VERA_AUDIO_DATA */
        inc src                         /* advance the source pointer */
        bne pw_dec
        inc src+1
    pw_dec:
        lda count                       /* 16-bit decrement of the count */
        bne pw_dec_low
        dec count+1
    pw_dec_low:
        dec count
        jmp pw_loop
    pw_done:
    }
}

// =====================================================================
// x16clib :: x16/pcmstream.c -- AFLOW-driven PCM streaming
// =====================================================================
// x16_pcm_write() primes a FIFO; it cannot PLAY anything longer than
// the FIFO's 4 KB. Streaming works the way the hardware intends: VERA
// raises AFLOW whenever the FIFO drops below 1/4 full, and the
// interrupt refills it from the sample buffer.
//
// AFLOW HAS NO ISR ACKNOWLEDGE. It clears only when the FIFO rises back
// over 1/4, so when the data runs out the refiller must disable it in
// IEN or the interrupt storms forever.
//
// The ca65 build kept its source pointer in a self-modified absolute
// operand because interrupt context owns no zero page. Here the pointer
// gets a DEDICATED pinned zp slot instead: it belongs to this module
// alone, mainline code only writes it before AFLOW is enabled, so the
// refiller may use it from the interrupt safely. (The original's
// cross-function self-modification is not expressible here: an asm
// label inside one Oscar64 function is invisible to every other, so
// the pointer lives in the __zeropage region instead.)
// =====================================================================

#include <x16/irq.h>

// system/irq.c's AFLOW service vector; claiming it is this module's
// whole relationship with the interrupt system.
extern volatile x16_irq_handler x16__irq_aflow_vec;

void x16__pcm_stream_isr(void);

// The refiller's walking source pointer -- see the header comment.
__zeropage volatile const char* x16__pcm_sp;

volatile char x16__pcm_rem0 = 0;  // bytes still to feed
volatile char x16__pcm_rem1 = 0;
volatile char x16__pcm_active = 0;

// The ISR's address, reachable from install code as data.
volatile x16_irq_handler x16__pcm_isr_ptr = &x16__pcm_stream_isr;

// ---------------------------------------------------------------------
// The AFLOW service, reached through x16__irq_aflow_vec -- and also
// called directly (as a plain C call) by stream_start to prime the
// FIFO. Pushes bytes until the FIFO is full or the data is gone; when
// the data runs out it disables AFLOW itself. Clobbers A/X/Y, which is
// fine: the KERNAL's stub restores them.
// ---------------------------------------------------------------------
void x16__pcm_stream_isr(void) {
    __asm {
        lda x16__pcm_active
        bne psf_loop
        lda 0x9f26  /*VERA_IEN*/
        and #0xf7
        sta 0x9f26
        jmp psf_out

    psf_loop:
        lda x16__pcm_rem0
        ora x16__pcm_rem1
        beq psf_exhausted
        bit 0x9f3b   /* bit 7: FIFO full (VERA_AUDIO_CTRL) */
        bmi psf_out

        ldy #0
        lda (x16__pcm_sp),y
        sta 0x9f3d                      /* VERA_AUDIO_DATA */

        inc x16__pcm_sp                 /* advance the source */
        bne psf_dec
        inc x16__pcm_sp+1
    psf_dec:
        lda x16__pcm_rem0               /* 16-bit decrement */
        bne psf_dec_low
        dec x16__pcm_rem1
    psf_dec_low:
        dec x16__pcm_rem0
        jmp psf_loop

    psf_exhausted:
        lda 0x9f26  /*VERA_IEN*/          /* interrupt (leaving it on storms: */
        and #0xf7
        sta 0x9f26
        lda #0                          /* AFLOW only clears by refilling) */
        sta x16__pcm_active
    psf_out:
    }
}

// ---------------------------------------------------------------------
// Set the format and volume first with x16_pcm_ctrl(). The FIFO is
// primed here in one go, THEN the rate starts playback, so it cannot
// underrun at t=0. A rate of 0 primes without playing. Installs the
// CINV hook itself; requires interrupts enabled.
// ---------------------------------------------------------------------
void x16_pcm_stream_start(const void *data, unsigned int count,
                          unsigned char rate) {
    x16_pcm_stream_stop();              // quiesce a previous stream

    if (count == 0) {
        return;                         // zero bytes: nothing to play
    }
    x16__pcm_sp = (char*)data;
    __asm {
        lda count
        sta x16__pcm_rem0
        lda count+1
        sta x16__pcm_rem1
    }
    x16_irq_install();
    x16__pcm_active = 1;
    x16__pcm_stream_isr();              // prime before playback starts

    if (x16__pcm_active) {              // anything left to stream?
        __asm {
            php
            sei
            lda x16__pcm_isr_ptr        /* claim the AFLOW service... */
            sta x16__irq_aflow_vec
            lda x16__pcm_isr_ptr+1
            sta x16__irq_aflow_vec+1
            lda 0x9f26  /*VERA_IEN*/
            ora #0x08
            sta 0x9f26
            plp
        }
    }
    x16_pcm_rate(rate);                 // ...and start the DAC
}

// ---------------------------------------------------------------------
// Stop refilling. What is already queued in the FIFO keeps playing;
// call x16_pcm_reset() or x16_pcm_rate(0) for immediate silence.
// ---------------------------------------------------------------------
void x16_pcm_stream_stop(void) {
    __asm {
        php
        sei
        lda 0x9f26  /*VERA_IEN*/
        and #0xf7
        sta 0x9f26
        lda #0
        sta x16__irq_aflow_vec          /* release the service */
        sta x16__irq_aflow_vec+1
        sta x16__pcm_active
        plp
    }
}

// 1 while data remains, 0 once the whole buffer has been handed to the
// FIFO. What is in the FIFO may still be playing.
unsigned char x16_pcm_stream_active(void) {
    return x16__pcm_active;
}
