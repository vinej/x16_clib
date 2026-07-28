; =====================================================================
; x16clib :: util/tscrunch.s -- TSCrunch decompression
; =====================================================================
; TSCrunch (Antonio Savona) is a byte-aligned LZ+RLE built to maximise
; 6502 DECODE SPEED -- the other end of the trade from ZX0: unpacks
; markedly faster, packs a little looser. Crunch with:
;
;       tscrunch data.bin data.tsc        (plain memory crunch)
;
; This is a 65C02 port of the reference decrunch_small.asm: the
; original leans on the NMOS undocumented opcodes LAX and ALR, which
; the X16's 65C02 does not have -- they are replaced with legal pairs,
; everything else (including the load-bearing carry choreography) is
; kept move for move. Copyright for the original algorithm and
; decruncher: Antonio Savona.
;
; RAM to RAM, forward only, cannot decompress in place. The decoder is
; self-modifying (two length operands are patched at run time), which
; is fine in a PRG: CODE loads into main RAM.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine (see util/zx0.s):
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   Returns: POINTER in __rc2/__rc3.

        .globl  x16_tsc_decompress

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void * x16_tsc_decompress (const void *src, void *dst)
;   Returns one past the last output byte.
; ---------------------------------------------------------------------
; Both arguments are pointers, so both arrive in __rc pairs: src in
; __rc2/__rc3, dst in __rc4/__rc5. AND THE RESULT IS A POINTER TOO,
; which llvm-mos returns in __rc2/__rc3 -- not in A/X, where the
; internal routine leaves it.
x16_tsc_decompress:
        lda     __rc2
        sta     X16_P0                  ; src
        lda     __rc3
        sta     X16_P1
        lda     __rc4
        sta     X16_P2                  ; dst
        lda     __rc5
        sta     X16_P3
        jsr     tsc_decompress          ; A = lo, X = hi
        sta     __rc2                   ; ...but a pointer returns in __rc2/3
        stx     __rc3
        rts

; =====================================================================
; Internal routine (the upstream x16_library body, verbatim)
; =====================================================================

; ---------------------------------------------------------------------
; tsc_decompress
;   in:  X16_P0/P1 = compressed data, X16_P2/P3 = output address
;   out: A/X = one past the last output byte
;        (X16_P0..P3 consumed; X16_T5..T7 used as scratch)
; ---------------------------------------------------------------------
tsc_decompress:
        ldy     #0
        lda     (X16_P0),y              ; the stream's first byte parameterises
        sta     tscrunch_optlen+1       ; the one-token zero-run length
        inc     X16_P0
        bne     tsc2_entry
        inc     X16_P1

tsc2_entry:
        lda     (X16_P0),y              ; (LAX) token
        tax
        bmi     tsc2_rleorlz

        cmp     #$20
        bcs     tsc2_lz2

        ; --- literal: token = count, the bytes follow ---------------------
        tay
tsc2_lit:
        lda     (X16_P0),y
        dey
        sta     (X16_P2),y
        bne     tsc2_lit
        txa                             ; carry is clear (cmp #$20 fell through)
        inx
tsc2_bump_zp:
        adc     X16_P2                  ; output += A (+ inherited carry)
        sta     X16_P2
        bcs     tsc2_put_hi
tsc2_put_ok:
        txa
tsc2_bump_get:
        adc     X16_P0                  ; input += X
        sta     X16_P0
        bcc     tsc2_entry
        inc     X16_P1
        bcs     tsc2_entry

tsc2_put_hi:
        inc     X16_P3
        clc
        bcc     tsc2_put_ok

        ; --- RLE or LZ (token bit 7 set) -----------------------------------
tsc2_rleorlz:
        and     #$7F                    ; (ALR #$7F)
        lsr
        bcc     tsc2_lz

        ; RLE: A = length field, carry is set for the +1 in tsc2_bump_zp
        beq     tsc2_optrun
        ldx     #2
        iny
        sta     X16_T5                  ; run length
        lda     (X16_P0),y              ; the byte to repeat
        ldy     X16_T5
tsc2_run_start:
        sta     (X16_P2),y
tsc2_rle_loop:
        dey
        sta     (X16_P2),y
        bne     tsc2_rle_loop
        lda     X16_T5
        bcs     tsc2_bump_zp            ; always (carry survived untouched)

tsc2_done:
        lda     X16_P2                  ; the end of the output
        ldx     X16_P3
        rts

        ; --- LZ2: a two-byte match with a one-byte token -------------------
tsc2_lz2:
        beq     tsc2_done               ; $20 is the end-of-stream marker
        ora     #$80                    ; carry is set: offset folds negative
        adc     X16_P2
        sta     X16_T6
        lda     X16_P3
        sbc     #$00
        sta     X16_T7
        lda     (X16_T6),y              ; y = 0
        sta     (X16_P2),y
        iny
        lda     (X16_T6),y
        sta     (X16_P2),y
        tya                             ; A = 1
        tax                             ; X = 1
        dey                             ; Y = 0
        beq     tsc2_bump_zp            ; always; carry set: output += 2

        ; --- LZ match ------------------------------------------------------
tsc2_lz:
        lsr                             ; carry: short (1) or long (0) offset
        sta     tsc2_lzto+1             ; length - 1
        iny
        lda     X16_P2
        bcc     tsc2_long
        sbc     (X16_P0),y              ; carry set: back = output - offset
        sta     X16_T6
        lda     X16_P3
        sbc     #$00
        ldx     #2
tsc2_lz_put:
        sta     X16_T7
        ldy     #0
        lda     (X16_T6),y              ; matches MUST copy forward
        sta     (X16_P2),y
tsc2_lz_loop:
        iny
        lda     (X16_T6),y
        sta     (X16_P2),y
tsc2_lzto:
        cpy     #0                      ; operand = length - 1 (self-modified)
        bne     tsc2_lz_loop
        tya
        ldy     #0
        ; llvm-mos PORT NOTE: upstream branches with `bcs` (always taken:
        ; the cpy equality above set the carry). This build's X16_P/T
        ; accesses assemble to 3-byte absolute forms, which pushes
        ; tsc2_bump_zp beyond the -128 branch range -- and llvm-mos's
        ; linker WRAPS an overflowing 8-bit displacement silently. jmp
        ; has the range and preserves the load-bearing carry.
        jmp     tsc2_bump_zp            ; carry still set: output += len+1

        ; --- the one-token zero run ----------------------------------------
tsc2_optrun:
tscrunch_optlen:
        ldy     #255                    ; operand = the stream's header byte
        sty     X16_T5
        ldx     #1                      ; A = 0: a run of zeros
        bne     tsc2_run_start

        ; --- long LZ: 15-bit offset, one more length bit --------------------
tsc2_long:
        adc     (X16_P0),y              ; carry clear, compensated by the encoder
        sta     X16_T6
        iny
        lda     (X16_P0),y              ; (LAX)
        tax
        ora     #$80
        adc     X16_P3                  ; the low add's carry ripples in here
        cpx     #$80                    ; offset bit 15 doubles as a length bit
        rol     tsc2_lzto+1
        ldx     #3
        bne     tsc2_lz_put             ; always
