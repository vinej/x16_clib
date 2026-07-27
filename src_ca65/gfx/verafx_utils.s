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

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa, popax

        .export         _x16_fxu_off
        .export         _x16_fxu_get_ctrl
        .export         _x16_fxu_set_ctrl
        .export         _x16_fxu_ctrl_on
        .export         _x16_fxu_ctrl_off
        .export         _x16_fxu_addr1_mode
        .export         _x16_fxu_cache_write_on
        .export         _x16_fxu_cache_write_off
        .export         _x16_fxu_cache_fill_on
        .export         _x16_fxu_cache_fill_off
        .export         _x16_fxu_cache_cycle_on
        .export         _x16_fxu_cache_cycle_off
        .export         _x16_fxu_transparent_on
        .export         _x16_fxu_transparent_off
        .export         _x16_fxu_4bit_on
        .export         _x16_fxu_4bit_off
        .export         _x16_fxu_hop_on
        .export         _x16_fxu_hop_off
        .export         _x16_fxu_set_mult
        .export         _x16_fxu_set_cache
        .export         _x16_fxu_reset_accum
        .export         _x16_fxu_accumulate
        .export         _x16_fxu_cache_fill0
        .export         _x16_fxu_cache_fill1
        .export         _x16_fxu_cache_write0
        .export         _x16_fxu_cache_write1
        .export         _x16_fxu_set_incr
        .export         _x16_fxu_set_pos
        .export         _x16_fxu_set_subpos
        .export         _x16_fxu_get_poly_fill
        .export         _x16_fxu_set_tilebase
        .export         _x16_fxu_set_mapbase

        .segment        "CODE"

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

; void __fastcall__ x16_fxu_set_ctrl(unsigned char v)
;   ...and ctrl_on/ctrl_off/addr1_mode/set_mult/set_tilebase/set_mapbase:
;   the single byte argument is already in A, so each is a straight jmp.
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

; void __fastcall__ x16_fxu_set_cache(unsigned char l, unsigned char m,
;                                     unsigned char h, unsigned char u)
_x16_fxu_set_cache:
        sta     X16_P3                  ; u (rightmost arg, in A)
        jsr     popa
        sta     X16_P2                  ; h
        jsr     popa
        sta     X16_P1                  ; m
        jsr     popa
        sta     X16_P0                  ; l
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

; void __fastcall__ x16_fxu_cache_write0(unsigned char mask)
;   ...and write1: the mask is already in A.
_x16_fxu_cache_write0:
        jmp     fxu_cache_write0

_x16_fxu_cache_write1:
        jmp     fxu_cache_write1

; void __fastcall__ x16_fxu_set_incr(unsigned int xi, unsigned int yi)
_x16_fxu_set_incr:
        sta     X16_P2                  ; yi lo (rightmost arg: A/X)
        stx     X16_P3
        jsr     popax                   ; xi
        sta     X16_P0
        stx     X16_P1
        jmp     fxu_set_incr

; void __fastcall__ x16_fxu_set_pos(unsigned int x, unsigned int y)
_x16_fxu_set_pos:
        sta     X16_P2                  ; y lo (rightmost arg: A/X)
        stx     X16_P3
        jsr     popax                   ; x
        sta     X16_P0
        stx     X16_P1
        jmp     fxu_set_pos

; void __fastcall__ x16_fxu_set_subpos(unsigned char xs, unsigned char ys)
;   The body wants A = X subpixel, X = Y subpixel. popa preserves X.
_x16_fxu_set_subpos:
        tax                             ; ys (rightmost arg, in A)
        jsr     popa                    ; xs
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
