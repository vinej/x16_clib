; =====================================================================
; x16clib :: audio/adpcm.s -- IMA ADPCM decoding (4:1 compression)
; =====================================================================
; The natural partner to the PCM streamer: IMA ADPCM stores 16-bit
; samples as 4-bit deltas, so a second of 16-bit mono at 16 kHz is 8 KB
; instead of 32 -- one RAM bank per second, streamable from disk.
;
; This is the canonical IMA/DVI algorithm (the one in WAV files, with the
; LOW nibble of each byte first). Decoder state is exposed: the predictor
; and the step index. IMA WAV block headers carry initial values for
; both; store them before decoding a block's payload.
;
; A separate object from audio/pcm.s: decoding does not need the FIFO,
; and priming the FIFO does not need a decoder.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X, then whatever __rc bytes the pointers
;   left free. So f(ptr, int, char) is ptr in __rc2/3, int in A/X, char in
;   __rc4.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_adpcm_init
        .globl  x16_adpcm_nibble
        .globl  x16_adpcm_block
        .globl  x16_adpcm_set_state
        .globl  x16_adpcm_predictor
        .globl  x16_adpcm_index

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void x16_adpcm_init(void) -- predictor 0, step index 0
; ---------------------------------------------------------------------
x16_adpcm_init:
adpcm_init:
        stz     adpcm_pred
        stz     adpcm_pred+1
        stz     adpcm_index
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_adpcm_set_state(int predictor, unsigned char index)
; int x16_adpcm_predictor(void)
; unsigned char x16_adpcm_index(void)
;
; An IMA WAV block header carries both; set them before decoding the
; block's payload.
; ---------------------------------------------------------------------
; predictor is an int -> A/X; index is a byte -> __rc2.
x16_adpcm_set_state:
        sta     adpcm_pred
        stx     adpcm_pred+1
        lda     __rc2
        sta     adpcm_index
        rts

x16_adpcm_predictor:
        lda     adpcm_pred
        ldx     adpcm_pred+1
        rts

x16_adpcm_index:
        lda     adpcm_index             ; a char return is A alone
        rts

; ---------------------------------------------------------------------
; int __fastcall__ x16_adpcm_nibble(unsigned char code)
;   The code already arrives in A, and the sample comes back in A/X:
;   no shim.
; ---------------------------------------------------------------------
x16_adpcm_nibble:
        jmp     adpcm_nibble

; ---------------------------------------------------------------------
; void __fastcall__ x16_adpcm_block(const void *src, void *dst,
;                                   unsigned int count)
;   `count` is the SOURCE byte count: two samples, four bytes, come out
;   for every byte in. Decoder state carries across calls, so feeding a
;   block in slices is fine.
; ---------------------------------------------------------------------
; src and dst are pointers (__rc2/__rc3, __rc4/__rc5); count is the only
; integer, so it takes A and X.
x16_adpcm_block:
        sta     X16_P4                  ; source count lo
        stx     X16_P5                  ; source count hi
        lda     __rc2
        sta     X16_P0                  ; src
        lda     __rc3
        sta     X16_P1
        lda     __rc4
        sta     X16_P2                  ; dst
        lda     __rc5
        sta     X16_P3
        jmp     adpcm_block

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; adpcm_nibble -- decode one 4-bit code
;   in:  A = the code (0-15)
;   out: A = sample low, X = sample high (signed 16-bit; also left in
;        adpcm_pred). Clobbers Y.
; ---------------------------------------------------------------------
adpcm_nibble:
        sta     ad_n
        lda     adpcm_index             ; step = steptab[index]
        asl     a
        tay
        lda     steps,y
        sta     ad_sh
        lda     steps+1,y
        sta     ad_sh+1

        ; diff = step>>3 (+ step if bit2) (+ step>>1 if bit1) (+ step>>2
        ; if bit0); max 1.875 * 32767 = 61436, which fits 16 bits unsigned
        stz     ad_diff
        stz     ad_diff+1
        lda     ad_n
        and     #4
        beq     .Ladpcm_nibble_no4
        lda     ad_sh
        sta     ad_diff
        lda     ad_sh+1
        sta     ad_diff+1
.Ladpcm_nibble_no4:
        lsr     ad_sh+1
        ror     ad_sh
        lda     ad_n
        and     #2
        beq     .Ladpcm_nibble_no2
        jsr     add_sh
.Ladpcm_nibble_no2:
        lsr     ad_sh+1
        ror     ad_sh
        lda     ad_n
        and     #1
        beq     .Ladpcm_nibble_no1
        jsr     add_sh
.Ladpcm_nibble_no1:
        lsr     ad_sh+1
        ror     ad_sh
        jsr     add_sh                  ; the unconditional step>>3

        ; predictor +/- diff, in 24 bits, saturated to 16
        lda     adpcm_pred              ; sign-extend the predictor
        sta     ad_p
        lda     adpcm_pred+1
        sta     ad_p+1
        stz     ad_p+2
        bpl     .Ladpcm_nibble_ext_ok
        dec     ad_p+2                  ; $FF
.Ladpcm_nibble_ext_ok:
        lda     ad_n
        and     #8
        bne     .Ladpcm_nibble_minus
        clc
        lda     ad_p
        adc     ad_diff
        sta     ad_p
        lda     ad_p+1
        adc     ad_diff+1
        sta     ad_p+1
        lda     ad_p+2
        adc     #0
        sta     ad_p+2
        bra     .Ladpcm_nibble_clamp
.Ladpcm_nibble_minus:
        sec
        lda     ad_p
        sbc     ad_diff
        sta     ad_p
        lda     ad_p+1
        sbc     ad_diff+1
        sta     ad_p+1
        lda     ad_p+2
        sbc     #0
        sta     ad_p+2

.Ladpcm_nibble_clamp:
        ; a legal 16-bit value has p+2 = $00 with p+1 bit7 clear, or
        ; p+2 = $FF with p+1 bit7 set; anything else saturates
        lda     ad_p+2
        beq     .Ladpcm_nibble_maybe_pos
        cmp     #$FF
        beq     .Ladpcm_nibble_maybe_neg
        bra     .Ladpcm_nibble_sat                    ; way out of range
.Ladpcm_nibble_maybe_pos:
        lda     ad_p+1
        bpl     .Ladpcm_nibble_in_range
        bra     .Ladpcm_nibble_sat_pos
.Ladpcm_nibble_maybe_neg:
        lda     ad_p+1
        bmi     .Ladpcm_nibble_in_range
        bra     .Ladpcm_nibble_sat_neg
.Ladpcm_nibble_sat:
        lda     ad_p+2
        bmi     .Ladpcm_nibble_sat_neg
.Ladpcm_nibble_sat_pos:
        lda     #$FF
        sta     ad_p
        lda     #$7F
        sta     ad_p+1
        bra     .Ladpcm_nibble_in_range
.Ladpcm_nibble_sat_neg:
        stz     ad_p
        lda     #$80
        sta     ad_p+1
.Ladpcm_nibble_in_range:
        lda     ad_p
        sta     adpcm_pred
        lda     ad_p+1
        sta     adpcm_pred+1

        ; index += indextab[n & 7], clamped to 0..88
        lda     ad_n
        and     #7
        tay
        lda     adpcm_index
        clc
        adc     idxtab,y
        bpl     .Ladpcm_nibble_not_neg
        lda     #0
.Ladpcm_nibble_not_neg:
        cmp     #89
        bcc     .Ladpcm_nibble_idx_ok
        lda     #88
.Ladpcm_nibble_idx_ok:
        sta     adpcm_index

        lda     adpcm_pred
        ldx     adpcm_pred+1
        rts

add_sh:
        clc
        lda     ad_diff
        adc     ad_sh
        sta     ad_diff
        lda     ad_diff+1
        adc     ad_sh+1
        sta     ad_diff+1
        rts

; ---------------------------------------------------------------------
; adpcm_block -- decode a run of bytes to 16-bit little-endian samples
;   in:  X16_P0/P1 = source (ADPCM bytes)
;        X16_P2/P3 = destination (4 bytes out per byte in)
;        X16_P4/P5 = SOURCE byte count
;
; Low nibble first, as in IMA WAV blocks. The parameter block is consumed
; (pointers advance).
; ---------------------------------------------------------------------
adpcm_block:
.Ladpcm_block_loop:
        lda     X16_P4
        ora     X16_P5
        beq     .Ladpcm_block_done

        ldy     #0
        lda     (X16_P0),y
        pha
        and     #$0F                    ; low nibble first
        jsr     emit
        pla
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        jsr     emit

        inc     X16_P0
        bne     .Ladpcm_block_next
        inc     X16_P1
.Ladpcm_block_next:
        lda     X16_P4
        bne     .Ladpcm_block_declo
        dec     X16_P5
.Ladpcm_block_declo:
        dec     X16_P4
        bra     .Ladpcm_block_loop
.Ladpcm_block_done:
        rts

; decode nibble A, append the sample to the output pointer
emit:
        jsr     adpcm_nibble
        ldy     #0
        sta     (X16_P2),y
        txa
        iny
        sta     (X16_P2),y
        clc
        lda     X16_P2
        adc     #2
        sta     X16_P2
        bcc     .Lemit_ok
        inc     X16_P3
.Lemit_ok:
        rts

; ---------------------------------------------------------------------
; Tables. idxtab's first four entries are -1; ca65 would reject `.byte -1`
; as out of range, so they are written as the two's-complement byte the
; `adc` consumes.
; ---------------------------------------------------------------------
        .section .rodata,"a",@progbits

idxtab: .byte   $FF, $FF, $FF, $FF, 2, 4, 6, 8

steps:
        .word   7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31
        .word   34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143
        .word   157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658
        .word   724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024
        .word   3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899
        .word   15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767

        .section .bss,"aw",@nobits

adpcm_pred:  .zero  2                     ; the predictor (signed 16-bit sample)
adpcm_index: .zero  1                     ; step table index 0-88

ad_n:    .zero  1
ad_sh:   .zero  2
ad_diff: .zero  2
ad_p:    .zero  3
