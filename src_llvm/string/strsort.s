; =====================================================================
; x16clib :: string/strsort.s -- sort an array of string pointers
; =====================================================================
; x16_str_sort orders an array of NUL-terminated-string POINTERS
; ascending by string content, using str_compare (ported from
; x16_library's string/strsort.asm). The strings never move -- only the
; pointer array is permuted, exactly the layout of a C `char *arr[]`.
;
; It carries its own (insertion) sort rather than calling a shared
; sort module, so a program that sorts strings pulls in only the string
; modules -- str_compare arrives from string/string.s, which ld65 links
; in anyway for any real user of this routine.
;
;   x16_str_sort   (const char **array, unsigned int count)
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; str_compare arrives from string/string.s: ELF resolves externs by name,
; so no declaration is needed here.
; llvm-mos argument placement, measured on the machine (see gfx/bitmap4l.s):
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X.

        .globl  x16_str_sort

        .section .text,"ax",@progbits

; =====================================================================
; C entry point
; =====================================================================

; ---------------------------------------------------------------------
; void x16_str_sort(const char **array, unsigned int count)
; array -> __rc2/__rc3 (the pointer), count -> A/X (the integer).
; ---------------------------------------------------------------------
x16_str_sort:
        sta     X16_P2                  ; count
        stx     X16_P3
        lda     __rc2
        sta     X16_P0                  ; array base
        lda     __rc3
        sta     X16_P1
        jmp     str_sort

; =====================================================================
; The library routine (x16_library string/strsort.asm, verbatim except
; for the colons ca65 wants on labels)
; =====================================================================

ss_base:  .word 0               ; base of the pointer array
ss_count: .word 0               ; element count
ss_i:     .word 0
ss_j:     .word 0
ss_key:   .word 0               ; the pointer being inserted

; ---------------------------------------------------------------------
; str_sort -- ascending sort of a string-pointer array
;   in: X16_P0/P1 = array base, X16_P2/P3 = element count
; ---------------------------------------------------------------------
str_sort:
    lda X16_P0
    sta ss_base
    lda X16_P1
    sta ss_base+1
    lda X16_P2
    sta ss_count
    lda X16_P3
    sta ss_count+1

    lda ss_count+1
    bne .Lstr_sort_start
    lda ss_count
    cmp #2
    bcs .Lstr_sort_start
.Lstr_sort_done:
    rts
.Lstr_sort_start:
    lda #1
    sta ss_i
    stz ss_i+1

.Lstr_sort_outer:
    lda ss_i+1
    cmp ss_count+1
    bcc .Lstr_sort_body
    bne .Lstr_sort_done
    lda ss_i
    cmp ss_count
    bcs .Lstr_sort_done
.Lstr_sort_body:
    ; key = arr[i]
    lda ss_i
    sta X16_T0
    lda ss_i+1
    sta X16_T1
    jsr strsort_addr4                 ; P4/P5 = &arr[i]
    ldy #0
    lda (X16_P4),y
    sta ss_key
    iny
    lda (X16_P4),y
    sta ss_key+1

    lda ss_i                   ; j = i - 1
    sec
    sbc #1
    sta ss_j
    lda ss_i+1
    sbc #0
    sta ss_j+1

.Lstr_sort_inner:
    lda ss_j                   ; P4/P5 = &arr[j]
    sta X16_T0
    lda ss_j+1
    sta X16_T1
    jsr strsort_addr4
    ; str_compare(s1 = *arr[j], s2 = key)  ->  A = -1/0/1
    lda ss_key
    sta X16_P0
    lda ss_key+1
    sta X16_P1
    ldy #1
    lda (X16_P4),y
    tax                        ; s1 high
    dey
    lda (X16_P4),y             ; s1 low
    jsr str_compare
    cmp #1
    bne .Lstr_sort_place_jp1             ; arr[j] <= key: stop shifting

    ; arr[j+1] = arr[j]   (P4/P5 = &arr[j] survives str_compare)
    lda ss_j
    clc
    adc #1
    sta X16_T0
    lda ss_j+1
    adc #0
    sta X16_T1
    jsr strsort_addr6                 ; P6/P7 = &arr[j+1]
    ldy #0
    lda (X16_P4),y
    sta (X16_P6),y
    iny
    lda (X16_P4),y
    sta (X16_P6),y

    lda ss_j                   ; j == 0 ? key belongs at arr[0]
    ora ss_j+1
    beq .Lstr_sort_place_0
    lda ss_j
    sec
    sbc #1
    sta ss_j
    lda ss_j+1
    sbc #0
    sta ss_j+1
    jmp .Lstr_sort_inner

.Lstr_sort_place_0:
    stz X16_T0
    stz X16_T1
    jsr strsort_addr6                 ; P6/P7 = &arr[0]
    bra .Lstr_sort_store

.Lstr_sort_place_jp1:
    lda ss_j
    clc
    adc #1
    sta X16_T0
    lda ss_j+1
    adc #0
    sta X16_T1
    jsr strsort_addr6                 ; P6/P7 = &arr[j+1]

.Lstr_sort_store:
    ldy #0
    lda ss_key
    sta (X16_P6),y
    iny
    lda ss_key+1
    sta (X16_P6),y

.Lstr_sort_next_i:
    inc ss_i
    bne .Lstr_sort_loop
    inc ss_i+1
.Lstr_sort_loop:
    jmp .Lstr_sort_outer

; X16_T0/T1 = index -> P4/P5 (strsort_addr4) or P6/P7 (strsort_addr6) = base + index*2
strsort_addr4:
    lda X16_T0
    asl
    sta X16_T2
    lda X16_T1
    rol
    sta X16_T3
    clc
    lda ss_base
    adc X16_T2
    sta X16_P4
    lda ss_base+1
    adc X16_T3
    sta X16_P5
    rts
strsort_addr6:
    lda X16_T0
    asl
    sta X16_T2
    lda X16_T1
    rol
    sta X16_T3
    clc
    lda ss_base
    adc X16_T2
    sta X16_P6
    lda ss_base+1
    adc X16_T3
    sta X16_P7
    rts
