; =====================================================================
; x16clib :: audio/pcmstream.s -- AFLOW-driven PCM streaming
; =====================================================================
; x16_pcm_write() primes a FIFO; it cannot PLAY anything longer than the
; FIFO's 4 KB. Streaming works the way the hardware intends: VERA raises
; AFLOW whenever the FIFO drops below 1/4 full, and the interrupt refills
; it from the sample buffer.
;
; AFLOW HAS NO ISR ACKNOWLEDGE. It clears only when the FIFO rises back
; over 1/4, so when the data runs out the refiller must disable it in IEN
; or the interrupt storms forever.
;
; The source pointer is kept inside an absolute `lda` (self-modified),
; not in zero page: the refill runs in interrupt context, where every
; zero-page scratch byte may belong to whatever code was interrupted.
; That is also why this handler needs no zero-page save, unlike the raster
; and collision callbacks in system/irq.s -- it touches neither block.
;
; A separate object from audio/pcm.s on purpose. Streaming needs the IRQ
; module; plain x16_pcm_put() does not, and linking one should not drag
; in the other.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X, then whatever __rc bytes the pointers
;   left free. So f(ptr, int, char) is ptr in __rc2/3, int in A/X, char in
;   __rc4.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_pcm_stream_start
        .globl  x16_pcm_stream_stop
        .globl  x16_pcm_stream_active

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void __fastcall__ x16_pcm_stream_start(const void *data,
;                                        unsigned int count,
;                                        unsigned char rate)
;
; Set the format and volume first with x16_pcm_ctrl(). The FIFO is primed
; here in one go, THEN the rate starts playback, so it cannot underrun at
; t=0. A rate of 0 primes without playing. Installs the CINV hook itself;
; requires interrupts enabled.
; ---------------------------------------------------------------------
; data is a pointer -> __rc2/__rc3. count is an int -> A/X. rate is a byte
; and the pointer has taken __rc2/__rc3, so it lands in __rc4.
x16_pcm_stream_start:
        sta     X16_P2                  ; count lo
        stx     X16_P3                  ; count hi
        lda     __rc2
        sta     X16_P0                  ; data lo
        lda     __rc3
        sta     X16_P1                  ; data hi
        lda     __rc4                   ; A = rate
        ; fall through

; pcm_stream_start -- in: X16_P0/P1 = sample data (low RAM)
;                         X16_P2/P3 = byte count
;                         A         = sample rate (0-128)
pcm_stream_start:
        pha
        jsr     pcm_stream_stop         ; quiesce a previous stream

        lda     X16_P0                  ; patch the source into the refiller
        sta     src_lda+1
        lda     X16_P1
        sta     src_lda+2
        lda     X16_P2
        sta     pcm_str_rem
        lda     X16_P3
        sta     pcm_str_rem+1
        ora     X16_P2
        beq     .Lpcm_stream_start_nothing                ; zero bytes: nothing to play

        jsr     irq_install
        lda     #1
        sta     pcm_str_active
        jsr     pcm_stream_fill         ; prime the FIFO before playback starts

        lda     pcm_str_active          ; anything left to stream?
        beq     .Lpcm_stream_start_go                     ; no: it all fit in the FIFO
        php
        sei
        lda     #<pcm_stream_isr        ; claim the AFLOW service...
        sta     irq_aflow_vec
        lda     #>pcm_stream_isr
        sta     irq_aflow_vec+1
        lda     #VERA_IRQ_AFLOW         ; ...and only then enable the source
        tsb     VERA_IEN
        plp
.Lpcm_stream_start_go:
        pla
        jmp     pcm_rate                ; ...and start the DAC
.Lpcm_stream_start_nothing:
        pla
        rts

; ---------------------------------------------------------------------
; void x16_pcm_stream_stop(void)
;   Stop refilling. What is already queued in the FIFO keeps playing; call
;   x16_pcm_reset() or x16_pcm_rate(0) for immediate silence.
; ---------------------------------------------------------------------
x16_pcm_stream_stop:
pcm_stream_stop:
        php
        sei
        lda     #VERA_IRQ_AFLOW
        trb     VERA_IEN
        stz     irq_aflow_vec           ; release the service
        stz     irq_aflow_vec+1
        stz     pcm_str_active
        plp
        rts

; ---------------------------------------------------------------------
; unsigned char x16_pcm_stream_active(void)
;   1 while data remains, 0 once the whole buffer has been handed to the
;   FIFO. What is in the FIFO may still be playing.
; ---------------------------------------------------------------------
x16_pcm_stream_active:
        lda     pcm_str_active          ; a char return is A alone
        rts

; ---------------------------------------------------------------------
; pcm_stream_isr -- the AFLOW service, reached through irq_aflow_vec.
; pcm_stream_fill -- push bytes until the FIFO is full or the data is
;                    gone; also used to prime. Clobbers A/X/Y, which is
;                    fine: the KERNAL's stub restores them.
; ---------------------------------------------------------------------
pcm_stream_isr:
        lda     pcm_str_active
        bne     pcm_stream_fill
        lda     #VERA_IRQ_AFLOW         ; stray AFLOW with no stream: mute it
        trb     VERA_IEN
        rts

; src_lda must be a plain label so pcm_stream_start can patch it from
; another scope. In ca65 a plain label ends the enclosing cheap-local
; scope -- ACME's zone-local `.src` did not -- so this loop's labels are
; plain too. That is the whole reason they are named rather than `@loop`.
pcm_stream_fill:
psf_loop:
        lda     pcm_str_rem
        ora     pcm_str_rem+1
        beq     psf_exhausted
        bit     VERA_AUDIO_CTRL         ; bit 7: FIFO full
        bmi     psf_full

src_lda:
        lda     $FFFF                   ; operand = current source (patched)
        sta     VERA_AUDIO_DATA

        inc     src_lda+1               ; advance the source
        bne     psf_dec
        inc     src_lda+2
psf_dec:
        lda     pcm_str_rem             ; 16-bit decrement
        bne     psf_dec_low
        dec     pcm_str_rem+1
psf_dec_low:
        dec     pcm_str_rem
        bra     psf_loop

psf_exhausted:
        lda     #VERA_IRQ_AFLOW         ; out of data: stop the refill interrupt
        trb     VERA_IEN                ; (leaving it enabled would storm: AFLOW
        stz     pcm_str_active          ; only clears by refilling the FIFO)
psf_full:
        rts

        .section .bss,"aw",@nobits

pcm_str_rem:    .zero  2                  ; bytes still to feed
pcm_str_active: .zero  1
