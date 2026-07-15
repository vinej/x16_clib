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

        include        "macros.inc"
        include        "x16zp.inc"

; (import: vera_fill)
; vbcc argument registers: the voice index rides in r0, and further char
; args each take an even register (r2, r4, r6); a 16-bit freq takes r2/r3.
        zpage	r0
        zpage	r2
        zpage	r3
        zpage	r4
        zpage	r6
        zpage	sp

        global	_x16_psg_init
        global	_x16_psg_voice_ptr
        global	_x16_psg_set_freq
        global	_x16_psg_set_vol
        global	_x16_psg_set_wave
        global	_x16_psg_note_off
        global	_x16_psg_env_start
        global	_x16_psg_env_release
        global	_x16_psg_env_stop
        global	_x16_psg_env_tick

PSG_PAN_BOTH      = %11000000
PSG_WAVE_NOISE    = %11000000   ; the mask for bits 7:6

        section text

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
; void x16_psg_voice_ptr(__reg("r0") unsigned char voice, __reg("r2") unsigned char offset)
;   psg_voice_ptr wants X = voice, A = offset.
_x16_psg_voice_ptr:
        ldx     r0                      ; X = voice
        lda     r2                      ; A = offset
        jmp     psg_voice_ptr

; ---------------------------------------------------------------------
; void x16_psg_set_freq(__reg("r0") unsigned char voice, __reg("r2/r3") unsigned int freq)
;   psg_set_freq wants X = voice, P0/P1 = freq.
; ---------------------------------------------------------------------
_x16_psg_set_freq:
        lda     r2
        sta     X16_P0                  ; freq lo
        lda     r3
        sta     X16_P1                  ; freq hi
        ldx     r0                      ; X = voice
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

; voice in r0, arg2 in r2, arg3 in r4.
; out: X = voice, A = second argument, Y = third argument
voice_marshal:
        ldx     r0                      ; X = voice
        lda     r2                      ; A = vol / wave
        ldy     r4                      ; Y = pan / width
        rts

; ---------------------------------------------------------------------
; void x16_psg_note_off(__reg("r0") unsigned char voice)
;   psg_note_off wants X = voice (it calls psg_voice_ptr).
; ---------------------------------------------------------------------
_x16_psg_note_off:
        ldx     r0
        jmp     psg_note_off

; ---------------------------------------------------------------------
; void __fastcall__ x16_psg_env_start(unsigned char voice,
;                                     unsigned char peak,
;                                     unsigned char attack,
;                                     unsigned char sustain,
;                                     unsigned char release)
;
; Five char arguments (voice, peak, attack, sustain, release): the first
; four ride r0/r2/r4/r6, and release spills to the C soft stack at (sp)+0.
; psg_env_start wants A = voice, P0 = peak, P1 = attack, P2 = sustain,
; P3 = release.
; ---------------------------------------------------------------------
_x16_psg_env_start:
        lda     r2
        sta     X16_P0                  ; peak volume
        lda     r4
        sta     X16_P1                  ; attack step
        lda     r6
        sta     X16_P2                  ; sustain ticks
        ldy     #0
        lda     (sp),y
        sta     X16_P3                  ; release step (stacked 5th arg)
        lda     r0                      ; A = voice
        jmp     psg_env_start

; ---------------------------------------------------------------------
; void __fastcall__ x16_psg_env_release(unsigned char voice)
; void __fastcall__ x16_psg_env_stop(unsigned char voice)
; void x16_psg_env_tick(void)
;   The voice already arrives in A: no shim.
; ---------------------------------------------------------------------
_x16_psg_env_release:
        jmp     psg_env_release

_x16_psg_env_stop:
        jmp     psg_env_stop

_x16_psg_env_tick:
        jmp     psg_env_tick

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
;
; The HIGH byte is written first, stepping the port DOWNWARD from offset
; 1. Low-byte-first leaves the voice running on new-low/old-high for a
; few cycles -- an audible click on every pitch change.
; ---------------------------------------------------------------------
psg_set_freq:
        lda     #1                      ; point at freq bits 15:8
        jsr     psg_voice_ptr
        lda     VERA_ADDR_H
        ora     #VERA_ADDR_H_DECR       ; ...and walk backwards
        sta     VERA_ADDR_H
        lda     X16_P1
        sta     VERA_DATA0              ; high byte first
        lda     X16_P0
        sta     VERA_DATA0              ; then low, at offset 0
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

; =====================================================================
; ASR envelopes -- the decay everybody hand-rolls in the frame loop
; (examples/bounce.c included). Per voice: attack ramps the volume to a
; peak, sustain holds it for a tick count, release ramps it back to
; silence. Drive psg_env_tick once per frame.
; =====================================================================

; ---------------------------------------------------------------------
; psg_env_start -- (re)trigger a voice's envelope
;   in:  A = voice (0-15)
;        X16_P0 = peak volume (0-63)
;        X16_P1 = attack step per tick (0 = jump straight to the peak)
;        X16_P2 = sustain ticks at the peak (0 = release immediately,
;                 255 = until psg_env_release)
;        X16_P3 = release step per tick (0 = hold until psg_env_stop)
; ---------------------------------------------------------------------
psg_env_start:
        and     #$0F
        tax
        lda     X16_P0
        and     #$3F
        sta     env_peak,x
        lda     X16_P1
        sta     env_astep,x
        lda     X16_P2
        sta     env_sus,x
        lda     X16_P3
        sta     env_rstep,x
        lda     X16_P1
        beq     .instant
        stz     env_vol,x
        lda     #1                      ; stage 1: attack
        sta     env_stage,x
        rts
.instant:
        lda     env_peak,x
        sta     env_vol,x
        lda     #2                      ; straight to sustain
        sta     env_stage,x
        jmp     env_write               ; make the jump audible immediately

; ---------------------------------------------------------------------
; psg_env_release -- in: A = voice. Enter the release phase now.
; psg_env_stop    -- in: A = voice. Silence and disarm immediately.
; ---------------------------------------------------------------------
psg_env_release:
        and     #$0F
        tax
        lda     env_stage,x
        beq     .done                   ; not playing
        lda     #3
        sta     env_stage,x
.done:
        rts

psg_env_stop:
        and     #$0F
        tax
        stz     env_stage,x
        stz     env_vol,x
        jmp     env_write

; ---------------------------------------------------------------------
; psg_env_tick -- advance every armed envelope one step and write the
; changed volumes to the PSG. Call once per frame. Clobbers A/X/Y and
; the port-0 address.
; ---------------------------------------------------------------------
psg_env_tick:
        ldx     #15
.voice:
        lda     env_stage,x
        beq     .next                   ; 0: idle
        cmp     #2
        beq     .sustain
        bcc     .attack                 ; 1

        ; --- release ---
        lda     env_rstep,x
        beq     .next                   ; rstep 0: hold until psg_env_stop
        sta     X16_T0
        lda     env_vol,x
        sec
        sbc     X16_T0
        bcs     .rel_ok
        lda     #0
.rel_ok:
        sta     env_vol,x
        bne     .write
        stz     env_stage,x             ; faded out: disarm
        bra     .write

.attack:
        lda     env_vol,x
        clc
        adc     env_astep,x
        cmp     env_peak,x
        bcc     .att_ok
        lda     env_peak,x              ; reached (or overshot) the peak
        pha
        lda     #2
        sta     env_stage,x
        pla
.att_ok:
        sta     env_vol,x
        bra     .write

.sustain:
        lda     env_sus,x
        cmp     #255
        beq     .next                   ; 255: hold until psg_env_release
        dec     env_sus,x
        bne     .next
        lda     #3                      ; sustain over: release
        sta     env_stage,x
        bra     .next                   ; volume unchanged this tick

.write:
        jsr     env_write
.next:
        dex
        bpl     .voice
        rts

; write voice X's env_vol to its volume bits, preserving the pan bits
; (via the host-readback shadow, like psg_note_off). Preserves X --
; psg_voice_ptr does too.
env_write:
        lda     #2
        jsr     psg_voice_ptr
        lda     VERA_DATA0              ; the shadow's pan bits
        and     #PSG_PAN_BOTH
        ora     env_vol,x
        sta     X16_T0
        lda     #2
        jsr     psg_voice_ptr
        lda     X16_T0
        sta     VERA_DATA0
        rts

        section bss

env_stage: reserve 16
env_vol:   reserve 16
env_peak:  reserve 16
env_astep: reserve 16
env_sus:   reserve 16
env_rstep: reserve 16
