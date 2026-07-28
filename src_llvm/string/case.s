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

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine (see gfx/bitmap4l.s):
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X. Returns: char in A.

        .globl  x16_str_lowerchar
        .globl  x16_str_lowerchar_iso
        .globl  x16_str_upperchar
        .globl  x16_str_upperchar_iso
        .globl  x16_str_lower
        .globl  x16_str_lower_iso
        .globl  x16_str_upper
        .globl  x16_str_upper_iso
        .globl  x16_str_compare_nocase
        .globl  x16_str_compare_nocase_iso

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_str_lowerchar(unsigned char c)
; ...and upperchar, and the _iso pair. The character arrives in A and
; the folded character returns in A: the asm entry IS the C entry.
; ---------------------------------------------------------------------
x16_str_lowerchar:
        jmp     str_lowerchar

x16_str_lowerchar_iso:
        jmp     str_lowerchar_iso

x16_str_upperchar:
        jmp     str_upperchar

x16_str_upperchar_iso:
        jmp     str_upperchar_iso

; ---------------------------------------------------------------------
; unsigned char x16_str_lower(char *s)
; ...and upper, and the _iso pair. In place; returns the length.
; s -> __rc2/__rc3; the asm wants it in A/X and answers in Y.
; ---------------------------------------------------------------------
x16_str_lower:
        lda     __rc2
        ldx     __rc3
        jsr     str_lower
        tya                             ; A = the length
        rts

x16_str_lower_iso:
        lda     __rc2
        ldx     __rc3
        jsr     str_lower_iso
        tya
        rts

x16_str_upper:
        lda     __rc2
        ldx     __rc3
        jsr     str_upper
        tya
        rts

x16_str_upper_iso:
        lda     __rc2
        ldx     __rc3
        jsr     str_upper_iso
        tya
        rts

; ---------------------------------------------------------------------
; signed char x16_str_compare_nocase(const char *s1, const char *s2)
; ...and the _iso form. -1 if s1 < s2 after folding, 0 if equal, 1 if
; greater. The asm's $FF IS -1 in the byte llvm-mos returns a signed
; char in: no extension needed.
; s1 -> __rc2/__rc3, s2 -> __rc4/__rc5.
; ---------------------------------------------------------------------
x16_str_compare_nocase:
        lda     __rc4
        sta     X16_P0                  ; s2
        lda     __rc5
        sta     X16_P1
        lda     __rc2                   ; A/X = s1
        ldx     __rc3
        jmp     str_compare_nocase      ; A = $FF / 0 / 1

x16_str_compare_nocase_iso:
        lda     __rc4
        sta     X16_P0                  ; s2
        lda     __rc5
        sta     X16_P1
        lda     __rc2                   ; A/X = s1
        ldx     __rc3
        jmp     str_compare_nocase_iso

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
    bcc .Lstr_lowerchar_done
    cmp #123
    bcs .Lstr_lowerchar_done
    and #%11011111
.Lstr_lowerchar_done:
    rts

str_lowerchar_iso:
    cmp #65
    bcc .Lstr_lowerchar_iso_done
    cmp #91
    bcs .Lstr_lowerchar_iso_done
    ora #$20
.Lstr_lowerchar_iso_done:
    rts

str_upperchar:
    cmp #65
    bcc .Lstr_upperchar_done
    cmp #91
    bcs .Lstr_upperchar_done
    ora #%00100000
.Lstr_upperchar_done:
    rts

str_upperchar_iso:
    cmp #97
    bcc .Lstr_upperchar_iso_done
    cmp #123
    bcs .Lstr_upperchar_iso_done
    and #%11011111
.Lstr_upperchar_iso_done:
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
.Lstr_lower_loop:
    lda (X16_T0),y
    beq .Lstr_lower_done
    jsr str_lowerchar
    sta (X16_T0),y
    iny
    bne .Lstr_lower_loop
.Lstr_lower_done:
    rts

str_lower_iso:
    sta X16_T0
    stx X16_T1
    ldy #0
.Lstr_lower_iso_loop:
    lda (X16_T0),y
    beq .Lstr_lower_iso_done
    jsr str_lowerchar_iso
    sta (X16_T0),y
    iny
    bne .Lstr_lower_iso_loop
.Lstr_lower_iso_done:
    rts

str_upper:
    sta X16_T0
    stx X16_T1
    ldy #0
.Lstr_upper_loop:
    lda (X16_T0),y
    beq .Lstr_upper_done
    jsr str_upperchar
    sta (X16_T0),y
    iny
    bne .Lstr_upper_loop
.Lstr_upper_done:
    rts

str_upper_iso:
    sta X16_T0
    stx X16_T1
    ldy #0
.Lstr_upper_iso_loop:
    lda (X16_T0),y
    beq .Lstr_upper_iso_done
    jsr str_upperchar_iso
    sta (X16_T0),y
    iny
    bne .Lstr_upper_iso_loop
.Lstr_upper_iso_done:
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
.Lstr_compare_nocase_loop:
    lda (X16_T0),y
    beq .Lstr_compare_nocase_s1end
    jsr str_lowerchar
    sta X16_T2
    lda (X16_P0),y
    jsr str_lowerchar
    cmp X16_T2
    bne .Lstr_compare_nocase_diff
    iny
    bne .Lstr_compare_nocase_loop
    lda #0
    rts
.Lstr_compare_nocase_diff:
    bcc .Lstr_compare_nocase_greater                ; folded s2 < folded s1 -> string1 sorts after
    lda #$FF
    rts
.Lstr_compare_nocase_greater:
    lda #1
    rts
.Lstr_compare_nocase_s1end:
    lda (X16_P0),y
    beq .Lstr_compare_nocase_same
    lda #$FF
    rts
.Lstr_compare_nocase_same:
    lda #0
    rts

str_compare_nocase_iso:
    sta X16_T0
    stx X16_T1
    ldy #0
.Lstr_compare_nocase_iso_loop:
    lda (X16_T0),y
    beq .Lstr_compare_nocase_iso_s1end
    jsr str_lowerchar_iso
    sta X16_T2
    lda (X16_P0),y
    jsr str_lowerchar_iso
    cmp X16_T2
    bne .Lstr_compare_nocase_iso_diff
    iny
    bne .Lstr_compare_nocase_iso_loop
    lda #0
    rts
.Lstr_compare_nocase_iso_diff:
    bcc .Lstr_compare_nocase_iso_greater
    lda #$FF
    rts
.Lstr_compare_nocase_iso_greater:
    lda #1
    rts
.Lstr_compare_nocase_iso_s1end:
    lda (X16_P0),y
    beq .Lstr_compare_nocase_iso_same
    lda #$FF
    rts
.Lstr_compare_nocase_iso_same:
    lda #0
    rts
