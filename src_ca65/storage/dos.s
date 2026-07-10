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

        .import         popa, popax

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

        .export         _x16_dos_set_device
        .export         _x16_dos_msg
        .export         _x16_dos_cmd
        .export         _x16_dos_status
        .export         _x16_dos_delete
        .export         _x16_dos_mkdir
        .export         _x16_dos_rmdir
        .export         _x16_dos_chdir
        .export         _x16_dos_rename

DOS_MSG_MAX = 64

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_dos_set_device(unsigned char device)
; const char *x16_dos_msg(void)
;   The reply text of the last command, NUL-terminated. Overwritten by
;   the next call, so copy it if you need to keep it.
; ---------------------------------------------------------------------
_x16_dos_set_device:
        sta     dos_device
        rts

_x16_dos_msg:
        lda     #<dos_msg
        ldx     #>dos_msg
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_dos_cmd(const char *cmd, unsigned char len)
;   A length of 0 sends nothing and just reads the pending status.
;
; popa/popax clobber Y, so the length rides the hardware stack across the
; pointer pop and lands in Y at the last moment.
; ---------------------------------------------------------------------
_x16_dos_cmd:
        pha                             ; len (rightmost arg, in A)
        jsr     popax                   ; cmd: A = low, X = high
        ply                             ; Y = len
        jsr     dos_cmd
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; unsigned char x16_dos_status(void)
; ---------------------------------------------------------------------
_x16_dos_status:
        jsr     dos_status
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_dos_delete(const char *name, unsigned char len)
; ...and mkdir / rmdir / chdir, all with the same shape.
; ---------------------------------------------------------------------
_x16_dos_delete:
        jsr     name_marshal
        jsr     dos_delete
        ldx     #0
        rts

_x16_dos_mkdir:
        jsr     name_marshal
        jsr     dos_mkdir
        ldx     #0
        rts

_x16_dos_rmdir:
        jsr     name_marshal
        jsr     dos_rmdir
        ldx     #0
        rts

_x16_dos_chdir:
        jsr     name_marshal
        jsr     dos_chdir
        ldx     #0
        rts

; in:  A = len, one pointer on the C stack
; out: A/X = name, Y = len
name_marshal:
        pha
        jsr     popax
        ply
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_dos_rename(const char *oldname,
;                                           unsigned char oldlen,
;                                           const char *newname,
;                                           unsigned char newlen)
;   Renames `oldname` to `newname`:  R:new=old
; ---------------------------------------------------------------------
_x16_dos_rename:
        pha                             ; newlen (rightmost arg, in A)
        jsr     popax
        sta     X16_T5                  ; newname -- dos_rename owns T0..T2
        stx     X16_T6
        jsr     popa
        sta     X16_P2                  ; oldlen
        jsr     popax
        sta     X16_P0                  ; oldname
        stx     X16_P1
        lda     X16_T5
        ldx     X16_T6
        ply                             ; Y = newlen
        jsr     dos_rename
        ldx     #0
        rts

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
        bcs     @no_channel

        ldx     #15
        jsr     CHKIN
        bcs     @no_channel

        ldy     #0
@read:
        jsr     CHRIN
        cmp     #$0D                    ; the status line ends with a CR
        beq     @got
        cpy     #(DOS_MSG_MAX - 1)
        bcs     @skip                   ; overlong: keep draining, stop storing
        sta     dos_msg,y
        iny
@skip:
        jsr     READST
        beq     @read                   ; keep going while the stream is alive
@got:
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

@no_channel:
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
@old:
        cpy     X16_P2
        beq     @send
        lda     (X16_P0),y
        sta     dos_cmdbuf,x
        inx
        iny
        bra     @old
@send:
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
@cp:
        cpy     X16_T2
        beq     @done
        lda     (X16_T0),y
        sta     dos_cmdbuf,x
        inx
        iny
        bra     @cp
@done:
        rts

; The device has a nonzero default, so it lives in DATA rather than BSS.
        .segment        "DATA"

dos_device: .byte 8

        .segment        "BSS"

dos_msg:    .res DOS_MSG_MAX
dos_cmdbuf: .res 80
