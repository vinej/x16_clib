// =====================================================================
// x16clib :: x16/bmx.c -- the X16's native bitmap file format
// =====================================================================
// BMX version 1 (the format Prog8 and the community tools write). Same
// code as src_ca65/storage/bmx.s; see that file (or the header) for the
// format table. Rows are written to VRAM x16__bmx stride bytes apart,
// so a 320-wide image is a plain contiguous load and a narrower one
// lands as a "stamp".
//
// The magic bytes are explicit hex ($42 $4D $58 = "BMX"): both cc65's
// cx16 charmap and KickC's default encoding would silently PETSCII-ify
// character literals.
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
__mem volatile char x16__bmx[11] = { 0, 0, 0, 0, 8, 0, 0, 1, 0, 0x40, 0x01 };

// info block copies are indirected: pinned zp pointer (KickC ignores
// __zp on parameters; see x16/zpsafe.h).
__address(0x78) char* volatile x16__bmx_p;

__mem volatile char x16__bmx_hdr[16];
__mem volatile char x16__bmx_cnt[2];
__mem volatile char x16__bmx_cur[3];
__mem volatile char x16__bmx_row[2];
__mem volatile char x16__bmx_rows[2];
__mem volatile char x16__bmx_t;
__mem volatile char x16__bmx_code;      // the last BMX_ERR_*, for lasterr
__mem volatile char x16__bmx_nlen;      // load_hires' measured name length

void x16_bmx_get_info(x16_bmx_info *out) {
    x16__bmx_p = (char*)out;
    asm {
        ldy #10                         // BMX_INFO_SIZE - 1
    bgi_out:
        lda x16__bmx,y
        sta (x16__bmx_p),y
        dey
        bpl bgi_out
    }
}

void x16_bmx_set_info(const x16_bmx_info *in) {
    x16__bmx_p = (char*)in;
    asm {
        ldy #10
    bsi_in:
        lda (x16__bmx_p),y
        sta x16__bmx,y
        dey
        bpl bsi_in
    }
}

// ---------------------------------------------------------------------
// Load: palette into the VERA palette, pixels into VRAM at vaddr (bit
// 16 = bank), rows stride apart. 0 on success, else X16_BMX_ERR_*.
// ---------------------------------------------------------------------
unsigned char x16_bmx_load(const char *name, __mem unsigned char len,
                           __mem unsigned char device,
                           __mem unsigned long vaddr) {
    __mem char r;
    x16__bmx_code = 0;
    asm {
        // --- open for sequential read -------------------------------
        lda len
        ldx name
        ldy name+1
        jsr $ffbd /*SETNAM*/
        lda #2
        ldx device
        ldy #0                          // sequential, no header games
        jsr $ffba /*SETLFS*/
        jsr $ffc0 /*OPEN*/
        bcs bl_open_bad
        ldx #2
        jsr $ffc6 /*CHKIN*/
        bcc bl_hdr
    bl_open_bad:
        jsr $ffcc /*CLRCHN*/
        lda #2
        jsr $ffc3 /*CLOSE*/
        lda #1 /*X16_BMX_ERR_IO*/
        sta x16__bmx_code
        sta r
        jmp bl_end

    bl_hdr:
        ldx #0                          // pull in the 16-byte header
    bl_get_hdr:
        jsr $ffcf /*CHRIN*/
        sta x16__bmx_hdr,x
        inx
        cpx #16
        bne bl_get_hdr

        // OPEN and CHKIN both succeed for a missing file: DOS reports
        // "62,FILE NOT FOUND" on the command channel and the KERNAL
        // only surfaces it in ST once a read has been attempted. So
        // without this, the 16 CHRINs return junk and a missing file
        // reads as "not a BMX". A real BMX has palette and pixels after
        // byte 16, so EOF cannot legitimately be set here either.
        jsr $ffb7 /*READST*/
        beq bl_validate
        lda #1 /*X16_BMX_ERR_IO*/
        bra bl_close_err

    bl_validate:
        lda x16__bmx_hdr
        cmp #$42                        // 'B'
        bne bl_bad_fmt
        lda x16__bmx_hdr+1
        cmp #$4d                        // 'M'
        bne bl_bad_fmt
        lda x16__bmx_hdr+2
        cmp #$58                        // 'X'
        bne bl_bad_fmt
        lda x16__bmx_hdr+3
        cmp #1
        bne bl_bad_fmt
        lda x16__bmx_hdr+14
        beq bl_fmt_ok
        lda #3 /*X16_BMX_ERR_PACKED*/
        bra bl_close_err
    bl_bad_fmt:
        lda #2 /*X16_BMX_ERR_FORMAT*/
    bl_close_err:
        pha
        jsr bl_close
        pla
        sta x16__bmx_code
        sta r
        jmp bl_end

    bl_fmt_ok:
        lda x16__bmx_hdr+4              // publish the header fields
        sta x16__bmx+4                  // bpp
        lda x16__bmx_hdr+6
        sta x16__bmx                    // width
        lda x16__bmx_hdr+7
        sta x16__bmx+1
        lda x16__bmx_hdr+8
        sta x16__bmx+2                  // height
        lda x16__bmx_hdr+9
        sta x16__bmx+3
        lda x16__bmx_hdr+11
        sta x16__bmx+5                  // palstart
        lda x16__bmx_hdr+15
        sta x16__bmx+8                  // border
        lda x16__bmx_hdr+10
        sta x16__bmx+6                  // palcount
        stz x16__bmx+7
        bne bl_pal_n
        inc x16__bmx+7                  // 0 in the file means 256
    bl_pal_n:

        // --- palette -> $1FA00 + palstart*2 -------------------------
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/
        lda x16__bmx+5                  // palstart
        asl                             // carry = address bit 8
        sta $9f20 /*VERA_ADDR_L*/
        lda #$fa                        // >VRAM_PALETTE
        adc #0
        sta $9f21 /*VERA_ADDR_M*/
        lda #$11                        // BANK | (INC_1 << 4)
        sta $9f22 /*VERA_ADDR_H*/

        lda x16__bmx+6                  // byte count = entries * 2
        sta x16__bmx_cnt
        lda x16__bmx+7
        sta x16__bmx_cnt+1
        asl x16__bmx_cnt
        rol x16__bmx_cnt+1
        jsr bl_bulk                     // MACPTR into DATA0; CHRIN fallback
        bcc bl_pal_done
        jmp bl_io_short
    bl_pal_done:

        // --- skip any gap up to the header's data offset ------------
        // expected position so far = 16 + palcount*2
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
        sec                             // gap = data offset - position
        lda x16__bmx_hdr+12
        sbc x16__bmx_cnt
        sta x16__bmx_cnt
        lda x16__bmx_hdr+13
        sbc x16__bmx_cnt+1
        sta x16__bmx_cnt+1
        bcc bl_data                     // offset before position: trust it
    bl_skip:
        lda x16__bmx_cnt
        ora x16__bmx_cnt+1
        beq bl_data
        jsr $ffcf /*CHRIN*/
        jsr bl_dec
        bra bl_skip

    bl_data:
        // Header, palette and gap all came out of the file, so every
        // pixel row must still be ahead. A nonzero ST here means the
        // file ended in the palette or the gap.
        jsr $ffb7 /*READST*/
        cmp #0
        beq bl_rows_ahead
        jmp bl_io_short
    bl_rows_ahead:

        // --- pixel rows, stride apart --------------------------------
        lda vaddr                       // the walking VRAM address
        sta x16__bmx_cur
        lda vaddr+1
        sta x16__bmx_cur+1
        lda vaddr+2
        and #$01
        sta x16__bmx_cur+2
        jsr bl_row_bytes                // row = width >> (3 - depth)

        lda x16__bmx+2                  // height
        sta x16__bmx_rows
        lda x16__bmx+3
        sta x16__bmx_rows+1
    bl_row:
        lda x16__bmx_rows
        ora x16__bmx_rows+1
        beq bl_done
        lda #$01 /*VERA_CTRL_ADDRSEL*/  // port 0 at cur, INC_1
        trb $9f25 /*VERA_CTRL*/
        lda x16__bmx_cur
        sta $9f20
        lda x16__bmx_cur+1
        sta $9f21
        lda x16__bmx_cur+2
        ora #$10                        // VERA_INC_1 << 4
        sta $9f22

        lda x16__bmx_row
        sta x16__bmx_cnt
        lda x16__bmx_row+1
        sta x16__bmx_cnt+1
        jsr bl_bulk                     // the whole row, MACPTR gulps
        bcc bl_row_done
        jmp bl_io_short
    bl_row_done:
        clc                             // cur += stride (17-bit)
        lda x16__bmx_cur
        adc x16__bmx+9                  // stride
        sta x16__bmx_cur
        lda x16__bmx_cur+1
        adc x16__bmx+10
        sta x16__bmx_cur+1
        lda x16__bmx_cur+2
        adc #0
        and #$01
        sta x16__bmx_cur+2
        lda x16__bmx_rows
        bne bl_dec_rows
        dec x16__bmx_rows+1
    bl_dec_rows:
        dec x16__bmx_rows

        // ST once per row, not per byte. Between rows the test is
        // exact: another row is expected, so any status at all (EOF
        // included) means the file is shorter than its header claims.
        // After the LAST row EOF is expected.
        lda x16__bmx_rows
        ora x16__bmx_rows+1
        beq bl_done
        jsr $ffb7 /*READST*/
        cmp #0
        beq bl_row

    bl_io_short:
        lda #1 /*X16_BMX_ERR_IO*/
        jmp bl_close_err

    bl_done:
        jsr bl_close
        stz r
        jmp bl_end

        // --- local helpers -------------------------------------------
    bl_close:
        jsr $ffcc /*CLRCHN*/
        lda #2
        jsr $ffc3 /*CLOSE*/
        rts

    bl_dec:
        lda x16__bmx_cnt
        bne bl_dec_lo
        dec x16__bmx_cnt+1
    bl_dec_lo:
        dec x16__bmx_cnt
        rts

        // row = width >> (3 - depth)
    bl_row_bytes:
        lda x16__bmx                    // width
        sta x16__bmx_row
        lda x16__bmx+1
        sta x16__bmx_row+1
        jsr bl_depth
        eor #$03                        // 3 - depth (depth is 0-3)
        tax
        beq bl_rb_done
    bl_rb_shift:
        lsr x16__bmx_row+1
        ror x16__bmx_row
        dex
        bne bl_rb_shift
    bl_rb_done:
        rts

        // A = the VERA depth code for bpp (8->3, 4->2, 2->1, 1->0)
    bl_depth:
        lda x16__bmx+4                  // bpp
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

        // Pull cnt bytes from the open file into VERA_DATA0. MACPTR
        // streams a block to a FIXED destination when carry is set on
        // entry -- exactly what a data port is. Not every device can
        // (MACPTR returns carry set), so the CHRIN loop stays as the
        // fallback. Zero bytes delivered means the file ran out: carry
        // set on exit.
    bl_bulk:
        lda x16__bmx_cnt
        ora x16__bmx_cnt+1
        beq bl_br_ok
        lda x16__bmx_cnt+1
        beq bl_small
        lda #255                        // largest single ask
        bra bl_ask
    bl_small:
        lda x16__bmx_cnt                // the exact remainder
    bl_ask:
        ldx #$23                        // <VERA_DATA0
        ldy #$9f                        // >VERA_DATA0
        sec                             // fixed destination
        jsr $ff44 /*MACPTR*/
        bcs bl_fallback                 // device cannot do block reads
        txa                             // X/Y = bytes actually delivered
        bne bl_got
        tya                             // ask was <= 255, so Y is 0 and
        beq bl_br_short                 // X = 0 means nothing came back
    bl_got:
        stx x16__bmx_t                  // cnt -= bytes read
        sec
        lda x16__bmx_cnt
        sbc x16__bmx_t
        sta x16__bmx_cnt
        sty x16__bmx_t
        lda x16__bmx_cnt+1
        sbc x16__bmx_t
        sta x16__bmx_cnt+1
        bra bl_bulk
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
        jsr $ffcf /*CHRIN*/
        sta $9f23 /*VERA_DATA0*/
        jsr bl_dec
        bra bl_fallback

    bl_end:
    }
    return r;
}

// ---------------------------------------------------------------------
// Save: write a BMX from VRAM. Describe the image with
// x16_bmx_set_info() first. CAVEAT: the palette region of VRAM reads
// back the last value the HOST wrote, so the palette saved is only
// meaningful if this program set those entries itself.
// ---------------------------------------------------------------------
unsigned char x16_bmx_save(const char *name, __mem unsigned char len,
                           __mem unsigned char device,
                           __mem unsigned long vaddr) {
    __mem char r;
    x16__bmx_code = 0;
    asm {
        // --- open for write ------------------------------------------
        lda len
        ldx name
        ldy name+1
        jsr $ffbd /*SETNAM*/
        lda #2
        ldx device
        ldy #1                          // write
        jsr $ffba /*SETLFS*/
        jsr $ffc0 /*OPEN*/
        bcs bs_open_bad
        ldx #2
        jsr $ffc9 /*CHKOUT*/
        bcc bs_hdr
    bs_open_bad:
        jsr $ffcc /*CLRCHN*/
        lda #2
        jsr $ffc3 /*CLOSE*/
        lda #1 /*X16_BMX_ERR_IO*/
        sta x16__bmx_code
        sta r
        jmp bs_end

    bs_hdr:
        lda #$42                        // 'B'
        sta x16__bmx_hdr
        lda #$4d                        // 'M'
        sta x16__bmx_hdr+1
        lda #$58                        // 'X'
        sta x16__bmx_hdr+2
        lda #1
        sta x16__bmx_hdr+3
        lda x16__bmx+4                  // bpp
        sta x16__bmx_hdr+4
        jsr bs_depth
        sta x16__bmx_hdr+5
        lda x16__bmx                    // width
        sta x16__bmx_hdr+6
        lda x16__bmx+1
        sta x16__bmx_hdr+7
        lda x16__bmx+2                  // height
        sta x16__bmx_hdr+8
        lda x16__bmx+3
        sta x16__bmx_hdr+9
        lda x16__bmx+6                  // palcount: 256 stores as 0
        sta x16__bmx_hdr+10
        lda x16__bmx+5                  // palstart
        sta x16__bmx_hdr+11
        lda x16__bmx+6                  // data offset = 16 + palcount*2
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
        stz x16__bmx_hdr+14             // uncompressed
        lda x16__bmx+8                  // border
        sta x16__bmx_hdr+15

        ldx #0
    bs_hdr_out:
        lda x16__bmx_hdr,x
        jsr $ffd2 /*CHROUT*/
        inx
        cpx #16
        bne bs_hdr_out

        // --- palette from the VRAM shadow ----------------------------
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        tsb $9f25                       // port 1 reads: CHROUT stays safe
        lda x16__bmx+5                  // palstart
        asl
        sta $9f20
        lda #$fa                        // >VRAM_PALETTE
        adc #0
        sta $9f21
        lda #$11
        sta $9f22
        lda #$01
        trb $9f25                       // leave ADDRSEL 0 for the KERNAL

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
        lda $9f24 /*VERA_DATA1*/
        jsr $ffd2 /*CHROUT*/
        jsr bs_dec
        bra bs_pal_out
    bs_pal_wrote:

        // --- pixel rows -----------------------------------------------
        lda vaddr
        sta x16__bmx_cur
        lda vaddr+1
        sta x16__bmx_cur+1
        lda vaddr+2
        and #$01
        sta x16__bmx_cur+2
        jsr bs_row_bytes

        lda x16__bmx+2                  // height
        sta x16__bmx_rows
        lda x16__bmx+3
        sta x16__bmx_rows+1
    bs_wrow:
        lda x16__bmx_rows
        ora x16__bmx_rows+1
        beq bs_wdone
        lda #$01 /*VERA_CTRL_ADDRSEL*/  // port 1 at cur, INC_1
        tsb $9f25
        lda x16__bmx_cur
        sta $9f20
        lda x16__bmx_cur+1
        sta $9f21
        lda x16__bmx_cur+2
        ora #$10
        sta $9f22
        lda #$01
        trb $9f25                       // ADDRSEL back for the KERNAL

        lda x16__bmx_row
        sta x16__bmx_cnt
        lda x16__bmx_row+1
        sta x16__bmx_cnt+1
    bs_wpix:
        lda x16__bmx_cnt
        ora x16__bmx_cnt+1
        beq bs_wrow_done
        lda $9f24 /*VERA_DATA1*/
        jsr $ffd2 /*CHROUT*/
        jsr bs_dec
        bra bs_wpix
    bs_wrow_done:
        clc
        lda x16__bmx_cur
        adc x16__bmx+9                  // stride
        sta x16__bmx_cur
        lda x16__bmx_cur+1
        adc x16__bmx+10
        sta x16__bmx_cur+1
        lda x16__bmx_cur+2
        adc #0
        and #$01
        sta x16__bmx_cur+2
        lda x16__bmx_rows
        bne bs_wdec
        dec x16__bmx_rows+1
    bs_wdec:
        dec x16__bmx_rows
        bra bs_wrow

    bs_wdone:
        jsr $ffcc /*CLRCHN*/
        lda #2
        jsr $ffc3 /*CLOSE*/
        stz r
        jmp bs_end

        // --- local helpers -------------------------------------------
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
        eor #$03
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
    }
    return r;
}

// ---------------------------------------------------------------------
// Why the last bmx_* call failed: the X16_BMX_ERR_* it returned, or 0
// after a call that worked. For call sites that could not keep the
// return value.
// ---------------------------------------------------------------------
unsigned char x16_bmx_lasterr(void) {
    return x16__bmx_code;
}

// ---------------------------------------------------------------------
// bmx_load's sibling for the MiSTer VERA_2 640x480 SDRAM layer (the
// gfx8h engine): the palette goes to the VERA_2 palette registers, the
// pixel rows stream into VERA_2 SDRAM 640 bytes apart from offset 0, so
// a full-width 640x480x8 image is a plain contiguous load.
//
// Feature-detect with x16_gfx8h_has() first: on stock hardware (and the
// emulator) the file still parses but the pixel writes go to open bus.
//
// Unlike x16_bmx_load there is no length argument: the name is an
// ordinary NUL-terminated C string, measured here (up to 255 bytes).
// The ca65 build keeps this in its own object so a program that never
// loads into VERA_2 SDRAM never links it; KickC's whole-program strip
// does that by itself, so it lives here beside its plumbing.
// ---------------------------------------------------------------------
unsigned char x16_bmx_load_hires(const char *name,
                                 __mem unsigned char device) {
    __mem char r;
    x16__bmx_code = 0;
    x16__bmx_nlen = 0;
    while (name[x16__bmx_nlen] != 0) {
        ++x16__bmx_nlen;                // a 255-byte name is its own problem
    }
    asm {
        // --- open for sequential read -------------------------------
        lda x16__bmx_nlen
        ldx name
        ldy name+1
        jsr $ffbd /*SETNAM*/
        lda #2
        ldx device
        ldy #0                          // sequential, no header games
        jsr $ffba /*SETLFS*/
        jsr $ffc0 /*OPEN*/
        bcs bh_open_bad
        ldx #2
        jsr $ffc6 /*CHKIN*/
        bcc bh_hdr
    bh_open_bad:
        jsr $ffcc /*CLRCHN*/
        lda #2
        jsr $ffc3 /*CLOSE*/
        lda #1 /*X16_BMX_ERR_IO*/
        sta x16__bmx_code
        sta r
        jmp bh_end

    bh_hdr:
        ldx #0                          // pull in the 16-byte header
    bh_get_hdr:
        jsr $ffcf /*CHRIN*/
        sta x16__bmx_hdr,x
        inx
        cpx #16
        bne bh_get_hdr

        jsr $ffb7 /*READST*/            // a short/absent header is I/O
        beq bh_validate
        lda #1 /*X16_BMX_ERR_IO*/
        bra bh_close_err
    bh_validate:
        lda x16__bmx_hdr
        cmp #$42                        // 'B'
        bne bh_bad_fmt
        lda x16__bmx_hdr+1
        cmp #$4d                        // 'M'
        bne bh_bad_fmt
        lda x16__bmx_hdr+2
        cmp #$58                        // 'X'
        bne bh_bad_fmt
        lda x16__bmx_hdr+3
        cmp #1
        bne bh_bad_fmt
        lda x16__bmx_hdr+14
        beq bh_fmt_ok
        lda #3 /*X16_BMX_ERR_PACKED*/
        bra bh_close_err
    bh_bad_fmt:
        lda #2 /*X16_BMX_ERR_FORMAT*/
    bh_close_err:
        pha
        jsr bh_close
        pla
        sta x16__bmx_code
        sta r
        jmp bh_end

    bh_fmt_ok:
        lda x16__bmx_hdr+4              // publish the header fields
        sta x16__bmx+4                  // bpp
        lda x16__bmx_hdr+6
        sta x16__bmx                    // width
        lda x16__bmx_hdr+7
        sta x16__bmx+1
        lda x16__bmx_hdr+8
        sta x16__bmx+2                  // height
        lda x16__bmx_hdr+9
        sta x16__bmx+3
        lda x16__bmx_hdr+11
        sta x16__bmx+5                  // palstart
        lda x16__bmx_hdr+15
        sta x16__bmx+8                  // border
        lda x16__bmx_hdr+10
        sta x16__bmx+6                  // palcount
        stz x16__bmx+7
        bne bh_pal_n
        inc x16__bmx+7                  // 0 in the file means 256
    bh_pal_n:

        // --- palette -> the VERA_2 palette (IDX auto-increments) ----
        lda x16__bmx+5                  // palstart
        sta $9f66 /*VERA2_PAL_IDX*/
        lda x16__bmx+6                  // entries remaining (16-bit)
        sta x16__bmx_cnt
        lda x16__bmx+7
        sta x16__bmx_cnt+1
    bh_pal_loop:
        lda x16__bmx_cnt
        ora x16__bmx_cnt+1
        beq bh_pal_done
        jsr $ffcf /*CHRIN*/
        sta $9f67 /*VERA2_PAL_LO*/
        jsr $ffcf /*CHRIN*/
        sta $9f68 /*VERA2_PAL_HI*/
        jsr bh_dec
        bra bh_pal_loop
    bh_pal_done:

        // --- skip any gap up to the header's data offset ------------
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
        sec                             // gap = data offset - position
        lda x16__bmx_hdr+12
        sbc x16__bmx_cnt
        sta x16__bmx_cnt
        lda x16__bmx_hdr+13
        sbc x16__bmx_cnt+1
        sta x16__bmx_cnt+1
        bcc bh_data
    bh_skip:
        lda x16__bmx_cnt
        ora x16__bmx_cnt+1
        beq bh_data
        jsr $ffcf /*CHRIN*/
        jsr bh_dec
        bra bh_skip

    bh_data:
        jsr $ffb7 /*READST*/
        cmp #0
        beq bh_rows_ahead
        jmp bh_io_short
    bh_rows_ahead:

        // --- pixel rows into VERA_2 SDRAM, 640 apart ----------------
        stz x16__bmx_cur                // SDRAM byte offset 0
        stz x16__bmx_cur+1
        stz x16__bmx_cur+2
        jsr bh_row_bytes                // row = width >> (3 - depth)

        lda x16__bmx+2                  // height
        sta x16__bmx_rows
        lda x16__bmx+3
        sta x16__bmx_rows+1
    bh_row:
        lda x16__bmx_rows
        ora x16__bmx_rows+1
        beq bh_done
        lda x16__bmx_cur                // VERA_2 addr = cur, INC_1
        sta $9f62 /*VERA2_ADDR_L*/
        lda x16__bmx_cur+1
        sta $9f63 /*VERA2_ADDR_M*/
        lda x16__bmx_cur+2
        and #$0f                        // VERA2_INC_1 code is 0: the
        sta $9f64 /*VERA2_ADDR_H*/      // high nibble stays clear

        lda x16__bmx_row
        sta x16__bmx_cnt
        lda x16__bmx_row+1
        sta x16__bmx_cnt+1
        jsr bh_bulk                     // the whole row, MACPTR gulps
        bcc bh_row_done
        jmp bh_io_short
    bh_row_done:
        clc                             // cur += 640 (20-bit)
        lda x16__bmx_cur
        adc #$80                        // <640
        sta x16__bmx_cur
        lda x16__bmx_cur+1
        adc #$02                        // >640
        sta x16__bmx_cur+1
        lda x16__bmx_cur+2
        adc #0
        sta x16__bmx_cur+2
        lda x16__bmx_rows
        bne bh_dec_rows
        dec x16__bmx_rows+1
    bh_dec_rows:
        dec x16__bmx_rows

        lda x16__bmx_rows               // ST once per row, exactly as
        ora x16__bmx_rows+1             // bmx_load: between rows any
        beq bh_done                     // status at all is a short file
        jsr $ffb7 /*READST*/
        cmp #0
        beq bh_row

    bh_io_short:
        lda #1 /*X16_BMX_ERR_IO*/
        jmp bh_close_err

    bh_done:
        jsr bh_close
        stz r
        jmp bh_end

        // --- local helpers -------------------------------------------
    bh_close:
        jsr $ffcc /*CLRCHN*/
        lda #2
        jsr $ffc3 /*CLOSE*/
        rts

    bh_dec:
        lda x16__bmx_cnt
        bne bh_dec_lo
        dec x16__bmx_cnt+1
    bh_dec_lo:
        dec x16__bmx_cnt
        rts

        // row = width >> (3 - depth)
    bh_row_bytes:
        lda x16__bmx                    // width
        sta x16__bmx_row
        lda x16__bmx+1
        sta x16__bmx_row+1
        jsr bh_depth
        eor #$03                        // 3 - depth (depth is 0-3)
        tax
        beq bh_rb_done
    bh_rb_shift:
        lsr x16__bmx_row+1
        ror x16__bmx_row
        dex
        bne bh_rb_shift
    bh_rb_done:
        rts

        // A = the VERA depth code for bpp (8->3, 4->2, 2->1, 1->0)
    bh_depth:
        lda x16__bmx+4                  // bpp
        cmp #8
        beq bh_dc8
        cmp #4
        beq bh_dc4
        cmp #2
        beq bh_dc2
        lda #0
        rts
    bh_dc8:
        lda #3
        rts
    bh_dc4:
        lda #2
        rts
    bh_dc2:
        lda #1
        rts

        // Pull cnt bytes from the open file into VERA2_DATA. The same
        // MACPTR streaming trick as bl_bulk, but the fixed destination
        // is the VERA_2 data register; CHRIN stays as the fallback.
    bh_bulk:
        lda x16__bmx_cnt
        ora x16__bmx_cnt+1
        beq bh_br_ok
        lda x16__bmx_cnt+1
        beq bh_small
        lda #255                        // largest single ask
        bra bh_ask
    bh_small:
        lda x16__bmx_cnt                // the exact remainder
    bh_ask:
        ldx #$65                        // <VERA2_DATA
        ldy #$9f                        // >VERA2_DATA
        sec                             // fixed destination
        jsr $ff44 /*MACPTR*/
        bcs bh_fallback                 // device cannot do block reads
        txa                             // X/Y = bytes actually delivered
        bne bh_got
        tya
        beq bh_br_short
    bh_got:
        stx x16__bmx_t                  // cnt -= bytes read
        sec
        lda x16__bmx_cnt
        sbc x16__bmx_t
        sta x16__bmx_cnt
        sty x16__bmx_t
        lda x16__bmx_cnt+1
        sbc x16__bmx_t
        sta x16__bmx_cnt+1
        bra bh_bulk
    bh_br_ok:
        clc
        rts
    bh_br_short:
        sec
        rts
    bh_fallback:
        lda x16__bmx_cnt
        ora x16__bmx_cnt+1
        beq bh_br_ok
        jsr $ffcf /*CHRIN*/
        sta $9f65 /*VERA2_DATA*/
        jsr bh_dec
        bra bh_fallback

    bh_end:
    }
    return r;
}
