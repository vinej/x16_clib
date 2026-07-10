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

        .import         popax

        .export         _x16_zx0_decompress

        .segment        "CODE"

; ---------------------------------------------------------------------
; void * __fastcall__ x16_zx0_decompress(const void *src, void *dst)
;   Returns one past the last output byte.
; ---------------------------------------------------------------------
_x16_zx0_decompress:
        sta     X16_P2                  ; dst (rightmost arg: A/X)
        stx     X16_P3
        jsr     popax                   ; src
        sta     X16_P0
        stx     X16_P1
        jmp     zx0_decompress          ; already returns A = lo, X = hi

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

@literals:
        jsr     gamma_n                 ; literal run length
@lit_byte:
        jsr     getbyte
        sta     (X16_P2)
        inc     X16_P2
        bne     @lit_dec
        inc     X16_P3
@lit_dec:
        jsr     dec_len
        bne     @lit_byte

        jsr     getbit
        bcs     @new_offset

@last_offset:
        jsr     gamma_n                 ; match length, offset unchanged
        jsr     copy
        jsr     getbit
        bcc     @literals

@new_offset:
        jsr     gamma_i                 ; the offset MSB, inverted gamma (v2)
        lda     zx_val+1                ; 256 is the end-of-stream marker
        beq     @not_end
        lda     zx_val
        bne     @not_end
        lda     X16_P2                  ; done: hand back the output end
        ldx     X16_P3
        rts
@not_end:
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
        bne     @len_ok
        inc     zx_val+1
@len_ok:
        jsr     copy
        jsr     getbit
        bcs     @new_offset
        bra     @literals

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
@byte:
        lda     (X16_T6)
        sta     (X16_P2)
        inc     X16_T6
        bne     @dst
        inc     X16_T7
@dst:
        inc     X16_P2
        bne     @count
        inc     X16_P3
@count:
        jsr     dec_len
        bne     @byte
        rts

; zx_val -= 1; Z set when it reaches zero (val >= 1 on entry)
dec_len:
        lda     zx_val
        bne     @lo
        dec     zx_val+1
@lo:
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
@more:
        jsr     getbit
        bcs     @done                   ; a 1 control bit ends the number
        jsr     getbit
        lda     #0
        rol     a                       ; A = the data bit
        eor     zx_inv
        lsr     a                       ; ...back into the carry
        rol     zx_val
        rol     zx_val+1
        bra     @more
@done:
        rts

; next bit into the carry. The buffer keeps a sentinel 1 in bit 0, so a
; zero buffer after the shift means "that carry was the sentinel":
; refill and take bit 7 of the fresh byte instead.
getbit:
        lda     zx_bt
        beq     @stream
        stz     zx_bt                   ; backtrack: the offset byte's low bit
        lda     zx_last
        lsr     a
        rts
@stream:
        asl     zx_bits
        beq     @refill
        rts
@refill:
        jsr     getbyte
        sec
        rol     a                       ; carry = bit 7, sentinel into bit 0
        sta     zx_bits
        rts

getbyte:
        lda     (X16_P0)
        sta     zx_last
        inc     X16_P0
        bne     @gb_ok
        inc     X16_P1
@gb_ok:
        lda     zx_last
        rts

        .segment        "BSS"

zx_bits: .res 1
zx_last: .res 1
zx_bt:   .res 1
zx_inv:  .res 1
zx_val:  .res 2
zx_off:  .res 2
zx_t:    .res 1
