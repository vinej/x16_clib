; =====================================================================
; x16clib :: storage/dos.s -- the DOS command channel
; =====================================================================
; fs_load/fs_save report failure, but never WHY. The answer lives on
; channel 15: every command sent there is answered with a status line
; like "62,FILE NOT FOUND,00,00". These routines send commands, read that
; line, and hand back the numeric code -- codes below 20 are success, 20
; and up are errors, exactly CBM DOS's convention.
;
; The device defaults to 8.
;
; cc65's <cbm.h> gives opendir/readdir and cbm_open, but nothing anywhere
; exposes the command channel, so a failed save cannot say why.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X, then whatever __rc bytes the pointers
;   left free. So f(ptr, int, char) is ptr in __rc2/3, int in A/X, char in
;   __rc4.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

; ---------------------------------------------------------------------
; ca65 -t cx16 TRANSLATES CHARACTER LITERALS TO PETSCII. ACME did not.
;
; `lda #'S'` assembles to $D3, not $53, and the drive answers
; "30,SYNTAX ERROR". Every byte that goes to the command channel, or that
; is compared against one coming back, is therefore written as its
; explicit value here.
; ---------------------------------------------------------------------
CH_S     = $53                          ; 'S' -- scratch
CH_M     = $4D                          ; 'M'
CH_R     = $52                          ; 'R' -- rename, and RD:
CH_C     = $43                          ; 'C'
CH_D     = $44                          ; 'D'
CH_COLON = $3A                          ; ':'
CH_EQ    = $3D                          ; '='
CH_ZERO  = $30                          ; '0'

        .globl  x16_dos_set_device
        .globl  x16_dos_msg
        .globl  x16_dos_cmd
        .globl  x16_dos_status
        .globl  x16_dos_delete
        .globl  x16_dos_mkdir
        .globl  x16_dos_rmdir
        .globl  x16_dos_chdir
        .globl  x16_dos_rename

DOS_MSG_MAX = 64

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_dos_set_device(unsigned char device)
; const char *x16_dos_msg(void)
;   The reply text of the last command, NUL-terminated. Overwritten by
;   the next call, so copy it if you need to keep it.
; ---------------------------------------------------------------------
x16_dos_set_device:
        sta     dos_device
        rts

; Returns `const char *`, and llvm-mos hands a POINTER back in __rc2/__rc3
; rather than A/X. Getting this wrong returns whatever was in __rc2 --
; usually a plausible-looking address.
x16_dos_msg:
        lda     #<dos_msg
        sta     __rc2
        lda     #>dos_msg
        sta     __rc3
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_dos_cmd(const char *cmd, unsigned char len)
;   A length of 0 sends nothing and just reads the pending status.
;
; ---------------------------------------------------------------------
; cmd is a pointer (__rc2/__rc3); len is the only integer, so it is in A.
; dos_cmd wants A/X = cmd, Y = len.
x16_dos_cmd:
        tay                             ; Y = len
        lda     __rc2
        ldx     __rc3
        jmp     dos_cmd                 ; status comes back in A

; ---------------------------------------------------------------------
; unsigned char x16_dos_status(void)
; ---------------------------------------------------------------------
x16_dos_status:
        jmp     dos_status

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_dos_delete(const char *name, unsigned char len)
; ...and mkdir / rmdir / chdir, all with the same shape.
; ---------------------------------------------------------------------
x16_dos_delete:
        jsr     name_marshal
        jmp     dos_delete

x16_dos_mkdir:
        jsr     name_marshal
        jmp     dos_mkdir

x16_dos_rmdir:
        jsr     name_marshal
        jmp     dos_rmdir

x16_dos_chdir:
        jsr     name_marshal
        jmp     dos_chdir

; in:  name in __rc2/__rc3 (a pointer), len in A
; out: A/X = name, Y = len
name_marshal:
        tay                             ; Y = len
        lda     __rc2
        ldx     __rc3                   ; A/X = name
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_dos_rename(const char *oldname,
;                                           unsigned char oldlen,
;                                           const char *newname,
;                                           unsigned char newlen)
;   Renames `oldname` to `newname`:  R:new=old
; ---------------------------------------------------------------------
; oldname -> __rc2/__rc3, newname -> __rc4/__rc5 (pointers, in order);
; oldlen -> A, newlen -> X (the integers). dos_rename wants
; X16_P0/P1 = oldname, X16_P2 = oldlen, A/X = newname, Y = newlen.
x16_dos_rename:
        sta     X16_P2                  ; oldlen
        txa
        tay                             ; Y = newlen
        lda     __rc2
        sta     X16_P0                  ; oldname
        lda     __rc3
        sta     X16_P1
        lda     __rc4                   ; A/X = newname
        ldx     __rc5
        jmp     dos_rename

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; dos_cmd -- send a raw DOS command and fetch the reply
;   in:  A = command low, X = command high, Y = length (0 = none: just
;        read the pending status)
;   out: A = status code (0-99; 255 if the channel would not open)
;        carry set when the code is an error (>= 20)
;        dos_msg = the full reply, NUL-terminated; Y = its length
; ---------------------------------------------------------------------
dos_cmd:
        sta     X16_T0
        stx     X16_T1
        tya                             ; SETNAM wants A = length, X/Y = address
        ldx     X16_T0
        ldy     X16_T1
        jsr     SETNAM

        lda     #15
        ldx     dos_device
        ldy     #15                     ; secondary 15: the command channel
        jsr     SETLFS
        jsr     OPEN
        bcs     .Ldos_cmd_no_channel

        ldx     #15
        jsr     CHKIN
        bcs     .Ldos_cmd_no_channel

        ldy     #0
.Ldos_cmd_read:
        jsr     CHRIN
        cmp     #$0D                    ; the status line ends with a CR
        beq     .Ldos_cmd_got
        cpy     #(DOS_MSG_MAX - 1)
        bcs     .Ldos_cmd_skip                   ; overlong: keep draining, stop storing
        sta     dos_msg,y
        iny
.Ldos_cmd_skip:
        jsr     READST
        beq     .Ldos_cmd_read                   ; keep going while the stream is alive
.Ldos_cmd_got:
        lda     #0
        sta     dos_msg,y
        phy
        jsr     CLRCHN
        lda     #15
        jsr     CLOSE
        ply

        ; the code is the first two digits: "62,FILE NOT FOUND,..."
        lda     dos_msg
        sec
        sbc     #CH_ZERO
        sta     X16_T0
        asl     a                       ; *10 = *8 + *2
        asl     a
        adc     X16_T0
        asl     a
        sta     X16_T0
        lda     dos_msg+1
        sec
        sbc     #CH_ZERO
        clc
        adc     X16_T0
        cmp     #20                     ; carry set = error class
        rts

.Ldos_cmd_no_channel:
        jsr     CLRCHN
        lda     #15
        jsr     CLOSE
        stz     dos_msg
        ldy     #0
        lda     #$FF
        sec
        rts

; ---------------------------------------------------------------------
; dos_status -- read the drive's pending status line
;   out: as dos_cmd. Note the first read after power-on returns code 73
;        (the DOS version banner) by design.
; ---------------------------------------------------------------------
dos_status:
        lda     #0
        tax
        tay
        jmp     dos_cmd

; ---------------------------------------------------------------------
; One-call wrappers. Each takes A = name low, X = name high, Y = name
; length, and returns like dos_cmd.
;
;   dos_delete   S:name       scratch a file
;   dos_mkdir    MD:name      make a directory
;   dos_rmdir    RD:name      remove a directory
;   dos_chdir    CD:name      change directory ("//" is the root)
;
; dos_rename additionally takes the OLD name in X16_P0/P1 with its length
; in X16_P2, and renames it to the A/X/Y name:  R:new=old
; ---------------------------------------------------------------------
dos_delete:
        jsr     stash_name
        lda     #CH_S
        sta     dos_cmdbuf
        lda     #CH_COLON
        sta     dos_cmdbuf+1
        ldx     #2
        bra     append_send

dos_mkdir:
        jsr     stash_name
        lda     #CH_M
        bra     dir_cmd
dos_rmdir:
        jsr     stash_name
        lda     #CH_R
        bra     dir_cmd
dos_chdir:
        jsr     stash_name
        lda     #CH_C
dir_cmd:
        sta     dos_cmdbuf
        lda     #CH_D
        sta     dos_cmdbuf+1
        lda     #CH_COLON
        sta     dos_cmdbuf+2
        ldx     #3
        bra     append_send

dos_rename:
        jsr     stash_name              ; the NEW name
        lda     #CH_R
        sta     dos_cmdbuf
        lda     #CH_COLON
        sta     dos_cmdbuf+1
        ldx     #2
        jsr     append                  ; R:new
        lda     #CH_EQ
        sta     dos_cmdbuf,x
        inx
        ldy     #0                      ; append the OLD name from X16_P0..P2
.Ldos_rename_old:
        cpy     X16_P2
        beq     .Ldos_rename_send
        lda     (X16_P0),y
        sta     dos_cmdbuf,x
        inx
        iny
        bra     .Ldos_rename_old
.Ldos_rename_send:
        bra     send

; park A/X/Y (name pointer + length) in T0/T1/T2
stash_name:
        sta     X16_T0
        stx     X16_T1
        sty     X16_T2
        rts

; copy the stashed name into dos_cmdbuf at X, then send; X advances
append_send:
        jsr     append
send:
        txa
        tay                             ; Y = total command length
        lda     #<dos_cmdbuf
        ldx     #>dos_cmdbuf
        jmp     dos_cmd

append:
        ldy     #0
.Lappend_cp:
        cpy     X16_T2
        beq     .Lappend_done
        lda     (X16_T0),y
        sta     dos_cmdbuf,x
        inx
        iny
        bra     .Lappend_cp
.Lappend_done:
        rts

; The device has a nonzero default, so it lives in DATA rather than BSS.
        .section .data,"aw",@progbits

dos_device: .byte 8

        .section .bss,"aw",@nobits

dos_msg:    .zero  DOS_MSG_MAX
dos_cmdbuf: .zero  80
