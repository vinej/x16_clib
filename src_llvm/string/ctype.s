; =====================================================================
; x16clib :: string/ctype.s -- single-character classification
; =====================================================================
; Character predicates (ported from x16_library's string/ctype.asm).
; Digits, hex digits and whitespace mean the same thing in PETSCII and
; ISO, so they have one routine each; the case-sensitive ones (upper /
; letter / print) come in a PETSCII form and an _iso form, because the
; two encodings place the letters differently.
;
; The asm answers in the carry; the C entry points turn that into 0/1.
;
;   x16_str_isdigit / _isxdigit / _islower / _isspace   (either encoding)
;   x16_str_isupper / _isletter / _isprint [_iso]
; =====================================================================

        .include        "macros.inc"

; ---------------------------------------------------------------------
; ca65 -t cx16 TRANSLATES CHARACTER LITERALS TO PETSCII. The upstream
; source assembles without a target and wrote `cmp #'a'` meaning 97;
; here that literal would silently become $41. Every range bound is
; therefore written as its explicit value, matching the upstream
; comments (which upstream verified against the untranslated literals).
; ---------------------------------------------------------------------
CH_0     = $30                          ; '0'
CH_9     = $39                          ; '9'
CH_UC_A  = $41                          ; 'A'
CH_UC_F  = $46                          ; 'F'
CH_UC_Z  = $5A                          ; 'Z'
CH_LC_A  = $61                          ; 'a'
CH_LC_F  = $66                          ; 'f'
CH_LC_Z  = $7A                          ; 'z'

        .globl  x16_str_isdigit
        .globl  x16_str_isxdigit
        .globl  x16_str_islower
        .globl  x16_str_isupper
        .globl  x16_str_isupper_iso
        .globl  x16_str_isletter
        .globl  x16_str_isletter_iso
        .globl  x16_str_isspace
        .globl  x16_str_isprint
        .globl  x16_str_isprint_iso

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_str_isdigit(unsigned char c)
; ...and every other predicate, all the same shape: the character
; arrives in A (llvm-mos puts the first integer byte there, exactly as
; cc65's fastcall did), the asm answers in the carry, and
; `lda #0 : rol a` turns that carry into the 0/1 a C caller expects.
; ---------------------------------------------------------------------
x16_str_isdigit:
        jsr     str_isdigit
        lda     #0
        rol     a                       ; A = the carry
        rts

x16_str_isxdigit:
        jsr     str_isxdigit
        lda     #0
        rol     a
        rts

x16_str_islower:
        jsr     str_islower
        lda     #0
        rol     a
        rts

x16_str_isupper:
        jsr     str_isupper
        lda     #0
        rol     a
        rts

x16_str_isupper_iso:
        jsr     str_isupper_iso
        lda     #0
        rol     a
        rts

x16_str_isletter:
        jsr     str_isletter
        lda     #0
        rol     a
        rts

x16_str_isletter_iso:
        jsr     str_isletter_iso
        lda     #0
        rol     a
        rts

x16_str_isspace:
        jsr     str_isspace
        lda     #0
        rol     a
        rts

x16_str_isprint:
        jsr     str_isprint
        lda     #0
        rol     a
        rts

x16_str_isprint_iso:
        jsr     str_isprint_iso
        lda     #0
        rol     a
        rts

; =====================================================================
; The library routines (x16_library string/ctype.asm, verbatim except
; for the colons ca65 wants on labels and the CH_* constants standing
; in for its character literals)
; =====================================================================

; ---------------------------------------------------------------------
; str_isdigit -- carry set if A is '0'..'9'
; ---------------------------------------------------------------------
str_isdigit:
    cmp #CH_0
    bcc .Lstr_isdigit_no
    cmp #CH_9+1
    bcs .Lstr_isdigit_no
    sec
    rts
.Lstr_isdigit_no:
    clc
    rts

; ---------------------------------------------------------------------
; str_isxdigit -- carry set if A is a hex digit (0-9, A-F, a-f)
; ---------------------------------------------------------------------
str_isxdigit:
    cmp #CH_0
    bcc .Lstr_isxdigit_no
    cmp #CH_9+1
    bcc .Lstr_isxdigit_yes
    cmp #CH_UC_A
    bcc .Lstr_isxdigit_no
    cmp #CH_UC_F+1
    bcc .Lstr_isxdigit_yes
    cmp #CH_LC_A
    bcc .Lstr_isxdigit_no
    cmp #CH_LC_F+1
    bcc .Lstr_isxdigit_yes
.Lstr_isxdigit_no:
    clc
    rts
.Lstr_isxdigit_yes:
    sec
    rts

; ---------------------------------------------------------------------
; str_islower -- carry set if A is 'a'..'z' (97-122). Same either encoding.
; ---------------------------------------------------------------------
str_islower:
    cmp #CH_LC_A
    bcc .Lstr_islower_no
    cmp #CH_LC_Z+1
    bcs .Lstr_islower_no
    sec
    rts
.Lstr_islower_no:
    clc
    rts

; ---------------------------------------------------------------------
; str_isupper -- PETSCII: the two upper-case ranges, 97-122 and 193-218
; ---------------------------------------------------------------------
str_isupper:
    cmp #97
    bcc .Lstr_isupper_no
    cmp #122+1
    bcc .Lstr_isupper_yes
    cmp #193
    bcc .Lstr_isupper_no
    cmp #218+1
    bcc .Lstr_isupper_yes
.Lstr_isupper_no:
    clc
    rts
.Lstr_isupper_yes:
    sec
    rts

; ---------------------------------------------------------------------
; str_isupper_iso -- ISO: 'A'..'Z' (65-90)
; ---------------------------------------------------------------------
str_isupper_iso:
    cmp #CH_UC_A
    bcc .Lstr_isupper_iso_no
    cmp #CH_UC_Z+1
    bcs .Lstr_isupper_iso_no
    sec
    rts
.Lstr_isupper_iso_no:
    clc
    rts

; ---------------------------------------------------------------------
; str_isletter -- PETSCII: a lower- or upper-case letter
; ---------------------------------------------------------------------
str_isletter:
    jsr str_islower
    bcs .Lstr_isletter_yes
    jmp str_isupper
.Lstr_isletter_yes:
    rts

; ---------------------------------------------------------------------
; str_isletter_iso -- ISO: a lower- or upper-case letter
; ---------------------------------------------------------------------
str_isletter_iso:
    jsr str_islower
    bcs .Lstr_isletter_iso_yes
    jmp str_isupper_iso
.Lstr_isletter_iso_yes:
    rts

; ---------------------------------------------------------------------
; str_isspace -- carry set if A is space, CR, LF, TAB, shift-CR or
; shift-space (32, 13, 10, 9, 141, 160)
; ---------------------------------------------------------------------
str_isspace:
    cmp #32
    beq .Lstr_isspace_yes
    cmp #13
    beq .Lstr_isspace_yes
    cmp #9
    beq .Lstr_isspace_yes
    cmp #10
    beq .Lstr_isspace_yes
    cmp #141
    beq .Lstr_isspace_yes
    cmp #160
    beq .Lstr_isspace_yes
    clc
    rts
.Lstr_isspace_yes:
    sec
    rts

; ---------------------------------------------------------------------
; str_isprint -- PETSCII printable: 32-127 or 160-255
; ---------------------------------------------------------------------
str_isprint:
    cmp #160
    bcs .Lstr_isprint_yes
    cmp #32
    bcc .Lstr_isprint_no
    cmp #128
    bcs .Lstr_isprint_no
    sec
    rts
.Lstr_isprint_no:
    clc
    rts
.Lstr_isprint_yes:
    sec
    rts

; ---------------------------------------------------------------------
; str_isprint_iso -- ISO printable: 32-126 or 160-255
; ---------------------------------------------------------------------
str_isprint_iso:
    cmp #160
    bcs .Lstr_isprint_iso_yes
    cmp #32
    bcc .Lstr_isprint_iso_no
    cmp #127
    bcs .Lstr_isprint_iso_no
    sec
    rts
.Lstr_isprint_iso_no:
    clc
    rts
.Lstr_isprint_iso_yes:
    sec
    rts
