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

; llvm-mos argument placement, measured on the machine:
;   INTEGER bytes fill A, then X, then __rc2, __rc3, ... left to right.
;   A POINTER takes a whole __rc pair (__rc2/__rc3, then __rc4/__rc5) and
;   consumes no A/X -- only zero page can be indirected through.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_pcm_ctrl
        .globl  x16_pcm_rate
        .globl  x16_pcm_reset
        .globl  x16_pcm_full
        .globl  x16_pcm_empty
        .globl  x16_pcm_put
        .globl  x16_pcm_write

; Cross-module: audio/pcmstream.s starts the DAC through this. Streaming
; lives in its own object so that a program using only pcm_put() does not
; drag the IRQ module in behind it.
        .globl  pcm_rate

PCM_FIFO_EMPTY  = %01000000
PCM_FIFO_RESET  = %10000000     ; on write
PCM_16BIT       = %00100000
PCM_STEREO      = %00010000

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void __fastcall__ x16_pcm_ctrl(unsigned char ctrl)
;   volume (3:0) | stereo | 16-bit | reset
; ---------------------------------------------------------------------
x16_pcm_ctrl:
        sta     VERA_AUDIO_CTRL
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_pcm_rate(unsigned char rate)
;   returns the rate actually written, which is `rate` clamped to 128
;
; The register cannot be read back, so returning the clamped value is the
; only way a caller -- or a test -- can confirm what landed there.
; ---------------------------------------------------------------------
x16_pcm_rate:
        jmp     pcm_rate                ; the clamped value comes back in A

pcm_rate:
        cmp     #129
        bcc     .Lpcm_rate_ok
        lda     #128                    ; anything above 128 is invalid
.Lpcm_rate_ok:
        sta     VERA_AUDIO_RATE
        rts

; ---------------------------------------------------------------------
; void x16_pcm_reset(void)
;   Clear the FIFO, keeping the current format and volume.
; ---------------------------------------------------------------------
x16_pcm_reset:
        lda     VERA_AUDIO_CTRL
        and     #(PCM_16BIT | PCM_STEREO | $0F)
        ora     #PCM_FIFO_RESET
        sta     VERA_AUDIO_CTRL
        rts

; ---------------------------------------------------------------------
; unsigned char x16_pcm_full(void)  -- 1 if it cannot take another byte
; unsigned char x16_pcm_empty(void) -- 1 if it has run dry
; ---------------------------------------------------------------------
x16_pcm_full:
        jsr     pcm_full
        lda     #0
        rol     a                       ; carry -> bit 0
        rts

pcm_full:
        lda     VERA_AUDIO_CTRL
        asl     a                       ; bit 7 into carry
        rts

x16_pcm_empty:
        jsr     pcm_empty
        lda     #0
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
x16_pcm_put:
        sta     VERA_AUDIO_DATA
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_pcm_write(const void *src, unsigned int count)
;
; Does not throttle: intended for priming an empty FIFO with up to 4 KB.
; Bytes written past a full FIFO are discarded by the hardware, so pace a
; longer stream yourself with x16_pcm_full().
; ---------------------------------------------------------------------
; src is a pointer (__rc2/__rc3); count is an int, so it takes A and X.
x16_pcm_write:
        sta     X16_P2                  ; count lo
        stx     X16_P3                  ; count hi
        lda     __rc2
        sta     X16_P0                  ; src lo
        lda     __rc3
        sta     X16_P1                  ; src hi
        ; fall through

; pcm_write -- in: X16_P0/P1 = source address, X16_P2/P3 = byte count
pcm_write:
        ldy     #0
.Lpcm_write_loop:
        lda     X16_P2
        ora     X16_P3
        beq     .Lpcm_write_done                   ; count exhausted

        lda     (X16_P0),y
        sta     VERA_AUDIO_DATA

        inc     X16_P0                  ; advance the source pointer
        bne     .Lpcm_write_dec
        inc     X16_P1
.Lpcm_write_dec:
        lda     X16_P2                  ; 16-bit decrement of the count
        bne     .Lpcm_write_dec_low
        dec     X16_P3
.Lpcm_write_dec_low:
        dec     X16_P2
        bra     .Lpcm_write_loop
.Lpcm_write_done:
        rts
