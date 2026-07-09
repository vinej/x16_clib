; =====================================================================
; x16clib :: audio/psg.s -- VERA PSG (16 voices)
; =====================================================================
; The voices live in VRAM at $1F9C0, four bytes each:
;   0  frequency 7:0
;   1  frequency 15:8
;   2  right(7) | left(6) | volume(5:0)
;   3  waveform(7:6) | pulse width or XOR(5:0)
;
; output_frequency = 25000000/512 / 2^17 * freq_word
;   -> freq_word = Hz * 2.68435 (approximately), so A4 (440 Hz) is 1181.
;
; That VRAM range is write-only. Reads return the last value the host
; wrote, which is why psg_note_off's read-modify-write only works on a
; voice this program has already set.
;
; Internal scratch: psg_voice_ptr owns X16_T2 and its callers own T3, so
; the C shims stage into T4/T5.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa
        .import         vera_fill

        .export         _x16_psg_init
        .export         _x16_psg_voice_ptr
        .export         _x16_psg_set_freq
        .export         _x16_psg_set_vol
        .export         _x16_psg_set_wave
        .export         _x16_psg_note_off

PSG_PAN_BOTH      = %11000000
PSG_WAVE_NOISE    = %11000000   ; the mask for bits 7:6

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void x16_psg_init(void) -- silence all 16 voices
; ---------------------------------------------------------------------
_x16_psg_init:
psg_init:
        vera_addr 0, VRAM_PSG, VERA_INC_1
        lda     #0
        ldx     #(16 * VERA_PSG_VOICE_SIZE)
        ldy     #0
        jmp     vera_fill

; ---------------------------------------------------------------------
; void __fastcall__ x16_psg_voice_ptr(unsigned char voice,
;                                     unsigned char offset)
; ---------------------------------------------------------------------
_x16_psg_voice_ptr:
        sta     X16_T4                  ; offset (rightmost arg, in A)
        jsr     popa                    ; voice
        tax
        lda     X16_T4
        jmp     psg_voice_ptr

; ---------------------------------------------------------------------
; void __fastcall__ x16_psg_set_freq(unsigned char voice, unsigned int freq)
; ---------------------------------------------------------------------
_x16_psg_set_freq:
        sta     X16_P0                  ; freq lo (rightmost arg: A/X)
        stx     X16_P1                  ; freq hi
        jsr     popa                    ; voice
        tax
        jmp     psg_set_freq

; ---------------------------------------------------------------------
; void __fastcall__ x16_psg_set_vol(unsigned char voice, unsigned char vol,
;                                   unsigned char pan)
; ---------------------------------------------------------------------
_x16_psg_set_vol:
        jsr     voice_marshal
        jmp     psg_set_vol

; ---------------------------------------------------------------------
; void __fastcall__ x16_psg_set_wave(unsigned char voice, unsigned char wave,
;                                    unsigned char width)
; ---------------------------------------------------------------------
_x16_psg_set_wave:
        jsr     voice_marshal
        jmp     psg_set_wave

; in:  A = third argument, two bytes on the C stack
; out: X = voice, A = second argument, Y = third argument
voice_marshal:
        sta     X16_T4                  ; pan / width
        jsr     popa
        sta     X16_T5                  ; vol / wave
        jsr     popa                    ; voice
        tax
        lda     X16_T5
        ldy     X16_T4
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_psg_note_off(unsigned char voice)
; ---------------------------------------------------------------------
_x16_psg_note_off:
        tax
        jmp     psg_note_off

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; psg_voice_ptr -- point data port 0 at a voice register
;   in:  X = voice (0-15), A = byte offset within the voice (0-3)
; ---------------------------------------------------------------------
psg_voice_ptr:
        sta     X16_T2
        lda     #VERA_CTRL_ADDRSEL
        trb     VERA_CTRL

        txa
        asl     a
        asl     a                       ; voice * 4, never carries (max 60)
        clc
        adc     X16_T2
        clc
        adc     #<VRAM_PSG              ; $C0 + up to 63, may carry
        sta     VERA_ADDR_L
        lda     #>VRAM_PSG
        adc     #0
        sta     VERA_ADDR_M
        lda     #(VERA_ADDR_H_BANK | (VERA_INC_1 << 4))
        sta     VERA_ADDR_H
        rts

; ---------------------------------------------------------------------
; psg_set_freq -- in: X = voice, X16_P0/P1 = frequency word
; ---------------------------------------------------------------------
psg_set_freq:
        lda     #0
        jsr     psg_voice_ptr
        lda     X16_P0
        sta     VERA_DATA0
        lda     X16_P1
        sta     VERA_DATA0
        rts

; ---------------------------------------------------------------------
; psg_set_vol -- in: X = voice, A = volume (0-63), Y = pan (PSG_PAN_*)
; ---------------------------------------------------------------------
psg_set_vol:
        and     #$3F
        sta     X16_T3
        tya
        and     #PSG_PAN_BOTH
        ora     X16_T3
        sta     X16_T3
        lda     #2
        jsr     psg_voice_ptr
        lda     X16_T3
        sta     VERA_DATA0
        rts

; ---------------------------------------------------------------------
; psg_set_wave -- in: X = voice, A = waveform (PSG_WAVE_*),
;                     Y = pulse width / XOR (0-63)
; ---------------------------------------------------------------------
psg_set_wave:
        and     #PSG_WAVE_NOISE         ; keep bits 7:6
        sta     X16_T3
        tya
        and     #$3F
        ora     X16_T3
        sta     X16_T3
        lda     #3
        jsr     psg_voice_ptr
        lda     X16_T3
        sta     VERA_DATA0
        rts

; ---------------------------------------------------------------------
; psg_note_off -- in: X = voice.  Volume to zero, everything else kept.
; ---------------------------------------------------------------------
psg_note_off:
        lda     #2
        jsr     psg_voice_ptr
        lda     VERA_DATA0              ; the host-written shadow
        and     #PSG_PAN_BOTH           ; keep the panning, drop the volume
        pha
        lda     #2
        jsr     psg_voice_ptr
        pla
        sta     VERA_DATA0
        rts
