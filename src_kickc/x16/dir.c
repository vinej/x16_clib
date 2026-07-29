// =====================================================================
// x16clib :: x16/dir.c -- walk a device directory
// =====================================================================
// Open the directory, pull one entry at a time, close it. Each entry
// yields a name, a block count and a type.
//
// The listing is BASIC program text, so this parses it: the two-byte
// load address, then per line a link, a line number that IS the block
// count, and text carrying the name in quotes followed by the type.
//
// A zero link ends the listing. The volume header comes back as
// X16_DIR_TYPE_HOST and the trailing "BLOCKS FREE." line as
// X16_DIR_TYPE_NONE with an empty name, rather than being hidden -- a
// caller that wants only files can test the type, and one that wants to
// show the header can.
//
// The assembly here is deliberately NMOS-safe (no stz, no bra): the
// Oscar64 tree is derived from this file and its target has neither.
// =====================================================================

#include <x16/dir.h>

// The caller's name buffer, pinned so the parser can store through it.
// ASM-ONLY: KickC silently drops __address() from a variable C also
// touches, and then (ptr),y assembles against an address in main memory
// (see x16/zpsafe.h). C hands the pointer over through the two bytes
// below instead.
__address(0x78) char* volatile x16__di_buf;

__mem volatile unsigned char x16__di_plo;
__mem volatile unsigned char x16__di_phi;
__mem volatile unsigned char x16__di_size;    // room in that buffer
__mem volatile unsigned char x16__di_dev;

__mem volatile unsigned char x16__di_ty;      // the type of the last entry
__mem volatile unsigned char x16__di_blo;     // its block count
__mem volatile unsigned char x16__di_bhi;
__mem volatile unsigned char x16__di_v;

// Parser scratch: bytes stored, and where in the line we are.
__mem volatile unsigned char x16__di_n;
__mem volatile unsigned char x16__di_st;
__mem volatile unsigned char x16__di_b;
__mem volatile unsigned char x16__di_link;

// "$", the whole-directory pattern.
__mem const char x16__di_dollar[1] = { 0x24 };

// ---------------------------------------------------------------------
// Open a directory. A NULL or empty path lists the whole device.
// Returns 1 if it opened, 0 if not.
//
// Logical file 3, clear of the loader's 1 and the DOS command channel's
// 15. Secondary 0 asks for the directory rather than a file.
// ---------------------------------------------------------------------
unsigned char x16_dir_open(const char *path, unsigned char len,
                           unsigned char device) {
    x16__di_plo = (unsigned char)((unsigned int)path & 0xFF);
    x16__di_phi = (unsigned char)((unsigned int)path >> 8);
    x16__di_size = len;
    x16__di_dev = device;
    asm {
        lda x16__di_size
        bne di_named
        lda #1                          // no path: just "$"
        ldx #<x16__di_dollar
        ldy #>x16__di_dollar
        jmp di_setnam
    di_named:
        ldx x16__di_plo
        ldy x16__di_phi
    di_setnam:
        jsr $ffbd /*SETNAM*/
        lda #3                          // DIR_LFN
        ldx x16__di_dev
        ldy #0                          // the directory, not a file
        jsr $ffba /*SETLFS*/
        jsr $ffc0 /*OPEN*/
        bcs di_openbad
        ldx #3
        jsr $ffc6 /*CHKIN*/
        bcs di_openbad
        jsr di_getb                     // the two load-address bytes,
        bcs di_openbad                  // which are discarded
        jsr di_getb
        bcs di_openbad
        lda #1
        sta x16__di_v
        jmp di_opendone
    di_openbad:
        lda #0
        sta x16__di_v
        jmp di_opendone

        // One byte off the channel; carry set once the stream ends.
    di_getb:
        jsr $ffcf /*CHRIN*/
        sta x16__di_b
        jsr $ffb7 /*READST*/
        cmp #0
        bne di_getb_end
        lda x16__di_b
        clc
        rts
    di_getb_end:
        sec
        rts
    di_opendone:
    }
    return x16__di_v;
}

// ---------------------------------------------------------------------
// The next entry, into `buf` (at most size-1 characters plus a NUL).
// Returns 1 if an entry was read, 0 at the end of the listing.
//
// A name longer than the buffer is truncated, not skipped, and the type
// is still parsed -- so a walk never loses an entry to a short buffer.
// ---------------------------------------------------------------------
unsigned char x16_dir_next(char *buf, unsigned char size) {
    x16__di_plo = (unsigned char)((unsigned int)buf & 0xFF);
    x16__di_phi = (unsigned char)((unsigned int)buf >> 8);
    x16__di_size = size;
    asm {
        lda x16__di_plo                 // hand the buffer to the pinned
        sta x16__di_buf                 // pointer, which C never touches
        lda x16__di_phi
        sta x16__di_buf+1

        lda #0
        sta x16__di_ty                  // NONE until the line says more
        sta x16__di_blo
        sta x16__di_bhi

        // These all bail to di_no, which sits past the whole parser
        // below -- too far for a relative branch, so they go through a
        // trampoline. Oscar64 rejects the long form outright ("Branch
        // target out of range"); mos-as would have taken it silently.
        ldx #3                          // the caller may have used the
        jsr $ffc6 /*CHKIN*/             // channel, so re-select it
        bcs di_bail
        jsr dn_getb                     // the link
        bcs di_bail
        sta x16__di_link
        jsr dn_getb
        bcs di_bail
        ora x16__di_link
        beq di_bail                     // a zero link ends the listing
        jsr dn_getb                     // the line number IS the blocks
        bcs di_bail
        sta x16__di_blo
        jsr dn_getb
        bcs di_bail
        sta x16__di_bhi
        jmp di_parse
    di_bail:
        jmp di_no
    di_parse:

        lda #0
        sta x16__di_n                   // name bytes stored so far
        sta x16__di_st                  // 0 before, 1 inside, 2 after
    di_text:
        jsr dn_getb
        bcs di_endline                  // the file ended: keep what we have
        cmp #0
        beq di_endline                  // and $00 ends the line properly
        ldx x16__di_st
        cpx #1
        beq di_inname
        cpx #2
        beq di_after
        cmp #$22                        // before the name: find the quote
        bne di_text
        inc x16__di_st
        jmp di_text
    di_inname:
        cmp #$22                        // the closing quote ends the name
        beq di_closed
        ldx x16__di_n
        inx
        cpx x16__di_size                // room for this byte AND a NUL?
        bcs di_text                     // no: drop it, keep parsing
        ldy x16__di_n                   // CHRIN may clobber Y, so load it
        sta (x16__di_buf),y             // here, not across the call
        inc x16__di_n
        jmp di_text
    di_closed:
        lda #2
        sta x16__di_st
        jmp di_text
    di_after:
        cmp #$20                        // the first non-space after the
        beq di_text                     // name is the type
        ldx x16__di_ty
        bne di_text                     // this line is already classified
        jsr di_classify
        jmp di_text
    di_endline:
        ldy x16__di_n
        lda #0
        sta (x16__di_buf),y             // NUL, inside the buffer
        lda #1
        sta x16__di_v
        jmp di_nextdone
    di_no:
        lda #0
        sta x16__di_v
        jmp di_nextdone

    di_classify:
        cmp #$50                        // 'P'
        beq di_t_prg
        cmp #$53                        // 'S'
        beq di_t_seq
        cmp #$55                        // 'U'
        beq di_t_usr
        cmp #$52                        // 'R'
        beq di_t_rel
        cmp #$44                        // 'D'
        beq di_t_dir
        cmp #$48                        // 'H'
        beq di_t_host
        rts
    di_t_prg:
        lda #1
        jmp di_setty
    di_t_seq:
        lda #2
        jmp di_setty
    di_t_usr:
        lda #3
        jmp di_setty
    di_t_rel:
        lda #4
        jmp di_setty
    di_t_dir:
        lda #5
        jmp di_setty
    di_t_host:
        lda #6
    di_setty:
        sta x16__di_ty
        rts

    dn_getb:
        jsr $ffcf /*CHRIN*/
        sta x16__di_b
        jsr $ffb7 /*READST*/
        cmp #0
        bne dn_getb_end
        lda x16__di_b
        clc
        rts
    dn_getb_end:
        sec
        rts
    di_nextdone:
    }
    return x16__di_v;
}

// ---------------------------------------------------------------------
// What the entry x16_dir_next() just read is, and how many blocks it
// occupies. Both describe the LAST entry read.
// ---------------------------------------------------------------------
unsigned char x16_dir_type(void) {
    return x16__di_ty;
}

unsigned int x16_dir_blocks(void) {
    return (unsigned int)x16__di_blo | ((unsigned int)x16__di_bhi << 8);
}

// ---------------------------------------------------------------------
// Close the directory channel. Safe to call even if the open failed.
// ---------------------------------------------------------------------
void x16_dir_close(void) {
    asm {
        jsr $ffcc /*CLRCHN*/
        lda #3
        jsr $ffc3 /*CLOSE*/
    }
}
