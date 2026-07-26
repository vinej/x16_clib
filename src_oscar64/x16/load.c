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
// given by its zp offset in A -- so this slot must be real zero page
// (__zeropage places it in Oscar64's $F7-$FF user region). The end
// out-parameter is indirected straight through the parameter.
__zeropage volatile const char* x16__fs_start;

volatile char x16__fs_e0;         // end address handed back by LOAD
volatile char x16__fs_e1;

void x16_fs_setname(const char *name, unsigned char len) {
    __asm {
        lda len
        ldx name
        ldy name+1
        jsr 0xffbd                      /* SETNAM */
    }
}

// ---------------------------------------------------------------------
// Returns 0 on success, else the KERNAL error code. *end receives the
// address one past the last byte loaded; end may be NULL.
// ---------------------------------------------------------------------
unsigned char x16_fs_load(const char *name, unsigned char len,
                          unsigned char device, unsigned char sa,
                          void *dest, unsigned int *end) {
    return __asm {
        lda len
        ldx name
        ldy name+1
        jsr 0xffbd                      /* SETNAM */
        lda #1                          /* logical file number */
        ldx device
        ldy sa
        jsr 0xffba                      /* SETLFS */
        lda #0                          /* LOAD A = 0: into system RAM */
        ldx dest
        ldy dest+1
        jsr 0xffd5              /* carry + A = error, X/Y = end (LOAD) */
        stx x16__fs_e0                  /* neither store touches the flags */
        sty x16__fs_e1
        php                             /* the carry is the success signal */
        pha
        lda end
        ora end+1
        beq fl_no_out                   /* end == NULL: caller doesn't care */
        ldy #0
        lda x16__fs_e0
        sta (end),y
        iny
        lda x16__fs_e1
        sta (end),y
    fl_no_out:
        pla                             /* A = KERNAL error code */
        plp                             /* carry set = it failed */
        bcs fl_failed
        lda #0                          /* success */
    fl_failed:
        sta accu
    };
}

// ---------------------------------------------------------------------
// `last` is exclusive: one past the last byte to write. Returns 0 on
// success, else the KERNAL error code. (The parameter is `last` rather
// than the header's `end` only in this file; the API is unchanged.)
// ---------------------------------------------------------------------
unsigned char x16_fs_save(const char *name, unsigned char len,
                          unsigned char device,
                          const void *start, const void *last) {
    x16__fs_start = (char*)start;       // SAVE reads the start via this zp slot
    return __asm {
        lda len
        ldx name
        ldy name+1
        jsr 0xffbd                      /* SETNAM */
        lda #1
        ldx device
        ldy #0                          /* secondary 0: no header relocation */
        jsr 0xffba                      /* SETLFS */
        lda #<x16__fs_start             /* A = zp offset of the pointer */
        ldx last                        /* X/Y = end address, exclusive */
        ldy last+1
        jsr 0xffd8              /* carry + A = error (SAVE) */
        bcs sv_failed
        lda #0
    sv_failed:
        sta accu
    };
}

// ---------------------------------------------------------------------
// Load straight into VRAM. Bit 16 of vaddr picks the VRAM bank, which
// turns into LOAD's A register (2 or 3); the secondary address is
// forced to 0 so the PRG header is skipped and the address honoured.
// ---------------------------------------------------------------------
unsigned char x16_fs_vload(const char *name, unsigned char len,
                           unsigned char device,
                           unsigned long vaddr) {
    return __asm {
        lda len
        ldx name
        ldy name+1
        jsr 0xffbd                      /* SETNAM */
        lda #1
        ldx device
        ldy #0                          /* SA = 0: skip the PRG header */
        jsr 0xffba                      /* SETLFS */
        lda vaddr+2
        and #0x01
        clc
        adc #2                          /* LOAD A: bank 0 -> 2, bank 1 -> 3 */
        ldx vaddr
        ldy vaddr+1
        jsr 0xffd5                      /* LOAD */
        bcs vl_failed
        lda #0
    vl_failed:
        sta accu
    };
}

// ---------------------------------------------------------------------
// The SYS address out of a PRG's BASIC stub, read without loading the
// file. A launcher has to know where to JSR before it hands the machine
// over, and loading the file to find out is the one thing it cannot do:
// the load would overwrite the launcher asking the question. So this
// reads the first few bytes off the disk and parses the stub in place:
//
//   two load-address bytes, two link bytes, two line-number bytes,
//   the SYS token ($9E), any spaces, then the address in ASCII.
//
// The address is read rather than assumed -- a compiler emitting
// "SYS 2071" today moves that number the moment the stub text changes.
// 0 doubles as "no entry here", since no PRG can start there.
//
// Uses logical file 1, as x16_fs_load does, on secondary address 2 so
// the bytes arrive raw rather than as a program to relocate.
//
// Oscar64's inline assembler is NMOS-only, so the result is zeroed with
// lda #0 / sta rather than stz, and pe_quit jumps past pe_getb.
// ---------------------------------------------------------------------
volatile char x16__pe_lo;               // the result, built a digit at a time
volatile char x16__pe_hi;
volatile char x16__pe_a;                // *10 scratch: the old low byte
volatile char x16__pe_b;                // ...and the old high byte
volatile char x16__pe_d;                // the digit being added
volatile char x16__pe_c;                // bytes left to skip
volatile char x16__pe_g;                // the byte pe_getb read

unsigned int x16_fs_prg_entry(const char *name, unsigned char len,
                              unsigned char device) {
    return __asm {
        lda #0
        sta x16__pe_lo
        sta x16__pe_hi
        lda len
        ldx name
        ldy name+1
        jsr 0xffbd                      /* SETNAM */
        lda #1                          /* logical file */
        ldx device
        ldy #2                          /* a plain data channel */
        jsr 0xffba                      /* SETLFS */
        jsr 0xffc0                      /* OPEN */
        bcs pe_quit
        ldx #1
        jsr 0xffc6                      /* CHKIN */
        bcs pe_quit

        lda #6                          /* load address, link, line number */
        sta x16__pe_c           /* CHRIN may clobber Y, so not there */
    pe_skip:
        jsr pe_getb
        bcs pe_quit
        dec x16__pe_c
        bne pe_skip

        jsr pe_getb                     /* the SYS token */
        bcs pe_quit
        cmp #0x9e
        bne pe_quit
    pe_space:
        jsr pe_getb
        bcs pe_quit
        cmp #0x20
        beq pe_space
                                /* A = the first character after them */
    pe_digit:
        cmp #0x30               /* a non-digit ends the number, and */
        bcc pe_quit             /* ending it before it starts leaves 0 */
        cmp #0x3a
        bcs pe_quit

        sec
        sbc #0x30
        sta x16__pe_d

        lda x16__pe_lo                  /* res = res * 10 + digit, */
        sta x16__pe_a           /* taking *10 as ((r * 4) + r) * 2 */
        lda x16__pe_hi
        sta x16__pe_b
        asl x16__pe_lo
        rol x16__pe_hi
        asl x16__pe_lo
        rol x16__pe_hi
        clc
        lda x16__pe_lo
        adc x16__pe_a
        sta x16__pe_lo
        lda x16__pe_hi
        adc x16__pe_b
        sta x16__pe_hi
        asl x16__pe_lo
        rol x16__pe_hi
        clc
        lda x16__pe_lo
        adc x16__pe_d
        sta x16__pe_lo
        lda x16__pe_hi
        adc #0
        sta x16__pe_hi

        jsr pe_getb
        bcc pe_digit            /* ran out of file: keep what we have */

    pe_quit:
        jsr 0xffcc                      /* CLRCHN */
        lda #1
        jsr 0xffc3                      /* CLOSE */
        jmp pe_done                     /* do not fall into pe_getb */

        /* one byte from the open channel; carry set if the file ended */
    pe_getb:
        jsr 0xffcf                      /* CHRIN */
        sta x16__pe_g
        jsr 0xffb7                      /* READST */
        cmp #0
        bne pe_getb_end
        lda x16__pe_g
        clc
        rts
    pe_getb_end:
        sec
        rts

    pe_done:
        lda x16__pe_lo
        sta accu
        lda x16__pe_hi
        sta accu+1
    };
}
