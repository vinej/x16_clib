; =====================================================================
; x16clib :: audio/pcm.s -- VERA PCM audio (4 KB FIFO)
; =====================================================================
; AUDIO_CTRL ($9F3B): bit 7 read = FIFO full, bit 6 read = FIFO empty,
;   bit 5 = 16-bit, bit 4 = stereo, bits 3:0 = volume (0-15).
;   Writing a 1 to bit 7 resets the FIFO.
; AUDIO_RATE ($9F3C): 0 stops playback, 128 = 48828 Hz. Above 128 is
;   invalid. The register is not readable, which is why pcm_rate returns
;   the value it actually wrote.
; AUDIO_DATA ($9F3D): each write pushes one byte. Writes are silently
;   dropped when the FIFO is full.
;
; Samples are two's-complement signed.
;
; Set the rate to 0, prime the FIFO, then set the real rate. Starting
; playback on an empty FIFO underruns immediately.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popax

        .export         _x16_pcm_ctrl
        .export         _x16_pcm_rate
        .export         _x16_pcm_reset
        .export         _x16_pcm_full
        .export         _x16_pcm_empty
        .export         _x16_pcm_put
        .export         _x16_pcm_write

PCM_FIFO_EMPTY  = %01000000
PCM_FIFO_RESET  = %10000000     ; on write
PCM_16BIT       = %00100000
PCM_STEREO      = %00010000

        .segment        "CODE"

; ---------------------------------------------------------------------
; void __fastcall__ x16_pcm_ctrl(unsigned char ctrl)
;   volume (3:0) | stereo | 16-bit | reset
; ---------------------------------------------------------------------
_x16_pcm_ctrl:
        sta     VERA_AUDIO_CTRL
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_pcm_rate(unsigned char rate)
;   returns the rate actually written, which is `rate` clamped to 128
;
; The register cannot be read back, so returning the clamped value is the
; only way a caller -- or a test -- can confirm what landed there.
; ---------------------------------------------------------------------
_x16_pcm_rate:
        jsr     pcm_rate                ; leaves the clamped value in A
        ldx     #0                      ; high byte, for int-promoting callers
        rts

pcm_rate:
        cmp     #129
        bcc     @ok
        lda     #128                    ; anything above 128 is invalid
@ok:
        sta     VERA_AUDIO_RATE
        rts

; ---------------------------------------------------------------------
; void x16_pcm_reset(void)
;   Clear the FIFO, keeping the current format and volume.
; ---------------------------------------------------------------------
_x16_pcm_reset:
        lda     VERA_AUDIO_CTRL
        and     #(PCM_16BIT | PCM_STEREO | $0F)
        ora     #PCM_FIFO_RESET
        sta     VERA_AUDIO_CTRL
        rts

; ---------------------------------------------------------------------
; unsigned char x16_pcm_full(void)  -- 1 if it cannot take another byte
; unsigned char x16_pcm_empty(void) -- 1 if it has run dry
; ---------------------------------------------------------------------
_x16_pcm_full:
        jsr     pcm_full
        lda     #0
        ldx     #0
        rol     a                       ; carry -> bit 0
        rts

pcm_full:
        lda     VERA_AUDIO_CTRL
        asl     a                       ; bit 7 into carry
        rts

_x16_pcm_empty:
        jsr     pcm_empty
        lda     #0
        ldx     #0
        rol     a
        rts

pcm_empty:
        lda     VERA_AUDIO_CTRL
        and     #PCM_FIFO_EMPTY
        cmp     #PCM_FIFO_EMPTY         ; carry set when the bit is set
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_pcm_put(unsigned char sample)
;   Dropped by the hardware if the FIFO is full. Safe from an ISR: it
;   touches no shared scratch.
; ---------------------------------------------------------------------
_x16_pcm_put:
        sta     VERA_AUDIO_DATA
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_pcm_write(const void *src, unsigned int count)
;
; Does not throttle: intended for priming an empty FIFO with up to 4 KB.
; Bytes written past a full FIFO are discarded by the hardware, so pace a
; longer stream yourself with x16_pcm_full().
; ---------------------------------------------------------------------
_x16_pcm_write:
        sta     X16_P2                  ; count lo (rightmost arg: A/X)
        stx     X16_P3                  ; count hi
        jsr     popax                   ; src
        sta     X16_P0
        stx     X16_P1
        ; fall through

; pcm_write -- in: X16_P0/P1 = source address, X16_P2/P3 = byte count
pcm_write:
        ldy     #0
@loop:
        lda     X16_P2
        ora     X16_P3
        beq     @done                   ; count exhausted

        lda     (X16_P0),y
        sta     VERA_AUDIO_DATA

        inc     X16_P0                  ; advance the source pointer
        bne     @dec
        inc     X16_P1
@dec:
        lda     X16_P2                  ; 16-bit decrement of the count
        bne     @dec_low
        dec     X16_P3
@dec_low:
        dec     X16_P2
        bra     @loop
@done:
        rts
