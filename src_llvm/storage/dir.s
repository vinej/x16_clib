; =====================================================================
; x16clib :: storage/dir.s -- reading a directory
; =====================================================================
; A drive hands its directory over as a BASIC program listing, which is
; a peculiar thing to have to parse but is what every CBM drive does:
;
;       load address (2)
;       link (2)  blocks (2)  text... $00      <- one entry
;       link (2)  blocks (2)  text... $00
;       $00 $00                                <- end
;
; The "line number" is the block count, and the text carries the name in
; quotes followed by its type:
;
;       "GAME.PRG"        PRG
;       "LEVELS"          DIR
;
; These routines walk that so a caller never sees it. The header line
; comes back as DIR_TYPE_HOST and the trailing "BLOCKS FREE." line as
; DIR_TYPE_NONE with an empty name, rather than being hidden -- a file
; browser wants to skip them, a disk info panel wants to show them, and
; this way neither has to re-parse anything.
;
; cc65's <cbm.h> has cbm_opendir/cbm_readdir over its own file table;
; these stand alone on LFN 3 and also classify the header and trailer.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; (import dropped: popa, popax)

        .globl  x16_dir_open
        .globl  x16_dir_next
        .globl  x16_dir_type
        .globl  x16_dir_blocks
        .globl  x16_dir_close

DIR_LFN = 3                     ; logical file: clear of fs_load's 1 and
                                ; of the command channel's 15

DIR_TYPE_NONE = 0               ; no name on the line: "BLOCKS FREE."
DIR_TYPE_PRG  = 1
DIR_TYPE_SEQ  = 2
DIR_TYPE_USR  = 3
DIR_TYPE_REL  = 4
DIR_TYPE_DIR  = 5
DIR_TYPE_HOST = 6               ; the header line naming the volume

; ---------------------------------------------------------------------
; ca65 -t cx16 TRANSLATES CHARACTER LITERALS TO PETSCII. Every byte
; compared against one coming off the bus (or sent to the drive) is
; therefore written as its explicit value here, as in storage/dos.s.
; ---------------------------------------------------------------------
CH_DOLLAR = $24                 ; '$'
CH_QUOTE  = $22                 ; '"'
CH_SPACE  = $20                 ; ' '
CH_P      = $50
CH_S      = $53
CH_U      = $55
CH_R      = $52
CH_D      = $44
CH_H      = $48

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_dir_open(const char *path,
;                                         unsigned char len,
;                                         unsigned char device)
;   1 if the directory opened, 0 if not.
; ---------------------------------------------------------------------
x16_dir_open:
        sta     mos8(X16_P2)            ; len: the first integer byte, A
        stx     mos8(X16_P3)            ; device: the next, X
        lda     mos8(__rc2)             ; path: the pointer pair
        sta     mos8(X16_P0)
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        jsr     dir_open                ; carry set = could not open
        lda     #0
        ldx     #0
        rol     a
        eor     #1                      ; report opened, not failed
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_dir_next(char *buf, unsigned char size)
;   1 if an entry was read, 0 at the end of the listing.
; ---------------------------------------------------------------------
x16_dir_next:
        sta     mos8(X16_P2)            ; size: the first integer byte, A
        lda     mos8(__rc2)             ; buf: the pointer pair
        sta     mos8(X16_P0)
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        jsr     dir_next                ; carry SET = an entry was read
        lda     #0
        ldx     #0
        rol     a
        rts

; ---------------------------------------------------------------------
; unsigned char x16_dir_type(void)
; unsigned int x16_dir_blocks(void)
; void x16_dir_close(void)
; ---------------------------------------------------------------------
x16_dir_type:
        lda     dir_ty
        ldx     #0
        rts

x16_dir_blocks:
        lda     dir_blk
        ldx     dir_blk+1
        rts

x16_dir_close:
        jmp     dir_close

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; dir_open -- open a directory for reading
;   in:  X16_P0/P1 = path address, X16_P2 = path length
;        (a length of 0 asks for "$", the current directory)
;        X16_P3    = device (usually 8)
;   out: carry set if the directory could not be opened
; ---------------------------------------------------------------------
dir_open:
        lda     mos8(X16_P2)
        bne     dir_named
        lda     #1                      ; no path given: just "$"
        ldx     #<dir_dollar
        ldy     #>dir_dollar
        bra     dir_setnam
dir_named:
        ldx     mos8(X16_P0)
        ldy     mos8(X16_P1)
dir_setnam:
        jsr     SETNAM
        lda     #DIR_LFN
        ldx     mos8(X16_P3)
        ldy     #0                      ; secondary 0: the directory, not a file
        jsr     SETLFS
        jsr     OPEN
        bcs     dir_openbad
        ldx     #DIR_LFN
        jsr     CHKIN
        bcs     dir_openbad
        jsr     dir_getb                ; the two load-address bytes, discarded
        bcs     dir_openbad
        jsr     dir_getb
        bcs     dir_openbad
        clc
        rts
dir_openbad:
        sec
        rts

; ---------------------------------------------------------------------
; dir_next -- read the next entry
;   in:  X16_P0/P1 = a buffer for the name, X16_P2 = its size (2-255)
;   out: carry SET if an entry was read, CLEAR at the end of the listing
;
; The name arrives NUL-terminated and truncated to fit -- the buffer
; size is honoured, so a long name cannot walk off the end of it.
; dir_type and dir_blocks then describe the entry just read.
; ---------------------------------------------------------------------
dir_next:
        stz     dir_ty                  ; DIR_TYPE_NONE until the line says more
        stz     dir_blk
        stz     dir_blk+1

        ldx     #DIR_LFN                ; the caller may have used the channel
        jsr     CHKIN                   ; in between, so re-select it every time
        bcs     dir_no

        jsr     dir_getb                ; link
        bcs     dir_no
        sta     mos8(X16_T0)
        jsr     dir_getb
        bcs     dir_no
        ora     mos8(X16_T0)
        beq     dir_no                  ; a zero link is the end of the listing

        jsr     dir_getb                ; the line number is the block count
        bcs     dir_no
        sta     dir_blk
        jsr     dir_getb
        bcs     dir_no
        sta     dir_blk+1

        stz     mos8(X16_T1)            ; name bytes stored so far
        stz     mos8(X16_T2)            ; 0 before the name, 1 inside, 2 after
dir_text:
        jsr     dir_getb
        bcs     dir_endline             ; the file ended: keep what we have
        cmp     #0
        beq     dir_endline             ; and $00 ends the line properly
        ldx     mos8(X16_T2)
        cpx     #1
        beq     dir_inname
        cpx     #2
        beq     dir_after
        cmp     #CH_QUOTE               ; before the name: find the quote
        bne     dir_text
        inc     mos8(X16_T2)
        bra     dir_text

dir_inname:
        cmp     #CH_QUOTE               ; the closing quote ends the name
        beq     dir_closed
        ldx     mos8(X16_T1)
        inx
        cpx     mos8(X16_P2)            ; room for this byte AND a terminator?
        bcs     dir_text                ; no: drop it, but keep parsing the type
        ldy     mos8(X16_T1)            ; CHRIN is free to clobber Y, so load it
        sta     (X16_P0),y              ; here rather than holding it across
        inc     mos8(X16_T1)
        bra     dir_text
dir_closed:
        lda     #2
        sta     mos8(X16_T2)
        bra     dir_text

dir_after:
        cmp     #CH_SPACE               ; the first non-space after the name is
        beq     dir_text                ; the type
        ldx     dir_ty
        bne     dir_text                ; already classified this line
        jsr     dir_classify
        bra     dir_text

dir_endline:
        ldy     mos8(X16_T1)
        lda     #0
        sta     (X16_P0),y              ; NUL-terminate within the buffer
        sec                             ; an entry was read
        rts
dir_no:
        clc
        rts

; The first letter is enough: PRG, SEQ, USR, REL, DIR and HOST do not
; collide. A suffix like PRG< (locked) classifies the same way.
dir_classify:
        cmp     #CH_P
        beq     dir_t_prg
        cmp     #CH_S
        beq     dir_t_seq
        cmp     #CH_U
        beq     dir_t_usr
        cmp     #CH_R
        beq     dir_t_rel
        cmp     #CH_D
        beq     dir_t_dir
        cmp     #CH_H
        beq     dir_t_host
        rts
dir_t_prg:
        lda     #DIR_TYPE_PRG
        bra     dir_setty
dir_t_seq:
        lda     #DIR_TYPE_SEQ
        bra     dir_setty
dir_t_usr:
        lda     #DIR_TYPE_USR
        bra     dir_setty
dir_t_rel:
        lda     #DIR_TYPE_REL
        bra     dir_setty
dir_t_dir:
        lda     #DIR_TYPE_DIR
        bra     dir_setty
dir_t_host:
        lda     #DIR_TYPE_HOST
dir_setty:
        sta     dir_ty
        rts

; ---------------------------------------------------------------------
; dir_close -- finished with the directory
; ---------------------------------------------------------------------
dir_close:
        jsr     CLRCHN
        lda     #DIR_LFN
        jmp     CLOSE

; one byte from the directory channel; carry set if the stream ended
dir_getb:
        jsr     CHRIN
        sta     mos8(X16_T3)
        jsr     READST
        cmp     #0
        bne     dir_getb_end
        lda     mos8(X16_T3)
        clc
        rts
dir_getb_end:
        sec
        rts

        .section .rodata,"a",@progbits

dir_dollar:
        .byte   CH_DOLLAR               ; "$"

        .section .bss,"aw",@nobits

dir_ty:  .zero  1
dir_blk: .zero  2
