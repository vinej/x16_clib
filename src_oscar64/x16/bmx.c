// =====================================================================
// x16clib :: x16/bmx.c -- the X16's native bitmap file format
// =====================================================================
// BMX version 1 (the format Prog8 and the community tools write). Same
// code as src_ca65/storage/bmx.s; see that file (or the header) for the
// format table. Rows are written to VRAM x16__bmx stride bytes apart,
// so a 320-wide image is a plain contiguous load and a narrower one
// lands as a "stamp".
//
// The magic bytes are explicit hex ($42 $4D $58 = "BMX"): cc65's cx16
// charmap silently PETSCII-ifies character literals, and the bytes stay
// explicit in every port so all four match.
//
// The ca65 build's shared helpers (open, close, row_bytes, depth_code,
// point_cur, dec_cnt, bulk_read) are local labels inside each entry's
// asm body here -- inline asm cannot jsr across functions, so load and
// save each carry their own copies.
// =====================================================================

#include <x16/bmx.h>

// The image description, one contiguous block: get/set_info block-copy
// an x16_bmx_info onto it, so the order is load-bearing:
//   +0 width  +2 height  +4 bpp  +5 palstart  +6 palcount  +8 border
//   +9 stride
// Three fields have nonzero defaults: bpp 8, palcount 256, stride 320.
volatile char x16__bmx[11] = { 0, 0, 0, 0, 8, 0, 0, 1, 0, 0x40, 0x01 };

// The info block copies indirect straight through the struct
// parameters -- Oscar64 keeps pointer parameters in zero page.

volatile char x16__bmx_hdr[16];
volatile char x16__bmx_cnt[2];
volatile char x16__bmx_cur[3];
volatile char x16__bmx_row[2];
volatile char x16__bmx_rows[2];
volatile char x16__bmx_t;

void x16_bmx_get_info(x16_bmx_info *out) {
    __asm {
        ldy #10                         /* BMX_INFO_SIZE - 1 */
    bgi_out:
        lda x16__bmx,y
        sta (out),y
        dey
        bpl bgi_out
    }
}

void x16_bmx_set_info(const x16_bmx_info *in) {
    __asm {
        ldy #10
    bsi_in:
        lda (in),y
        sta x16__bmx,y
        dey
        bpl bsi_in
    }
}

// ---------------------------------------------------------------------
// Load: palette into the VERA palette, pixels into VRAM at vaddr (bit
// 16 = bank), rows stride apart. 0 on success, else X16_BMX_ERR_*.
// ---------------------------------------------------------------------
unsigned char x16_bmx_load(const char *name, unsigned char len,
                           unsigned char device,
                           unsigned long vaddr) {
    /* --- open for sequential read ------------------------------- */
    return __asm {
        lda len
        ldx name
        ldy name+1
        jsr 0xffbd                      /* SETNAM */
        lda #2
        ldx device
        ldy #0                          /* sequential, no header games */
        jsr 0xffba                      /* SETLFS */
        jsr 0xffc0                      /* OPEN */
        bcs bl_open_bad
        ldx #2
        jsr 0xffc6                      /* CHKIN */
        bcc bl_hdr
    bl_open_bad:
        jsr 0xffcc                      /* CLRCHN */
        lda #2
        jsr 0xffc3                      /* CLOSE */
        lda #1                          /* X16_BMX_ERR_IO */
        sta accu
        jmp bl_end

    bl_hdr:
        ldx #0                          /* pull in the 16-byte header */
    bl_get_hdr:
        jsr 0xffcf                      /* CHRIN */
        sta x16__bmx_hdr,x
        inx
        cpx #16
        bne bl_get_hdr

        /* OPEN and CHKIN both succeed for a missing file: DOS reports */
        /* "62,FILE NOT FOUND" on the command channel and the KERNAL */
        /* only surfaces it in ST once a read has been attempted. So */
        /* without this, the 16 CHRINs return junk and a missing file */
        /* reads as "not a BMX". A real BMX has palette and pixels after */
        /* byte 16, so EOF cannot legitimately be set here either. */
        jsr 0xffb7                      /* READST */
        beq bl_validate
        lda #1                          /* X16_BMX_ERR_IO */
        jmp bl_close_err

    bl_validate:
        lda x16__bmx_hdr
        cmp #0x42                        /* 'B' */
        bne bl_bad_fmt
        lda x16__bmx_hdr+1
        cmp #0x4d                        /* 'M' */
        bne bl_bad_fmt
        lda x16__bmx_hdr+2
        cmp #0x58                        /* 'X' */
        bne bl_bad_fmt
        lda x16__bmx_hdr+3
        cmp #1
        bne bl_bad_fmt
        lda x16__bmx_hdr+14
        beq bl_fmt_ok
        lda #3                          /* X16_BMX_ERR_PACKED */
        jmp bl_close_err
    bl_bad_fmt:
        lda #2                          /* X16_BMX_ERR_FORMAT */
    bl_close_err:
        pha
        jsr bl_close
        pla
        sta accu
        jmp bl_end

    bl_fmt_ok:
        lda x16__bmx_hdr+4              /* publish the header fields */
        sta x16__bmx+4                  /* bpp */
        lda x16__bmx_hdr+6
        sta x16__bmx                    /* width */
        lda x16__bmx_hdr+7
        sta x16__bmx+1
        lda x16__bmx_hdr+8
        sta x16__bmx+2                  /* height */
        lda x16__bmx_hdr+9
        sta x16__bmx+3
        lda x16__bmx_hdr+11
        sta x16__bmx+5                  /* palstart */
        lda x16__bmx_hdr+15
        sta x16__bmx+8                  /* border */
        lda #0                          /* zero the high byte BEFORE the */
        sta x16__bmx+7                  /* load whose Z flag bne tests */
        lda x16__bmx_hdr+10
        sta x16__bmx+6                  /* palcount */
        bne bl_pal_n
        inc x16__bmx+7                  /* 0 in the file means 256 */
    bl_pal_n:

        /* --- palette -> 0x1FA00 + palstart*2 ------------------------- */
        lda 0x9f25  /*VERA_CTRL*/
        and #0xfe
        sta 0x9f25
        lda x16__bmx+5                  /* palstart */
        asl                             /* carry = address bit 8 */
        sta 0x9f20                      /* VERA_ADDR_L */
        lda #0xfa                        /* >VRAM_PALETTE */
        adc #0
        sta 0x9f21                      /* VERA_ADDR_M */
        lda #0x11                        /* BANK | (INC_1 << 4) */
        sta 0x9f22                      /* VERA_ADDR_H */

        lda x16__bmx+6                  /* byte count = entries * 2 */
        sta x16__bmx_cnt
        lda x16__bmx+7
        sta x16__bmx_cnt+1
        asl x16__bmx_cnt
        rol x16__bmx_cnt+1
        jsr bl_bulk                     /* MACPTR into DATA0; CHRIN fallback */
        bcc bl_pal_done
        jmp bl_io_short
    bl_pal_done:

        /* --- skip any gap up to the header's data offset ------------ */
        /* expected position so far = 16 + palcount*2 */
        lda x16__bmx+6
        sta x16__bmx_cnt
        lda x16__bmx+7
        sta x16__bmx_cnt+1
        asl x16__bmx_cnt
        rol x16__bmx_cnt+1
        clc
        lda x16__bmx_cnt
        adc #16
        sta x16__bmx_cnt
        lda x16__bmx_cnt+1
        adc #0
        sta x16__bmx_cnt+1
        sec                             /* gap = data offset - position */
        lda x16__bmx_hdr+12
        sbc x16__bmx_cnt
        sta x16__bmx_cnt
        lda x16__bmx_hdr+13
        sbc x16__bmx_cnt+1
        sta x16__bmx_cnt+1
        bcc bl_data                     /* offset before position: trust it */
    bl_skip:
        lda x16__bmx_cnt
        ora x16__bmx_cnt+1
        beq bl_data
        jsr 0xffcf                      /* CHRIN */
        jsr bl_dec
        jmp bl_skip

    bl_data:
        /* Header, palette and gap all came out of the file, so every */
        /* pixel row must still be ahead. A nonzero ST here means the */
        /* file ended in the palette or the gap. */
        jsr 0xffb7                      /* READST */
        cmp #0
        beq bl_rows_ahead
        jmp bl_io_short
    bl_rows_ahead:

        /* --- pixel rows, stride apart -------------------------------- */
        lda vaddr                       /* the walking VRAM address */
        sta x16__bmx_cur
        lda vaddr+1
        sta x16__bmx_cur+1
        lda vaddr+2
        and #0x01
        sta x16__bmx_cur+2
        jsr bl_row_bytes                /* row = width >> (3 - depth) */

        lda x16__bmx+2                  /* height */
        sta x16__bmx_rows
        lda x16__bmx+3
        sta x16__bmx_rows+1
    bl_row:
        lda x16__bmx_rows
        ora x16__bmx_rows+1
        beq bl_done
        lda 0x9f25  /*VERA_CTRL*/
        and #0xfe
        sta 0x9f25
        lda x16__bmx_cur
        sta 0x9f20
        lda x16__bmx_cur+1
        sta 0x9f21
        lda x16__bmx_cur+2
        ora #0x10                        /* VERA_INC_1 << 4 */
        sta 0x9f22

        lda x16__bmx_row
        sta x16__bmx_cnt
        lda x16__bmx_row+1
        sta x16__bmx_cnt+1
        jsr bl_bulk                     /* the whole row, MACPTR gulps */
        bcc bl_row_done
        jmp bl_io_short
    bl_row_done:
        clc                             /* cur += stride (17-bit) */
        lda x16__bmx_cur
        adc x16__bmx+9                  /* stride */
        sta x16__bmx_cur
        lda x16__bmx_cur+1
        adc x16__bmx+10
        sta x16__bmx_cur+1
        lda x16__bmx_cur+2
        adc #0
        and #0x01
        sta x16__bmx_cur+2
        lda x16__bmx_rows
        bne bl_dec_rows
        dec x16__bmx_rows+1
    bl_dec_rows:
        dec x16__bmx_rows

        /* ST once per row, not per byte. Between rows the test is */
        /* exact: another row is expected, so any status at all (EOF */
        /* included) means the file is shorter than its header claims. */
        /* After the LAST row EOF is expected. */
        lda x16__bmx_rows
        ora x16__bmx_rows+1
        beq bl_done
        jsr 0xffb7                      /* READST */
        cmp #0
        beq bl_row

    bl_io_short:
        lda #1                          /* X16_BMX_ERR_IO */
        jmp bl_close_err

    bl_done:
        jsr bl_close
        lda #0
        sta accu
        jmp bl_end

        /* --- local helpers ------------------------------------------- */
    bl_close:
        jsr 0xffcc                      /* CLRCHN */
        lda #2
        jsr 0xffc3                      /* CLOSE */
        rts

    bl_dec:
        lda x16__bmx_cnt
        bne bl_dec_lo
        dec x16__bmx_cnt+1
    bl_dec_lo:
        dec x16__bmx_cnt
        rts

        /* row = width >> (3 - depth) */
    bl_row_bytes:
        lda x16__bmx                    /* width */
        sta x16__bmx_row
        lda x16__bmx+1
        sta x16__bmx_row+1
        jsr bl_depth
        eor #0x03                        /* 3 - depth (depth is 0-3) */
        tax
        beq bl_rb_done
    bl_rb_shift:
        lsr x16__bmx_row+1
        ror x16__bmx_row
        dex
        bne bl_rb_shift
    bl_rb_done:
        rts

        /* A = the VERA depth code for bpp (8->3, 4->2, 2->1, 1->0) */
    bl_depth:
        lda x16__bmx+4                  /* bpp */
        cmp #8
        beq bl_dc8
        cmp #4
        beq bl_dc4
        cmp #2
        beq bl_dc2
        lda #0
        rts
    bl_dc8:
        lda #3
        rts
    bl_dc4:
        lda #2
        rts
    bl_dc2:
        lda #1
        rts

        /* Pull cnt bytes from the open file into VERA_DATA0. MACPTR */
        /* streams a block to a FIXED destination when carry is set on */
        /* entry -- exactly what a data port is. Not every device can */
        /* (MACPTR returns carry set), so the CHRIN loop stays as the */
        /* fallback. Zero bytes delivered means the file ran out: carry */
        /* set on exit. */
    bl_bulk:
        lda x16__bmx_cnt
        ora x16__bmx_cnt+1
        beq bl_br_ok
        lda x16__bmx_cnt+1
        beq bl_small
        lda #255                        /* largest single ask */
        jmp bl_ask
    bl_small:
        lda x16__bmx_cnt                /* the exact remainder */
    bl_ask:
        ldx #0x23                        /* <VERA_DATA0 */
        ldy #0x9f                        /* >VERA_DATA0 */
        sec                             /* fixed destination */
        jsr 0xff44                      /* MACPTR */
        bcs bl_fallback                 /* device cannot do block reads */
        txa                             /* X/Y = bytes actually delivered */
        bne bl_got
        tya                             /* ask was <= 255, so Y is 0 and */
        beq bl_br_short                 /* X = 0 means nothing came back */
    bl_got:
        stx x16__bmx_t                  /* cnt -= bytes read */
        sec
        lda x16__bmx_cnt
        sbc x16__bmx_t
        sta x16__bmx_cnt
        sty x16__bmx_t
        lda x16__bmx_cnt+1
        sbc x16__bmx_t
        sta x16__bmx_cnt+1
        jmp bl_bulk
    bl_br_ok:
        clc
        rts
    bl_br_short:
        sec
        rts
    bl_fallback:
        lda x16__bmx_cnt
        ora x16__bmx_cnt+1
        beq bl_br_ok
        jsr 0xffcf                      /* CHRIN */
        sta 0x9f23                      /* VERA_DATA0 */
        jsr bl_dec
        jmp bl_fallback

    bl_end:
    };
}

// ---------------------------------------------------------------------
// Save: write a BMX from VRAM. Describe the image with
// x16_bmx_set_info() first. CAVEAT: the palette region of VRAM reads
// back the last value the HOST wrote, so the palette saved is only
// meaningful if this program set those entries itself.
// ---------------------------------------------------------------------
unsigned char x16_bmx_save(const char *name, unsigned char len,
                           unsigned char device,
                           unsigned long vaddr) {
    /* --- open for write ------------------------------------------ */
    return __asm {
        lda len
        ldx name
        ldy name+1
        jsr 0xffbd                      /* SETNAM */
        lda #2
        ldx device
        ldy #1                          /* write */
        jsr 0xffba                      /* SETLFS */
        jsr 0xffc0                      /* OPEN */
        bcs bs_open_bad
        ldx #2
        jsr 0xffc9                      /* CHKOUT */
        bcc bs_hdr
    bs_open_bad:
        jsr 0xffcc                      /* CLRCHN */
        lda #2
        jsr 0xffc3                      /* CLOSE */
        lda #1                          /* X16_BMX_ERR_IO */
        sta accu
        jmp bs_end

    bs_hdr:
        lda #0x42                        /* 'B' */
        sta x16__bmx_hdr
        lda #0x4d                        /* 'M' */
        sta x16__bmx_hdr+1
        lda #0x58                        /* 'X' */
        sta x16__bmx_hdr+2
        lda #1
        sta x16__bmx_hdr+3
        lda x16__bmx+4                  /* bpp */
        sta x16__bmx_hdr+4
        jsr bs_depth
        sta x16__bmx_hdr+5
        lda x16__bmx                    /* width */
        sta x16__bmx_hdr+6
        lda x16__bmx+1
        sta x16__bmx_hdr+7
        lda x16__bmx+2                  /* height */
        sta x16__bmx_hdr+8
        lda x16__bmx+3
        sta x16__bmx_hdr+9
        lda x16__bmx+6                  /* palcount: 256 stores as 0 */
        sta x16__bmx_hdr+10
        lda x16__bmx+5                  /* palstart */
        sta x16__bmx_hdr+11
        lda x16__bmx+6                  /* data offset = 16 + palcount*2 */
        sta x16__bmx_cnt
        lda x16__bmx+7
        sta x16__bmx_cnt+1
        asl x16__bmx_cnt
        rol x16__bmx_cnt+1
        clc
        lda x16__bmx_cnt
        adc #16
        sta x16__bmx_hdr+12
        lda x16__bmx_cnt+1
        adc #0
        sta x16__bmx_hdr+13
        lda #0
        sta x16__bmx_hdr+14 /* uncompressed */
        lda x16__bmx+8                  /* border */
        sta x16__bmx_hdr+15

        ldx #0
    bs_hdr_out:
        lda x16__bmx_hdr,x
        jsr 0xffd2                      /* CHROUT */
        inx
        cpx #16
        bne bs_hdr_out

        /* --- palette from the VRAM shadow ---------------------------- */
        lda 0x9f25  /* port 1 reads: CHROUT stays safe */
        ora #0x01
        sta 0x9f25
        lda x16__bmx+5                  /* palstart */
        asl
        sta 0x9f20
        lda #0xfa                        /* >VRAM_PALETTE */
        adc #0
        sta 0x9f21
        lda #0x11
        sta 0x9f22
        lda 0x9f25  /* leave ADDRSEL 0 for the KERNAL */
        and #0xfe
        sta 0x9f25

        lda x16__bmx+6
        sta x16__bmx_cnt
        lda x16__bmx+7
        sta x16__bmx_cnt+1
        asl x16__bmx_cnt
        rol x16__bmx_cnt+1
    bs_pal_out:
        lda x16__bmx_cnt
        ora x16__bmx_cnt+1
        beq bs_pal_wrote
        lda 0x9f24                      /* VERA_DATA1 */
        jsr 0xffd2                      /* CHROUT */
        jsr bs_dec
        jmp bs_pal_out
    bs_pal_wrote:

        /* --- pixel rows ----------------------------------------------- */
        lda vaddr
        sta x16__bmx_cur
        lda vaddr+1
        sta x16__bmx_cur+1
        lda vaddr+2
        and #0x01
        sta x16__bmx_cur+2
        jsr bs_row_bytes

        lda x16__bmx+2                  /* height */
        sta x16__bmx_rows
        lda x16__bmx+3
        sta x16__bmx_rows+1
    bs_wrow:
        lda x16__bmx_rows
        ora x16__bmx_rows+1
        beq bs_wdone
        lda 0x9f25
        ora #0x01
        sta 0x9f25
        lda x16__bmx_cur
        sta 0x9f20
        lda x16__bmx_cur+1
        sta 0x9f21
        lda x16__bmx_cur+2
        ora #0x10
        sta 0x9f22
        lda 0x9f25  /* ADDRSEL back for the KERNAL */
        and #0xfe
        sta 0x9f25

        lda x16__bmx_row
        sta x16__bmx_cnt
        lda x16__bmx_row+1
        sta x16__bmx_cnt+1
    bs_wpix:
        lda x16__bmx_cnt
        ora x16__bmx_cnt+1
        beq bs_wrow_done
        lda 0x9f24                      /* VERA_DATA1 */
        jsr 0xffd2                      /* CHROUT */
        jsr bs_dec
        jmp bs_wpix
    bs_wrow_done:
        clc
        lda x16__bmx_cur
        adc x16__bmx+9                  /* stride */
        sta x16__bmx_cur
        lda x16__bmx_cur+1
        adc x16__bmx+10
        sta x16__bmx_cur+1
        lda x16__bmx_cur+2
        adc #0
        and #0x01
        sta x16__bmx_cur+2
        lda x16__bmx_rows
        bne bs_wdec
        dec x16__bmx_rows+1
    bs_wdec:
        dec x16__bmx_rows
        jmp bs_wrow

    bs_wdone:
        jsr 0xffcc                      /* CLRCHN */
        lda #2
        jsr 0xffc3                      /* CLOSE */
        lda #0
        sta accu
        jmp bs_end

        /* --- local helpers ------------------------------------------- */
    bs_dec:
        lda x16__bmx_cnt
        bne bs_dec_lo
        dec x16__bmx_cnt+1
    bs_dec_lo:
        dec x16__bmx_cnt
        rts

    bs_depth:
        lda x16__bmx+4
        cmp #8
        beq bs_dc8
        cmp #4
        beq bs_dc4
        cmp #2
        beq bs_dc2
        lda #0
        rts
    bs_dc8:
        lda #3
        rts
    bs_dc4:
        lda #2
        rts
    bs_dc2:
        lda #1
        rts

    bs_row_bytes:
        lda x16__bmx
        sta x16__bmx_row
        lda x16__bmx+1
        sta x16__bmx_row+1
        jsr bs_depth
        eor #0x03
        tax
        beq bs_rb_done
    bs_rb_shift:
        lsr x16__bmx_row+1
        ror x16__bmx_row
        dex
        bne bs_rb_shift
    bs_rb_done:
        rts

    bs_end:
    };
}
