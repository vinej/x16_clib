; =====================================================================
; x16clib :: string/slice.s -- copying pieces of a string
; =====================================================================
; Copy the left end, the right end, or an interior run of a source
; string into a target buffer, NUL-terminated; and trim whitespace off
; either end in place (ported from x16_library's string/slice.asm).
; You must make the target buffer big enough and keep the lengths
; within the source -- there are no bounds checks. Whitespace is space,
; TAB, CR, LF, shift-CR (141) and shift-space (160), the same set as
; x16_str_isspace.
;
;   x16_str_left / _right   (char *target, const char *source, unsigned char length)
;   x16_str_slice           (char *target, const char *source,
;                            unsigned char start, unsigned char length)
;   x16_str_ltrim / _rtrim / _trim   (char *s) -> new length, in place
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers: target r0/r1, source r2/r3, then the byte
; arguments on the next even registers (r4, r6). Returns: char in a.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3
        zpage	r4
        zpage	r6

        global	_x16_str_left
        global	_x16_str_right
        global	_x16_str_slice
        global	_x16_str_ltrim
        global	_x16_str_rtrim
        global	_x16_str_trim

        section text

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void x16_str_left(__reg("r0/r1") char *target,
;                   __reg("r2/r3") const char *source,
;                   __reg("r4") unsigned char length)
;   target = the first `length` characters of source.
;   The asm wants A/X = source, P0/P1 = target, Y = length.
; ---------------------------------------------------------------------
_x16_str_left:
        lda     r0
        sta     X16_P0                  ; target
        lda     r1
        sta     X16_P1
        ldy     r4                      ; Y = length
        lda     r2                      ; A/X = source
        ldx     r3
        jmp     str_left

; ---------------------------------------------------------------------
; void x16_str_right(__reg("r0/r1") char *target,
;                    __reg("r2/r3") const char *source,
;                    __reg("r4") unsigned char length)
;   target = the last `length` characters of source.
; ---------------------------------------------------------------------
_x16_str_right:
        lda     r0
        sta     X16_P0                  ; target
        lda     r1
        sta     X16_P1
        ldy     r4
        lda     r2
        ldx     r3
        jmp     str_right

; ---------------------------------------------------------------------
; void x16_str_slice(__reg("r0/r1") char *target,
;                    __reg("r2/r3") const char *source,
;                    __reg("r4") unsigned char start,
;                    __reg("r6") unsigned char length)
;   target = `length` characters of source starting at index `start`.
; ---------------------------------------------------------------------
_x16_str_slice:
        lda     r0
        sta     X16_P0                  ; target
        lda     r1
        sta     X16_P1
        lda     r4
        sta     X16_P2                  ; start
        ldy     r6                      ; Y = length
        lda     r2                      ; A/X = source
        ldx     r3
        jmp     str_slice

; ---------------------------------------------------------------------
; unsigned char x16_str_ltrim(__reg("r0/r1") char *s)
; ...and rtrim, and trim. In place; return the new length.
; The asm wants the pointer in A/X and answers in Y.
; ---------------------------------------------------------------------
_x16_str_ltrim:
        lda     r0
        ldx     r1
        jsr     str_ltrim
        tya                             ; A = the new length
        ldx     #0                      ; high byte, for int-promoting callers
        rts

_x16_str_rtrim:
        lda     r0
        ldx     r1
        jsr     str_rtrim
        tya
        ldx     #0
        rts

_x16_str_trim:
        lda     r0
        ldx     r1
        jsr     str_trim
        tya
        ldx     #0
        rts

; =====================================================================
; The library routines (x16_library string/slice.asm, verbatim except
; for the colons ca65 wants on labels)
; =====================================================================

; ---------------------------------------------------------------------
; str_left -- copy the first `length` characters.
;   in: A = source low, X = source high, X16_P0/P1 = target, Y = length
; ---------------------------------------------------------------------
str_left:
    sta X16_T0
    stx X16_T1
    lda #0
    sta (X16_P0),y              ; terminate the target at [length]
    cpy #0
    beq .done
.loop:
    dey
    lda (X16_T0),y
    sta (X16_P0),y
    cpy #0
    bne .loop
.done:
    rts

; ---------------------------------------------------------------------
; str_right -- copy the last `length` characters.
;   in: A = source low, X = source high, X16_P0/P1 = target, Y = length
; ---------------------------------------------------------------------
str_right:
    sty X16_T2                  ; length
    sta X16_T0
    stx X16_T1
    ldy #0                      ; measure the source
.len:
    lda (X16_T0),y
    beq .gotlen
    iny
    bne .len
.gotlen:
    tya                         ; source += (total - length)
    sec
    sbc X16_T2
    clc
    adc X16_T0
    sta X16_T0
    bcc .nc
    inc X16_T1
.nc:
    ldy X16_T2                  ; then it is just a left-copy of `length`
    lda #0
    sta (X16_P0),y
    cpy #0
    beq .done
.loop:
    dey
    lda (X16_T0),y
    sta (X16_P0),y
    cpy #0
    bne .loop
.done:
    rts

; ---------------------------------------------------------------------
; str_slice -- copy `length` characters starting at `start`.
;   in: A = source low, X = source high, X16_P0/P1 = target,
;       X16_P2 = start, Y = length
; ---------------------------------------------------------------------
str_slice:
    sta X16_T0
    stx X16_T1
    lda X16_T0                  ; source += start
    clc
    adc X16_P2
    sta X16_T0
    bcc .nc
    inc X16_T1
.nc:
    lda #0
    sta (X16_P0),y              ; terminate the target at [length]
    cpy #0
    beq .done
.loop:
    dey
    lda (X16_T0),y
    sta (X16_P0),y
    cpy #0
    bne .loop
.done:
    rts

; ---------------------------------------------------------------------
; str_rtrim -- drop trailing whitespace, in place.
;   in: A = low, X = high.  out: Y = the new length
; Whitespace is space, TAB, CR, LF, shift-CR (141) and shift-space (160),
; the same set as str_isspace.
; ---------------------------------------------------------------------
str_rtrim:
    sta X16_T0
    stx X16_T1
    ldy #0
.len:
    lda (X16_T0),y
    beq .back
    iny
    bne .len
.back:
    cpy #0
    beq .cut                    ; empty, or every char was whitespace
    dey
    lda (X16_T0),y
    jsr slice_slice_isws
    bcs .back                   ; whitespace: keep stepping back
    iny                         ; keep the last non-whitespace character
.cut:
    lda #0
    sta (X16_T0),y
    rts

; ---------------------------------------------------------------------
; str_ltrim -- drop leading whitespace, shifting the rest down, in place.
;   in: A = low, X = high.  out: Y = the new length
; ---------------------------------------------------------------------
str_ltrim:
    sta X16_T0
    stx X16_T1
    ldy #0
.skip:
    lda (X16_T0),y
    beq .blank                  ; ran off the end: all whitespace
    jsr slice_slice_isws
    bcc .found
    iny
    bne .skip
.found:
    cpy #0
    beq .nolead                 ; nothing to strip
    tya                         ; T2/T3 = source = string + first-kept index
    clc
    adc X16_T0
    sta X16_T2
    lda X16_T1
    adc #0
    sta X16_T3
    ldy #0
.shift:
    lda (X16_T2),y
    sta (X16_T0),y
    beq .done
    iny
    bne .shift
.done:
    rts
.nolead:
    ldy #0                      ; unchanged; count its length for the caller
.nll:
    lda (X16_T0),y
    beq .nldone
    iny
    bne .nll
.nldone:
    rts
.blank:
    lda #0                      ; all whitespace -> empty string
    sta (X16_T0)
    ldy #0
    rts

; ---------------------------------------------------------------------
; str_trim -- drop whitespace from both ends, in place.
;   in: A = low, X = high.  out: Y = the new length
; ---------------------------------------------------------------------
str_trim:
    sta X16_T6
    stx X16_T7
    jsr str_rtrim
    lda X16_T6
    ldx X16_T7
    jmp str_ltrim

; whitespace test: A = char -> carry set if whitespace. Preserves A, X, Y.
slice_slice_isws:
    cmp #32
    beq slice_isws_yes
    cmp #13
    beq slice_isws_yes
    cmp #10
    beq slice_isws_yes
    cmp #9
    beq slice_isws_yes
    cmp #141
    beq slice_isws_yes
    cmp #160
    beq slice_isws_yes
    clc
    rts
slice_isws_yes:
    sec
    rts
