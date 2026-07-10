; =====================================================================
; x16clib :: util/zx0.s -- ZX0 decompression (Einar Saukas's format)
; =====================================================================
; The ROM's LZSA2 (x16_mem_decompress) is free and fast; ZX0 packs
; tighter. This decodes the MODERN ZX0 v2 stream -- what `zx0` and
; `salvador` emit by default, not their -classic mode.
;
;       salvador data.bin data.zx0
;
; RAM to RAM only (the match copier reads the output back). Cannot
; decompress in place. Ported from the reference dzx0.c: three states
; (literals / repeat last offset / new offset), interlaced Elias gamma
; lengths, and the offset byte's low bit seeding the next length.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   INTEGER bytes fill A, then X, then __rc2, __rc3, ... left to right.
;   A POINTER takes a whole __rc pair (__rc2/__rc3, then __rc4/__rc5) and
;   consumes no A/X -- only zero page can be indirected through.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_zx0_decompress

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void * __fastcall__ x16_zx0_decompress(const void *src, void *dst)
;   Returns one past the last output byte.
; ---------------------------------------------------------------------
; Both arguments are pointers, so both arrive in __rc pairs rather than
; A/X: src in __rc2/__rc3, dst in __rc4/__rc5. AND THE RESULT IS A POINTER
; TOO, which llvm-mos returns in __rc2/__rc3 -- not in A/X, where the
; internal routine leaves it.
x16_zx0_decompress:
        lda     __rc2
        sta     X16_P0                  ; src
        lda     __rc3
        sta     X16_P1
        lda     __rc4
        sta     X16_P2                  ; dst
        lda     __rc5
        sta     X16_P3

        jsr     zx0_decompress          ; A = lo, X = hi

        sta     __rc2                   ; ...but a pointer returns in __rc2/3
        stx     __rc3
        rts

; =====================================================================
; Internal routine
; =====================================================================

; ---------------------------------------------------------------------
; zx0_decompress
;   in:  X16_P0/P1 = compressed data, X16_P2/P3 = output address
;   out: A/X = one past the last output byte
;        (X16_P0..P3 are consumed; X16_T6/T7 used as the copy pointer)
; ---------------------------------------------------------------------
zx0_decompress:
        stz     zx_bits                 ; empty bit buffer: first use refills
        stz     zx_bt
        lda     #1                      ; the initial offset is 1
        sta     zx_off
        stz     zx_off+1

.Lzx0_decompress_literals:
        jsr     gamma_n                 ; literal run length
.Lzx0_decompress_lit_byte:
        jsr     getbyte
        sta     (X16_P2)
        inc     X16_P2
        bne     .Lzx0_decompress_lit_dec
        inc     X16_P3
.Lzx0_decompress_lit_dec:
        jsr     dec_len
        bne     .Lzx0_decompress_lit_byte

        jsr     getbit
        bcs     .Lzx0_decompress_new_offset

.Lzx0_decompress_last_offset:
        jsr     gamma_n                 ; match length, offset unchanged
        jsr     copy
        jsr     getbit
        bcc     .Lzx0_decompress_literals

.Lzx0_decompress_new_offset:
        jsr     gamma_i                 ; the offset MSB, inverted gamma (v2)
        lda     zx_val+1                ; 256 is the end-of-stream marker
        beq     .Lzx0_decompress_not_end
        lda     zx_val
        bne     .Lzx0_decompress_not_end
        lda     X16_P2                  ; done: hand back the output end
        ldx     X16_P3
        rts
.Lzx0_decompress_not_end:
        lda     zx_val                  ; offset = MSB*128 - (next byte >> 1)
        lsr     a
        sta     zx_off+1
        lda     #0
        ror     a
        sta     zx_off
        jsr     getbyte                 ; ...which also latches zx_last
        lsr     a
        sta     zx_t
        sec
        lda     zx_off
        sbc     zx_t
        sta     zx_off
        lda     zx_off+1
        sbc     #0
        sta     zx_off+1
        lda     #1                      ; that byte's low bit is the FIRST bit
        sta     zx_bt                   ; of the coming length gamma
        jsr     gamma_n
        inc     zx_val                  ; new-offset match lengths are +1
        bne     .Lzx0_decompress_len_ok
        inc     zx_val+1
.Lzx0_decompress_len_ok:
        jsr     copy
        jsr     getbit
        bcs     .Lzx0_decompress_new_offset
        bra     .Lzx0_decompress_literals

; --- plumbing ---------------------------------------------------------

; copy zx_val bytes from (output - zx_off) to the output
copy:
        sec
        lda     X16_P2
        sbc     zx_off
        sta     X16_T6
        lda     X16_P3
        sbc     zx_off+1
        sta     X16_T7
.Lcopy_byte:
        lda     (X16_T6)
        sta     (X16_P2)
        inc     X16_T6
        bne     .Lcopy_dst
        inc     X16_T7
.Lcopy_dst:
        inc     X16_P2
        bne     .Lcopy_count
        inc     X16_P3
.Lcopy_count:
        jsr     dec_len
        bne     .Lcopy_byte
        rts

; zx_val -= 1; Z set when it reaches zero (val >= 1 on entry)
dec_len:
        lda     zx_val
        bne     .Ldec_len_lo
        dec     zx_val+1
.Ldec_len_lo:
        dec     zx_val
        lda     zx_val
        ora     zx_val+1
        rts

; interlaced Elias gamma into zx_val: normal and inverted data bits
gamma_i:
        lda     #1
        bra     gamma
gamma_n:
        lda     #0
gamma:
        sta     zx_inv
        lda     #1
        sta     zx_val
        stz     zx_val+1
.Lgamma_more:
        jsr     getbit
        bcs     .Lgamma_done                   ; a 1 control bit ends the number
        jsr     getbit
        lda     #0
        rol     a                       ; A = the data bit
        eor     zx_inv
        lsr     a                       ; ...back into the carry
        rol     zx_val
        rol     zx_val+1
        bra     .Lgamma_more
.Lgamma_done:
        rts

; next bit into the carry. The buffer keeps a sentinel 1 in bit 0, so a
; zero buffer after the shift means "that carry was the sentinel":
; refill and take bit 7 of the fresh byte instead.
getbit:
        lda     zx_bt
        beq     .Lgetbit_stream
        stz     zx_bt                   ; backtrack: the offset byte's low bit
        lda     zx_last
        lsr     a
        rts
.Lgetbit_stream:
        asl     zx_bits
        beq     .Lgetbit_refill
        rts
.Lgetbit_refill:
        jsr     getbyte
        sec
        rol     a                       ; carry = bit 7, sentinel into bit 0
        sta     zx_bits
        rts

getbyte:
        lda     (X16_P0)
        sta     zx_last
        inc     X16_P0
        bne     .Lgetbyte_gb_ok
        inc     X16_P1
.Lgetbyte_gb_ok:
        lda     zx_last
        rts

        .section .bss,"aw",@nobits

zx_bits: .zero  1
zx_last: .zero  1
zx_bt:   .zero  1
zx_inv:  .zero  1
zx_val:  .zero  2
zx_off:  .zero  2
zx_t:    .zero  1
