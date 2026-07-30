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
; Two sources: low RAM (pcm_stream_start) and banked RAM
; (pcm_stream_start_bank), selected by pcm_str_mode. Banked mode walks
; the window, rolling $C000 back to $A000 and stepping RAM_BANK, so a
; sample can be any length -- hence the 24-bit byte count. RAM_BANK is
; the one piece of state the refiller does not own, so it saves and
; restores the interrupted code's value around every refill.
;
; A separate object from audio/pcm.s on purpose. Streaming needs the IRQ
; module; plain x16_pcm_put() does not, and linking one should not drag
; in the other.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa, popax, popeax
        .importzp       sreg
        .import         pcm_rate                ; audio/pcm.s
        .import         irq_install             ; system/irq.s
        .import         irq_aflow_vec           ; ...where we hang the service

        .export         _x16_pcm_stream_start
        .export         _x16_pcm_stream_start_bank
        .export         _x16_pcm_stream_stop
        .export         _x16_pcm_stream_active

        ; audio/zsm.s drives the streamer directly and owns the loop flag
        .export         pcm_stream_start, pcm_stream_start_bank
        .export         pcm_stream_stop, pcm_str_loop

        .segment        "CODE"

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
_x16_pcm_stream_start:
        pha                             ; rate (rightmost arg, in A)
        jsr     popax
        sta     X16_P2                  ; count
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; data
        stx     X16_P1
        pla
        ; fall through

; pcm_stream_start -- in: X16_P0/P1 = sample data (low RAM)
;                         X16_P2/P3 = byte count
;                         A         = sample rate (0-128)
pcm_stream_start:
        pha
        jsr     pcm_stream_stop         ; quiesce a previous stream

        stz     pcm_str_mode            ; 0 = low RAM
        lda     X16_P0                  ; patch the source into the refiller
        sta     src_lda+1
        sta     pcm_str_rsrc            ; ...and snapshot it, so a looping
        lda     X16_P1                  ; stream can rewind to the start
        sta     src_lda+2
        sta     pcm_str_rsrc+1
        lda     X16_P2
        sta     pcm_str_rem
        sta     pcm_str_rlen
        lda     X16_P3
        sta     pcm_str_rem+1
        sta     pcm_str_rlen+1
        stz     pcm_str_rem+2           ; a low-RAM buffer cannot reach 64 KB,
        stz     pcm_str_rlen+2          ; so the top byte of the count is 0
        ora     X16_P2
        bne     @common
        pla                             ; zero bytes: nothing to play
        rts
@common:                                ; start_common is a long way past
        jmp     start_common            ; the banked entry below

; ---------------------------------------------------------------------
; void __fastcall__ x16_pcm_stream_start_bank(unsigned int offset,
;                                             unsigned long count,
;                                             unsigned char bank,
;                                             unsigned char rate)
;
; Play a sample living in banked RAM. `offset` is 0-8191 within the bank
; window; `count` is a 24-bit byte total (whole songs), so its top byte
; is ignored. The refiller maps banks in as it goes and always restores
; the interrupted code's RAM_BANK, so the main program never notices.
; ---------------------------------------------------------------------
_x16_pcm_stream_start_bank:
        pha                             ; rate (rightmost arg, in A)
        jsr     popa
        sta     X16_P5                  ; bank
        jsr     popeax                  ; count: A/X = low 16, sreg = high
        sta     X16_P2
        stx     X16_P3
        lda     sreg
        sta     X16_P4
        jsr     popax
        sta     X16_P0                  ; offset
        stx     X16_P1
        pla
        ; fall through

; pcm_stream_start_bank -- in: X16_P0/P1    = offset in the bank window
;                              X16_P2/P3/P4 = byte count (24-bit)
;                              X16_P5       = starting bank
;                              A            = sample rate (0-128)
pcm_stream_start_bank:
        pha
        jsr     pcm_stream_stop

        lda     #1
        sta     pcm_str_mode
        lda     X16_P0                  ; window address = $A000 + offset
        sta     src_lda+1
        sta     pcm_str_rsrc
        lda     X16_P1
        clc
        adc     #$A0
        sta     src_lda+2
        sta     pcm_str_rsrc+1
        lda     X16_P5
        sta     pcm_str_bank
        sta     pcm_str_rbank
        lda     X16_P2
        sta     pcm_str_rem
        sta     pcm_str_rlen
        lda     X16_P3
        sta     pcm_str_rem+1
        sta     pcm_str_rlen+1
        lda     X16_P4
        sta     pcm_str_rem+2
        sta     pcm_str_rlen+2
        ora     X16_P2
        ora     X16_P3
        bne     start_common
        pla                             ; zero bytes: nothing to play
        rts

; the shared tail: hook, prime, arm AFLOW if data remains, set the rate
start_common:
        jsr     irq_install
        lda     #1
        sta     pcm_str_active
        jsr     pcm_stream_fill         ; prime the FIFO before playback starts

        lda     pcm_str_active          ; anything left to stream?
        beq     @go                     ; no: it all fit in the FIFO
        php
        sei
        lda     #<pcm_stream_isr        ; claim the AFLOW service...
        sta     irq_aflow_vec
        lda     #>pcm_stream_isr
        sta     irq_aflow_vec+1
        lda     #VERA_IRQ_AFLOW         ; ...and only then enable the source
        tsb     VERA_IEN
        plp
@go:
        pla
        jmp     pcm_rate                ; ...and start the DAC

; ---------------------------------------------------------------------
; void x16_pcm_stream_stop(void)
;   Stop refilling. What is already queued in the FIFO keeps playing; call
;   x16_pcm_reset() or x16_pcm_rate(0) for immediate silence.
; ---------------------------------------------------------------------
_x16_pcm_stream_stop:
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
_x16_pcm_stream_active:
        lda     pcm_str_active
        ldx     #0                      ; high byte, for int-promoting callers
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
        lda     pcm_str_mode
        beq     psf_loop
        lda     RAM_BANK                ; banked source: map it in, and put the
        sta     pcm_str_svbk            ; interrupted code's bank back on exit
        lda     pcm_str_bank
        sta     RAM_BANK
psf_loop:
        lda     pcm_str_rem
        ora     pcm_str_rem+1
        ora     pcm_str_rem+2
        beq     psf_exhausted
        bit     VERA_AUDIO_CTRL         ; bit 7: FIFO full
        bpl     psf_feed
        jmp     psf_full                ; out of branch range from here
psf_feed:

src_lda:
        lda     $FFFF                   ; operand = current source (patched)
        sta     VERA_AUDIO_DATA

        inc     src_lda+1               ; advance the source
        bne     psf_dec
        inc     src_lda+2
        lda     pcm_str_mode
        beq     psf_dec
        lda     src_lda+2               ; banked: roll $C000 -> $A000, bank + 1
        cmp     #$C0
        bne     psf_dec
        lda     #$A0
        sta     src_lda+2
        inc     pcm_str_bank
        lda     pcm_str_bank
        sta     RAM_BANK
psf_dec:
        lda     pcm_str_rem             ; 24-bit decrement
        bne     psf_dec_low
        lda     pcm_str_rem+1
        bne     psf_dec_mid
        dec     pcm_str_rem+2
psf_dec_mid:
        dec     pcm_str_rem+1
psf_dec_low:
        dec     pcm_str_rem
        bra     psf_loop

psf_exhausted:
        ; Out of data. If the caller asked for a loop and the snapshot
        ; is not empty, rewind to the start and keep feeding; the FIFO
        ; never notices the seam.
        lda     pcm_str_loop
        beq     psf_stop
        lda     pcm_str_rlen
        ora     pcm_str_rlen+1
        ora     pcm_str_rlen+2
        beq     psf_stop                ; an empty snapshot cannot loop
        lda     pcm_str_rsrc
        sta     src_lda+1
        lda     pcm_str_rsrc+1
        sta     src_lda+2
        lda     pcm_str_rlen
        sta     pcm_str_rem
        lda     pcm_str_rlen+1
        sta     pcm_str_rem+1
        lda     pcm_str_rlen+2
        sta     pcm_str_rem+2
        lda     pcm_str_mode
        bne     psf_rewind_bank
        jmp     psf_loop                ; psf_loop is out of range from here
psf_rewind_bank:
        lda     pcm_str_rbank           ; ...including the starting bank
        sta     pcm_str_bank
        sta     RAM_BANK
        jmp     psf_loop

psf_stop:
        lda     #VERA_IRQ_AFLOW         ; stop the refill interrupt
        trb     VERA_IEN                ; (leaving it enabled would storm: AFLOW
        stz     pcm_str_active          ; only clears by refilling the FIFO)
psf_full:
        lda     pcm_str_mode
        beq     psf_out
        lda     pcm_str_svbk            ; the interrupted code's bank goes back
        sta     RAM_BANK
psf_out:
        rts

        .segment        "BSS"

pcm_str_rem:    .res 3                  ; bytes still to feed (24-bit)
pcm_str_active: .res 1

; Caller-owned: nonzero means wrap to the start when the data runs out.
; Set or clear it BEFORE pcm_stream_start; it survives a stop, because
; the flag only matters at the moment the data is exhausted.
pcm_str_loop:   .res 1
pcm_str_mode:   .res 1                  ; 0 = low RAM source, 1 = banked RAM
pcm_str_bank:   .res 1                  ; bank being read now (mode 1)
pcm_str_rsrc:   .res 2                  ; rewind snapshot: source...
pcm_str_rbank:  .res 1                  ; ...bank...
pcm_str_rlen:   .res 3                  ; ...and byte count
pcm_str_svbk:   .res 1                  ; the interrupted code's RAM_BANK
