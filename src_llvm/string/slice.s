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

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine (see gfx/bitmap4l.s):
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X. Returns: char in A.

        .globl  x16_str_left
        .globl  x16_str_right
        .globl  x16_str_slice
        .globl  x16_str_ltrim
        .globl  x16_str_rtrim
        .globl  x16_str_trim

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void x16_str_left(char *target, const char *source,
;                   unsigned char length)
;   target = the first `length` characters of source.
; target -> __rc2/__rc3, source -> __rc4/__rc5, length -> A (the only
; integer byte). The asm wants A/X = source, P0/P1 = target, Y = length.
; ---------------------------------------------------------------------
x16_str_left:
        tay                             ; Y = length
        lda     mos8(__rc2)
        sta     mos8(X16_P0)            ; target
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        lda     mos8(__rc4)             ; A/X = source
        ldx     mos8(__rc5)
        jmp     str_left

; ---------------------------------------------------------------------
; void x16_str_right(char *target, const char *source,
;                    unsigned char length)
;   target = the last `length` characters of source.
; ---------------------------------------------------------------------
x16_str_right:
        tay                             ; Y = length
        lda     mos8(__rc2)
        sta     mos8(X16_P0)            ; target
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        lda     mos8(__rc4)             ; A/X = source
        ldx     mos8(__rc5)
        jmp     str_right

; ---------------------------------------------------------------------
; void x16_str_slice(char *target, const char *source,
;                    unsigned char start, unsigned char length)
;   target = `length` characters of source starting at index `start`.
; target -> __rc2/__rc3, source -> __rc4/__rc5, start -> A, length -> X
; (the integer bytes, left to right). The asm wants A/X = source,
; P0/P1 = target, P2 = start, Y = length.
; ---------------------------------------------------------------------
x16_str_slice:
        sta     mos8(X16_P2)            ; start
        txa
        tay                             ; Y = length
        lda     mos8(__rc2)
        sta     mos8(X16_P0)            ; target
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        lda     mos8(__rc4)             ; A/X = source
        ldx     mos8(__rc5)
        jmp     str_slice

; ---------------------------------------------------------------------
; unsigned char x16_str_ltrim(char *s)
; ...and rtrim, and trim. In place; return the new length.
; s -> __rc2/__rc3; the asm wants it in A/X and answers in Y.
; ---------------------------------------------------------------------
x16_str_ltrim:
        lda     mos8(__rc2)
        ldx     mos8(__rc3)
        jsr     str_ltrim
        tya                             ; A = the new length
        rts

x16_str_rtrim:
        lda     mos8(__rc2)
        ldx     mos8(__rc3)
        jsr     str_rtrim
        tya
        rts

x16_str_trim:
        lda     mos8(__rc2)
        ldx     mos8(__rc3)
        jsr     str_trim
        tya
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
    sta mos8(X16_T0)
    stx mos8(X16_T1)
    lda #0
    sta (X16_P0),y              ; terminate the target at [length]
    cpy #0
    beq .Lstr_left_done
.Lstr_left_loop:
    dey
    lda (X16_T0),y
    sta (X16_P0),y
    cpy #0
    bne .Lstr_left_loop
.Lstr_left_done:
    rts

; ---------------------------------------------------------------------
; str_right -- copy the last `length` characters.
;   in: A = source low, X = source high, X16_P0/P1 = target, Y = length
; ---------------------------------------------------------------------
str_right:
    sty mos8(X16_T2)            ; length
    sta mos8(X16_T0)
    stx mos8(X16_T1)
    ldy #0                      ; measure the source
.Lstr_right_len:
    lda (X16_T0),y
    beq .Lstr_right_gotlen
    iny
    bne .Lstr_right_len
.Lstr_right_gotlen:
    tya                         ; source += (total - length)
    sec
    sbc mos8(X16_T2)
    clc
    adc mos8(X16_T0)
    sta mos8(X16_T0)
    bcc .Lstr_right_nc
    inc mos8(X16_T1)
.Lstr_right_nc:
    ldy mos8(X16_T2)            ; then it is just a left-copy of `length`
    lda #0
    sta (X16_P0),y
    cpy #0
    beq .Lstr_right_done
.Lstr_right_loop:
    dey
    lda (X16_T0),y
    sta (X16_P0),y
    cpy #0
    bne .Lstr_right_loop
.Lstr_right_done:
    rts

; ---------------------------------------------------------------------
; str_slice -- copy `length` characters starting at `start`.
;   in: A = source low, X = source high, X16_P0/P1 = target,
;       X16_P2 = start, Y = length
; ---------------------------------------------------------------------
str_slice:
    sta mos8(X16_T0)
    stx mos8(X16_T1)
    lda mos8(X16_T0)            ; source += start
    clc
    adc mos8(X16_P2)
    sta mos8(X16_T0)
    bcc .Lstr_slice_nc
    inc mos8(X16_T1)
.Lstr_slice_nc:
    lda #0
    sta (X16_P0),y              ; terminate the target at [length]
    cpy #0
    beq .Lstr_slice_done
.Lstr_slice_loop:
    dey
    lda (X16_T0),y
    sta (X16_P0),y
    cpy #0
    bne .Lstr_slice_loop
.Lstr_slice_done:
    rts

; ---------------------------------------------------------------------
; str_rtrim -- drop trailing whitespace, in place.
;   in: A = low, X = high.  out: Y = the new length
; Whitespace is space, TAB, CR, LF, shift-CR (141) and shift-space (160),
; the same set as str_isspace.
; ---------------------------------------------------------------------
str_rtrim:
    sta mos8(X16_T0)
    stx mos8(X16_T1)
    ldy #0
.Lstr_rtrim_len:
    lda (X16_T0),y
    beq .Lstr_rtrim_back
    iny
    bne .Lstr_rtrim_len
.Lstr_rtrim_back:
    cpy #0
    beq .Lstr_rtrim_cut                    ; empty, or every char was whitespace
    dey
    lda (X16_T0),y
    jsr slice_slice_isws
    bcs .Lstr_rtrim_back                   ; whitespace: keep stepping back
    iny                         ; keep the last non-whitespace character
.Lstr_rtrim_cut:
    lda #0
    sta (X16_T0),y
    rts

; ---------------------------------------------------------------------
; str_ltrim -- drop leading whitespace, shifting the rest down, in place.
;   in: A = low, X = high.  out: Y = the new length
; ---------------------------------------------------------------------
str_ltrim:
    sta mos8(X16_T0)
    stx mos8(X16_T1)
    ldy #0
.Lstr_ltrim_skip:
    lda (X16_T0),y
    beq .Lstr_ltrim_blank                  ; ran off the end: all whitespace
    jsr slice_slice_isws
    bcc .Lstr_ltrim_found
    iny
    bne .Lstr_ltrim_skip
.Lstr_ltrim_found:
    cpy #0
    beq .Lstr_ltrim_nolead                 ; nothing to strip
    tya                         ; T2/T3 = source = string + first-kept index
    clc
    adc mos8(X16_T0)
    sta mos8(X16_T2)
    lda mos8(X16_T1)
    adc #0
    sta mos8(X16_T3)
    ldy #0
.Lstr_ltrim_shift:
    lda (X16_T2),y
    sta (X16_T0),y
    beq .Lstr_ltrim_done
    iny
    bne .Lstr_ltrim_shift
.Lstr_ltrim_done:
    rts
.Lstr_ltrim_nolead:
    ldy #0                      ; unchanged; count its length for the caller
.Lstr_ltrim_nll:
    lda (X16_T0),y
    beq .Lstr_ltrim_nldone
    iny
    bne .Lstr_ltrim_nll
.Lstr_ltrim_nldone:
    rts
.Lstr_ltrim_blank:
    lda #0                      ; all whitespace -> empty string
    sta (X16_T0)
    ldy #0
    rts

; ---------------------------------------------------------------------
; str_trim -- drop whitespace from both ends, in place.
;   in: A = low, X = high.  out: Y = the new length
; ---------------------------------------------------------------------
str_trim:
    sta mos8(X16_T6)
    stx mos8(X16_T7)
    jsr str_rtrim
    lda mos8(X16_T6)
    ldx mos8(X16_T7)
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
