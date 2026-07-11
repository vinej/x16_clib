// =====================================================================
// x16clib :: x16/load.c -- load and save
// =====================================================================
// Same code as src_ca65/storage/load.s. Device 8 is the SD card.
// Filenames are (address, length), not NUL-terminated.
//
// Two different registers steer a load, and they are easy to conflate:
//
//   SETLFS's secondary address says how to TREAT the file:
//     0  skip the 2-byte PRG header, load at the address you pass
//     1  skip it, load at the address the header itself names
//     2  raw: no header to skip, load everything at your address
//
//   LOAD's own A register says WHERE memory-wise:
//     0  system RAM        1  verify only
//     2  VRAM bank 0       3  VRAM bank 1
// =====================================================================

#include <x16/load.h>

// KERNAL SAVE takes the start address through a zero-page pointer,
// given by its zp offset in A -- so this slot must be zp. The end
// out-parameter is indirected, so it is pinned too (KickC ignores __zp
// on parameters; see x16/zpsafe.h).
__address(0x78) const char* volatile x16__fs_start;
__address(0x7a) char* volatile x16__fs_endp;

__mem volatile char x16__fs_e0;         // end address handed back by LOAD
__mem volatile char x16__fs_e1;

void x16_fs_setname(const char *name, __mem unsigned char len) {
    asm {
        lda len
        ldx name
        ldy name+1
        jsr $ffbd /*SETNAM*/
    }
}

// ---------------------------------------------------------------------
// Returns 0 on success, else the KERNAL error code. *end receives the
// address one past the last byte loaded; end may be NULL.
// ---------------------------------------------------------------------
unsigned char x16_fs_load(const char *name, __mem unsigned char len,
                          __mem unsigned char device, __mem unsigned char sa,
                          void *dest, unsigned int *end) {
    __mem char r;
    x16__fs_endp = (char*)end;
    asm {
        lda len
        ldx name
        ldy name+1
        jsr $ffbd /*SETNAM*/
        lda #1                          // logical file number
        ldx device
        ldy sa
        jsr $ffba /*SETLFS*/
        lda #0                          // LOAD A = 0: into system RAM
        ldx dest
        ldy dest+1
        jsr $ffd5 /*LOAD*/              // carry + A = error, X/Y = end
        stx x16__fs_e0                  // neither store touches the flags
        sty x16__fs_e1
        php                             // the carry is the success signal
        pha
        lda x16__fs_endp
        ora x16__fs_endp+1
        beq fl_no_out                   // end == NULL: caller doesn't care
        ldy #0
        lda x16__fs_e0
        sta (x16__fs_endp),y
        iny
        lda x16__fs_e1
        sta (x16__fs_endp),y
    fl_no_out:
        pla                             // A = KERNAL error code
        plp                             // carry set = it failed
        bcs fl_failed
        lda #0                          // success
    fl_failed:
        sta r
    }
    return r;
}

// ---------------------------------------------------------------------
// `last` is exclusive: one past the last byte to write. Returns 0 on
// success, else the KERNAL error code. (The parameter is `last` rather
// than the header's `end` only in this file; the API is unchanged.)
// ---------------------------------------------------------------------
unsigned char x16_fs_save(const char *name, __mem unsigned char len,
                          __mem unsigned char device,
                          const void *start, const void *last) {
    __mem char r;
    x16__fs_start = (char*)start;       // SAVE reads the start via this zp slot
    asm {
        lda len
        ldx name
        ldy name+1
        jsr $ffbd /*SETNAM*/
        lda #1
        ldx device
        ldy #0                          // secondary 0: no header relocation
        jsr $ffba /*SETLFS*/
        lda #<x16__fs_start             // A = zp offset of the pointer
        ldx last                        // X/Y = end address, exclusive
        ldy last+1
        jsr $ffd8 /*SAVE*/              // carry + A = error
        bcs sv_failed
        lda #0
    sv_failed:
        sta r
    }
    return r;
}

// ---------------------------------------------------------------------
// Load straight into VRAM. Bit 16 of vaddr picks the VRAM bank, which
// turns into LOAD's A register (2 or 3); the secondary address is
// forced to 0 so the PRG header is skipped and the address honoured.
// ---------------------------------------------------------------------
unsigned char x16_fs_vload(const char *name, __mem unsigned char len,
                           __mem unsigned char device,
                           __mem unsigned long vaddr) {
    __mem char r;
    asm {
        lda len
        ldx name
        ldy name+1
        jsr $ffbd /*SETNAM*/
        lda #1
        ldx device
        ldy #0                          // SA = 0: skip the PRG header
        jsr $ffba /*SETLFS*/
        lda vaddr+2
        and #$01
        clc
        adc #2                          // LOAD A: bank 0 -> 2, bank 1 -> 3
        ldx vaddr
        ldy vaddr+1
        jsr $ffd5 /*LOAD*/
        bcs vl_failed
        lda #0
    vl_failed:
        sta r
    }
    return r;
}
