; =====================================================================
; x16clib :: gfx/verafx_utils.s -- low-level VERA FX primitives
; =====================================================================
; Raw building blocks for custom FX workflows: FX_CTRL/MULT control,
; cache fill/write/cycle toggles, 32-bit cache loading, multiplier
; accumulator triggers, increment/position registers, 16-bit hop, and
; polygon-fill reads.
;
; They are deliberately separate from gfx/verafx.s. That module is the
; high-level helper bundle; this one is for code that wants to compose
; the documented FX registers directly. Probe with x16_vera_has_fx()
; first, exactly as for the high-level routines.
;
; Unlike the verafx.s helpers, NOTHING here resets FX_CTRL on the way
; out: these are register knobs, and turning one leaves the others
; alone. Call x16_fxu_off() (or x16_fx_off()) when you are done.
;
; The routine bodies are the x16_library verafx_utils module, byte for
; byte; only the C shims in front are new.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers. Single-byte arguments pin to the a register
; (a straight jmp, as x16_gfx8h_clear); the four cache bytes ride the
; even registers r0/r2/r4/r6; the 16-bit increment/position pairs ride
; r0/r1 and r2/r3; subpos pins a + r0.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3
        zpage	r4
        zpage	r6

        global	_x16_fxu_off
        global	_x16_fxu_get_ctrl
        global	_x16_fxu_set_ctrl
        global	_x16_fxu_ctrl_on
        global	_x16_fxu_ctrl_off
        global	_x16_fxu_addr1_mode
        global	_x16_fxu_cache_write_on
        global	_x16_fxu_cache_write_off
        global	_x16_fxu_cache_fill_on
        global	_x16_fxu_cache_fill_off
        global	_x16_fxu_cache_cycle_on
        global	_x16_fxu_cache_cycle_off
        global	_x16_fxu_transparent_on
        global	_x16_fxu_transparent_off
        global	_x16_fxu_4bit_on
        global	_x16_fxu_4bit_off
        global	_x16_fxu_hop_on
        global	_x16_fxu_hop_off
        global	_x16_fxu_set_mult
        global	_x16_fxu_set_cache
        global	_x16_fxu_reset_accum
        global	_x16_fxu_accumulate
        global	_x16_fxu_cache_fill0
        global	_x16_fxu_cache_fill1
        global	_x16_fxu_cache_write0
        global	_x16_fxu_cache_write1
        global	_x16_fxu_set_incr
        global	_x16_fxu_set_pos
        global	_x16_fxu_set_subpos
        global	_x16_fxu_get_poly_fill
        global	_x16_fxu_set_tilebase
        global	_x16_fxu_set_mapbase

        section text

; =====================================================================
; C entry points
; =====================================================================

; void x16_fxu_off(void)
_x16_fxu_off:
        jmp     fxu_off

; unsigned char x16_fxu_get_ctrl(void)
_x16_fxu_get_ctrl:
        jsr     fxu_get_ctrl
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; void x16_fxu_set_ctrl(__reg("a") unsigned char v)
;   ...and ctrl_on/ctrl_off/addr1_mode/set_mult/set_tilebase/set_mapbase:
;   the single byte argument is already in a, so each is a straight jmp.
_x16_fxu_set_ctrl:
        jmp     fxu_set_ctrl

_x16_fxu_ctrl_on:
        jmp     fxu_ctrl_on

_x16_fxu_ctrl_off:
        jmp     fxu_ctrl_off

_x16_fxu_addr1_mode:
        jmp     fxu_addr1_mode

_x16_fxu_set_mult:
        jmp     fxu_set_mult

_x16_fxu_set_tilebase:
        jmp     fxu_set_tilebase

_x16_fxu_set_mapbase:
        jmp     fxu_set_mapbase

; void x16_fxu_cache_write_on(void) ...and the other no-arg toggles
_x16_fxu_cache_write_on:
        jmp     fxu_cache_write_on

_x16_fxu_cache_write_off:
        jmp     fxu_cache_write_off

_x16_fxu_cache_fill_on:
        jmp     fxu_cache_fill_on

_x16_fxu_cache_fill_off:
        jmp     fxu_cache_fill_off

_x16_fxu_cache_cycle_on:
        jmp     fxu_cache_cycle_on

_x16_fxu_cache_cycle_off:
        jmp     fxu_cache_cycle_off

_x16_fxu_transparent_on:
        jmp     fxu_transparent_on

_x16_fxu_transparent_off:
        jmp     fxu_transparent_off

_x16_fxu_4bit_on:
        jmp     fxu_4bit_on

_x16_fxu_4bit_off:
        jmp     fxu_4bit_off

_x16_fxu_hop_on:
        jmp     fxu_hop_on

_x16_fxu_hop_off:
        jmp     fxu_hop_off

_x16_fxu_reset_accum:
        jmp     fxu_reset_accum

_x16_fxu_accumulate:
        jmp     fxu_accumulate

; void x16_fxu_set_cache(__reg("r0") unsigned char l, __reg("r2") unsigned char m,
;                        __reg("r4") unsigned char h, __reg("r6") unsigned char u)
;   The body wants P0..P3 = L, M, H, U.
_x16_fxu_set_cache:
        lda     r0
        sta     X16_P0                  ; l
        lda     r2
        sta     X16_P1                  ; m
        lda     r4
        sta     X16_P2                  ; h
        lda     r6
        sta     X16_P3                  ; u
        jmp     fxu_set_cache

; unsigned char x16_fxu_cache_fill0(void) / x16_fxu_cache_fill1(void)
_x16_fxu_cache_fill0:
        jsr     fxu_cache_fill0
        ldx     #0
        rts

_x16_fxu_cache_fill1:
        jsr     fxu_cache_fill1
        ldx     #0
        rts

; void x16_fxu_cache_write0(__reg("a") unsigned char mask)
;   ...and write1: the mask is already in a.
_x16_fxu_cache_write0:
        jmp     fxu_cache_write0

_x16_fxu_cache_write1:
        jmp     fxu_cache_write1

; void x16_fxu_set_incr(__reg("r0/r1") unsigned int xi,
;                       __reg("r2/r3") unsigned int yi)
_x16_fxu_set_incr:
        lda     r0
        sta     X16_P0                  ; xi lo
        lda     r1
        sta     X16_P1                  ; xi hi
        lda     r2
        sta     X16_P2                  ; yi lo
        lda     r3
        sta     X16_P3                  ; yi hi
        jmp     fxu_set_incr

; void x16_fxu_set_pos(__reg("r0/r1") unsigned int x,
;                      __reg("r2/r3") unsigned int y)
_x16_fxu_set_pos:
        lda     r0
        sta     X16_P0                  ; x lo
        lda     r1
        sta     X16_P1                  ; x hi
        lda     r2
        sta     X16_P2                  ; y lo
        lda     r3
        sta     X16_P3                  ; y hi
        jmp     fxu_set_pos

; void x16_fxu_set_subpos(__reg("a") unsigned char xs, __reg("r0") unsigned char ys)
;   The body wants A = X subpixel, X = Y subpixel.
_x16_fxu_set_subpos:
        ldx     r0                      ; ys
        jmp     fxu_set_subpos

; unsigned int x16_fxu_get_poly_fill(void)
;   The body already returns A = low, X = high.
_x16_fxu_get_poly_fill:
        jmp     fxu_get_poly_fill

; =====================================================================
; the x16_library verafx_utils module, verbatim
; =====================================================================

; ---------------------------------------------------------------------
; fxu_off -- disable FX helpers and return to DCSEL 0
; ---------------------------------------------------------------------
fxu_off:
    vera_dcsel 2
    stz VERA_FX_CTRL
    stz VERA_FX_MULT
    vera_dcsel 0
    rts

; ---------------------------------------------------------------------
; FX_CTRL helpers
; ---------------------------------------------------------------------
fxu_get_ctrl:
    vera_dcsel 2
    lda VERA_FX_CTRL
    sta X16_T0
    vera_dcsel 0
    lda X16_T0
    rts

fxu_set_ctrl:
    pha
    vera_dcsel 2
    pla
    sta VERA_FX_CTRL
    vera_dcsel 0
    rts

fxu_ctrl_on:
    pha
    vera_dcsel 2
    pla
    tsb VERA_FX_CTRL
    vera_dcsel 0
    rts

fxu_ctrl_off:
    pha
    vera_dcsel 2
    pla
    trb VERA_FX_CTRL
    vera_dcsel 0
    rts

; fxu_addr1_mode -- set only the FX_CTRL Addr1 Mode field
;   in: A = VERA_FX_ADDR1_* value
fxu_addr1_mode:
    and #%00000011
    sta X16_T0
    vera_dcsel 2
    lda VERA_FX_CTRL
    and #%11111100
    ora X16_T0
    sta VERA_FX_CTRL
    vera_dcsel 0
    rts

fxu_cache_write_on:
    lda #VERA_FX_CACHE_WRITE
    jmp fxu_ctrl_on

fxu_cache_write_off:
    lda #VERA_FX_CACHE_WRITE
    jmp fxu_ctrl_off

fxu_cache_fill_on:
    lda #VERA_FX_CACHE_FILL
    jmp fxu_ctrl_on

fxu_cache_fill_off:
    lda #VERA_FX_CACHE_FILL
    jmp fxu_ctrl_off

fxu_cache_cycle_on:
    lda #VERA_FX_CACHE_CYCLE
    jmp fxu_ctrl_on

fxu_cache_cycle_off:
    lda #VERA_FX_CACHE_CYCLE
    jmp fxu_ctrl_off

fxu_transparent_on:
    lda #VERA_FX_TRANSPARENT
    jmp fxu_ctrl_on

fxu_transparent_off:
    lda #VERA_FX_TRANSPARENT
    jmp fxu_ctrl_off

fxu_4bit_on:
    lda #VERA_FX_4BIT_MODE
    jmp fxu_ctrl_on

fxu_4bit_off:
    lda #VERA_FX_4BIT_MODE
    jmp fxu_ctrl_off

fxu_hop_on:
    lda #VERA_FX_16BIT_HOP
    jmp fxu_ctrl_on

fxu_hop_off:
    lda #VERA_FX_16BIT_HOP
    jmp fxu_ctrl_off

; ---------------------------------------------------------------------
; FX_MULT / cache helpers
; ---------------------------------------------------------------------
fxu_set_mult:
    pha
    vera_dcsel 2
    pla
    sta VERA_FX_MULT
    vera_dcsel 0
    rts

; fxu_set_cache -- set all four bytes of the 32-bit cache
;   in: X16_P0..P3 = cache L, M, H, U
fxu_set_cache:
    vera_dcsel 6
    lda X16_P0
    sta VERA_FX_CACHE_L
    lda X16_P1
    sta VERA_FX_CACHE_M
    lda X16_P2
    sta VERA_FX_CACHE_H
    lda X16_P3
    sta VERA_FX_CACHE_U
    vera_dcsel 0
    rts

; fxu_reset_accum -- clear the multiplier accumulator
fxu_reset_accum:
    vera_dcsel 6
    lda VERA_FX_ACCUM_RESET
    vera_dcsel 0
    rts

; fxu_accumulate -- trigger multiply-then-accumulate
fxu_accumulate:
    vera_dcsel 6
    lda VERA_FX_ACCUM
    vera_dcsel 0
    rts

; fxu_cache_fill0/1 -- read DATA0/1, filling the cache when enabled
;   out: A = byte read from the selected data port
fxu_cache_fill0:
    lda VERA_DATA0
    rts

fxu_cache_fill1:
    lda VERA_DATA1
    rts

; fxu_cache_write0/1 -- write DATA0/1, flushing the cache when enabled
;   in: A = cache nibble mask
fxu_cache_write0:
    sta VERA_DATA0
    rts

fxu_cache_write1:
    sta VERA_DATA1
    rts

; ---------------------------------------------------------------------
; Increment, position, tile/map, and polygon-fill helpers
; ---------------------------------------------------------------------
; fxu_set_incr -- set X/Y increment registers
;   in: X16_P0/P1 = X increment, X16_P2/P3 = Y increment
fxu_set_incr:
    vera_dcsel 3
    lda X16_P0
    sta VERA_FX_X_INCR_L
    lda X16_P1
    sta VERA_FX_X_INCR_H
    lda X16_P2
    sta VERA_FX_Y_INCR_L
    lda X16_P3
    sta VERA_FX_Y_INCR_H
    vera_dcsel 0
    rts

; fxu_set_pos -- set X/Y position registers
;   in: X16_P0/P1 = X position, X16_P2/P3 = Y position
fxu_set_pos:
    vera_dcsel 4
    lda X16_P0
    sta VERA_FX_X_POS_L
    lda X16_P1
    sta VERA_FX_X_POS_H
    lda X16_P2
    sta VERA_FX_Y_POS_L
    lda X16_P3
    sta VERA_FX_Y_POS_H
    vera_dcsel 0
    rts

; fxu_set_subpos -- set X/Y subpixel registers
;   in: A = X subpixel, X = Y subpixel
fxu_set_subpos:
    sta X16_T0
    stx X16_T1
    vera_dcsel 5
    lda X16_T0
    sta VERA_FX_X_POS_S
    lda X16_T1
    sta VERA_FX_Y_POS_S
    vera_dcsel 0
    rts

; fxu_get_poly_fill -- read polygon fill length
;   out: A = low byte/nibble pattern, X = high byte
fxu_get_poly_fill:
    vera_dcsel 5
    lda VERA_FX_POLY_FILL_L
    sta X16_T0
    lda VERA_FX_POLY_FILL_H
    sta X16_T1
    vera_dcsel 0
    lda X16_T0
    ldx X16_T1
    rts

; fxu_set_tilebase / fxu_set_mapbase -- raw affine base register writes
;   in: A = precomposed FX_TILEBASE/FX_MAPBASE value
fxu_set_tilebase:
    pha
    vera_dcsel 2
    pla
    sta VERA_FX_TILEBASE
    vera_dcsel 0
    rts

fxu_set_mapbase:
    pha
    vera_dcsel 2
    pla
    sta VERA_FX_MAPBASE
    vera_dcsel 0
    rts
