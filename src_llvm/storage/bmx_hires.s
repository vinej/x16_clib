; =====================================================================
; x16clib :: storage/bmx_hires.s -- BMX into the VERA_2 SDRAM bitmap
; =====================================================================
; bmx_load's sibling for the MiSTer VERA_2 640x480 layer (the gfx8h
; engine): the palette goes to the VERA_2 palette registers, the pixel
; rows stream into VERA_2 SDRAM 640 bytes apart from offset 0, so a
; full-width 640x480x8 image is a plain contiguous load.
;
; A separate module, not part of storage/bmx.s: ld.lld links a member
; only when something references it, and a program that never loads into
; VERA_2 SDRAM should not carry this code. The shared plumbing --
; open/close, header buffer, counters, the error-code byte -- is
; imported from bmx.s under its bmx_* export names.
;
; Feature-detect with x16_gfx8h_has() first: on stock hardware (and the
; emulator) the file still parses but the pixel writes go to open bus.
;
; The routine bodies are the x16_library bmx_load_hires trio, byte for
; byte; only the C shim in front is new. Magic bytes are explicit values
; for the same PETSCII reason as in bmx.s.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; (imports resolved by name: bmx_open_read, bmx_close_file,
;  bmx_row_bytes, bmx_dec_cnt, bmx_code, bmx_hdr, bmx_cnt, bmx_cur,
;  bmx_row, bmx_rows, bmx_t, bmx_width, bmx_height, bmx_bpp,
;  bmx_palstart, bmx_palcount, bmx_border -- all from storage/bmx.s)

; llvm-mos argument placement, measured on the machine:
;   POINTERS take aligned __rc PAIRS -- __rc2/__rc3 first.
;   INTEGER bytes take A, then X. So (const char *name, unsigned char
;   device) is name -> __rc2/__rc3 and device -> A.
; Returns: char in A.

        .globl  x16_bmx_load_hires

CH_B = $42
CH_M = $4D
CH_X = $58

BMX_ERR_IO     = 1                      ; open/read/write failed
BMX_ERR_FORMAT = 2                      ; not a BMX, or not version 1
BMX_ERR_PACKED = 3                      ; compressed data is not supported

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_bmx_load_hires(const char *name, unsigned char device)
;   0 on success, else an X16_BMX_ERR_* code.
;
; Unlike x16_bmx_load there is no length argument: the name is an
; ordinary NUL-terminated C string, measured here (up to 255 bytes).
; The body reads no T bytes, so T0/T1 is free to indirect through.
; ---------------------------------------------------------------------
x16_bmx_load_hires:
        sta     mos8(X16_P3)            ; device (the only integer arg: A)
        lda     mos8(__rc2)
        sta     mos8(X16_P0)            ; name
        sta     mos8(X16_T0)
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        sta     mos8(X16_T1)
        ldy     #0
.Lx16_bmx_load_hires_len:
        lda     (X16_T0),y
        beq     .Lx16_bmx_load_hires_got
        iny
        bne     .Lx16_bmx_load_hires_len         ; a 255-byte name is its own problem
.Lx16_bmx_load_hires_got:
        sty     mos8(X16_P2)            ; length
        jsr     bmx_load_hires          ; carry set = failure, A = code
        bcs     .Lx16_bmx_load_hires_err
        lda     #0
.Lx16_bmx_load_hires_err:
        rts

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; bmx_load_hires -- load a BMX file into the VERA_2 640x480 SDRAM
;             bitmap (the gfx8h engine). Palette goes to the VERA_2
;             palette, pixels stream to VERA_2 SDRAM starting at offset 0.
;   in:  X16_P0/P1 = filename address, X16_P2 = length
;        X16_P3    = device (usually 8)
;   out: carry clear on success, set with A = BMX_ERR_* on failure
;        bmx_width/height/bpp/palstart/palcount/border reflect the file
;   note: select the hi-res 8bpp mode first (gfx8h_init). Rows land
;         BMX_HIRES_STRIDE (640) bytes apart from SDRAM offset 0, so a
;         full-width 640x480x8 image is a plain contiguous load. Unlike
;         bmx_load (which targets VERA VRAM through DATA0), this streams
;         into the MiSTer VERA_2 SDRAM address space through VERA2_DATA.
; ---------------------------------------------------------------------
BMX_HIRES_STRIDE = 640

bmx_load_hires:
        stz     bmx_code
        jsr     bmx_open_read
        bcc     .Lbmx_load_hires_hdr
        lda     #BMX_ERR_IO
        sta     bmx_code
        rts
.Lbmx_load_hires_hdr:
        ldx     #0                      ; pull in the 16-byte header
.Lbmx_load_hires_get_hdr:
        jsr     CHRIN
        sta     bmx_hdr,x
        inx
        cpx     #16
        bne     .Lbmx_load_hires_get_hdr

        jsr     READST                  ; a short/absent header is an I/O error
        beq     .Lbmx_load_hires_validate
        lda     #BMX_ERR_IO
        sta     bmx_code
        bra     .Lbmx_load_hires_close_err
.Lbmx_load_hires_validate:
        lda     bmx_hdr
        cmp     #CH_B
        bne     .Lbmx_load_hires_bad_fmt
        lda     bmx_hdr+1
        cmp     #CH_M
        bne     .Lbmx_load_hires_bad_fmt
        lda     bmx_hdr+2
        cmp     #CH_X
        bne     .Lbmx_load_hires_bad_fmt
        lda     bmx_hdr+3
        cmp     #1
        bne     .Lbmx_load_hires_bad_fmt
        lda     bmx_hdr+14
        beq     .Lbmx_load_hires_fmt_ok
        lda     #BMX_ERR_PACKED
        sta     bmx_code
        bra     .Lbmx_load_hires_close_err
.Lbmx_load_hires_bad_fmt:
        lda     #BMX_ERR_FORMAT
        sta     bmx_code
.Lbmx_load_hires_close_err:
        pha
        jsr     bmx_close_file
        pla
        sec
        rts

.Lbmx_load_hires_fmt_ok:
        lda     bmx_hdr+4               ; publish the header fields
        sta     bmx_bpp
        lda     bmx_hdr+6
        sta     bmx_width
        lda     bmx_hdr+7
        sta     bmx_width+1
        lda     bmx_hdr+8
        sta     bmx_height
        lda     bmx_hdr+9
        sta     bmx_height+1
        lda     bmx_hdr+11
        sta     bmx_palstart
        lda     bmx_hdr+15
        sta     bmx_border
        lda     bmx_hdr+10
        sta     bmx_palcount
        stz     bmx_palcount+1
        bne     .Lbmx_load_hires_pal_n
        inc     bmx_palcount+1          ; 0 in the file means 256
.Lbmx_load_hires_pal_n:

        ; --- palette -> the VERA_2 palette (IDX auto-increments) -----------
        lda     bmx_palstart
        sta     VERA2_PAL_IDX
        lda     bmx_palcount            ; entries remaining (16-bit)
        sta     bmx_cnt
        lda     bmx_palcount+1
        sta     bmx_cnt+1
.Lbmx_load_hires_pal_loop:
        lda     bmx_cnt
        ora     bmx_cnt+1
        beq     .Lbmx_load_hires_pal_done
        jsr     CHRIN
        sta     VERA2_PAL_LO
        jsr     CHRIN
        sta     VERA2_PAL_HI
        jsr     bmx_dec_cnt
        bra     .Lbmx_load_hires_pal_loop
.Lbmx_load_hires_pal_done:

        ; --- skip any gap up to the header's data offset -------------------
        lda     bmx_palcount
        sta     bmx_cnt
        lda     bmx_palcount+1
        sta     bmx_cnt+1
        asl     bmx_cnt
        rol     bmx_cnt+1
        clc
        lda     bmx_cnt
        adc     #16
        sta     bmx_cnt
        lda     bmx_cnt+1
        adc     #0
        sta     bmx_cnt+1
        sec                             ; gap = data offset - position
        lda     bmx_hdr+12
        sbc     bmx_cnt
        sta     bmx_cnt
        lda     bmx_hdr+13
        sbc     bmx_cnt+1
        sta     bmx_cnt+1
        bcc     .Lbmx_load_hires_data
.Lbmx_load_hires_skip:
        lda     bmx_cnt
        ora     bmx_cnt+1
        beq     .Lbmx_load_hires_data
        jsr     CHRIN
        jsr     bmx_dec_cnt
        bra     .Lbmx_load_hires_skip

.Lbmx_load_hires_data:
        jsr     READST
        cmp     #0
        beq     .Lbmx_load_hires_rows_ahead
        jmp     .Lbmx_load_hires_io_short
.Lbmx_load_hires_rows_ahead:

        ; --- pixel rows into VERA_2 SDRAM, BMX_HIRES_STRIDE apart -----------
        stz     bmx_cur                 ; SDRAM byte offset 0
        stz     bmx_cur+1
        stz     bmx_cur+2
        jsr     bmx_row_bytes           ; bmx_row = width >> (3 - depth)

        lda     bmx_height
        sta     bmx_rows
        lda     bmx_height+1
        sta     bmx_rows+1
.Lbmx_load_hires_row:
        lda     bmx_rows
        ora     bmx_rows+1
        beq     .Lbmx_load_hires_done
        jsr     bmx_point_cur2          ; VERA_2 addr = bmx_cur, INC_1

        lda     bmx_row
        sta     bmx_cnt
        lda     bmx_row+1
        sta     bmx_cnt+1
        jsr     bmx_bulk_read2          ; the whole row, MACPTR into VERA2_DATA
        bcc     .Lbmx_load_hires_row_done
        jmp     .Lbmx_load_hires_io_short
.Lbmx_load_hires_row_done:
        clc                             ; cur += 640 (20-bit)
        lda     bmx_cur
        adc     #<BMX_HIRES_STRIDE
        sta     bmx_cur
        lda     bmx_cur+1
        adc     #>BMX_HIRES_STRIDE
        sta     bmx_cur+1
        lda     bmx_cur+2
        adc     #0
        sta     bmx_cur+2
        lda     bmx_rows
        bne     .Lbmx_load_hires_dec_rows
        dec     bmx_rows+1
.Lbmx_load_hires_dec_rows:
        dec     bmx_rows

        lda     bmx_rows
        ora     bmx_rows+1
        beq     .Lbmx_load_hires_done
        jsr     READST
        cmp     #0
        beq     .Lbmx_load_hires_row

.Lbmx_load_hires_io_short:
        lda     #BMX_ERR_IO
        sta     bmx_code
        jmp     .Lbmx_load_hires_close_err

.Lbmx_load_hires_done:
        jsr     bmx_close_file
        clc
        rts

; VERA_2 SDRAM address <- bmx_cur, auto-increment by 1
bmx_point_cur2:
        lda     bmx_cur
        sta     VERA2_ADDR_L
        lda     bmx_cur+1
        sta     VERA2_ADDR_M
        lda     bmx_cur+2
        and     #$0F
        ora     #(VERA2_INC_1 << 4)     ; VERA2_INC_1 code is 0: high nibble stays
        sta     VERA2_ADDR_H
        rts

; read bmx_cnt bytes from the open channel into VERA2_DATA (the SDRAM
; port is already aimed). Same MACPTR streaming trick as bmx.s's
; bulk_read, but the fixed destination is the VERA_2 data register.
;   out: carry clear = done; carry set = the stream died mid-read
bmx_bulk_read2:
.Lbmx_bulk_read2_more:
        lda     bmx_cnt
        ora     bmx_cnt+1
        beq     .Lbmx_bulk_read2_br_ok
        lda     bmx_cnt+1
        beq     .Lbmx_bulk_read2_small
        lda     #255
        bra     .Lbmx_bulk_read2_ask
.Lbmx_bulk_read2_small:
        lda     bmx_cnt
.Lbmx_bulk_read2_ask:
        ldx     #<VERA2_DATA
        ldy     #>VERA2_DATA
        sec                             ; fixed destination: stream into VERA_2
        jsr     MACPTR
        bcs     .Lbmx_bulk_read2_fallback
        txa
        bne     .Lbmx_bulk_read2_got
        tya
        beq     .Lbmx_bulk_read2_br_short
.Lbmx_bulk_read2_got:
        stx     bmx_t
        sec
        lda     bmx_cnt
        sbc     bmx_t
        sta     bmx_cnt
        sty     bmx_t
        lda     bmx_cnt+1
        sbc     bmx_t
        sta     bmx_cnt+1
        bra     .Lbmx_bulk_read2_more
.Lbmx_bulk_read2_br_ok:
        clc
        rts
.Lbmx_bulk_read2_br_short:
        sec
        rts
.Lbmx_bulk_read2_fallback:
        lda     bmx_cnt
        ora     bmx_cnt+1
        beq     .Lbmx_bulk_read2_br_ok
        jsr     CHRIN
        sta     VERA2_DATA
        jsr     bmx_dec_cnt
        bra     .Lbmx_bulk_read2_fallback
