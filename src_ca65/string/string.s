; =====================================================================
; x16clib :: string/string.s -- 0-terminated string fundamentals
; =====================================================================
; Measure, copy, append, compare, hash (ported from x16_library's
; string/string.asm). Strings are NUL-terminated, at most 255 characters
; plus the NUL, and there are no bounds checks -- make the target buffers
; big enough. The routines are pure memory ops, so they work on PETSCII
; and ISO bytes alike; only the case/ctype modules care which encoding
; the bytes are in.
;
; cc65's own <string.h> exists, but these match the assembly library's
; semantics exactly (length in a byte, compare answering -1/0/1, the
; capped copies NUL-terminating at the cap) and cost no C runtime.
;
;   x16_str_length / x16_str_hash        (const char *s)
;   x16_str_copy / x16_str_ncopy         (char *target, const char *source[, max])
;   x16_str_append / x16_str_nappend     (char *target, const char *suffix[, max])
;   x16_str_compare                      (const char *s1, const char *s2)
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popax

        .export         _x16_str_length
        .export         _x16_str_copy
        .export         _x16_str_ncopy
        .export         _x16_str_append
        .export         _x16_str_nappend
        .export         _x16_str_compare
        .export         _x16_str_hash
        .export         str_compare             ; raw entry, used by string/strsort.s

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_str_length(const char *s)
;   The pointer already arrives as A = low, X = high: no popping.
; ---------------------------------------------------------------------
_x16_str_length:
        jsr     str_length
        tya                             ; Y = the length
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_str_copy(char *target, const char *source)
;   Returns the length copied.
;
; The asm wants the source in A/X and the target in X16_P0/P1. popax
; clobbers A/X, so the source (the rightmost arg, already in A/X) parks
; in T6/T7 -- str_copy itself only touches T0/T1.
; ---------------------------------------------------------------------
_x16_str_copy:
        sta     X16_T6                  ; source (rightmost arg, in A/X)
        stx     X16_T7
        jsr     popax
        sta     X16_P0                  ; target
        stx     X16_P1
        lda     X16_T6
        ldx     X16_T7
        jsr     str_copy
        tya                             ; Y = length copied
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_str_ncopy(char *target, const char *source,
;                                          unsigned char maxlength)
;   Copies at most maxlength bytes, then NUL-terminates. Returns the
;   length of the target string.
;
; popa/popax clobber Y, so maxlength rides the hardware stack across the
; pointer pops and lands in Y at the last moment (the dos.s idiom).
; ---------------------------------------------------------------------
_x16_str_ncopy:
        pha                             ; maxlength (rightmost arg, in A)
        jsr     popax
        sta     X16_T6                  ; source
        stx     X16_T7
        jsr     popax
        sta     X16_P0                  ; target
        stx     X16_P1
        lda     X16_T6
        ldx     X16_T7
        ply                             ; Y = maxlength
        jsr     str_ncopy
        tya                             ; Y = target length
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_str_append(char *target, const char *suffix)
;   Returns the length of the resulting string.
; ---------------------------------------------------------------------
_x16_str_append:
        sta     X16_P0                  ; suffix (rightmost arg, in A/X)
        stx     X16_P1
        jsr     popax                   ; target -> A/X
        jsr     str_append              ; A = resulting length
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_str_nappend(char *target, const char *suffix,
;                                            unsigned char maxlength)
;   Appends, but never lets the target exceed maxlength characters.
;   Returns the length of the resulting string (unchanged if there was
;   no room at all).
; ---------------------------------------------------------------------
_x16_str_nappend:
        pha                             ; maxlength (rightmost arg, in A)
        jsr     popax
        sta     X16_P0                  ; suffix
        stx     X16_P1
        jsr     popax                   ; target -> A/X
        ply                             ; Y = maxlength
        jsr     str_nappend             ; A = resulting length
        ldx     #0
        rts

; ---------------------------------------------------------------------
; signed char __fastcall__ x16_str_compare(const char *s1, const char *s2)
;   -1 if s1 < s2, 0 if equal, 1 if greater -- strict byte order, the
;   asm's $FF sign-extended so C sees a true -1.
; ---------------------------------------------------------------------
_x16_str_compare:
        sta     X16_P0                  ; s2 (rightmost arg, in A/X)
        stx     X16_P1
        jsr     popax                   ; s1 -> A/X
        jsr     str_compare             ; A = $FF / 0 / 1
        ldx     #0
        cmp     #$80                    ; sign-extend for the int promotion
        bcc     :+
        ldx     #$FF
:       rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_str_hash(const char *s)
; ---------------------------------------------------------------------
_x16_str_hash:
        jsr     str_hash
        ldx     #0
        rts

; =====================================================================
; The library routines (x16_library string/string.asm, verbatim except
; for the colons ca65 wants on labels)
; =====================================================================

; ---------------------------------------------------------------------
; str_length -- in: A = low, X = high.  out: Y = length (A clobbered)
; Counts up to the first NUL. A string of 256+ bytes reports 0.
; ---------------------------------------------------------------------
str_length:
    sta X16_T0
    stx X16_T1
    ldy #0
@loop:
    lda (X16_T0),y
    beq @done
    iny
    bne @loop
@done:
    rts

; ---------------------------------------------------------------------
; str_copy -- copy a string, overwriting the target.
;   in:  A = source low, X = source high, X16_P0/P1 = target
;   out: Y = length copied
; ---------------------------------------------------------------------
str_copy:
    sta X16_T0
    stx X16_T1
    ldy #0
@loop:
    lda (X16_T0),y
    sta (X16_P0),y              ; copies the NUL too, then stops
    beq @done
    iny
    bne @loop
@done:
    rts

; ---------------------------------------------------------------------
; str_ncopy -- copy at most maxlength bytes, then NUL-terminate.
;   in:  A = source low, X = source high, X16_P0/P1 = target,
;        Y = maxlength
;   out: Y = length of the target string
; ---------------------------------------------------------------------
str_ncopy:
    sta X16_T0
    stx X16_T1
    sty X16_T2                  ; maxlength
    ldy #0
@loop:
    cpy X16_T2
    beq @cap                    ; hit the cap
    lda (X16_T0),y
    sta (X16_P0),y
    beq @done                   ; copied the NUL
    iny
    bne @loop
@cap:
    lda #0
    sta (X16_P0),y              ; terminate at the cap
@done:
    rts

; ---------------------------------------------------------------------
; str_append -- append a suffix to a target string.
;   in:  A = target low, X = target high, X16_P0/P1 = suffix
;   out: A = length of the resulting string
; ---------------------------------------------------------------------
str_append:
    jsr str_length              ; T0/T1 = target, Y = its length
    sty X16_T2
    tya                         ; T0/T1 += length -> the append point
    clc
    adc X16_T0
    sta X16_T0
    bcc @nc
    inc X16_T1
@nc:
    ldy #0
@loop:
    lda (X16_P0),y              ; copy the suffix in
    sta (X16_T0),y
    beq @done
    iny
    bne @loop
@done:
    tya                         ; result length = target + suffix
    clc
    adc X16_T2
    rts

; ---------------------------------------------------------------------
; str_nappend -- append, but never let the target exceed maxlength.
;   in:  A = target low, X = target high, X16_P0/P1 = suffix,
;        Y = maxlength
;   out: A = length of the resulting string (unchanged if it would
;        overflow the cap)
; ---------------------------------------------------------------------
str_nappend:
    sty X16_T3                  ; maxlength
    jsr str_length              ; T0/T1 = target, Y = its length
    sty X16_T2                  ; current length
    cpy X16_T3
    bcs @toosmall               ; length >= max: no room, leave it be
    lda X16_T3                  ; room = max - length
    sec
    sbc X16_T2
    sta X16_T3
    lda X16_T2                  ; T0/T1 += length -> the append point
    clc
    adc X16_T0
    sta X16_T0
    bcc @nc
    inc X16_T1
@nc:
    ldy #0
@loop:
    cpy X16_T3                  ; stop at the room limit
    beq @cap
    lda (X16_P0),y
    sta (X16_T0),y
    beq @done
    iny
    bne @loop
@cap:
    lda #0
    sta (X16_T0),y              ; terminate at the cap
@done:
    tya                         ; result length = old length + appended
    clc
    adc X16_T2
    rts
@toosmall:
    lda X16_T2                  ; unchanged length
    rts

; ---------------------------------------------------------------------
; str_compare -- compare two strings, case-sensitively, for sorting.
;   in:  A = string1 low, X = string1 high, X16_P0/P1 = string2
;   out: A = $FF (-1) if string1 < string2, 0 if equal, 1 if greater
; ---------------------------------------------------------------------
str_compare:
    sta X16_T0
    stx X16_T1
    ldy #0
@loop:
    lda (X16_T0),y              ; string1 char
    beq @s1end
    cmp (X16_P0),y              ; vs string2 char
    bne @diff
    iny
    bne @loop
    lda #0                      ; ran the whole page: equal
    rts
@s1end:
    lda (X16_P0),y              ; string1 ended; string2 too?
    beq @equal
    lda #$FF                    ; string1 is the shorter -> before
    rts
@diff:
    bcs @greater                ; carry from cmp: set if s1 >= s2
    lda #$FF
    rts
@greater:
    lda #1
    rts
@equal:
    lda #0
    rts

; ---------------------------------------------------------------------
; str_hash -- an 8-bit rolling hash of the string.
;   in: A = low, X = high.  out: A = hash
;   hash(-1) = 179; hash(i) = rol(hash(i-1)) XOR string[i]
; ---------------------------------------------------------------------
str_hash:
    sta X16_T0
    stx X16_T1
    lda #179
    sta X16_T2
    ldy #0
    clc
@loop:
    lda (X16_T0),y
    beq @done
    rol X16_T2
    eor X16_T2
    sta X16_T2
    iny
    bne @loop
@done:
    lda X16_T2
    rts
