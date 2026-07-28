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

; llvm-mos argument placement, measured on the machine (see gfx/bitmap4l.s):
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X, then whatever __rc bytes the pointers
;   left free. Returns: char in A.

        .globl  x16_sort_u8
        .globl  x16_sort_s8
        .globl  x16_sort_u16
        .globl  x16_sort_s16
        .globl  x16_sort

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void x16_sort_u8  (unsigned char *arr, unsigned int count)
; void x16_sort_s8  (signed char *arr, unsigned int count)
; void x16_sort_u16 (unsigned int *arr, unsigned int count)
; void x16_sort_s16 (int *arr, unsigned int count)
; arr -> __rc2/__rc3 (the pointer), count -> A/X (the integer).
; ---------------------------------------------------------------------
x16_sort_u8:
        jsr     sort_args
        jmp     sort_u8

x16_sort_s8:
        jsr     sort_args
        jmp     sort_s8

x16_sort_u16:
        jsr     sort_args
        jmp     sort_u16

x16_sort_s16:
        jsr     sort_args
        jmp     sort_s16

; ---------------------------------------------------------------------
; void x16_sort (void *base, unsigned int count, x16_sort_cmp_t cmp)
;   2-byte elements (pointers, pairs, 16-bit handles), ordered by a C
;   comparator: unsigned char cmp(const void *a, const void *b),
;   returning nonzero iff A must sort AFTER B.
; base -> __rc2/__rc3 and cmp -> __rc4/__rc5 (the pointers, in order);
; count -> A/X (the integer).
; ---------------------------------------------------------------------
x16_sort:
        jsr     sort_args
        lda     __rc4                   ; cmp, behind the trampoline below
        sta     srt_ccmp
        lda     __rc5
        sta     srt_ccmp+1
        lda     #<sort_c_tramp
        sta     X16_P4
        lda     #>sort_c_tramp
        sta     X16_P5
        jmp     sort_ptr

; base (__rc2/__rc3) -> P0/P1, count (A/X) -> P2/P3
sort_args:
        sta     X16_P2
        stx     X16_P3
        lda     __rc2
        sta     X16_P0
        lda     __rc3
        sta     X16_P1
        rts

; The assembly-side comparator handed to sort_ptr: forward the two
; element addresses to the C comparator (both are pointers, so they go
; in __rc2/__rc3 and __rc4/__rc5), then turn its A into the carry the
; engine wants. The engine's P4/P5 must survive: the trampoline only
; reads them, and compiled C code touches llvm-mos's imaginary
; registers, never the library's reserved X16_P block.
sort_c_tramp:
        lda     X16_P4                  ; element A -> first argument
        sta     __rc2
        lda     X16_P5
        sta     __rc3
        lda     X16_P6                  ; element B -> second argument
        sta     __rc4
        lda     X16_P7
        sta     __rc5
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
        bne     .Lsort_next_i_loop
        inc     srt_i+1
.Lsort_next_i_loop:
        jmp     sort_outer

; --- address arithmetic ----------------------------------------------
; sort_addr2 / sort_addr3 : X16_T0/T1 = index -> P4/P5 (resp. P6/P7) = base+index*size
sort_addr2:
        ldx     srt_size
        cpx     #2
        beq     .Lsort_addr2_two
        clc
        lda     srt_base
        adc     X16_T0
        sta     X16_P4
        lda     srt_base+1
        adc     X16_T1
        sta     X16_P5
        rts
.Lsort_addr2_two:
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
        beq     .Lsort_addr3_two3
        clc
        lda     srt_base
        adc     X16_T0
        sta     X16_P6
        lda     srt_base+1
        adc     X16_T1
        sta     X16_P7
        rts
.Lsort_addr3_two3:
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
        bne     .Lsort_load_key_done
        iny
        lda     (X16_P4),y
        sta     srt_key+1
.Lsort_load_key_done:
        rts

sort_store_key:
        ldy     #0
        lda     srt_key
        sta     (X16_P6),y
        ldx     srt_size
        cpx     #2
        bne     .Lsort_store_key_done2
        iny
        lda     srt_key+1
        sta     (X16_P6),y
.Lsort_store_key_done2:
        rts

sort_copy_elem:
        ldy     #0
        lda     (X16_P4),y
        sta     (X16_P6),y
        ldx     srt_size
        cpx     #2
        bne     .Lsort_copy_elem_done3
        iny
        lda     (X16_P4),y
        sta     (X16_P6),y
.Lsort_copy_elem_done3:
        rts

sort_callcmp:
        jmp     (srt_cmp)

; --- built-in comparators (A at P4/P5, B at P6/P7; C set iff A > B) ----
; Each is self-contained (no far branches to shared exits).
sort_cmp_u8:
        ldy     #0
        lda     (X16_P4),y
        cmp     (X16_P6),y              ; C = (A >= B)
        bne     .Lsort_cmp_u8_ret                    ; not equal -> C is already (A > B)
        clc                             ; equal -> not greater
.Lsort_cmp_u8_ret:
        rts

sort_cmp_s8:
        ldy     #0
        lda     (X16_P4),y
        cmp     (X16_P6),y
        beq     .Lsort_cmp_s8_eq
        lda     (X16_P4),y
        sec
        sbc     (X16_P6),y
        bvc     .Lsort_cmp_s8_nov
        eor     #$80
.Lsort_cmp_s8_nov:
        bmi     .Lsort_cmp_s8_lt                     ; N set -> A < B
        sec                             ; A > B
        rts
.Lsort_cmp_s8_lt:
.Lsort_cmp_s8_eq:
        clc
        rts

sort_cmp_u16:
        ldy     #1
        lda     (X16_P4),y              ; high bytes
        cmp     (X16_P6),y
        bne     .Lsort_cmp_u16_ne                     ; high differs -> C decides
        dey
        lda     (X16_P4),y              ; low bytes
        cmp     (X16_P6),y
        bne     .Lsort_cmp_u16_ne
        clc                             ; fully equal
        rts
.Lsort_cmp_u16_ne:
        rts                             ; C = (A > B), since not equal

sort_cmp_s16:
        ldy     #1
        lda     (X16_P4),y
        cmp     (X16_P6),y
        bne     .Lsort_cmp_s16_hidiff
        dey
        lda     (X16_P4),y
        cmp     (X16_P6),y              ; hi equal: low bytes decide (same sign)
        bne     .Lsort_cmp_s16_lodiff
        clc                             ; fully equal
        rts
.Lsort_cmp_s16_lodiff:
        rts                             ; C = (A > B)
.Lsort_cmp_s16_hidiff:
        lda     (X16_P4),y              ; y=1, signed compare of high bytes
        sec
        sbc     (X16_P6),y
        bvc     .Lsort_cmp_s16_nov2
        eor     #$80
.Lsort_cmp_s16_nov2:
        bmi     .Lsort_cmp_s16_lt2                    ; A < B
        sec                             ; A > B
        rts
.Lsort_cmp_s16_lt2:
        clc
        rts

        .section .bss,"aw",@nobits

srt_base:  .zero  2               ; base address of the array
srt_count: .zero  2               ; element count
srt_size:  .zero  1               ; element size in bytes (1 or 2)
srt_cmp:   .zero  2               ; comparator routine vector
srt_i:     .zero  2               ; outer index
srt_j:     .zero  2               ; inner index
srt_key:   .zero  2               ; the element being inserted
srt_ccmp:  .zero  2               ; the C comparator behind sort_c_tramp
