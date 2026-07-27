; =====================================================================
; x16clib :: util/sort.s -- in-place sorting of memory blocks
; =====================================================================
; Sorts a contiguous block of fixed-size elements in place, ascending.
; There is no "array type" -- you pass a base address and an element
; count, which is exactly what a high-level array is underneath.
;
;   sort_u8  / sort_s8   -- byte elements, unsigned / signed
;   sort_u16 / sort_s16  -- word elements, unsigned / signed
;   sort_ptr             -- 2-byte elements ordered by a caller comparator
;
; One insertion-sort engine drives them all through a comparator vector;
; the typed entries just pick the element size and the comparator. O(n^2)
; but tiny and stable -- right for the modest arrays a 6502 sorts.
;
; Comparator ABI (used by sort_ptr, and internally):
;   in:  X16_PTR2 (P4/P5) = address of element A
;        X16_PTR3 (P6/P7) = address of element B
;   out: carry SET if A must sort AFTER B (A > B), clear otherwise.
;   May use A/X/Y; must not disturb the srt_* state.
;
; The C-facing x16_sort() wraps sort_ptr behind a trampoline, so the
; comparator can be an ordinary C function returning nonzero for
; "A sorts after B". C code compiled by cc65 never writes the library's
; X16_P block, so the engine's element pointers survive the call.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popax, pushax

        .export         _x16_sort_u8
        .export         _x16_sort_s8
        .export         _x16_sort_u16
        .export         _x16_sort_s16
        .export         _x16_sort

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_sort_u8  (unsigned char *arr, unsigned int count)
; void __fastcall__ x16_sort_s8  (signed char *arr, unsigned int count)
; void __fastcall__ x16_sort_u16 (unsigned int *arr, unsigned int count)
; void __fastcall__ x16_sort_s16 (int *arr, unsigned int count)
; ---------------------------------------------------------------------
_x16_sort_u8:
        jsr     sort_pop_args
        jmp     sort_u8

_x16_sort_s8:
        jsr     sort_pop_args
        jmp     sort_s8

_x16_sort_u16:
        jsr     sort_pop_args
        jmp     sort_u16

_x16_sort_s16:
        jsr     sort_pop_args
        jmp     sort_s16

; ---------------------------------------------------------------------
; void __fastcall__ x16_sort (void *base, unsigned int count,
;                             x16_sort_cmp_t cmp)
;   2-byte elements (pointers, pairs, 16-bit handles), ordered by a C
;   comparator: unsigned char __fastcall__ cmp(const void *a,
;   const void *b), returning nonzero iff A must sort AFTER B.
; ---------------------------------------------------------------------
_x16_sort:
        sta     srt_ccmp                ; cmp (rightmost arg, in A/X)
        stx     srt_ccmp+1
        jsr     popax                   ; count -- now A/X match sort_pop_args
        jsr     sort_pop_args
        lda     #<sort_c_tramp
        sta     X16_P4
        lda     #>sort_c_tramp
        sta     X16_P5
        jmp     sort_ptr

; count arrives in A/X; the array base is still on the C stack.
sort_pop_args:
        sta     X16_P2
        stx     X16_P3
        jsr     popax                   ; base
        sta     X16_P0
        stx     X16_P1
        rts

; The assembly-side comparator handed to sort_ptr: forward the two
; element addresses to the C comparator (first arg pushed, second in
; A/X, cc65 fastcall), then turn its A into the carry the engine wants.
; The engine's P4/P5 must survive: the trampoline only reads them, and
; compiled C code touches cc65's own zero page, never the X16_P block.
sort_c_tramp:
        lda     X16_P4                  ; element A
        ldx     X16_P5
        jsr     pushax
        lda     X16_P6                  ; element B
        ldx     X16_P7
        jsr     sort_c_call
        cmp     #1                      ; carry set iff A (the verdict) != 0
        rts
sort_c_call:
        jmp     (srt_ccmp)

; =====================================================================
; Internal routines (the upstream x16_library body, verbatim)
; =====================================================================

; ---------------------------------------------------------------------
; public entry points -- in: X16_P0/P1 = base, X16_P2/P3 = count
; ---------------------------------------------------------------------
sort_u8:
        ldx     #1
        lda     #<sort_cmp_u8
        ldy     #>sort_cmp_u8
        bra     sort_setup
sort_s8:
        ldx     #1
        lda     #<sort_cmp_s8
        ldy     #>sort_cmp_s8
        bra     sort_setup
sort_u16:
        ldx     #2
        lda     #<sort_cmp_u16
        ldy     #>sort_cmp_u16
        bra     sort_setup
sort_s16:
        ldx     #2
        lda     #<sort_cmp_s16
        ldy     #>sort_cmp_s16
        bra     sort_setup

; sort_ptr -- element size 2, comparator address in X16_P4/P5
sort_ptr:
        lda     X16_P4
        ldy     X16_P5
        ldx     #2
        ; fall through to sort_setup

sort_setup:
        stx     srt_size
        sta     srt_cmp
        sty     srt_cmp+1
        lda     X16_P0
        sta     srt_base
        lda     X16_P1
        sta     srt_base+1
        lda     X16_P2
        sta     srt_count
        lda     X16_P3
        sta     srt_count+1

        ; nothing to do for fewer than two elements
        lda     srt_count+1
        bne     sort_start
        lda     srt_count
        cmp     #2
        bcs     sort_start
sort_done:
        rts
sort_start:
        lda     #1                      ; i = 1
        sta     srt_i
        stz     srt_i+1

sort_outer:
        ; while i < count
        lda     srt_i+1
        cmp     srt_count+1
        bcc     sort_body
        bne     sort_done
        lda     srt_i
        cmp     srt_count
        bcs     sort_done
sort_body:
        ; key = arr[i]
        lda     srt_i
        sta     X16_T0
        lda     srt_i+1
        sta     X16_T1
        jsr     sort_addr2              ; P4/P5 = &arr[i]
        jsr     sort_load_key

        ; j = i - 1  (i >= 1 so this does not underflow)
        lda     srt_i
        sec
        sbc     #1
        sta     srt_j
        lda     srt_i+1
        sbc     #0
        sta     srt_j+1

sort_inner:
        ; P4/P5 = &arr[j],  P6/P7 = &srt_key,  compare
        lda     srt_j
        sta     X16_T0
        lda     srt_j+1
        sta     X16_T1
        jsr     sort_addr2              ; P4/P5 = &arr[j]
        lda     #<srt_key
        sta     X16_P6
        lda     #>srt_key
        sta     X16_P7
        jsr     sort_callcmp            ; carry set if arr[j] > key
        bcc     sort_place_jp1

        ; arr[j+1] = arr[j]
        lda     srt_j                   ; T0/T1 = j+1
        clc
        adc     #1
        sta     X16_T0
        lda     srt_j+1
        adc     #0
        sta     X16_T1
        jsr     sort_addr3              ; P6/P7 = &arr[j+1]  (dest; P4/P5 still &arr[j])
        jsr     sort_copy_elem

        ; if j == 0, key belongs at arr[0]
        lda     srt_j
        ora     srt_j+1
        beq     sort_place_0

        lda     srt_j                   ; j--
        sec
        sbc     #1
        sta     srt_j
        lda     srt_j+1
        sbc     #0
        sta     srt_j+1
        bra     sort_inner

sort_place_0:
        stz     X16_T0                  ; &arr[0]
        stz     X16_T1
        jsr     sort_addr3
        jsr     sort_store_key
        bra     sort_next_i

sort_place_jp1:
        lda     srt_j                   ; &arr[j+1]
        clc
        adc     #1
        sta     X16_T0
        lda     srt_j+1
        adc     #0
        sta     X16_T1
        jsr     sort_addr3
        jsr     sort_store_key

sort_next_i:
        inc     srt_i
        bne     @loop
        inc     srt_i+1
@loop:
        jmp     sort_outer

; --- address arithmetic ----------------------------------------------
; sort_addr2 / sort_addr3 : X16_T0/T1 = index -> P4/P5 (resp. P6/P7) = base+index*size
sort_addr2:
        ldx     srt_size
        cpx     #2
        beq     @two
        clc
        lda     srt_base
        adc     X16_T0
        sta     X16_P4
        lda     srt_base+1
        adc     X16_T1
        sta     X16_P5
        rts
@two:
        lda     X16_T0
        asl
        sta     X16_T2
        lda     X16_T1
        rol
        sta     X16_T3
        clc
        lda     srt_base
        adc     X16_T2
        sta     X16_P4
        lda     srt_base+1
        adc     X16_T3
        sta     X16_P5
        rts

sort_addr3:
        ldx     srt_size
        cpx     #2
        beq     @two3
        clc
        lda     srt_base
        adc     X16_T0
        sta     X16_P6
        lda     srt_base+1
        adc     X16_T1
        sta     X16_P7
        rts
@two3:
        lda     X16_T0
        asl
        sta     X16_T2
        lda     X16_T1
        rol
        sta     X16_T3
        clc
        lda     srt_base
        adc     X16_T2
        sta     X16_P6
        lda     srt_base+1
        adc     X16_T3
        sta     X16_P7
        rts

; --- element moves ---------------------------------------------------
sort_load_key:
        ldy     #0
        lda     (X16_P4),y
        sta     srt_key
        ldx     srt_size
        cpx     #2
        bne     @done
        iny
        lda     (X16_P4),y
        sta     srt_key+1
@done:
        rts

sort_store_key:
        ldy     #0
        lda     srt_key
        sta     (X16_P6),y
        ldx     srt_size
        cpx     #2
        bne     @done2
        iny
        lda     srt_key+1
        sta     (X16_P6),y
@done2:
        rts

sort_copy_elem:
        ldy     #0
        lda     (X16_P4),y
        sta     (X16_P6),y
        ldx     srt_size
        cpx     #2
        bne     @done3
        iny
        lda     (X16_P4),y
        sta     (X16_P6),y
@done3:
        rts

sort_callcmp:
        jmp     (srt_cmp)

; --- built-in comparators (A at P4/P5, B at P6/P7; C set iff A > B) ----
; Each is self-contained (no far branches to shared exits).
sort_cmp_u8:
        ldy     #0
        lda     (X16_P4),y
        cmp     (X16_P6),y              ; C = (A >= B)
        bne     @ret                    ; not equal -> C is already (A > B)
        clc                             ; equal -> not greater
@ret:
        rts

sort_cmp_s8:
        ldy     #0
        lda     (X16_P4),y
        cmp     (X16_P6),y
        beq     @eq
        lda     (X16_P4),y
        sec
        sbc     (X16_P6),y
        bvc     @nov
        eor     #$80
@nov:
        bmi     @lt                     ; N set -> A < B
        sec                             ; A > B
        rts
@lt:
@eq:
        clc
        rts

sort_cmp_u16:
        ldy     #1
        lda     (X16_P4),y              ; high bytes
        cmp     (X16_P6),y
        bne     @ne                     ; high differs -> C decides
        dey
        lda     (X16_P4),y              ; low bytes
        cmp     (X16_P6),y
        bne     @ne
        clc                             ; fully equal
        rts
@ne:
        rts                             ; C = (A > B), since not equal

sort_cmp_s16:
        ldy     #1
        lda     (X16_P4),y
        cmp     (X16_P6),y
        bne     @hidiff
        dey
        lda     (X16_P4),y
        cmp     (X16_P6),y              ; hi equal: low bytes decide (same sign)
        bne     @lodiff
        clc                             ; fully equal
        rts
@lodiff:
        rts                             ; C = (A > B)
@hidiff:
        lda     (X16_P4),y              ; y=1, signed compare of high bytes
        sec
        sbc     (X16_P6),y
        bvc     @nov2
        eor     #$80
@nov2:
        bmi     @lt2                    ; A < B
        sec                             ; A > B
        rts
@lt2:
        clc
        rts

        .segment        "BSS"

srt_base:  .res 2               ; base address of the array
srt_count: .res 2               ; element count
srt_size:  .res 1               ; element size in bytes (1 or 2)
srt_cmp:   .res 2               ; comparator routine vector
srt_i:     .res 2               ; outer index
srt_j:     .res 2               ; inner index
srt_key:   .res 2               ; the element being inserted
srt_ccmp:  .res 2               ; the C comparator behind sort_c_tramp
