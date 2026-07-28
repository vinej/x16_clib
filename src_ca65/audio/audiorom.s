; =====================================================================
; x16clib :: audio/audiorom.s -- the AUDIO ROM bank's API, wrapped
; =====================================================================
; Thin wrappers over the Commander X16 ROM audio bank (BANK_AUDIO, $0A).
; The entry points live at $C000+ inside that bank -- NOT in the $FFxx
; KERNAL table -- so every call here crosses banks through the KERNAL's
; JSRFAR, which preserves the flags in BOTH directions: several of these
; calls take carry as an input and nearly all of them report success or
; failure in it.
;
; The ROM driver keeps its own PSG/YM volume, pan, attenuation and patch
; shadows coherent. That is the whole reason to call the ROM rather than
; the library's own audio/psg.s and audio/ym.s, which poke the hardware
; directly and know nothing of those shadows. Do not mix the two layers
; on the same voice and expect the ROM's idea of its state to survive.
;
; Prefix convention, inherited from upstream: ar_* = audio ROM.
;
; x16_ar_fmplaystring/x16_ar_psgplaystring play their string TO THE END
; before returning, pacing themselves on the 60 Hz jiffy clock -- which
; only advances while the VSYNC interrupt runs. Do not call them with
; interrupts off (or in the headless testbench): they never come back.
; The chordstring calls strike their notes and return at once.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa, popax

; --- BASIC-compatible FM/PSG helpers ---------------------------------
        .export         _x16_ar_fmfreq
        .export         _x16_ar_fmnote
        .export         _x16_ar_fmvib
        .export         _x16_ar_fmplaystring
        .export         _x16_ar_psgplaystring
        .export         _x16_ar_fmchordstring
        .export         _x16_ar_psgchordstring
        .export         _x16_ar_playstring_voice
        .export         _x16_ar_psgfreq
        .export         _x16_ar_psgnote
        .export         _x16_ar_psgwav

; --- note conversions ------------------------------------------------
        .export         _x16_ar_note_bas2fm
        .export         _x16_ar_note_bas2midi
        .export         _x16_ar_note_bas2psg
        .export         _x16_ar_note_fm2bas
        .export         _x16_ar_note_fm2midi
        .export         _x16_ar_note_fm2psg
        .export         _x16_ar_note_freq2bas
        .export         _x16_ar_note_freq2fm
        .export         _x16_ar_note_freq2midi
        .export         _x16_ar_note_freq2psg
        .export         _x16_ar_note_midi2bas
        .export         _x16_ar_note_midi2fm
        .export         _x16_ar_note_midi2psg
        .export         _x16_ar_note_psg2bas
        .export         _x16_ar_note_psg2fm
        .export         _x16_ar_note_psg2midi

; --- ROM PSG API -----------------------------------------------------
        .export         _x16_ar_psg_init
        .export         _x16_ar_psg_playfreq
        .export         _x16_ar_psg_read
        .export         _x16_ar_psg_setatten
        .export         _x16_ar_psg_setfreq
        .export         _x16_ar_psg_setpan
        .export         _x16_ar_psg_setvol
        .export         _x16_ar_psg_write
        .export         _x16_ar_psg_write_fast
        .export         _x16_ar_psg_getatten
        .export         _x16_ar_psg_getpan

; --- ROM YM/FM API ---------------------------------------------------
        .export         _x16_ar_ym_init
        .export         _x16_ar_ym_loaddefpatches
        .export         _x16_ar_ym_loadpatch
        .export         _x16_ar_ym_loadpatch_ram
        .export         _x16_ar_ym_loadpatchlfn
        .export         _x16_ar_ym_playdrum
        .export         _x16_ar_ym_playnote
        .export         _x16_ar_ym_setatten
        .export         _x16_ar_ym_setdrum
        .export         _x16_ar_ym_setnote
        .export         _x16_ar_ym_setpan
        .export         _x16_ar_ym_read
        .export         _x16_ar_ym_release
        .export         _x16_ar_ym_trigger
        .export         _x16_ar_ym_write
        .export         _x16_ar_ym_getatten
        .export         _x16_ar_ym_getpan
        .export         _x16_ar_audio_init
        .export         _x16_ar_ym_get_chip_type

        .segment        "CODE"

; =====================================================================
; Marshalling helpers.
;
; cc65 fastcall: the rightmost argument arrives in A (or A/X for a
; word), the rest wait on the C stack in left-to-right order, popped
; back rightmost-first with popa/popax. popa preserves X and the carry
; but clobbers Y, so anything destined for Y goes through scratch and
; is loaded last. A carry input is computed with `cmp #1` (set iff the
; flag byte is nonzero) BEFORE the value registers are loaded -- lda,
; ldx, ldy, pla and jsr all leave carry alone, and JSRFAR hands the
; caller's flags to the callee intact.
; =====================================================================

; C f(a, b)                       -> A = a, X = b
arg_ax:
        tax
        jmp     popa

; C f(a, b)                       -> X = a, Y = b   (A unused)
arg_xy:
        sta     X16_T0                  ; b
        jsr     popa                    ; a
        tax
        ldy     X16_T0
        rts

; C f(a, word)                    -> A = a, X = lo, Y = hi
arg_axy:
        sta     X16_T0
        stx     X16_T1
        jsr     popa
        ldx     X16_T0
        ldy     X16_T1
        rts

; C f(word)                       -> X = lo, Y = hi
arg_wxy:
        pha
        txa
        tay
        plx
        rts

; C f(a, b, c)                    -> A = a, X = b, Y = c
arg_3:
        sta     X16_T1                  ; c
        jsr     popa
        sta     X16_T0                  ; b
        jsr     popa                    ; A = a
        ldx     X16_T0
        ldy     X16_T1
        rts

; C f(a, b, c, flag)              -> A = a, X = b, Y = c, C = flag != 0
arg_3f:
        sta     X16_T3                  ; flag
        jsr     popa
        sta     X16_T1                  ; c
        jsr     popa
        sta     X16_T0                  ; b
        jsr     popa                    ; A = a
        pha
        lda     X16_T3
        cmp     #1                      ; carry = flag != 0
        pla                             ; pla touches N/Z only
        ldx     X16_T0
        ldy     X16_T1
        rts

; C f(a, word, flag)              -> A = a, X = lo, Y = hi, C = flag != 0
arg_wf:
        sta     X16_T3                  ; flag
        jsr     popax                   ; word
        sta     X16_T0
        stx     X16_T1
        jsr     popa                    ; A = a
        pha
        lda     X16_T3
        cmp     #1
        pla
        ldx     X16_T0
        ldy     X16_T1
        rts

; C f(ptr, len)                   -> A = len, X = ptr lo, Y = ptr hi
arg_str:
        sta     X16_T2                  ; len
        jsr     popax                   ; ptr: A = lo, X = hi
        stx     X16_T1
        tax                             ; X = lo
        ldy     X16_T1                  ; Y = hi
        lda     X16_T2
        rts

; C f(reg, value)                 -> A = value, X = reg
arg_va:
        pha                             ; value
        jsr     popa                    ; reg
        tax
        pla
        rts

; ---------------------------------------------------------------------
; Return conversions.
; ---------------------------------------------------------------------

; carry -> 0 (clear, success) / 1 (set, failure)
ret_carry:
        lda     #0
        ldx     #0
        rol     a
        rts

; X -> unsigned char. The conversions answer in X on success AND in the
; error path (the ROM parks 0 in X/Y with carry set), where A is not
; always written.
ret_x:
        txa
        ldx     #0
        rts

; A -> unsigned char
ret_a:
        ldx     #0
        rts

; X/Y -> unsigned int: A = X (low), X = Y (high)
ret_xy:
        txa
        phy
        plx
        rts

; =====================================================================
; BASIC-compatible FM/PSG utility and play-string calls.
; =====================================================================

; unsigned char x16_ar_fmfreq(channel, hz, noretrigger)
_x16_ar_fmfreq:
        jsr     arg_wf
        jsrfar  rom_bas_fmfreq, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_fmnote(channel, octnote, kf, noretrigger)
_x16_ar_fmnote:
        jsr     arg_3f
        jsrfar  rom_bas_fmnote, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_fmvib(speed, depth)
_x16_ar_fmvib:
        jsr     arg_ax
        jsrfar  rom_bas_fmvib, BANK_AUDIO
        jmp     ret_carry

; void x16_ar_fmplaystring(s, len)     -- BLOCKS until the string ends
_x16_ar_fmplaystring:
        jsr     arg_str
        jsrfar  rom_bas_fmplaystring, BANK_AUDIO
        rts

; void x16_ar_psgplaystring(s, len)    -- BLOCKS until the string ends
_x16_ar_psgplaystring:
        jsr     arg_str
        jsrfar  rom_bas_psgplaystring, BANK_AUDIO
        rts

; void x16_ar_fmchordstring(s, len)    -- strikes the chord, returns
_x16_ar_fmchordstring:
        jsr     arg_str
        jsrfar  rom_bas_fmchordstring, BANK_AUDIO
        rts

; void x16_ar_psgchordstring(s, len)
_x16_ar_psgchordstring:
        jsr     arg_str
        jsrfar  rom_bas_psgchordstring, BANK_AUDIO
        rts

; void x16_ar_playstring_voice(voice)
_x16_ar_playstring_voice:
        jsrfar  rom_bas_playstringvoice, BANK_AUDIO
        rts

; unsigned char x16_ar_psgfreq(voice, hz)
_x16_ar_psgfreq:
        jsr     arg_axy
        jsrfar  rom_bas_psgfreq, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_psgnote(voice, octnote, kf)
_x16_ar_psgnote:
        jsr     arg_3
        jsrfar  rom_bas_psgnote, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_psgwav(voice, waveform)
_x16_ar_psgwav:
        jsr     arg_ax
        jsrfar  rom_bas_psgwav, BANK_AUDIO
        jmp     ret_carry

; =====================================================================
; Note conversions. Failed conversions come back as 0: the ROM parks
; 0 in X/Y with carry set for out-of-range input.
; =====================================================================

; unsigned char x16_ar_note_bas2fm(octnote)         -> YM KC
_x16_ar_note_bas2fm:
        tax
        jsrfar  rom_notecon_bas2fm, BANK_AUDIO
        jmp     ret_x

; unsigned char x16_ar_note_bas2midi(octnote)       -> MIDI note
_x16_ar_note_bas2midi:
        tax
        jsrfar  rom_notecon_bas2midi, BANK_AUDIO
        jmp     ret_x

; unsigned int x16_ar_note_bas2psg(octnote, kf)     -> PSG frequency
_x16_ar_note_bas2psg:
        jsr     arg_xy
        jsrfar  rom_notecon_bas2psg, BANK_AUDIO
        jmp     ret_xy

; unsigned char x16_ar_note_fm2bas(kc)              -> BASIC oct/note
_x16_ar_note_fm2bas:
        tax
        jsrfar  rom_notecon_fm2bas, BANK_AUDIO
        jmp     ret_x

; unsigned char x16_ar_note_fm2midi(kc)             -> MIDI note
_x16_ar_note_fm2midi:
        tax
        jsrfar  rom_notecon_fm2midi, BANK_AUDIO
        jmp     ret_x

; unsigned int x16_ar_note_fm2psg(kc, kf)           -> PSG frequency
_x16_ar_note_fm2psg:
        jsr     arg_xy
        jsrfar  rom_notecon_fm2psg, BANK_AUDIO
        jmp     ret_xy

; unsigned int x16_ar_note_freq2bas(hz)             -> oct/note | KF<<8
_x16_ar_note_freq2bas:
        jsr     arg_wxy
        jsrfar  rom_notecon_freq2bas, BANK_AUDIO
        jmp     ret_xy

; unsigned int x16_ar_note_freq2fm(hz)              -> KC | KF<<8
_x16_ar_note_freq2fm:
        jsr     arg_wxy
        jsrfar  rom_notecon_freq2fm, BANK_AUDIO
        jmp     ret_xy

; unsigned int x16_ar_note_freq2midi(hz)            -> note | KF<<8
_x16_ar_note_freq2midi:
        jsr     arg_wxy
        jsrfar  rom_notecon_freq2midi, BANK_AUDIO
        jmp     ret_xy

; unsigned int x16_ar_note_freq2psg(hz)             -> PSG frequency
_x16_ar_note_freq2psg:
        jsr     arg_wxy
        jsrfar  rom_notecon_freq2psg, BANK_AUDIO
        jmp     ret_xy

; unsigned char x16_ar_note_midi2bas(midinote)      -> BASIC oct/note
; The one conversion whose input rides A, not X (so it is here already).
_x16_ar_note_midi2bas:
        jsrfar  rom_notecon_midi2bas, BANK_AUDIO
        jmp     ret_x

; unsigned char x16_ar_note_midi2fm(midinote)       -> YM KC
_x16_ar_note_midi2fm:
        tax
        jsrfar  rom_notecon_midi2fm, BANK_AUDIO
        jmp     ret_x

; unsigned int x16_ar_note_midi2psg(midinote, kf)   -> PSG frequency
_x16_ar_note_midi2psg:
        jsr     arg_xy
        jsrfar  rom_notecon_midi2psg, BANK_AUDIO
        jmp     ret_xy

; unsigned int x16_ar_note_psg2bas(freq)            -> oct/note | KF<<8
_x16_ar_note_psg2bas:
        jsr     arg_wxy
        jsrfar  rom_notecon_psg2bas, BANK_AUDIO
        jmp     ret_xy

; unsigned int x16_ar_note_psg2fm(freq)             -> KC | KF<<8
_x16_ar_note_psg2fm:
        jsr     arg_wxy
        jsrfar  rom_notecon_psg2fm, BANK_AUDIO
        jmp     ret_xy

; unsigned int x16_ar_note_psg2midi(freq)           -> note | KF<<8
_x16_ar_note_psg2midi:
        jsr     arg_wxy
        jsrfar  rom_notecon_psg2midi, BANK_AUDIO
        jmp     ret_xy

; =====================================================================
; ROM PSG API. These keep the ROM's volume/pan/attenuation shadows
; coherent; none of them reports an error.
; =====================================================================

; void x16_ar_psg_init(void)
_x16_ar_psg_init:
        jsrfar  rom_psg_init, BANK_AUDIO
        rts

; void x16_ar_psg_playfreq(voice, freq)
_x16_ar_psg_playfreq:
        jsr     arg_axy
        jsrfar  rom_psg_playfreq, BANK_AUDIO
        rts

; unsigned char x16_ar_psg_read(reg, cooked)
;   cooked nonzero reads volumes with attenuation applied; zero reads
;   the raw value as written.
_x16_ar_psg_read:
        cmp     #1                      ; carry = cooked != 0
        jsr     popa                    ; reg (popa keeps the carry)
        tax
        jsrfar  rom_psg_read, BANK_AUDIO
        jmp     ret_a

; void x16_ar_psg_setatten(voice, atten)
_x16_ar_psg_setatten:
        jsr     arg_ax
        jsrfar  rom_psg_setatten, BANK_AUDIO
        rts

; void x16_ar_psg_setfreq(voice, freq)
_x16_ar_psg_setfreq:
        jsr     arg_axy
        jsrfar  rom_psg_setfreq, BANK_AUDIO
        rts

; void x16_ar_psg_setpan(voice, pan)
_x16_ar_psg_setpan:
        jsr     arg_ax
        jsrfar  rom_psg_setpan, BANK_AUDIO
        rts

; void x16_ar_psg_setvol(voice, vol)
_x16_ar_psg_setvol:
        jsr     arg_ax
        jsrfar  rom_psg_setvol, BANK_AUDIO
        rts

; void x16_ar_psg_write(reg, value)
_x16_ar_psg_write:
        jsr     arg_va
        jsrfar  rom_psg_write, BANK_AUDIO
        rts

; void x16_ar_psg_write_fast(reg, value)  -- caller prepoints VERA
_x16_ar_psg_write_fast:
        jsr     arg_va
        jsrfar  rom_psg_write_fast, BANK_AUDIO
        rts

; unsigned char x16_ar_psg_getatten(voice)
_x16_ar_psg_getatten:
        jsrfar  rom_psg_getatten, BANK_AUDIO
        jmp     ret_x

; unsigned char x16_ar_psg_getpan(voice)
_x16_ar_psg_getpan:
        jsrfar  rom_psg_getpan, BANK_AUDIO
        jmp     ret_x

; =====================================================================
; ROM YM/FM API. The YM sits behind a busy flag, so nearly every call
; can time out: 0 = OK, 1 = failed.
; =====================================================================

; unsigned char x16_ar_ym_init(void)
_x16_ar_ym_init:
        jsrfar  rom_ym_init, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_loaddefpatches(void)
_x16_ar_ym_loaddefpatches:
        jsrfar  rom_ym_loaddefpatches, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_loadpatch(channel, patch)  -- ROM patch 0-31
_x16_ar_ym_loadpatch:
        tax                             ; patch number
        jsr     popa                    ; channel
        sec                             ; carry set = X is a ROM patch
        jsrfar  rom_ym_loadpatch, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_loadpatch_ram(channel, patch)  -- 26 bytes in RAM
_x16_ar_ym_loadpatch_ram:
        pha                             ; patch pointer -> X/Y
        txa
        tay
        plx
        jsr     popa                    ; channel (popa keeps the carry...)
        clc                             ; ...but the intent deserves the line
        jsrfar  rom_ym_loadpatch, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_loadpatchlfn(channel, lfn)
_x16_ar_ym_loadpatchlfn:
        jsr     arg_ax
        jsrfar  rom_ym_loadpatchlfn, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_playdrum(channel, midinote)
_x16_ar_ym_playdrum:
        jsr     arg_ax
        jsrfar  rom_ym_playdrum, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_playnote(channel, kc, kf, noretrigger)
_x16_ar_ym_playnote:
        jsr     arg_3f
        jsrfar  rom_ym_playnote, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_setatten(channel, atten)
_x16_ar_ym_setatten:
        jsr     arg_ax
        jsrfar  rom_ym_setatten, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_setdrum(channel, midinote)  -- no trigger
_x16_ar_ym_setdrum:
        jsr     arg_ax
        jsrfar  rom_ym_setdrum, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_setnote(channel, kc, kf)    -- no trigger
_x16_ar_ym_setnote:
        jsr     arg_3
        jsrfar  rom_ym_setnote, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_setpan(channel, pan)
_x16_ar_ym_setpan:
        jsr     arg_ax
        jsrfar  rom_ym_setpan, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_read(reg, cooked)
;   cooked nonzero reads TL values with attenuation applied.
_x16_ar_ym_read:
        cmp     #1                      ; carry = cooked != 0
        jsr     popa                    ; reg
        tax
        jsrfar  rom_ym_read, BANK_AUDIO
        jmp     ret_a

; unsigned char x16_ar_ym_release(channel)
_x16_ar_ym_release:
        jsrfar  rom_ym_release, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_trigger(channel, noretrigger)
_x16_ar_ym_trigger:
        cmp     #1                      ; carry = noretrigger != 0
        jsr     popa                    ; channel
        jsrfar  rom_ym_trigger, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_write(reg, value)  -- raw, but keeps shadows
_x16_ar_ym_write:
        jsr     arg_va
        jsrfar  rom_ym_write, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_getatten(channel)
_x16_ar_ym_getatten:
        jsrfar  rom_ym_getatten, BANK_AUDIO
        jmp     ret_x

; unsigned char x16_ar_ym_getpan(channel)
_x16_ar_ym_getpan:
        jsrfar  rom_ym_getpan, BANK_AUDIO
        jmp     ret_x

; unsigned char x16_ar_audio_init(void)  -- YM + PSG + default patches
_x16_ar_audio_init:
        jsrfar  rom_audio_init, BANK_AUDIO
        jmp     ret_carry

; unsigned char x16_ar_ym_get_chip_type(void)
;   0 none, 1 OPP, 2 OPM, 3 unexpected. Detected during ym_init.
_x16_ar_ym_get_chip_type:
        jsrfar  rom_ym_get_chip_type, BANK_AUDIO
        jmp     ret_a
