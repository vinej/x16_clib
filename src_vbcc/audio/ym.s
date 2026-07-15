; =====================================================================
; x16clib :: audio/ym.s -- YM2151 FM synthesiser
; =====================================================================
; The chip is at YM_REG ($9F40) and YM_DATA ($9F41). Note: NOT $9FE0.
;
; Two ways in, and they do not mix freely:
;
;   ym_write   writes a chip register directly. Fast, complete access to
;              everything (LFO, per-operator envelopes) -- but the ROM
;              audio driver keeps RAM shadows of volume and pan, and a
;              raw write leaves those stale.
;
;   ym_poke    goes through the ROM driver in BANK_AUDIO, keeping its
;              shadows coherent. Use this if you also use the note API.
;
; ---------------------------------------------------------------------
; ***  THE CHANNEL GOES IN .A, NOT .X.  ***
;
; Every ROM-driver entry below takes the FM channel (0-7) in .A and its
; payload in .X. That is the opposite of the register-level ym_write, and
; the opposite of what you would guess. Get it backwards and the chip
; plays a valid-looking note on the wrong channel: it fails silently, not
; loudly. The C shims below therefore all end with the channel in A --
; and popa preserves X, which is what makes `tax` before the pop correct.
;
; jsrfar restores the callee's processor status on the way out, so the
; carry survives in BOTH directions: it can be passed in (ym_patch, and
; the retrigger flag of ym_note) and read back out as an error.
;
; ym_write brackets its two halves in sei/plp. The shims must not add a
; cli of their own.
; =====================================================================

        include        "macros.inc"

; vbcc argument registers: the channel/register rides in r0, and further
; char args each take an even register (r2, r4, r6); a 16-bit or pointer
; arg takes r2/r3.
        zpage	r0
        zpage	r2
        zpage	r3
        zpage	r4
        zpage	r6

        global	_x16_ym_write
        global	_x16_ym_busy
        global	_x16_ym_init
        global	_x16_ym_poke
        global	_x16_ym_patch_rom
        global	_x16_ym_patch_ram
        global	_x16_ym_note
        global	_x16_ym_note_bas
        global	_x16_ym_release_note
        global	_x16_ym_vol
        global	_x16_ym_pan
        global	_x16_ym_get_pan
        global	_x16_ym_get_vol
        global	_x16_ym_drum

YM_TIMEOUT = 128                ; busy-wait spins before giving up

        section text

; =====================================================================
; C entry points
; =====================================================================

; Turn "carry set = failure" into "1 = success", with the high byte
; cleared for callers who promote the result to int.
carry_to_ok	macro
        lda     #0
        ldx     #0
        rol     a                       ; carry -> bit 0
        eor     #1                      ; report success, not failure
endm

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_ym_write(unsigned char reg,
;                                         unsigned char value)
;   returns 1 on success, 0 if the chip stayed busy
;
; Register-level: value in A, register in X. Note this is the reverse of
; every ROM-driver call below.
; ---------------------------------------------------------------------
; ym_write(__reg("r0") reg, __reg("r2") value): ym_write wants A=value, X=reg.
_x16_ym_write:
        lda     r2                      ; A = value
        ldx     r0                      ; X = reg
        jsr     ym_write
        carry_to_ok
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_ym_poke(unsigned char reg, unsigned char value)
;   Same register/value order as x16_ym_write, but through the ROM driver
;   so its volume and pan shadows stay coherent.
; ---------------------------------------------------------------------
; ym_poke(__reg("r0") reg, __reg("r2") value): A=value, X=reg.
_x16_ym_poke:
        lda     r2
        ldx     r0
        jmp     ym_poke

; ---------------------------------------------------------------------
; unsigned char x16_ym_busy(void)
; ---------------------------------------------------------------------
_x16_ym_busy:
        jsr     ym_busy
        lda     #0
        ldx     #0
        rol     a                       ; carry -> bit 0; busy is not an error
        rts

; ---------------------------------------------------------------------
; unsigned char x16_ym_init(void)
;   Reset the chip and load the default patches. 0 means no YM2151 here.
;   Must precede x16_ym_patch_*.
; ---------------------------------------------------------------------
_x16_ym_init:
        jsr     ym_init
        carry_to_ok
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_ym_patch_rom(unsigned char channel,
;                                             unsigned char patch)
;   ROM patch index 0-162. Carry SET selects the ROM table.
;
; popa leaves X alone, so the payload can go to X before the channel is
; popped into A. Every channel-in-A shim below uses that.
; ---------------------------------------------------------------------
; ym_patch_rom(__reg("r0") channel, __reg("r2") patch): A=channel, X=patch.
_x16_ym_patch_rom:
        lda     r0                      ; A = channel
        ldx     r2                      ; X = patch index
        sec                             ; ROM patch table
        jsr     ym_patch
        carry_to_ok
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_ym_patch_ram(unsigned char channel,
;                                             const void *patch)
;   Carry CLEAR selects a patch in RAM, addressed by X/Y.
; ---------------------------------------------------------------------
; ym_patch_ram(__reg("r0") channel, __reg("r2/r3") patch): A=channel,
; X/Y = patch address, carry clear (RAM).
_x16_ym_patch_ram:
        lda     r0                      ; A = channel
        ldx     r2                      ; X = patch lo
        ldy     r3                      ; Y = patch hi
        clc                             ; patch in RAM
        jsr     ym_patch
        carry_to_ok
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_ym_note(unsigned char channel, unsigned char kc,
;                               unsigned char kf, unsigned char retrigger)
;   Raw YM2151 key code and key fraction (pitch bend).
;   retrigger != 0 restarts the envelope; 0 only changes the pitch.
; ---------------------------------------------------------------------
; ym_note(__reg("r0") channel, __reg("r2") kc, __reg("r4") kf, __reg("r6") retrigger)
;   ym_note wants A=channel, X=kc, Y=kf; ym_tmp holds retrigger.
_x16_ym_note:
        lda     r6
        sta     ym_tmp                  ; retrigger
        lda     r0                      ; A = channel
        ldx     r2                      ; X = kc
        ldy     r4                      ; Y = kf
        jsr     set_retrigger_carry
        jmp     ym_note

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_ym_note_bas(unsigned char channel,
;                                            unsigned char note,
;                                            unsigned char retrigger)
;   A packed note, (octave << 4) | 1..12; note 0 releases. This is the
;   one for playing tunes: the ROM's BASIC shim converts it to a key code.
; ---------------------------------------------------------------------
; ym_note_bas(__reg("r0") channel, __reg("r2") note, __reg("r4") retrigger)
;   ym_note_bas wants A=channel, X=note; ym_tmp holds retrigger.
_x16_ym_note_bas:
        lda     r4
        sta     ym_tmp                  ; retrigger
        lda     r0                      ; A = channel
        ldx     r2                      ; X = note
        jsr     set_retrigger_carry
        jsr     ym_note_bas
        carry_to_ok
        rts

; in:  A = channel, ym_tmp = retrigger flag
; out: A = channel, carry clear to retrigger. X and Y untouched.
set_retrigger_carry:
        pha
        lda     ym_tmp
        beq     .hold
        pla
        clc                             ; retrigger the envelope
        rts
.hold:
        pla
        sec                             ; change pitch only
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_ym_release_note(unsigned char channel)
;   Channel already in A: no shim.
; ---------------------------------------------------------------------
_x16_ym_release_note:
        jmp     ym_release_note

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_ym_vol(unsigned char channel,
;                                       unsigned char atten)
;   atten 0 is the patch's own volume; larger is quieter.
; ---------------------------------------------------------------------
_x16_ym_vol:
        lda     r0                      ; A = channel
        ldx     r2                      ; X = second arg
        jsr     ym_vol
        carry_to_ok
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_ym_pan(unsigned char channel,
;                                       unsigned char pan)
;   0 off, 1 left, 2 right, 3 both.
; ---------------------------------------------------------------------
_x16_ym_pan:
        lda     r0                      ; A = channel
        ldx     r2                      ; X = second arg
        jsr     ym_pan
        carry_to_ok
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_ym_drum(unsigned char channel,
;                                        unsigned char note)
;   Drum note 25-87.
; ---------------------------------------------------------------------
; ym_drum(__reg("r0") channel, __reg("r2") note): A=channel, X=note.
_x16_ym_drum:
        lda     r0                      ; A = channel
        ldx     r2                      ; X = drum note
        jsr     ym_drum
        carry_to_ok
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_ym_get_pan(unsigned char channel)
; unsigned char __fastcall__ x16_ym_get_vol(unsigned char channel)
;
; The ROM driver returns its shadow in X. These agree with the chip only
; if you have been writing through x16_ym_poke / _vol / _pan rather than
; the raw x16_ym_write.
; ---------------------------------------------------------------------
_x16_ym_get_pan:
        jsr     ym_get_pan
        txa
        ldx     #0
        rts

_x16_ym_get_vol:
        jsr     ym_get_vol
        txa
        ldx     #0
        rts

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; ym_write -- raw register write
;   in:  A = value, X = register
;   out: carry clear on success, set if the chip stayed busy
;   Preserves A and X.
;
; The busy flag must be clear before touching YM_REG, and the chip needs
; settling time between the register select and the data write. Wrapped
; in sei so an interrupt cannot land between the two halves and leave a
; half-issued write behind.
; ---------------------------------------------------------------------
ym_write:
        php
        sei

        ldy     #YM_TIMEOUT
.wait:
        dey
        bmi     .timeout
        bit     YM_DATA
        bmi     .wait                   ; busy

        stx     YM_REG
        nop                             ; settling time between select and data
        nop
        nop
        sta     YM_DATA

        plp
        clc
        rts
.timeout:
        plp
        sec
        rts

; ---------------------------------------------------------------------
; ym_busy -- out: carry set while the chip is busy
; ---------------------------------------------------------------------
ym_busy:
        lda     YM_DATA
        asl     a                       ; bit 7 into carry
        rts

; ---------------------------------------------------------------------
; ROM driver entry points, in BANK_AUDIO at $C000+ rather than the $FFxx
; jump table, so they go through jsrfar. Channel in A, payload in X.
; ---------------------------------------------------------------------

; ym_init -- out: carry set on failure
ym_init:
        jsrfar  rom_audio_init, BANK_AUDIO
        jsrfar  rom_ym_loaddefpatches, BANK_AUDIO
        rts

; ym_poke -- in: A = value, X = register. Preserves A and X.
ym_poke:
        jsrfar  rom_ym_write, BANK_AUDIO
        rts

; ym_patch -- in: A = channel; carry set -> X = ROM index,
;                              carry clear -> X/Y = RAM address
;             out: carry set on failure
ym_patch:
        jsrfar  rom_ym_loadpatch, BANK_AUDIO
        rts

; ym_note -- in: A = channel, X = KC, Y = KF; carry clear to retrigger
ym_note:
        jsrfar  rom_ym_playnote, BANK_AUDIO
        rts

; ym_note_bas -- in: A = channel, X = packed note (0 releases)
;                carry clear to retrigger; out: carry set on failure
ym_note_bas:
        jsrfar  rom_bas_fmnote, BANK_AUDIO
        rts

; ym_release_note -- in: A = channel
ym_release_note:
        jsrfar  rom_ym_release, BANK_AUDIO
        rts

; ym_vol -- in: A = channel, X = attenuation
ym_vol:
        jsrfar  rom_ym_setatten, BANK_AUDIO
        rts

; ym_pan -- in: A = channel, X = 0 off, 1 left, 2 right, 3 both
ym_pan:
        jsrfar  rom_ym_setpan, BANK_AUDIO
        rts

; ym_get_pan -- in: A = channel. out: X = pan
ym_get_pan:
        jsrfar  rom_ym_getpan, BANK_AUDIO
        rts

; ym_get_vol -- in: A = channel. out: X = attenuation
ym_get_vol:
        jsrfar  rom_ym_getatten, BANK_AUDIO
        rts

; ym_drum -- in: A = channel, X = drum note (25-87)
ym_drum:
        jsrfar  rom_ym_playdrum, BANK_AUDIO
        rts

; ---------------------------------------------------------------------
; Shim scratch. Not the X16_T block: a shim must survive a jsrfar into
; ROM, and keeping its two bytes here makes that independent of whatever
; the T registers are doing.
; ---------------------------------------------------------------------
        section bss

ym_tmp: reserve    2
