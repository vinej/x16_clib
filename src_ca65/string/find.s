; =====================================================================
; x16clib :: string/find.s -- searching within a string
; =====================================================================
; Locate a character (forward or backward), find the first line ending,
; test membership, or match a wildcard pattern (ported from x16_library's
; string/find.asm). The find routines answer with the index when they
; hit and 255 when they miss; contains and pattern_match answer 0/1.
;
;   x16_str_find / _rfind    (const char *s, unsigned char c) -> index | 255
;   x16_str_contains         (const char *s, unsigned char c) -> 0/1
;   x16_str_find_eol         (const char *s)                  -> index | 255
;   x16_str_pattern_match    (const char *s, const char *pattern) -> 0/1
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popax

; ---------------------------------------------------------------------
; ca65 -t cx16 translates character literals to PETSCII. '*' and '?'
; happen to survive (codes $20-$3F map to themselves), but per this
; tree's convention every compared byte is written as its explicit
; value anyway.
; ---------------------------------------------------------------------
CH_STAR  = $2A                          ; '*'
CH_QMARK = $3F                          ; '?'

        .export         _x16_str_find
        .export         _x16_str_rfind
        .export         _x16_str_contains
        .export         _x16_str_find_eol
        .export         _x16_str_pattern_match

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_str_find(const char *s, unsigned char c)
;   First index of c, scanning left to right; 255 when it is not there.
;
; c (the rightmost arg) arrives in A and the asm wants it in Y -- but
; popax clobbers Y, so c rides the hardware stack across the pointer
; pop and lands in Y at the last moment (the dos.s idiom).
; ---------------------------------------------------------------------
_x16_str_find:
        pha                             ; the character
        jsr     popax                   ; s -> A/X
        ply                             ; Y = the character
        jsr     str_find                ; A = index or 255
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_str_rfind(const char *s, unsigned char c)
;   First index of c scanning right to left; 255 when it is not there.
; ---------------------------------------------------------------------
_x16_str_rfind:
        pha
        jsr     popax
        ply
        jsr     str_rfind
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_str_contains(const char *s, unsigned char c)
;   1 if c occurs in s, else 0 -- the asm's carry, made C-shaped.
; ---------------------------------------------------------------------
_x16_str_contains:
        pha
        jsr     popax
        ply
        jsr     str_contains            ; carry set when found
        lda     #0
        rol     a                       ; A = the carry
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_str_find_eol(const char *s)
;   First index of a CR (13) or LF (10); 255 when the string has neither.
; ---------------------------------------------------------------------
_x16_str_find_eol:
        jsr     str_find_eol
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_str_pattern_match(const char *s,
;                                                  const char *pattern)
;   1 if s matches the wildcard pattern, else 0. In the pattern, '?'
;   matches any single character and '*' any run including none.
; ---------------------------------------------------------------------
_x16_str_pattern_match:
        sta     X16_P0                  ; pattern (rightmost arg, in A/X)
        stx     X16_P1
        jsr     popax                   ; s -> A/X
        jsr     str_pattern_match       ; A = 1/0 already
        ldx     #0
        rts

; =====================================================================
; The library routines (x16_library string/find.asm, verbatim except
; for the colons ca65 wants on labels and the CH_* constants standing
; in for its character literals)
; =====================================================================

; ---------------------------------------------------------------------
; str_find -- first index of a character, scanning left to right.
;   in:  A = low, X = high, Y = character
;   out: A = the index, or 255 when the character is not there
;        (the carry says the same thing: set when found)
; ---------------------------------------------------------------------
str_find:
    sta X16_T0
    stx X16_T1
    sty X16_T2
    ldy #0
@loop:
    lda (X16_T0),y
    beq @notfound
    cmp X16_T2
    beq @found
    iny
    bne @loop
@notfound:
    lda #255
    clc
    rts
@found:
    tya
    sec
    rts

; ---------------------------------------------------------------------
; str_contains -- carry set if the character occurs in the string.
;   in: A = low, X = high, Y = character
; ---------------------------------------------------------------------
str_contains:
    jmp str_find

; ---------------------------------------------------------------------
; str_find_eol -- first index of a CR (13) or LF (10).
;   in:  A = low, X = high
;   out: A = the index, or 255 when the character is not there
;        (the carry says the same thing: set when found)
; ---------------------------------------------------------------------
str_find_eol:
    sta X16_T0
    stx X16_T1
    ldy #0
@loop:
    lda (X16_T0),y
    beq @notfound
    cmp #13
    beq @found
    cmp #10
    beq @found
    iny
    bne @loop
@notfound:
    lda #255
    clc
    rts
@found:
    tya
    sec
    rts

; ---------------------------------------------------------------------
; str_rfind -- first index of a character, scanning right to left.
;   in:  A = low, X = high, Y = character
;   out: A = the index, or 255 when the character is not there
;        (the carry says the same thing: set when found)
; ---------------------------------------------------------------------
str_rfind:
    sty X16_T2
    sta X16_T0
    stx X16_T1
    ldy #0
@len:
    lda (X16_T0),y
    beq @gotlen
    iny
    bne @len
@gotlen:
    cpy #0
    beq @notfound               ; empty string
    dey                         ; start at the last character
@loop:
    lda (X16_T0),y
    cmp X16_T2
    beq @found
    dey
    cpy #255                    ; walked past index 0
    bne @loop
@notfound:
    lda #255
    clc
    rts
@found:
    tya
    sec
    rts

; ---------------------------------------------------------------------
; str_pattern_match -- match a string against a wildcard pattern.
;   in:  A = string low, X = string high, X16_P0/P1 = pattern
;   out: carry set (and A = 1) if it matches, else carry clear (A = 0)
;
; In the pattern, '?' matches any single character and '*' matches any
; run of characters including none. Case-sensitive. Both string and
; pattern are NUL-terminated and at most 255 long. Each '*' costs 4 bytes
; of CPU stack. Algorithm from 6502.org/source/strings/patmatch.htm.
;
; The whole matcher is written with zone-local labels (no @cheap) because
; it self-modifies (the pattern address is patched into two loads) and
; recurses -- an SMC target mid-routine would otherwise split a cheap
; scope under some assemblers.
; ---------------------------------------------------------------------
str_pattern_match:
    sta X16_T0                  ; strptr = the string
    stx X16_T1
    lda X16_P0                  ; patch the pattern address into both loads
    sta find_pm_pat1+1
    sta find_pm_pat2+1
    lda X16_P1
    sta find_pm_pat1+2
    sta find_pm_pat2+2
    jsr find_pm_match               ; carry = the match result
    lda #0
    bcc @done                   ; keep the carry; set A = 1 on a match
    lda #1
@done:
    rts

find_pm_match:
    ldx #0                      ; X indexes the pattern
    ldy #$ff                    ; Y indexes the string (iny brings it to 0)
find_pm_next:
find_pm_pat1:
    lda $ffff,x                 ; pattern[X]  (address patched above)
    cmp #CH_STAR
    beq find_pm_star
    iny
    cmp #CH_QMARK
    bne find_pm_reg
    lda (X16_T0),y              ; '?' matches anything but the terminator
    beq find_pm_fail
find_pm_reg:
    cmp (X16_T0),y
    bne find_pm_fail
    inx
    cmp #0                      ; matched the NUL: end of both
    bne find_pm_next
    rts                         ; carry set = match
find_pm_star:
    inx
find_pm_pat2:
    cmp $ffff,x                 ; a run of '*' is the same as one
    beq find_pm_star
find_pm_stloop:
    txa
    pha
    tya
    pha
    jsr find_pm_next                ; try to match the rest here
    pla
    tay
    pla
    tax
    bcs find_pm_done                ; it matched: keep the carry set
    iny
    lda (X16_T0),y              ; grow what '*' swallows, unless at the end
    bne find_pm_stloop
find_pm_fail:
    clc
find_pm_done:
    rts
