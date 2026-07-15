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

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers. NOTE r0..r5 here are vbcc's pseudo-registers,
; the SAME physical $02..$07 the KERNAL's own r0/r1/r2 occupy. Each shim
; copies its arguments into the P block FIRST, so the internal routine can
; then stuff KERNAL args into $02.. without racing the incoming values.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3
        zpage	r4
        zpage	r5

        global	_x16_mem_fill
        global	_x16_mem_copy
        global	_x16_mem_crc
        global	_x16_mem_decompress

        section text

; ---------------------------------------------------------------------
; void x16_mem_fill(__reg("r0/r1") void *dst, __reg("r2/r3") unsigned int count,
;                   __reg("r4") unsigned char value)
;
; A target in $9F00-$9FFF is written repeatedly without incrementing: to
; fill VRAM, point port 0 first and pass X16_VERA_DATA0 as the target.
; ---------------------------------------------------------------------
_x16_mem_fill:
        lda     r0
        sta     X16_P0                  ; dst
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; count
        lda     r3
        sta     X16_P3
        lda     r4                      ; A = value
        jmp     mem_fill

; ---------------------------------------------------------------------
; void x16_mem_copy(__reg("r0/r1") const void *src, __reg("r2/r3") void *dst,
;                   __reg("r4/r5") unsigned int count)
;
; The regions may overlap. Source or target in $9F00-$9FFF is not
; incremented, so this uploads to VRAM (dst = X16_VERA_DATA0), downloads
; from VRAM (src = X16_VERA_DATA0), or copies VRAM to VRAM (port to port).
; ---------------------------------------------------------------------
_x16_mem_copy:
        lda     r0
        sta     X16_P0                  ; src
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; dst
        lda     r3
        sta     X16_P3
        lda     r4
        sta     X16_P4                  ; count
        lda     r5
        sta     X16_P5
        jmp     mem_copy

; ---------------------------------------------------------------------
; unsigned int x16_mem_crc(__reg("r0/r1") const void *addr,
;                          __reg("r2/r3") unsigned int count)
;   CRC-16/IBM-3740. An empty block gives the algorithm's init value, $FFFF.
; ---------------------------------------------------------------------
_x16_mem_crc:
        lda     r0
        sta     X16_P0                  ; addr
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; count
        lda     r3
        sta     X16_P3
        jmp     mem_crc                 ; already returns A = lo, X = hi

; ---------------------------------------------------------------------
; void *x16_mem_decompress(__reg("r0/r1") const void *src, __reg("r2/r3") void *dst)
;   Returns one past the last output byte.
;
; Compress with:  lzsa -r -f2 <original> <compressed>   (raw LZSA2 block,
; no frame header). Cannot decompress in place.
; ---------------------------------------------------------------------
_x16_mem_decompress:
        lda     r0
        sta     X16_P0                  ; src
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; dst
        lda     r3
        sta     X16_P3
        jmp     mem_decompress          ; already returns A = lo, X = hi

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; mem_fill -- in: X16_P0/P1 = target, X16_P2/P3 = count, A = value
; ---------------------------------------------------------------------
mem_fill:
        ldx     X16_P2                  ; a zero count fills nothing
        bne     .go
        ldx     X16_P3
        beq     .done
.go:
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
.done:
        rts

; ---------------------------------------------------------------------
; mem_copy -- in: X16_P0/P1 = source, X16_P2/P3 = target,
;                 X16_P4/P5 = byte count
; ---------------------------------------------------------------------
mem_copy:
        lda     X16_P4                  ; a zero count copies nothing
        ora     X16_P5
        beq     .done
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
.done:
        rts

; ---------------------------------------------------------------------
; mem_crc -- in: X16_P0/P1 = address, X16_P2/P3 = count
;            out: A = CRC low, X = CRC high
; ---------------------------------------------------------------------
mem_crc:
        lda     X16_P2
        ora     X16_P3
        bne     .go
        lda     #$FF                    ; empty block: the $FFFF init value
        tax
        rts
.go:
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
