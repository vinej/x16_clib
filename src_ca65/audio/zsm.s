; =====================================================================
; x16clib :: audio/zsm.s -- compact ZSM stream player
; =====================================================================
; Supports ZSM revision 1 streams loaded in normal 16-bit address space:
;   - ZSM header validation ('z','m'), stream starts at header+16
;   - PSG register writes
;   - YM2151 register/value batch writes
;   - delay commands, EOF, and 16-bit loop offsets
;   - PCM EXTCMD channel 0 commands 0/1 (AUDIO_CTRL/AUDIO_RATE)
;   - PCM instrument triggers (EXTCMD command 2) from the optional PCM
;     table, for memory-resident sample data in 16-bit address space,
;     through the AFLOW streamer in audio/pcmstream.s
;
; Call x16_zsm_tick() at the ZSM header's tick rate; use
; x16_zsm_get_tickrate() after init if you need to configure your
; scheduler. The player touches the library's zero-page scratch, so
; tick from the main loop, not from an interrupt handler.
;
; PORT NOTES, both proven by test/runner8.c:
;
; The ACME original counted EXTCMD payload bytes in X16_T1 -- which
; zsm_next, the stream reader, uses as the high half of its pointer
; (X16_TPTR0 = T0/T1). Every EXTCMD would run the counter over its own
; stream pointer and desynchronise. The counter lives in zsm_extlen,
; a plain byte, here.
;
; The original's PCM trigger also added its instrument-table offset
; from T1/T2 AFTER parking the table pointer in TPTR0 (= T0/T1),
; overwriting the offset's low byte with the pointer's high byte. The
; shifted offset lives in T2/T3 here, out of the alias's way.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popax
        .import         pcm_stream_start        ; audio/pcmstream.s
        .import         pcm_stream_stop
        .import         pcm_str_loop

        .export         _x16_zsm_init
        .export         _x16_zsm_init_stream
        .export         _x16_zsm_play
        .export         _x16_zsm_stop
        .export         _x16_zsm_rewind
        .export         _x16_zsm_get_tickrate
        .export         _x16_zsm_status
        .export         _x16_zsm_tick
        .export         _x16_zsm_lasterr
        .export         _x16_zsm_pcm_present
        .export         _x16_zsm_pcm_trigger

; ---------------------------------------------------------------------
; ca65 -t cx16 TRANSLATES CHARACTER LITERALS TO PETSCII. ACME did not.
; The magics on disk are ASCII bytes, so they are spelled out here.
; ---------------------------------------------------------------------
CH_LZ = $7A                             ; 'z'
CH_LM = $6D                             ; 'm'
CH_P  = $50
CH_C  = $43
CH_M  = $4D

; Error codes; keep x16/zsm.h's X16_ZSM_ERR_* in agreement.
ZSM_ERR_NONE    = 0
ZSM_ERR_MAGIC   = 1
ZSM_ERR_VERSION = 2
ZSM_ERR_RANGE   = 3
ZSM_ERR_PCM     = 4

; Status bits; keep x16/zsm.h's X16_ZSM_* in agreement.
ZSM_FLAG_ACTIVE = %00000001
ZSM_FLAG_LOOP   = %00000010
ZSM_FLAG_EOF    = %00000100
ZSM_FLAG_PCM    = %00001000

ZSM_MAX_VERSION = 1
ZSM_YM_TIMEOUT  = 128

ZSM_PCM_FIFO_RESET = %10000000
ZSM_PCM_16BIT      = %00100000
ZSM_PCM_STEREO     = %00010000

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_zsm_init(const void *header)
;   0 on success, else X16_ZSM_ERR_*.
; ---------------------------------------------------------------------
_x16_zsm_init:
        sta     X16_P0
        stx     X16_P1
        jsr     zsm_init
        bcs     @err                    ; A already holds the code
        lda     #0
@err:
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_zsm_init_stream(const void *stream,
;                                       const void *loop)
; ---------------------------------------------------------------------
_x16_zsm_init_stream:
        sta     X16_P2                  ; loop (rightmost arg, in A/X)
        stx     X16_P3
        jsr     popax                   ; stream
        sta     X16_P0
        stx     X16_P1
        jmp     zsm_init_stream

; ---------------------------------------------------------------------
; void x16_zsm_play(void) / x16_zsm_stop(void) / x16_zsm_rewind(void)
; ---------------------------------------------------------------------
_x16_zsm_play:
        jmp     zsm_play

_x16_zsm_stop:
        jmp     zsm_stop

_x16_zsm_rewind:
        jmp     zsm_rewind

; ---------------------------------------------------------------------
; unsigned int x16_zsm_get_tickrate(void)
;   The internal routine already answers low in A, high in X -- which
;   IS cc65's int return.
; ---------------------------------------------------------------------
_x16_zsm_get_tickrate = zsm_get_tickrate

; ---------------------------------------------------------------------
; unsigned char x16_zsm_status(void) -- X16_ZSM_* flag bits
; unsigned char x16_zsm_tick(void)   -- advance one tick; same bits
; ---------------------------------------------------------------------
_x16_zsm_status:
        jsr     zsm_status
        ldx     #0
        rts

_x16_zsm_tick:
        jsr     zsm_tick
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char x16_zsm_lasterr(void)
; ---------------------------------------------------------------------
_x16_zsm_lasterr:
        jsr     zsm_lasterr
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char x16_zsm_pcm_present(void)
; void __fastcall__ x16_zsm_pcm_trigger(unsigned char instrument)
; ---------------------------------------------------------------------
_x16_zsm_pcm_present:
        jsr     zsm_pcm_present
        lda     #0
        ldx     #0
        rol     a                       ; carry -> bit 0
        rts

_x16_zsm_pcm_trigger:
        jmp     zsm_pcm_trigger

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; zsm_lasterr -- why the last zsm_init failed
;   out: A = ZSM_ERR_* (ZSM_ERR_NONE after one that worked)
;
; zsm_init answers with both a carry and a code, and a caller that can
; only read one of them needs the code: "it would not start" is not much
; to go on when the answer is that the file is a version too new.
; ---------------------------------------------------------------------
zsm_lasterr:
        lda     zsm_code
        rts

; ---------------------------------------------------------------------
; zsm_init -- initialize from a ZSM file header
;   in:  X16_P0/P1 = pointer to the 16-byte ZSM header
;   out: carry clear on success
;        carry set on failure, A = ZSM_ERR_*
;
; Only 16-bit loop offsets are supported. A file with loop offset bit
; 16 set returns ZSM_ERR_RANGE.
; ---------------------------------------------------------------------
zsm_init:
        lda     X16_P0
        sta     zsm_baseL
        lda     X16_P1
        sta     zsm_baseH

        ldy     #0
        lda     (X16_PTR0),y
        cmp     #CH_LZ
        bne     @magic
        iny
        lda     (X16_PTR0),y
        cmp     #CH_LM
        bne     @magic

        ldy     #2
        lda     (X16_PTR0),y
        cmp     #ZSM_MAX_VERSION + 1
        bcs     @version

        ldy     #$0c
        lda     (X16_PTR0),y
        sta     zsm_tickL
        iny
        lda     (X16_PTR0),y
        sta     zsm_tickH

        jsr     zsm_pcm_init
        bcs     @pcm_error

        clc
        lda     zsm_baseL
        adc     #16
        sta     zsm_ptrL
        lda     zsm_baseH
        adc     #0
        sta     zsm_ptrH
        lda     zsm_ptrL
        sta     zsm_startL
        lda     zsm_ptrH
        sta     zsm_startH

        ldy     #3
        lda     (X16_PTR0),y
        sta     X16_T0
        iny
        lda     (X16_PTR0),y
        sta     X16_T1
        iny
        lda     (X16_PTR0),y
        bne     @range

        lda     X16_T0
        ora     X16_T1
        beq     @noloop
        clc
        lda     zsm_baseL
        adc     X16_T0
        sta     zsm_loopL
        lda     zsm_baseH
        adc     X16_T1
        sta     zsm_loopH
        lda     #(ZSM_FLAG_ACTIVE | ZSM_FLAG_LOOP)
        bra     @state
@noloop:
        stz     zsm_loopL
        stz     zsm_loopH
        lda     #ZSM_FLAG_ACTIVE
@state:
        sta     zsm_flags
        stz     zsm_delay
        lda     #ZSM_ERR_NONE
        sta     zsm_code
        clc
        rts
@magic:
        lda     #ZSM_ERR_MAGIC
        sta     zsm_code
        sec
        rts
@version:
        lda     #ZSM_ERR_VERSION
        sta     zsm_code
        sec
        rts
@range:
        lda     #ZSM_ERR_RANGE
        sta     zsm_code
        sec
        rts
@pcm_error:
        lda     #ZSM_ERR_PCM
        sta     zsm_code
        sec
        rts

; ---------------------------------------------------------------------
; zsm_init_stream -- initialize a raw headerless ZSM stream
;   in: X16_P0/P1 = stream pointer, X16_P2/P3 = loop pointer or 0
; ---------------------------------------------------------------------
zsm_init_stream:
        lda     X16_P0
        sta     zsm_baseL
        sta     zsm_ptrL
        sta     zsm_startL
        lda     X16_P1
        sta     zsm_baseH
        sta     zsm_ptrH
        sta     zsm_startH
        lda     X16_P2
        sta     zsm_loopL
        lda     X16_P3
        sta     zsm_loopH
        stz     zsm_pcm_flags
        stz     zsm_pcm_rate
        lda     X16_P2
        ora     X16_P3
        beq     @noloop
        lda     #(ZSM_FLAG_ACTIVE | ZSM_FLAG_LOOP)
        bra     @state
@noloop:
        lda     #ZSM_FLAG_ACTIVE
@state:
        sta     zsm_flags
        stz     zsm_delay
        lda     #60
        sta     zsm_tickL
        stz     zsm_tickH
        clc
        rts

; ---------------------------------------------------------------------
; zsm_play / zsm_stop / zsm_rewind
; ---------------------------------------------------------------------
zsm_play:
        lda     #ZSM_FLAG_ACTIVE
        tsb     zsm_flags
        rts

zsm_stop:
        lda     #ZSM_FLAG_ACTIVE
        trb     zsm_flags
        jsr     pcm_stream_stop
        stz     VERA_AUDIO_RATE
        rts

zsm_rewind:
        lda     zsm_startL
        sta     zsm_ptrL
        lda     zsm_startH
        sta     zsm_ptrH
        stz     zsm_delay
        lda     #ZSM_FLAG_EOF
        trb     zsm_flags
        rts

; ---------------------------------------------------------------------
; zsm_get_tickrate -- out: A = low byte, X = high byte
; ---------------------------------------------------------------------
zsm_get_tickrate:
        lda     zsm_tickL
        ldx     zsm_tickH
        rts

; ---------------------------------------------------------------------
; zsm_status
;   out: A = ZSM_FLAG_* bits, carry set if active
; ---------------------------------------------------------------------
zsm_status:
        lda     zsm_flags
        lsr
        lda     zsm_flags
        rts

; ---------------------------------------------------------------------
; zsm_tick -- advance playback by one player tick
;   out: A = ZSM_FLAG_* bits, carry set if still active
; ---------------------------------------------------------------------
zsm_tick:
        lda     zsm_flags
        and     #ZSM_FLAG_ACTIVE
        beq     @inactive
        lda     zsm_delay
        beq     @commands
        dec     zsm_delay
        bra     zsm_status
@commands:
        jsr     zsm_next
        cmp     #$40
        bcc     @psg
        beq     @ext
        cmp     #$80
        bcc     @ym
        beq     @eof

        and     #$7f                    ; delay 1..127 ticks
        sta     zsm_delay
        bra     zsm_status

@psg:
        tax                             ; X = PSG register offset
        jsr     zsm_next                ; A = value (X survives)
        jsr     zsm_psg_write
        bra     @commands

@ym:
        and     #$3f                    ; number of reg/value pairs
        tax
        beq     @commands
@ym_loop:
        phx
        jsr     zsm_next
        tax                             ; X = YM register
        jsr     zsm_next                ; A = value
        jsr     zsm_ym_write
        plx
        dex
        bne     @ym_loop
        bra     @commands

@ext:
        jsr     zsm_next                ; ccnnnnnn
        tax
        and     #$3f
        sta     zsm_extlen              ; remaining payload length -- NOT
                                        ; X16_T1: zsm_next owns T0/T1
        txa
        and     #%11000000              ; channel 0 = PCM, others skipped
        bne     @skip_ext
        jsr     zsm_ext_pcm
        bra     @commands
@skip_ext:
        jsr     zsm_skip_ext
        bra     @commands

@eof:
        lda     zsm_flags
        and     #ZSM_FLAG_LOOP
        beq     @stop_eof
        lda     zsm_loopL
        sta     zsm_ptrL
        lda     zsm_loopH
        sta     zsm_ptrH
        bra     @commands
@stop_eof:
        lda     #ZSM_FLAG_ACTIVE
        trb     zsm_flags
        lda     #ZSM_FLAG_EOF
        tsb     zsm_flags
@inactive:
        jmp     zsm_status

; ---------------------------------------------------------------------
; zsm_next -- read one stream byte and advance zsm_ptr
;   out: A = the byte. Preserves X; clobbers Y and X16_T0/T1 (TPTR0).
; ---------------------------------------------------------------------
zsm_next:
        lda     zsm_ptrL
        sta     X16_TPTR0
        lda     zsm_ptrH
        sta     X16_TPTR0+1
        ldy     #0
        lda     (X16_TPTR0),y
        inc     zsm_ptrL
        bne     @done
        inc     zsm_ptrH
@done:
        rts

; ---------------------------------------------------------------------
; zsm_skip_ext -- skip zsm_extlen stream bytes
; ---------------------------------------------------------------------
zsm_skip_ext:
        lda     zsm_extlen
        beq     @done
@loop:
        jsr     zsm_next
        dec     zsm_extlen
        bne     @loop
@done:
        rts

; ---------------------------------------------------------------------
; zsm_ext_pcm -- handle EXTCMD channel 0 command/argument pairs
;   zsm_extlen = payload length. Unknown/truncated commands are
;   consumed. Command 0 = AUDIO_CTRL, 1 = AUDIO_RATE, 2 = instrument
;   trigger.
; ---------------------------------------------------------------------
zsm_ext_pcm:
        lda     zsm_extlen
        beq     zep_done
zep_loop:
        jsr     zsm_next
        tax                             ; command
        dec     zsm_extlen
        beq     zep_done                ; truncated command: consumed
        jsr     zsm_next                ; argument (X survives)
        tay
        dec     zsm_extlen
        txa
        beq     zep_ctrl
        cmp     #1
        beq     zep_rate
        cmp     #2
        beq     zep_trigger
        bra     zep_next                ; unknown command: ignored
zep_ctrl:
        tya
        sta     VERA_AUDIO_CTRL
        bra     zep_next
zep_rate:
        tya
        sta     zsm_pcm_rate
        sta     VERA_AUDIO_RATE
        bra     zep_next
zep_trigger:
        tya
        jsr     zsm_pcm_trigger
zep_next:
        lda     zsm_extlen
        bne     zep_loop
zep_done:
        rts

; ---------------------------------------------------------------------
; zsm_pcm_init -- parse optional PCM header/table from the ZSM header
;   in: X16_P0/P1 = ZSM header pointer
;   out: carry set if the PCM header is present but unsupported/invalid
; ---------------------------------------------------------------------
zsm_pcm_init:
        stz     zsm_pcm_flags
        stz     zsm_pcm_rate

        ldy     #6
        lda     (X16_PTR0),y
        sta     X16_T0
        iny
        lda     (X16_PTR0),y
        sta     X16_T1
        iny
        lda     (X16_PTR0),y
        beq     @pcm_offset_ok
        jmp     @range
@pcm_offset_ok:
        lda     X16_T0
        ora     X16_T1
        bne     @has_pcm
        clc                             ; no PCM header
        rts
@has_pcm:
        clc
        lda     zsm_baseL
        adc     X16_T0
        sta     zsm_pcm_hdrL
        lda     zsm_baseH
        adc     X16_T1
        sta     zsm_pcm_hdrH
        bcs     @range

        lda     zsm_pcm_hdrL
        sta     X16_TPTR0
        lda     zsm_pcm_hdrH
        sta     X16_TPTR0+1
        ldy     #0
        lda     (X16_TPTR0),y
        cmp     #CH_P
        bne     @range
        iny
        lda     (X16_TPTR0),y
        cmp     #CH_C
        bne     @range
        iny
        lda     (X16_TPTR0),y
        cmp     #CH_M
        bne     @range
        iny
        lda     (X16_TPTR0),y
        sta     zsm_pcm_last

        ; data base = pcm header + 4 + 16 * (last index + 1)
        lda     zsm_pcm_last
        cmp     #$ff
        bne     @count_to_bytes
        stz     X16_T0                  ; 256 entries: 4096 bytes exactly
        lda     #$10
        sta     X16_T1
        bra     @table_bytes
@count_to_bytes:
        inc     a
        sta     X16_T0
        stz     X16_T1
        asl     X16_T0
        rol     X16_T1
        asl     X16_T0
        rol     X16_T1
        asl     X16_T0
        rol     X16_T1
        asl     X16_T0
        rol     X16_T1
@table_bytes:
        clc
        lda     zsm_pcm_hdrL
        adc     #4
        sta     zsm_pcm_dataL
        lda     zsm_pcm_hdrH
        adc     #0
        sta     zsm_pcm_dataH
        clc
        lda     zsm_pcm_dataL
        adc     X16_T0
        sta     zsm_pcm_dataL
        lda     zsm_pcm_dataH
        adc     X16_T1
        sta     zsm_pcm_dataH
        bcs     @range

        lda     #(ZSM_FLAG_PCM)
        tsb     zsm_pcm_flags
        clc
        rts
@range:
        sec
        rts

; ---------------------------------------------------------------------
; zsm_pcm_present -- out: carry set if a supported PCM table is present
; ---------------------------------------------------------------------
zsm_pcm_present:
        lda     zsm_pcm_flags
        and     #ZSM_FLAG_PCM
        beq     @no
        sec
        rts
@no:
        clc
        rts

; ---------------------------------------------------------------------
; zsm_pcm_trigger -- start the PCM instrument in A
;
; Silently ignores a missing table, an out-of-range index, a sample
; with a >64K offset or length, and a zero-length sample.
; ---------------------------------------------------------------------
zsm_pcm_trigger:
        sta     X16_T4                  ; instrument index
        jsr     zsm_pcm_present
        bcs     @present
        rts
@present:
        lda     X16_T4
        cmp     zsm_pcm_last
        bcc     @index_ok
        beq     @index_ok
        rts
@index_ok:
        ; instrument pointer = header + 4 + index*16. The shifted index
        ; goes in T2/T3: TPTR0 is T0/T1, and the ACME original's use of
        ; T1/T2 here overwrote its own offset -- see the file header.
        lda     X16_T4
        sta     X16_T2
        stz     X16_T3
        asl     X16_T2
        rol     X16_T3
        asl     X16_T2
        rol     X16_T3
        asl     X16_T2
        rol     X16_T3
        asl     X16_T2
        rol     X16_T3
        clc
        lda     zsm_pcm_hdrL
        adc     #4
        sta     X16_TPTR0
        lda     zsm_pcm_hdrH
        adc     #0
        sta     X16_TPTR0+1
        clc
        lda     X16_TPTR0
        adc     X16_T2
        sta     X16_TPTR0
        lda     X16_TPTR0+1
        adc     X16_T3
        sta     X16_TPTR0+1

        ldy     #1
        lda     (X16_TPTR0),y           ; instrument AUDIO_CTRL format bits
        and     #(ZSM_PCM_16BIT | ZSM_PCM_STEREO)
        sta     X16_T3

        ldy     #4                      ; sample offset high byte unsupported
        lda     (X16_TPTR0),y
        bne     @done
        ldy     #7                      ; sample length high byte unsupported
        lda     (X16_TPTR0),y
        bne     @done

        ldy     #2
        lda     (X16_TPTR0),y
        sta     X16_T4                  ; sample offset low
        iny
        lda     (X16_TPTR0),y
        sta     X16_T5                  ; sample offset high
        ldy     #5
        lda     (X16_TPTR0),y
        sta     X16_P2                  ; sample length low
        iny
        lda     (X16_TPTR0),y
        sta     X16_P3                  ; sample length high
        ora     X16_P2
        beq     @done

        ldy     #8
        lda     (X16_TPTR0),y
        and     #%10000000
        sta     pcm_str_loop

        ; sample source = pcm data base + sample offset
        clc
        lda     zsm_pcm_dataL
        adc     X16_T4
        sta     X16_P0
        lda     zsm_pcm_dataH
        adc     X16_T5
        sta     X16_P1
        bcs     @done                   ; crossed 64K, unsupported here

        stz     VERA_AUDIO_RATE
        lda     VERA_AUDIO_CTRL
        and     #$0f
        ora     X16_T3
        ora     #ZSM_PCM_FIFO_RESET
        sta     VERA_AUDIO_CTRL

        lda     zsm_pcm_rate
        jmp     pcm_stream_start
@done:
        rts

; ---------------------------------------------------------------------
; zsm_psg_write -- write A to PSG register offset X
; ---------------------------------------------------------------------
zsm_psg_write:
        sta     X16_T0
        lda     #VERA_CTRL_ADDRSEL
        trb     VERA_CTRL
        txa
        clc
        adc     #<VRAM_PSG
        sta     VERA_ADDR_L
        lda     #>VRAM_PSG
        adc     #0
        sta     VERA_ADDR_M
        lda     #VERA_ADDR_H_BANK
        sta     VERA_ADDR_H
        lda     X16_T0
        sta     VERA_DATA0
        rts

; ---------------------------------------------------------------------
; zsm_ym_write -- raw YM register write
;   in: A = value, X = register
; ---------------------------------------------------------------------
zsm_ym_write:
        sta     X16_T0
        stx     X16_T1
        php
        sei
        ldy     #ZSM_YM_TIMEOUT
@wait:
        dey
        bmi     @done
        bit     YM_DATA
        bmi     @wait
        lda     X16_T1
        sta     YM_REG
        nop
        nop
        nop
        lda     X16_T0
        sta     YM_DATA
@done:
        plp
        rts

; ---------------------------------------------------------------------
; Player state. DATA rather than BSS for the sake of the two initial
; values the ACME original carried: a 60 Hz default tick rate.
; ---------------------------------------------------------------------
        .segment        "DATA"

zsm_code:     .byte 0                   ; the last ZSM_ERR_*, for zsm_lasterr
zsm_baseL:    .byte 0
zsm_baseH:    .byte 0
zsm_startL:   .byte 0
zsm_startH:   .byte 0
zsm_ptrL:     .byte 0
zsm_ptrH:     .byte 0
zsm_loopL:    .byte 0
zsm_loopH:    .byte 0
zsm_tickL:    .byte 60
zsm_tickH:    .byte 0
zsm_delay:    .byte 0
zsm_flags:    .byte 0
zsm_extlen:   .byte 0                   ; EXTCMD payload countdown
zsm_pcm_hdrL: .byte 0
zsm_pcm_hdrH: .byte 0
zsm_pcm_dataL: .byte 0
zsm_pcm_dataH: .byte 0
zsm_pcm_last: .byte 0
zsm_pcm_rate: .byte 0
zsm_pcm_flags: .byte 0
