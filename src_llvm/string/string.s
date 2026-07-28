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

; llvm-mos argument placement, measured on the machine (see gfx/bitmap4l.s):
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X, then whatever __rc bytes the pointers
;   left free. Returns: char in A; int in A/X; POINTER in __rc2/__rc3.

        .globl  x16_str_length
        .globl  x16_str_copy
        .globl  x16_str_ncopy
        .globl  x16_str_append
        .globl  x16_str_nappend
        .globl  x16_str_compare
        .globl  x16_str_hash
        .globl  str_compare  ; raw entry, used by string/strsort.s

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_str_length(const char *s)
; s -> __rc2/__rc3. The asm wants it in A/X.
; ---------------------------------------------------------------------
x16_str_length:
        lda     mos8(__rc2)
        ldx     mos8(__rc3)
        jsr     str_length
        tya                             ; A = the length
        rts

; ---------------------------------------------------------------------
; unsigned char x16_str_copy(char *target, const char *source)
;   Returns the length copied.
; target -> __rc2/__rc3, source -> __rc4/__rc5. The asm wants the source
; in A/X and the target in X16_P0/P1.
; ---------------------------------------------------------------------
x16_str_copy:
        lda     mos8(__rc2)
        sta     mos8(X16_P0)            ; target
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        lda     mos8(__rc4)             ; A/X = source
        ldx     mos8(__rc5)
        jsr     str_copy
        tya                             ; A = length copied
        rts

; ---------------------------------------------------------------------
; unsigned char x16_str_ncopy(char *target, const char *source,
;                             unsigned char maxlength)
;   Copies at most maxlength bytes, then NUL-terminates. Returns the
;   length of the target string.
; target -> __rc2/__rc3, source -> __rc4/__rc5, maxlength -> A (the only
; integer byte). The asm wants A/X = source, P0/P1 = target, Y = max.
; ---------------------------------------------------------------------
x16_str_ncopy:
        tay                             ; Y = maxlength
        lda     mos8(__rc2)
        sta     mos8(X16_P0)            ; target
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        lda     mos8(__rc4)             ; A/X = source
        ldx     mos8(__rc5)
        jsr     str_ncopy
        tya                             ; A = target length
        rts

; ---------------------------------------------------------------------
; unsigned char x16_str_append(char *target, const char *suffix)
;   Returns the length of the resulting string.
; target -> __rc2/__rc3, suffix -> __rc4/__rc5. The asm wants A/X =
; target, P0/P1 = suffix.
; ---------------------------------------------------------------------
x16_str_append:
        lda     mos8(__rc4)
        sta     mos8(X16_P0)            ; suffix
        lda     mos8(__rc5)
        sta     mos8(X16_P1)
        lda     mos8(__rc2)             ; A/X = target
        ldx     mos8(__rc3)
        jmp     str_append              ; A = resulting length

; ---------------------------------------------------------------------
; unsigned char x16_str_nappend(char *target, const char *suffix,
;                               unsigned char maxlength)
;   Appends, but never lets the target exceed maxlength characters.
;   Returns the length of the resulting string (unchanged if there was
;   no room at all).
; target -> __rc2/__rc3, suffix -> __rc4/__rc5, maxlength -> A.
; ---------------------------------------------------------------------
x16_str_nappend:
        tay                             ; Y = maxlength
        lda     mos8(__rc4)
        sta     mos8(X16_P0)            ; suffix
        lda     mos8(__rc5)
        sta     mos8(X16_P1)
        lda     mos8(__rc2)             ; A/X = target
        ldx     mos8(__rc3)
        jmp     str_nappend             ; A = resulting length

; ---------------------------------------------------------------------
; signed char x16_str_compare(const char *s1, const char *s2)
;   -1 if s1 < s2, 0 if equal, 1 if greater. The asm's $FF IS -1 in the
;   byte llvm-mos returns a signed char in: no extension needed.
; s1 -> __rc2/__rc3, s2 -> __rc4/__rc5.
; ---------------------------------------------------------------------
x16_str_compare:
        lda     mos8(__rc4)
        sta     mos8(X16_P0)            ; s2
        lda     mos8(__rc5)
        sta     mos8(X16_P1)
        lda     mos8(__rc2)             ; A/X = s1
        ldx     mos8(__rc3)
        jmp     str_compare             ; A = $FF / 0 / 1

; ---------------------------------------------------------------------
; unsigned char x16_str_hash(const char *s)
; s -> __rc2/__rc3.
; ---------------------------------------------------------------------
x16_str_hash:
        lda     mos8(__rc2)
        ldx     mos8(__rc3)
        jmp     str_hash

; =====================================================================
; The library routines (x16_library string/string.asm, verbatim except
; for the colons ca65 wants on labels)
; =====================================================================

; ---------------------------------------------------------------------
; str_length -- in: A = low, X = high.  out: Y = length (A clobbered)
; Counts up to the first NUL. A string of 256+ bytes reports 0.
; ---------------------------------------------------------------------
str_length:
    sta mos8(X16_T0)
    stx mos8(X16_T1)
    ldy #0
.Lstr_length_loop:
    lda (X16_T0),y
    beq .Lstr_length_done
    iny
    bne .Lstr_length_loop
.Lstr_length_done:
    rts

; ---------------------------------------------------------------------
; str_copy -- copy a string, overwriting the target.
;   in:  A = source low, X = source high, X16_P0/P1 = target
;   out: Y = length copied
; ---------------------------------------------------------------------
str_copy:
    sta mos8(X16_T0)
    stx mos8(X16_T1)
    ldy #0
.Lstr_copy_loop:
    lda (X16_T0),y
    sta (X16_P0),y              ; copies the NUL too, then stops
    beq .Lstr_copy_done
    iny
    bne .Lstr_copy_loop
.Lstr_copy_done:
    rts

; ---------------------------------------------------------------------
; str_ncopy -- copy at most maxlength bytes, then NUL-terminate.
;   in:  A = source low, X = source high, X16_P0/P1 = target,
;        Y = maxlength
;   out: Y = length of the target string
; ---------------------------------------------------------------------
str_ncopy:
    sta mos8(X16_T0)
    stx mos8(X16_T1)
    sty mos8(X16_T2)            ; maxlength
    ldy #0
.Lstr_ncopy_loop:
    cpy mos8(X16_T2)
    beq .Lstr_ncopy_cap                    ; hit the cap
    lda (X16_T0),y
    sta (X16_P0),y
    beq .Lstr_ncopy_done                   ; copied the NUL
    iny
    bne .Lstr_ncopy_loop
.Lstr_ncopy_cap:
    lda #0
    sta (X16_P0),y              ; terminate at the cap
.Lstr_ncopy_done:
    rts

; ---------------------------------------------------------------------
; str_append -- append a suffix to a target string.
;   in:  A = target low, X = target high, X16_P0/P1 = suffix
;   out: A = length of the resulting string
; ---------------------------------------------------------------------
str_append:
    jsr str_length              ; T0/T1 = target, Y = its length
    sty mos8(X16_T2)
    tya                         ; T0/T1 += length -> the append point
    clc
    adc mos8(X16_T0)
    sta mos8(X16_T0)
    bcc .Lstr_append_nc
    inc mos8(X16_T1)
.Lstr_append_nc:
    ldy #0
.Lstr_append_loop:
    lda (X16_P0),y              ; copy the suffix in
    sta (X16_T0),y
    beq .Lstr_append_done
    iny
    bne .Lstr_append_loop
.Lstr_append_done:
    tya                         ; result length = target + suffix
    clc
    adc mos8(X16_T2)
    rts

; ---------------------------------------------------------------------
; str_nappend -- append, but never let the target exceed maxlength.
;   in:  A = target low, X = target high, X16_P0/P1 = suffix,
;        Y = maxlength
;   out: A = length of the resulting string (unchanged if it would
;        overflow the cap)
; ---------------------------------------------------------------------
str_nappend:
    sty mos8(X16_T3)            ; maxlength
    jsr str_length              ; T0/T1 = target, Y = its length
    sty mos8(X16_T2)            ; current length
    cpy mos8(X16_T3)
    bcs .Lstr_nappend_toosmall               ; length >= max: no room, leave it be
    lda mos8(X16_T3)            ; room = max - length
    sec
    sbc mos8(X16_T2)
    sta mos8(X16_T3)
    lda mos8(X16_T2)            ; T0/T1 += length -> the append point
    clc
    adc mos8(X16_T0)
    sta mos8(X16_T0)
    bcc .Lstr_nappend_nc
    inc mos8(X16_T1)
.Lstr_nappend_nc:
    ldy #0
.Lstr_nappend_loop:
    cpy mos8(X16_T3)            ; stop at the room limit
    beq .Lstr_nappend_cap
    lda (X16_P0),y
    sta (X16_T0),y
    beq .Lstr_nappend_done
    iny
    bne .Lstr_nappend_loop
.Lstr_nappend_cap:
    lda #0
    sta (X16_T0),y              ; terminate at the cap
.Lstr_nappend_done:
    tya                         ; result length = old length + appended
    clc
    adc mos8(X16_T2)
    rts
.Lstr_nappend_toosmall:
    lda mos8(X16_T2)            ; unchanged length
    rts

; ---------------------------------------------------------------------
; str_compare -- compare two strings, case-sensitively, for sorting.
;   in:  A = string1 low, X = string1 high, X16_P0/P1 = string2
;   out: A = $FF (-1) if string1 < string2, 0 if equal, 1 if greater
; ---------------------------------------------------------------------
str_compare:
    sta mos8(X16_T0)
    stx mos8(X16_T1)
    ldy #0
.Lstr_compare_loop:
    lda (X16_T0),y              ; string1 char
    beq .Lstr_compare_s1end
    cmp (X16_P0),y              ; vs string2 char
    bne .Lstr_compare_diff
    iny
    bne .Lstr_compare_loop
    lda #0                      ; ran the whole page: equal
    rts
.Lstr_compare_s1end:
    lda (X16_P0),y              ; string1 ended; string2 too?
    beq .Lstr_compare_equal
    lda #$FF                    ; string1 is the shorter -> before
    rts
.Lstr_compare_diff:
    bcs .Lstr_compare_greater                ; carry from cmp: set if s1 >= s2
    lda #$FF
    rts
.Lstr_compare_greater:
    lda #1
    rts
.Lstr_compare_equal:
    lda #0
    rts

; ---------------------------------------------------------------------
; str_hash -- an 8-bit rolling hash of the string.
;   in: A = low, X = high.  out: A = hash
;   hash(-1) = 179; hash(i) = rol(hash(i-1)) XOR string[i]
; ---------------------------------------------------------------------
str_hash:
    sta mos8(X16_T0)
    stx mos8(X16_T1)
    lda #179
    sta mos8(X16_T2)
    ldy #0
    clc
.Lstr_hash_loop:
    lda (X16_T0),y
    beq .Lstr_hash_done
    rol mos8(X16_T2)
    eor mos8(X16_T2)
    sta mos8(X16_T2)
    iny
    bne .Lstr_hash_loop
.Lstr_hash_done:
    lda mos8(X16_T2)
    rts
