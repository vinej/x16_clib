// =====================================================================
// x16clib :: x16/dos.c -- the DOS command channel
// =====================================================================
// fs_load/fs_save report failure, but never WHY. The answer lives on
// channel 15: every command sent there is answered with a status line
// like "62,FILE NOT FOUND,00,00". These send commands, read that line,
// and hand back the numeric code -- below 20 is success, 20 and up are
// errors, exactly CBM DOS's convention. Same code as
// src_ca65/storage/dos.s.
//
// The command strings are assembled in plain C here -- the ca65 build
// had to do it in assembly with explicit byte values because its cx16
// charmap turns 'S' into PETSCII $D3. KickC's default encoding has the
// same trap, so the bytes stay explicit hex.
// =====================================================================

#include <x16/dos.h>

__mem volatile char x16__dos_device = 8;
__mem volatile char x16__dos_t;
__mem char x16__dos_msg[64];
__mem char x16__dos_buf[80];

void x16_dos_set_device(__mem unsigned char device) {
    x16__dos_device = device;
}

// The reply text of the last command, NUL-terminated. Overwritten by
// the next call, so copy it if you need to keep it.
const char *x16_dos_msg(void) {
    return x16__dos_msg;
}

// ---------------------------------------------------------------------
// Send a raw DOS command and fetch the reply. A length of 0 sends
// nothing and just reads the pending status. Returns the status code
// (0-99; 255 if the channel would not open).
// ---------------------------------------------------------------------
unsigned char x16_dos_cmd(const char *cmd, __mem unsigned char len) {
    __mem char r;
    asm {
        lda len                         // SETNAM: the command IS the name
        ldx cmd
        ldy cmd+1
        jsr $ffbd /*SETNAM*/
        lda #15
        ldx x16__dos_device
        ldy #15                         // secondary 15: the command channel
        jsr $ffba /*SETLFS*/
        jsr $ffc0 /*OPEN*/
        bcs dc_no_channel
        ldx #15
        jsr $ffc6 /*CHKIN*/
        bcs dc_no_channel

        ldy #0
    dc_read:
        jsr $ffcf /*CHRIN*/
        cmp #$0d                        // the status line ends with a CR
        beq dc_got
        cpy #63                         // DOS_MSG_MAX - 1
        bcs dc_skip                     // overlong: drain, stop storing
        sta x16__dos_msg,y
        iny
    dc_skip:
        jsr $ffb7 /*READST*/
        beq dc_read                     // while the stream is alive
    dc_got:
        lda #0
        sta x16__dos_msg,y
        jsr $ffcc /*CLRCHN*/
        lda #15
        jsr $ffc3 /*CLOSE*/

        // the code is the first two digits: "62,FILE NOT FOUND,..."
        lda x16__dos_msg
        sec
        sbc #$30                        // '0'
        sta x16__dos_t
        asl                             // *10 = *8 + *2
        asl
        adc x16__dos_t
        asl
        sta x16__dos_t
        lda x16__dos_msg+1
        sec
        sbc #$30
        clc
        adc x16__dos_t
        sta r
        bra dc_end

    dc_no_channel:
        jsr $ffcc /*CLRCHN*/
        lda #15
        jsr $ffc3 /*CLOSE*/
        stz x16__dos_msg
        lda #$ff
        sta r
    dc_end:
    }
    return r;
}

// Read the drive's pending status line. Note the first read after
// power-on returns code 73 (the DOS version banner) by design.
unsigned char x16_dos_status(void) {
    return x16_dos_cmd(0, 0);
}

// Internal: append `len` bytes of `name` to the command buffer at `at`,
// then send. Plain C -- KickC manages its own pointer access here.
unsigned char x16__dos_name_cmd(const char *name, unsigned char len,
                                unsigned char at) {
    unsigned char i;
    for (i = 0; i < len; i++) {
        x16__dos_buf[at + i] = name[i];
    }
    return x16_dos_cmd(x16__dos_buf, at + len);
}

// S:name -- scratch a file
unsigned char x16_dos_delete(const char *name, unsigned char len) {
    x16__dos_buf[0] = 0x53;             // 'S'
    x16__dos_buf[1] = 0x3A;             // ':'
    return x16__dos_name_cmd(name, len, 2);
}

// MD:name
unsigned char x16_dos_mkdir(const char *name, unsigned char len) {
    x16__dos_buf[0] = 0x4D;             // 'M'
    x16__dos_buf[1] = 0x44;             // 'D'
    x16__dos_buf[2] = 0x3A;             // ':'
    return x16__dos_name_cmd(name, len, 3);
}

// RD:name
unsigned char x16_dos_rmdir(const char *name, unsigned char len) {
    x16__dos_buf[0] = 0x52;             // 'R'
    x16__dos_buf[1] = 0x44;             // 'D'
    x16__dos_buf[2] = 0x3A;             // ':'
    return x16__dos_name_cmd(name, len, 3);
}

// CD:name ("//" is the root)
unsigned char x16_dos_chdir(const char *name, unsigned char len) {
    x16__dos_buf[0] = 0x43;             // 'C'
    x16__dos_buf[1] = 0x44;             // 'D'
    x16__dos_buf[2] = 0x3A;             // ':'
    return x16__dos_name_cmd(name, len, 3);
}

// R:new=old
unsigned char x16_dos_rename(const char *oldname, unsigned char oldlen,
                             const char *newname, unsigned char newlen) {
    unsigned char i;
    unsigned char at;

    x16__dos_buf[0] = 0x52;             // 'R'
    x16__dos_buf[1] = 0x3A;             // ':'
    at = 2;
    for (i = 0; i < newlen; i++) {
        x16__dos_buf[at] = newname[i];
        at++;
    }
    x16__dos_buf[at] = 0x3D;            // '='
    at++;
    for (i = 0; i < oldlen; i++) {
        x16__dos_buf[at] = oldname[i];
        at++;
    }
    return x16_dos_cmd(x16__dos_buf, at);
}
