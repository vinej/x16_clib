; =====================================================================
; x16clib :: string/case.s -- upper/lower case conversion
; =====================================================================
; Whole-string (in place) and single-character case folding, in both
; encodings (ported from x16_library's string/case.asm). PETSCII and ISO
; place the letters at different codes, so the two encodings genuinely
; swap: PETSCII "lower" is numerically ISO "upper" and vice versa --
; that is not a bug, it is the charset. The whole-string routines return
; the string length; the compare routines fold both sides before
; comparing and answer -1/0/1 like x16_str_compare.
;
;   x16_str_lowerchar / _upperchar [_iso]   (unsigned char c) -> folded c
;   x16_str_lower / _upper [_iso]           (char *s) -> length, in place
;   x16_str_compare_nocase [_iso]           (const char *s1, const char *s2)
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers: a lone character rides the accumulator
; (__reg("a")); pointers ride r0/r1 and r2/r3. Returns: char in a.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3

        global	_x16_str_lowerchar
        global	_x16_str_lowerchar_iso
        global	_x16_str_upperchar
        global	_x16_str_upperchar_iso
        global	_x16_str_lower
        global	_x16_str_lower_iso
        global	_x16_str_upper
        global	_x16_str_upper_iso
        global	_x16_str_compare_nocase
        global	_x16_str_compare_nocase_iso

        section text

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_str_lowerchar(__reg("a") unsigned char c)
; ...and upperchar, and the _iso pair. The character arrives in A:
; no shim beyond the return convention.
; ---------------------------------------------------------------------
_x16_str_lowerchar:
        jsr     str_lowerchar
        ldx     #0                      ; high byte, for int-promoting callers
        rts

_x16_str_lowerchar_iso:
        jsr     str_lowerchar_iso
        ldx     #0
        rts

_x16_str_upperchar:
        jsr     str_upperchar
        ldx     #0
        rts

_x16_str_upperchar_iso:
        jsr     str_upperchar_iso
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char x16_str_lower(__reg("r0/r1") char *s)
; ...and upper, and the _iso pair. In place; returns the length.
; The asm wants the pointer in A/X and answers in Y.
; ---------------------------------------------------------------------
_x16_str_lower:
        lda     r0
        ldx     r1
        jsr     str_lower
        tya                             ; A = the length
        ldx     #0
        rts

_x16_str_lower_iso:
        lda     r0
        ldx     r1
        jsr     str_lower_iso
        tya
        ldx     #0
        rts

_x16_str_upper:
        lda     r0
        ldx     r1
        jsr     str_upper
        tya
        ldx     #0
        rts

_x16_str_upper_iso:
        lda     r0
        ldx     r1
        jsr     str_upper_iso
        tya
        ldx     #0
        rts

; ---------------------------------------------------------------------
; signed char x16_str_compare_nocase(__reg("r0/r1") const char *s1,
;                                    __reg("r2/r3") const char *s2)
; ...and the _iso form. -1 if s1 < s2 after folding, 0 if equal, 1 if
; greater -- the asm's $FF sign-extended so an int-promoting caller
; sees a true -1.
; ---------------------------------------------------------------------
_x16_str_compare_nocase:
        lda     r2
        sta     X16_P0                  ; s2
        lda     r3
        sta     X16_P1
        lda     r0                      ; A/X = s1
        ldx     r1
        jsr     str_compare_nocase      ; A = $FF / 0 / 1
        ldx     #0
        cmp     #$80                    ; sign-extend for the int promotion
        bcc     .done
        ldx     #$FF
.done:
        rts

_x16_str_compare_nocase_iso:
        lda     r2
        sta     X16_P0                  ; s2
        lda     r3
        sta     X16_P1
        lda     r0                      ; A/X = s1
        ldx     r1
        jsr     str_compare_nocase_iso
        ldx     #0
        cmp     #$80
        bcc     .done2
        ldx     #$FF
.done2:
        rts

; =====================================================================
; The library routines (x16_library string/case.asm, verbatim except
; for the colons ca65 wants on labels)
; =====================================================================

; ---------------------------------------------------------------------
; str_lowerchar / str_lowerchar_iso -- fold one character to lower case
; str_upperchar / str_upperchar_iso -- ...to upper case.  in/out: A
; ---------------------------------------------------------------------
str_lowerchar:
    and #$7f
    cmp #97
    bcc .done
    cmp #123
    bcs .done
    and #%11011111
.done:
    rts

str_lowerchar_iso:
    cmp #65
    bcc .done
    cmp #91
    bcs .done
    ora #$20
.done:
    rts

str_upperchar:
    cmp #65
    bcc .done
    cmp #91
    bcs .done
    ora #%00100000
.done:
    rts

str_upperchar_iso:
    cmp #97
    bcc .done
    cmp #123
    bcs .done
    and #%11011111
.done:
    rts

; ---------------------------------------------------------------------
; str_lower / str_lower_iso -- fold a whole string to lower case in place.
; str_upper / str_upper_iso -- ...to upper case.
;   in: A = low, X = high.  out: Y = length
; ---------------------------------------------------------------------
str_lower:
    sta X16_T0
    stx X16_T1
    ldy #0
.loop:
    lda (X16_T0),y
    beq .done
    jsr str_lowerchar
    sta (X16_T0),y
    iny
    bne .loop
.done:
    rts

str_lower_iso:
    sta X16_T0
    stx X16_T1
    ldy #0
.loop:
    lda (X16_T0),y
    beq .done
    jsr str_lowerchar_iso
    sta (X16_T0),y
    iny
    bne .loop
.done:
    rts

str_upper:
    sta X16_T0
    stx X16_T1
    ldy #0
.loop:
    lda (X16_T0),y
    beq .done
    jsr str_upperchar
    sta (X16_T0),y
    iny
    bne .loop
.done:
    rts

str_upper_iso:
    sta X16_T0
    stx X16_T1
    ldy #0
.loop:
    lda (X16_T0),y
    beq .done
    jsr str_upperchar_iso
    sta (X16_T0),y
    iny
    bne .loop
.done:
    rts

; ---------------------------------------------------------------------
; str_compare_nocase / str_compare_nocase_iso -- case-insensitive compare.
;   in:  A = string1 low, X = string1 high, X16_P0/P1 = string2
;   out: A = $FF (-1) if string1 < string2, 0 if equal, 1 if greater
; ---------------------------------------------------------------------
str_compare_nocase:
    sta X16_T0
    stx X16_T1
    ldy #0
.loop:
    lda (X16_T0),y
    beq .s1end
    jsr str_lowerchar
    sta X16_T2
    lda (X16_P0),y
    jsr str_lowerchar
    cmp X16_T2
    bne .diff
    iny
    bne .loop
    lda #0
    rts
.diff:
    bcc .greater                ; folded s2 < folded s1 -> string1 sorts after
    lda #$FF
    rts
.greater:
    lda #1
    rts
.s1end:
    lda (X16_P0),y
    beq .same
    lda #$FF
    rts
.same:
    lda #0
    rts

str_compare_nocase_iso:
    sta X16_T0
    stx X16_T1
    ldy #0
.loop:
    lda (X16_T0),y
    beq .s1end
    jsr str_lowerchar_iso
    sta X16_T2
    lda (X16_P0),y
    jsr str_lowerchar_iso
    cmp X16_T2
    bne .diff
    iny
    bne .loop
    lda #0
    rts
.diff:
    bcc .greater
    lda #$FF
    rts
.greater:
    lda #1
    rts
.s1end:
    lda (X16_P0),y
    beq .same
    lda #$FF
    rts
.same:
    lda #0
    rts
