; =====================================================================
; x16clib :: audio/wavfile.s -- parse a WAV/RIFF header
; =====================================================================
; wav_parse_header reads a RIFF/WAVE header from a memory buffer and
; publishes the PCM format, so the caller can hand the numbers to the
; PCM streamer (audio/pcm.s, audio/pcmstream.s) and stream the sample
; data that follows. Parsing the small header from RAM keeps this
; independent of how the file was read (LOAD, MACPTR, a bank, ...);
; the caller streams the bulk data itself.
;
; WAV layout:  "RIFF" <size> "WAVE"  then 8-byte-headed chunks; the
; "fmt " chunk carries the format, the "data" chunk the samples.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; (import dropped: X16_TPTR0, __rc2)

        .globl  x16_wav_parse_header
        .globl  x16_wav_get_info
        .globl  x16_wav_rate
        .globl  x16_wav_data_len

; ---------------------------------------------------------------------
; ca65 -t cx16 TRANSLATES CHARACTER LITERALS TO PETSCII. ACME did not.
; The RIFF magic bytes on disk are ASCII, so every one is written as
; its explicit value here; `cmp #'R'` would assemble to cmp #$D2 and
; never match a real WAV file.
; ---------------------------------------------------------------------
CH_R  = $52
CH_I  = $49
CH_F  = $46
CH_W  = $57
CH_A  = $41
CH_V  = $56
CH_E  = $45
CH_LF = $66                             ; 'f'
CH_LM = $6D                             ; 'm'
CH_LT = $74                             ; 't'
CH_SP = $20                             ; ' '
CH_LD = $64                             ; 'd'
CH_LA = $61                             ; 'a'

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_wav_parse_header(const void *buf)
;   1 = parsed, fields published; 0 = not a RIFF/WAVE, or no fmt+data.
; ---------------------------------------------------------------------
x16_wav_parse_header:
        lda     mos8(__rc2)             ; a lone pointer never lands in A/X
        sta     mos8(X16_P0)            ; -- it takes an __rc pair
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        jsr     wav_parse_header
        lda     #0
        ldx     #0
        rol     a                       ; carry -> bit 0 (set = failed)
        eor     #1                      ; ...inverted: 1 = success
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_wav_get_info(x16_wav_info *out)
;   Copies the published fields. The struct in x16/wavfile.h mirrors
;   the block below byte for byte -- the order is load-bearing.
; ---------------------------------------------------------------------
x16_wav_get_info:
        lda     mos8(__rc2)             ; a lone pointer never lands in
        sta     mos8(X16_TPTR0)         ; A/X -- it takes an __rc pair
        lda     mos8(__rc3)
        sta     mos8(X16_TPTR0+1)
        ldy     #WAV_INFO_SIZE - 1
.Lx16_wav_get_info_out:
        lda     wav_format,y
        sta     (X16_TPTR0),y
        dey
        bpl     .Lx16_wav_get_info_out
        rts

; ---------------------------------------------------------------------
; unsigned long x16_wav_rate(void)      -- sample rate in Hz
; unsigned long x16_wav_data_len(void)  -- sample data bytes
;   Shortcuts for the two fields C reads most; the rest come cheaper
;   through x16_wav_get_info().
; ---------------------------------------------------------------------
x16_wav_rate:
        lda     wav_rate+2
        sta     mos8(__rc2)
        lda     wav_rate+3
        sta     mos8(__rc3)
        ldx     wav_rate+1
        lda     wav_rate
        rts

x16_wav_data_len:
        lda     wav_data_len+2
        sta     mos8(__rc2)
        lda     wav_data_len+3
        sta     mos8(__rc3)
        ldx     wav_data_len+1
        lda     wav_data_len
        rts

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; wav_parse_header -- parse a WAV header from a buffer
;   in:  X16_P0/P1 = pointer to the header bytes (consumed as a walking
;        pointer; the buffer must hold everything up to the data chunk)
;   out: carry clear on success, with wav_format/channels/rate/bits and
;        wav_data_off/wav_data_len filled in; carry set if the buffer is
;        not RIFF/WAVE or has no fmt+data chunks.
;
; Plain wavp_* labels rather than cheap locals: the walker is one long
; routine and the targets span more than one cheap-local scope.
; ---------------------------------------------------------------------
wav_parse_header:
        bra     wavp_begin
wavp_bad:
        sec
        rts
wavp_begin:
        ldy     #0                      ; "RIFF"
        lda     (X16_P0),y
        cmp     #CH_R
        bne     wavp_bad
        iny
        lda     (X16_P0),y
        cmp     #CH_I
        bne     wavp_bad
        iny
        lda     (X16_P0),y
        cmp     #CH_F
        bne     wavp_bad
        iny
        lda     (X16_P0),y
        cmp     #CH_F
        bne     wavp_bad
        ldy     #8                      ; "WAVE"
        lda     (X16_P0),y
        cmp     #CH_W
        bne     wavp_bad
        iny
        lda     (X16_P0),y
        cmp     #CH_A
        bne     wavp_bad
        iny
        lda     (X16_P0),y
        cmp     #CH_V
        bne     wavp_bad
        iny
        lda     (X16_P0),y
        cmp     #CH_E
        bne     wavp_bad

        stz     wavp_fmt
        lda     #12                     ; first chunk starts at offset 12
        sta     wavp_cur
        stz     wavp_cur+1
        lda     mos8(X16_P0)
        clc
        adc     #12
        sta     mos8(X16_P0)
        lda     mos8(X16_P1)
        adc     #0
        sta     mos8(X16_P1)

wavp_chunk:
        ldy     #0                      ; "fmt " ?
        lda     (X16_P0),y
        cmp     #CH_LF
        bne     wavp_not_fmt
        iny
        lda     (X16_P0),y
        cmp     #CH_LM
        bne     wavp_not_fmt
        iny
        lda     (X16_P0),y
        cmp     #CH_LT
        bne     wavp_not_fmt
        iny
        lda     (X16_P0),y
        cmp     #CH_SP
        bne     wavp_not_fmt
        ; fmt chunk body starts at +8
        ldy     #8
        lda     (X16_P0),y
        sta     wav_format
        ldy     #10
        lda     (X16_P0),y
        sta     wav_channels
        ldy     #12
        lda     (X16_P0),y
        sta     wav_rate
        iny
        lda     (X16_P0),y
        sta     wav_rate+1
        iny
        lda     (X16_P0),y
        sta     wav_rate+2
        iny
        lda     (X16_P0),y
        sta     wav_rate+3
        ldy     #22
        lda     (X16_P0),y
        sta     wav_bits
        inc     wavp_fmt
        bra     wavp_advance

wavp_not_fmt:
        ldy     #0                      ; "data" ?
        lda     (X16_P0),y
        cmp     #CH_LD
        bne     wavp_advance
        iny
        lda     (X16_P0),y
        cmp     #CH_LA
        bne     wavp_advance
        iny
        lda     (X16_P0),y
        cmp     #CH_LT
        bne     wavp_advance
        iny
        lda     (X16_P0),y
        cmp     #CH_LA
        bne     wavp_advance
        ; data chunk: length at +4, sample data at +8
        ldy     #4
        lda     (X16_P0),y
        sta     wav_data_len
        iny
        lda     (X16_P0),y
        sta     wav_data_len+1
        iny
        lda     (X16_P0),y
        sta     wav_data_len+2
        iny
        lda     (X16_P0),y
        sta     wav_data_len+3
        lda     wavp_cur
        clc
        adc     #8
        sta     wav_data_off
        lda     wavp_cur+1
        adc     #0
        sta     wav_data_off+1
        lda     wavp_fmt                ; a data chunk before fmt is malformed
        bne     wavp_datok
        jmp     wavp_bad
wavp_datok:
        clc
        rts

wavp_advance:
        ldy     #4                      ; chunk size (32-bit; header chunks are small)
        lda     (X16_P0),y
        sta     wavp_sz
        iny
        lda     (X16_P0),y
        sta     wavp_sz+1
        iny
        lda     (X16_P0),y
        sta     wavp_sz+2
        iny
        lda     (X16_P0),y
        sta     wavp_sz+3
        lda     wavp_sz                 ; pad an odd size up to even
        and     #1
        beq     wavp_even
        inc     wavp_sz
        bne     wavp_even
        inc     wavp_sz+1
wavp_even:
        lda     wavp_sz                 ; adv = 8 + size (16-bit is plenty pre-data)
        clc
        adc     #8
        sta     wavp_adv
        lda     wavp_sz+1
        adc     #0
        sta     wavp_adv+1
        lda     mos8(X16_P0)            ; walk the pointer and the offset
        clc
        adc     wavp_adv
        sta     mos8(X16_P0)
        lda     mos8(X16_P1)
        adc     wavp_adv+1
        sta     mos8(X16_P1)
        lda     wavp_cur
        clc
        adc     wavp_adv
        sta     wavp_cur
        lda     wavp_cur+1
        adc     wavp_adv+1
        sta     wavp_cur+1
        lda     wavp_cur+1              ; bail if we walk past a sane header size
        cmp     #4                      ; 1 KB of chunks without a data: give up
        bcc     wavp_more
        jmp     wavp_bad
wavp_more:
        jmp     wavp_chunk

        .section .bss,"aw",@nobits

; The published format. CONTIGUOUS AND IN THIS ORDER: x16_wav_get_info
; block-copies these WAV_INFO_SIZE bytes onto the C struct.
wav_format:   .zero  1                    ; audio format code (1 = PCM)
wav_channels: .zero  1                    ; channel count
wav_rate:     .zero  4                    ; sample rate, little-endian
wav_bits:     .zero  1                    ; bits per sample
wav_data_off: .zero  2                    ; sample data offset in the buffer
wav_data_len: .zero  4                    ; sample data length in bytes
WAV_INFO_SIZE = 13

; Chunk-walker scratch.
wavp_cur:     .zero  2                    ; current chunk offset from the base
wavp_sz:      .zero  4                    ; current chunk size
wavp_adv:     .zero  2                    ; bytes to the next chunk
wavp_fmt:     .zero  1                    ; have we seen a fmt chunk yet?
