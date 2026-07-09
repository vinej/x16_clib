; =====================================================================
; x16clib :: storage/mem.s -- KERNAL block memory operations
; =====================================================================
; Thin wrappers over the KERNAL's block routines -- MEMORY_FILL,
; MEMORY_COPY, MEMORY_CRC and MEMORY_DECOMPRESS. These live in the
; $FExx jump table, so no bank switching is needed.
;
; ONE PROPERTY MAKES THESE SPECIAL: addresses in $9F00-$9FFF are NOT
; incremented during the operation. Point a VERA data port somewhere and
; pass $9F23 (VERA_DATA0) as the source or target, and these routines
; stream straight into or out of VRAM at the port's own increment.
; mem_decompress with target VERA_DATA0 unpacks assets directly into
; video memory -- no staging buffer.
;
; All four take a 16-bit byte count; the KERNAL's virtual registers
; r0-r2 are used for arguments and are treated as caller-save, exactly
; like everywhere else in this library. They sit at $02-$21, below
; cc65's zero-page window, so nothing of cc65's is at risk.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa, popax

        .export         _x16_mem_fill
        .export         _x16_mem_copy
        .export         _x16_mem_crc
        .export         _x16_mem_decompress

        .segment        "CODE"

; ---------------------------------------------------------------------
; void __fastcall__ x16_mem_fill(void *dst, unsigned int count,
;                                unsigned char value)
;
; A target in $9F00-$9FFF is written repeatedly without incrementing:
; to fill VRAM, point port 0 first and pass X16_VERA_DATA0 as the target.
; ---------------------------------------------------------------------
_x16_mem_fill:
        pha                             ; value (rightmost arg, in A)
        jsr     popax
        sta     X16_P2                  ; count
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; dst
        stx     X16_P1
        pla
        jmp     mem_fill

; ---------------------------------------------------------------------
; void __fastcall__ x16_mem_copy(const void *src, void *dst,
;                                unsigned int count)
;
; The regions may overlap. Source or target in $9F00-$9FFF is not
; incremented, so this uploads to VRAM (dst = X16_VERA_DATA0), downloads
; from VRAM (src = X16_VERA_DATA0), or copies VRAM to VRAM (port to port).
; ---------------------------------------------------------------------
_x16_mem_copy:
        sta     X16_P4                  ; count (rightmost arg: A/X)
        stx     X16_P5
        jsr     popax
        sta     X16_P2                  ; dst
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; src
        stx     X16_P1
        jmp     mem_copy

; ---------------------------------------------------------------------
; unsigned int __fastcall__ x16_mem_crc(const void *addr, unsigned int count)
;   CRC-16/IBM-3740. An empty block gives the algorithm's init value, $FFFF.
; ---------------------------------------------------------------------
_x16_mem_crc:
        sta     X16_P2                  ; count (rightmost arg: A/X)
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; addr
        stx     X16_P1
        jmp     mem_crc                 ; already returns A = lo, X = hi

; ---------------------------------------------------------------------
; void * __fastcall__ x16_mem_decompress(const void *src, void *dst)
;   Returns one past the last output byte.
;
; Compress with:  lzsa -r -f2 <original> <compressed>   (raw LZSA2 block,
; no frame header).
;
; Cannot decompress in place. The input may sit in banked RAM (map the
; bank yourself; 8 KB limit). A target in $9F00-$9FFF is not incremented:
; point port 0 at VRAM and pass X16_VERA_DATA0 to unpack assets straight
; into video memory.
; ---------------------------------------------------------------------
_x16_mem_decompress:
        sta     X16_P2                  ; dst (rightmost arg: A/X)
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; src
        stx     X16_P1
        jmp     mem_decompress          ; already returns A = lo, X = hi

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; mem_fill -- in: X16_P0/P1 = target, X16_P2/P3 = count, A = value
; ---------------------------------------------------------------------
mem_fill:
        ldx     X16_P2                  ; a zero count fills nothing
        bne     @go
        ldx     X16_P3
        beq     @done
@go:
        pha
        lda     X16_P0
        sta     r0L
        lda     X16_P1
        sta     r0H
        lda     X16_P2
        sta     r1L
        lda     X16_P3
        sta     r1H
        pla
        jmp     MEMORY_FILL
@done:
        rts

; ---------------------------------------------------------------------
; mem_copy -- in: X16_P0/P1 = source, X16_P2/P3 = target,
;                 X16_P4/P5 = byte count
; ---------------------------------------------------------------------
mem_copy:
        lda     X16_P4                  ; a zero count copies nothing
        ora     X16_P5
        beq     @done
        lda     X16_P0
        sta     r0L
        lda     X16_P1
        sta     r0H
        lda     X16_P2
        sta     r1L
        lda     X16_P3
        sta     r1H
        lda     X16_P4
        sta     r2L
        lda     X16_P5
        sta     r2H
        jmp     MEMORY_COPY
@done:
        rts

; ---------------------------------------------------------------------
; mem_crc -- in: X16_P0/P1 = address, X16_P2/P3 = count
;            out: A = CRC low, X = CRC high
; ---------------------------------------------------------------------
mem_crc:
        lda     X16_P2
        ora     X16_P3
        bne     @go
        lda     #$FF                    ; empty block: the $FFFF init value
        tax
        rts
@go:
        lda     X16_P0
        sta     r0L
        lda     X16_P1
        sta     r0H
        lda     X16_P2
        sta     r1L
        lda     X16_P3
        sta     r1H
        jsr     MEMORY_CRC
        lda     r2L
        ldx     r2H
        rts

; ---------------------------------------------------------------------
; mem_decompress -- in: X16_P0/P1 = compressed data, X16_P2/P3 = output
;                   out: A/X = address one past the last output byte
; ---------------------------------------------------------------------
mem_decompress:
        lda     X16_P0
        sta     r0L
        lda     X16_P1
        sta     r0H
        lda     X16_P2
        sta     r1L
        lda     X16_P3
        sta     r1H
        jsr     MEMORY_DECOMPRESS
        lda     r1L                     ; the KERNAL leaves r1 one past the end
        ldx     r1H
        rts
