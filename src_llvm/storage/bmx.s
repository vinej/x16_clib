; =====================================================================
; x16clib :: storage/bmx.s -- the X16's native bitmap file format
; =====================================================================
; BMX version 1 (the format Prog8 and the community tools write):
;
;   offset  size  field
;   0-2     3     magic "BMX"
;   3       1     version (1)
;   4       1     bits per pixel (1/2/4/8)
;   5       1     VERA colour depth code (0-3; log2 of the bpp)
;   6-7     2     width in pixels, little-endian
;   8-9     2     height
;   10      1     palette entries (0 means 256)
;   11      1     first palette index
;   12-13   2     file offset of the pixel data
;   14      1     compression (0 = none; nothing else is supported)
;   15      1     border colour
;
; The palette follows the header (2 bytes per entry, GB then R -- VERA's
; own layout), the pixel data follows the palette.
;
; Rows are written to VRAM bmx_stride bytes apart (default 320, the
; full-screen bitmap stride) -- so a 320-wide image is a plain contiguous
; load, and a narrower one lands as a "stamp" with the surrounding pixels
; untouched. bmx_save reads rows the same way.
;
; CAVEAT for bmx_save: the palette region of VRAM reads back the last
; value the HOST wrote, so the palette saved is only meaningful if this
; program set those entries itself (x16_pal_set / x16_pal_load / a
; previous x16_bmx_load).
;
; ---------------------------------------------------------------------
; ca65 -t cx16 translates character literals to PETSCII, so `cmp #'B'`
; would compare against $C2 and no BMX file would ever validate. The
; magic bytes are written as their explicit values. See storage/dos.s.
; ---------------------------------------------------------------------

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   POINTERS take aligned __rc PAIRS -- __rc2/__rc3, __rc4/__rc5, ... in
;   order, skipping any pair already partly used.
;   INTEGER bytes take A, then X, then single __rc bytes from __rc2 up,
;   skipping any byte a pointer already claimed.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_bmx_load
        .globl  x16_bmx_save
        .globl  x16_bmx_get_info
        .globl  x16_bmx_set_info

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
; unsigned char __fastcall__ x16_bmx_load(const char *name, unsigned char len,
;                                         unsigned char device,
;                                         unsigned long vaddr)
;   0 on success, else an X16_BMX_ERR_* code.
;
; vaddr goes last so cc65 hands all four bytes over in registers, as with
; x16_vera_addr0(). Bit 16 picks the VRAM bank.
; ---------------------------------------------------------------------
x16_bmx_load:
        jsr     file_marshal
        jsr     bmx_load                ; carry set = failure, A = code
        bcs     .Lx16_bmx_load_err
        lda     #0
.Lx16_bmx_load_err:
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_bmx_save(const char *name, unsigned char len,
;                                         unsigned char device,
;                                         unsigned long vaddr)
;   Describe the image with x16_bmx_set_info() first.
; ---------------------------------------------------------------------
x16_bmx_save:
        jsr     file_marshal
        jsr     bmx_save
        bcs     .Lx16_bmx_save_err
        lda     #0
.Lx16_bmx_save_err:
        rts

; in:  name -> __rc2/__rc3 (a pointer, so it takes the first pair);
;      len -> A, device -> X, vaddr -> __rc4..__rc7
; out: X16_P0/P1 = name, P2 = len, P3 = device, P4 = bank, P5/P6 = address
file_marshal:
        sta     X16_P2                  ; name length
        stx     X16_P3                  ; device
        lda     __rc2
        sta     X16_P0                  ; name
        lda     __rc3
        sta     X16_P1
        lda     __rc4
        sta     X16_P5                  ; vaddr bits 0-7
        lda     __rc5
        sta     X16_P6                  ; vaddr bits 8-15
        lda     __rc6
        and     #$01
        sta     X16_P4                  ; vaddr bit 16 -> VRAM bank
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_bmx_get_info(x16_bmx_info *out)
; void __fastcall__ x16_bmx_set_info(const x16_bmx_info *in)
;
; x16_bmx_info is { unsigned int width, height; unsigned char bpp,
; palstart; unsigned int palcount; unsigned char border; unsigned int
; stride; } -- eleven bytes (2+2+1+1+2+1+2), matching bmx_width..
; bmx_stride byte for byte. Keep the struct and those directives in step.
; ---------------------------------------------------------------------
BMX_INFO_SIZE = 11

; `out` is a pointer -> __rc2/__rc3. bmx.s uses no T bytes, so T0/T1 is
; free to indirect through.
x16_bmx_get_info:
        lda     __rc2
        sta     X16_T0
        lda     __rc3
        sta     X16_T1
        ldy     #BMX_INFO_SIZE - 1
.Lx16_bmx_get_info_out:
        lda     bmx_width,y
        sta     (X16_T0),y
        dey
        bpl     .Lx16_bmx_get_info_out
        rts

x16_bmx_set_info:
        lda     __rc2
        sta     X16_T0
        lda     __rc3
        sta     X16_T1
        ldy     #BMX_INFO_SIZE - 1
.Lx16_bmx_set_info_in:
        lda     (X16_T0),y
        sta     bmx_width,y
        dey
        bpl     .Lx16_bmx_set_info_in
        rts

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; bmx_load -- palette into the VERA palette, pixels into VRAM
;   in:  X16_P0/P1 = filename, X16_P2 = length, X16_P3 = device
;        X16_P4 = VRAM bank (0/1), X16_P5/P6 = VRAM address
;   out: carry clear on success, set with A = BMX_ERR_* on failure
; ---------------------------------------------------------------------
bmx_load:
        jsr     open_read
        bcc     .Lbmx_load_hdr
        lda     #BMX_ERR_IO
        rts
.Lbmx_load_hdr:
        ldx     #0                      ; pull in the 16-byte header
.Lbmx_load_get_hdr:
        jsr     CHRIN
        sta     bmx_hdr,x
        inx
        cpx     #16
        bne     .Lbmx_load_get_hdr

        ; OPEN and CHKIN both succeed for a file that does not exist: CBM
        ; DOS reports "62,FILE NOT FOUND" on the command channel, and the
        ; KERNAL only surfaces it in ST once a read has been attempted. So
        ; without this the 16 CHRINs above return junk, the magic check
        ; fails, and a missing file is reported as "not a BMX". ST is also
        ; nonzero for a header shorter than 16 bytes -- likewise an I/O
        ; error, not a format error. A real BMX has palette and pixels
        ; after byte 16, so EOF cannot legitimately be set here.
        jsr     READST
        beq     .Lbmx_load_validate
        lda     #BMX_ERR_IO
        bra     .Lbmx_load_close_err

.Lbmx_load_validate:
        lda     bmx_hdr                 ; validate
        cmp     #CH_B
        bne     .Lbmx_load_bad_fmt
        lda     bmx_hdr+1
        cmp     #CH_M
        bne     .Lbmx_load_bad_fmt
        lda     bmx_hdr+2
        cmp     #CH_X
        bne     .Lbmx_load_bad_fmt
        lda     bmx_hdr+3
        cmp     #1
        bne     .Lbmx_load_bad_fmt
        lda     bmx_hdr+14
        beq     .Lbmx_load_fmt_ok
        lda     #BMX_ERR_PACKED
        bra     .Lbmx_load_close_err
.Lbmx_load_bad_fmt:
        lda     #BMX_ERR_FORMAT
.Lbmx_load_close_err:
        pha
        jsr     close_file
        pla
        sec
        rts

.Lbmx_load_fmt_ok:
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
        bne     .Lbmx_load_pal_n
        inc     bmx_palcount+1          ; 0 in the file means 256
.Lbmx_load_pal_n:

        ; --- palette -> $1FA00 + palstart*2 -------------------------------
        lda     #VERA_CTRL_ADDRSEL
        trb     VERA_CTRL
        lda     bmx_palstart
        asl     a                       ; carry = address bit 8
        sta     VERA_ADDR_L
        lda     #>VRAM_PALETTE
        adc     #0
        sta     VERA_ADDR_M
        lda     #(VERA_ADDR_H_BANK | (VERA_INC_1 << 4))
        sta     VERA_ADDR_H

        lda     bmx_palcount            ; byte count = entries * 2
        sta     bmx_cnt
        lda     bmx_palcount+1
        sta     bmx_cnt+1
        asl     bmx_cnt
        rol     bmx_cnt+1
        jsr     bulk_read               ; MACPTR into DATA0; CHRIN fallback
        bcc     .Lbmx_load_pal_done
        jmp     .Lbmx_load_io_short
.Lbmx_load_pal_done:

        ; --- skip any gap up to the header's data offset -------------------
        ; expected position so far = 16 + palcount*2
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
        bcc     .Lbmx_load_data                   ; offset before position: trust the data
.Lbmx_load_skip:
        lda     bmx_cnt
        ora     bmx_cnt+1
        beq     .Lbmx_load_data
        jsr     CHRIN
        jsr     dec_cnt
        bra     .Lbmx_load_skip

.Lbmx_load_data:
        ; The header, the palette and any gap all came out of the file, so
        ; every pixel row must still be ahead of us. A nonzero ST here means
        ; the file ended somewhere in the palette or the gap. (EOF cannot be
        ; legitimate at this point unless the image has no rows at all, and
        ; a zero-height BMX describes nothing.)
        jsr     READST
        cmp     #0
        beq     .Lbmx_load_rows_ahead
        jmp     .Lbmx_load_io_short               ; @io_short is past the row loop: too
                                        ; far for a relative branch from here
.Lbmx_load_rows_ahead:

        ; --- pixel rows, bmx_stride apart ----------------------------------
        lda     X16_P5                  ; the walking VRAM address
        sta     bmx_cur
        lda     X16_P6
        sta     bmx_cur+1
        lda     X16_P4
        and     #$01
        sta     bmx_cur+2
        jsr     row_bytes               ; bmx_row = width >> (3 - depth)

        lda     bmx_height
        sta     bmx_rows
        lda     bmx_height+1
        sta     bmx_rows+1
.Lbmx_load_row:
        lda     bmx_rows
        ora     bmx_rows+1
        beq     .Lbmx_load_done
        jsr     point_cur               ; port 0 at bmx_cur, INC_1

        lda     bmx_row
        sta     bmx_cnt
        lda     bmx_row+1
        sta     bmx_cnt+1
        jsr     bulk_read               ; the whole row in MACPTR-sized gulps
        bcc     .Lbmx_load_row_done
        jmp     .Lbmx_load_io_short
.Lbmx_load_row_done:
        clc                             ; cur += stride (17-bit)
        lda     bmx_cur
        adc     bmx_stride
        sta     bmx_cur
        lda     bmx_cur+1
        adc     bmx_stride+1
        sta     bmx_cur+1
        lda     bmx_cur+2
        adc     #0
        and     #$01
        sta     bmx_cur+2
        lda     bmx_rows
        bne     .Lbmx_load_dec_rows
        dec     bmx_rows+1
.Lbmx_load_dec_rows:
        dec     bmx_rows

        ; ST is checked once per row, not once per byte: CHRIN is already the
        ; slow part, and a per-pixel READST would double it. Between rows the
        ; test is exact -- another row is expected, so any status at all (EOF
        ; included) means the file is shorter than its own header claims.
        ; After the LAST row EOF is not merely allowed but expected, since the
        ; final pixel is the final byte of the file.
        lda     bmx_rows
        ora     bmx_rows+1
        beq     .Lbmx_load_done
        jsr     READST
        cmp     #0
        beq     .Lbmx_load_row

.Lbmx_load_io_short:
        lda     #BMX_ERR_IO
        jmp     .Lbmx_load_close_err

.Lbmx_load_done:
        jsr     close_file
        clc
        rts

; ---------------------------------------------------------------------
; bmx_save -- write a BMX file from VRAM
;   in:  X16_P0/P1 = filename, X16_P2 = length, X16_P3 = device
;        X16_P4 = VRAM bank, X16_P5/P6 = VRAM address of the image
;        bmx_width/height/bpp/palstart/palcount/border/stride describe
;        what to save (bpp 8 and stride 320 are the defaults)
;   out: carry clear on success, set with A = BMX_ERR_IO on failure
; ---------------------------------------------------------------------
bmx_save:
        jsr     open_write
        bcc     .Lbmx_save_wr_hdr
        lda     #BMX_ERR_IO
        rts
.Lbmx_save_wr_hdr:
        lda     #CH_B
        sta     bmx_hdr
        lda     #CH_M
        sta     bmx_hdr+1
        lda     #CH_X
        sta     bmx_hdr+2
        lda     #1
        sta     bmx_hdr+3
        lda     bmx_bpp
        sta     bmx_hdr+4
        jsr     depth_code
        sta     bmx_hdr+5
        lda     bmx_width
        sta     bmx_hdr+6
        lda     bmx_width+1
        sta     bmx_hdr+7
        lda     bmx_height
        sta     bmx_hdr+8
        lda     bmx_height+1
        sta     bmx_hdr+9
        lda     bmx_palcount            ; 256 stores as 0
        sta     bmx_hdr+10
        lda     bmx_palstart
        sta     bmx_hdr+11
        lda     bmx_palcount            ; data offset = 16 + palcount*2
        sta     bmx_cnt
        lda     bmx_palcount+1
        sta     bmx_cnt+1
        asl     bmx_cnt
        rol     bmx_cnt+1
        clc
        lda     bmx_cnt
        adc     #16
        sta     bmx_hdr+12
        lda     bmx_cnt+1
        adc     #0
        sta     bmx_hdr+13
        stz     bmx_hdr+14              ; uncompressed
        lda     bmx_border
        sta     bmx_hdr+15

        ldx     #0
.Lbmx_save_hdr_out:
        lda     bmx_hdr,x
        jsr     CHROUT
        inx
        cpx     #16
        bne     .Lbmx_save_hdr_out

        ; --- palette from the VRAM shadow ----------------------------------
        lda     #VERA_CTRL_ADDRSEL
        tsb     VERA_CTRL               ; port 1 reads, so CHROUT stays safe
        lda     bmx_palstart
        asl     a
        sta     VERA_ADDR_L
        lda     #>VRAM_PALETTE
        adc     #0
        sta     VERA_ADDR_M
        lda     #(VERA_ADDR_H_BANK | (VERA_INC_1 << 4))
        sta     VERA_ADDR_H
        lda     #VERA_CTRL_ADDRSEL
        trb     VERA_CTRL

        lda     bmx_palcount
        sta     bmx_cnt
        lda     bmx_palcount+1
        sta     bmx_cnt+1
        asl     bmx_cnt
        rol     bmx_cnt+1
.Lbmx_save_pal_out:
        lda     bmx_cnt
        ora     bmx_cnt+1
        beq     .Lbmx_save_pal_wrote
        lda     VERA_DATA1
        jsr     CHROUT
        jsr     dec_cnt
        bra     .Lbmx_save_pal_out
.Lbmx_save_pal_wrote:

        ; --- pixel rows -----------------------------------------------------
        lda     X16_P5
        sta     bmx_cur
        lda     X16_P6
        sta     bmx_cur+1
        lda     X16_P4
        and     #$01
        sta     bmx_cur+2
        jsr     row_bytes

        lda     bmx_height
        sta     bmx_rows
        lda     bmx_height+1
        sta     bmx_rows+1
.Lbmx_save_wrow:
        lda     bmx_rows
        ora     bmx_rows+1
        beq     .Lbmx_save_wdone
        jsr     point_cur1              ; port 1 at bmx_cur

        lda     bmx_row
        sta     bmx_cnt
        lda     bmx_row+1
        sta     bmx_cnt+1
.Lbmx_save_wpix:
        lda     bmx_cnt
        ora     bmx_cnt+1
        beq     .Lbmx_save_wrow_done
        lda     VERA_DATA1
        jsr     CHROUT
        jsr     dec_cnt
        bra     .Lbmx_save_wpix
.Lbmx_save_wrow_done:
        clc
        lda     bmx_cur
        adc     bmx_stride
        sta     bmx_cur
        lda     bmx_cur+1
        adc     bmx_stride+1
        sta     bmx_cur+1
        lda     bmx_cur+2
        adc     #0
        and     #$01
        sta     bmx_cur+2
        lda     bmx_rows
        bne     .Lbmx_save_wdec
        dec     bmx_rows+1
.Lbmx_save_wdec:
        dec     bmx_rows
        bra     .Lbmx_save_wrow

.Lbmx_save_wdone:
        jsr     close_file
        clc
        rts

; --- plumbing ---------------------------------------------------------

open_read:
        lda     X16_P2
        ldx     X16_P0
        ldy     X16_P1
        jsr     SETNAM
        lda     #2
        ldx     X16_P3
        ldy     #0                      ; sequential read, no header games
        jsr     SETLFS
        jsr     OPEN
        bcs     open_bad
        ldx     #2
        jsr     CHKIN
        bcs     open_bad
        clc
        rts

open_write:
        lda     X16_P2
        ldx     X16_P0
        ldy     X16_P1
        jsr     SETNAM
        lda     #2
        ldx     X16_P3
        ldy     #1                      ; write
        jsr     SETLFS
        jsr     OPEN
        bcs     open_bad
        ldx     #2
        jsr     CHKOUT
        bcs     open_bad
        clc
        rts

open_bad:
        jsr     CLRCHN
        lda     #2
        jsr     CLOSE
        sec
        rts

; The ACME original had two labels, .close_read and .close_write, on this
; one routine; reads and writes close the same way.
close_file:
        jsr     CLRCHN
        lda     #2
        jsr     CLOSE
        rts

; bmx_row = bmx_width >> (3 - depth): the bytes in one row of pixels
row_bytes:
        lda     bmx_width
        sta     bmx_row
        lda     bmx_width+1
        sta     bmx_row+1
        jsr     depth_code
        eor     #$03                    ; 3 - depth (depth is 0-3)
        tax
        beq     .Lrow_bytes_rb_done
.Lrow_bytes_rb_shift:
        lsr     bmx_row+1
        ror     bmx_row
        dex
        bne     .Lrow_bytes_rb_shift
.Lrow_bytes_rb_done:
        rts

; A = the VERA depth code for bmx_bpp (8->3, 4->2, 2->1, 1->0)
depth_code:
        lda     bmx_bpp
        cmp     #8
        beq     .Ldepth_code_dc8
        cmp     #4
        beq     .Ldepth_code_dc4
        cmp     #2
        beq     .Ldepth_code_dc2
        lda     #0
        rts
.Ldepth_code_dc8:
        lda     #3
        rts
.Ldepth_code_dc4:
        lda     #2
        rts
.Ldepth_code_dc2:
        lda     #1
        rts

point_cur:                              ; port 0 at bmx_cur, INC_1
        lda     #VERA_CTRL_ADDRSEL
        trb     VERA_CTRL
        lda     bmx_cur
        sta     VERA_ADDR_L
        lda     bmx_cur+1
        sta     VERA_ADDR_M
        lda     bmx_cur+2
        ora     #(VERA_INC_1 << 4)
        sta     VERA_ADDR_H
        rts

point_cur1:                             ; port 1 at bmx_cur, INC_1
        lda     #VERA_CTRL_ADDRSEL
        tsb     VERA_CTRL
        lda     bmx_cur
        sta     VERA_ADDR_L
        lda     bmx_cur+1
        sta     VERA_ADDR_M
        lda     bmx_cur+2
        ora     #(VERA_INC_1 << 4)
        sta     VERA_ADDR_H
        lda     #VERA_CTRL_ADDRSEL      ; leave ADDRSEL alone for the KERNAL
        trb     VERA_CTRL
        rts

dec_cnt:
        lda     bmx_cnt
        bne     .Ldec_cnt_dc_lo
        dec     bmx_cnt+1
.Ldec_cnt_dc_lo:
        dec     bmx_cnt
        rts

; ---------------------------------------------------------------------
; bulk_read -- pull bmx_cnt bytes from the open file into VERA_DATA0.
;
; MACPTR hands the whole block to the device driver, which streams it to
; a FIXED destination when carry is set on entry -- exactly what a VERA
; data port is. That replaces one CHRIN per byte. Not every device can do
; block reads (MACPTR returns carry set), so the CHRIN loop stays as the
; fallback path.
;
; A device may deliver fewer bytes than asked for, which is normal; zero
; bytes is not, and means the file ran out. That short-read return is a
; second, independent truncation signal alongside bmx_load's READST
; checks -- the fallback path has no equivalent, which is why those
; checks are not redundant either.
;
;   in:  bmx_cnt = byte count, port 0 pointed at the destination
;   out: carry clear on success; carry set if the file ran out early
;        bmx_cnt = 0, A/X/Y clobbered
; ---------------------------------------------------------------------
bulk_read:
.Lbulk_read_more:
        lda     bmx_cnt
        ora     bmx_cnt+1
        beq     .Lbulk_read_br_ok
        lda     bmx_cnt+1
        beq     .Lbulk_read_small
        lda     #255                    ; a big remainder: largest single ask
        bra     .Lbulk_read_ask
.Lbulk_read_small:
        lda     bmx_cnt                 ; the exact remainder
.Lbulk_read_ask:
        ldx     #<VERA_DATA0
        ldy     #>VERA_DATA0
        sec                             ; fixed destination: stream into VERA
        jsr     MACPTR
        bcs     .Lbulk_read_fallback               ; the device cannot do block reads
        txa                             ; X/Y = bytes actually delivered
        bne     .Lbulk_read_got
        tya                             ; the ask was <= 255, so Y is 0 and
        beq     .Lbulk_read_br_short               ; X = 0 means nothing came back
.Lbulk_read_got:
        stx     bmx_t                   ; bmx_cnt -= bytes read
        sec
        lda     bmx_cnt
        sbc     bmx_t
        sta     bmx_cnt
        sty     bmx_t
        lda     bmx_cnt+1
        sbc     bmx_t
        sta     bmx_cnt+1
        bra     .Lbulk_read_more
.Lbulk_read_br_ok:
        clc
        rts
.Lbulk_read_br_short:
        sec
        rts
.Lbulk_read_fallback:
        lda     bmx_cnt
        ora     bmx_cnt+1
        beq     .Lbulk_read_br_ok
        jsr     CHRIN
        sta     VERA_DATA0
        jsr     dec_cnt
        bra     .Lbulk_read_fallback

; ---------------------------------------------------------------------
; The image description. Three of these have nonzero defaults, so the
; whole block lives in DATA -- and it must stay contiguous and in this
; order: x16_bmx_get_info/set_info block-copy an x16_bmx_info onto it.
; ---------------------------------------------------------------------
        .section .data,"aw",@progbits

bmx_width:    .word 0
bmx_height:   .word 0
bmx_bpp:      .byte 8
bmx_palstart: .byte 0
bmx_palcount: .word 256                 ; 1-256 entries
bmx_border:   .byte 0
bmx_stride:   .word 320                 ; VRAM bytes between row starts

        .section .bss,"aw",@nobits

bmx_hdr:  .zero  16
bmx_cnt:  .zero  2
bmx_cur:  .zero  3
bmx_row:  .zero  2
bmx_rows: .zero  2
bmx_t:    .zero  1                        ; bulk_read's subtrahend
