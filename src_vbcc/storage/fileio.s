; =====================================================================
; x16clib :: storage/fileio.s -- generic KERNAL file/channel I/O
; =====================================================================
; Streamed file/channel I/O: OPEN/CLOSE, CHKIN/CHKOUT, CHRIN/CHROUT,
; READST, and the setup calls that feed them. For one-shot PRG LOAD/SAVE
; keep using storage/load.s's x16_fs_* helpers; for the WHY of a failure
; ask storage/dos.s.
;
; The internal helpers use the shared parameter block:
;       X16_P0/P1 = filename address
;       X16_P2    = filename length
;       X16_P3    = logical file number
;       X16_P4    = device
;       X16_P5    = secondary address
;
; cc65's <cbm.h> has cbm_k_* twins for the raw wrappers; these exist so
; the same API is available in every port of the library, and so the
; composite open_read/open_write helpers have something to stand on.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r4
        zpage	r6
        zpage	sp


        global	_x16_fio_set_lfs
        global	_x16_fio_set_name
        global	_x16_fio_open
        global	_x16_fio_close
        global	_x16_fio_chkin
        global	_x16_fio_chkout
        global	_x16_fio_clrchn
        global	_x16_fio_chrin
        global	_x16_fio_chrout
        global	_x16_fio_readst
        global	_x16_fio_getin
        global	_x16_fio_close_all
        global	_x16_fio_close_device
        global	_x16_fio_open_named
        global	_x16_fio_open_read
        global	_x16_fio_open_write
        global	_x16_fio_close_named

        section text

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_fio_set_lfs(unsigned char lfn, unsigned char device,
;                                   unsigned char secondary)
;
; popa clobbers Y, so the secondary rides the hardware stack across the
; pops and lands in Y at the last moment.
; ---------------------------------------------------------------------
_x16_fio_set_lfs:
        lda     r0                      ; lfn
        ldx     r2                      ; device
        ldy     r4                      ; secondary
        jmp     SETLFS

; ---------------------------------------------------------------------
; void __fastcall__ x16_fio_set_name(const char *name, unsigned char len)
; ---------------------------------------------------------------------
_x16_fio_set_name:
        ldx     r0                      ; SETNAM wants X = low, Y = high
        ldy     r1
        lda     r2                      ; A = len
        jmp     SETNAM

; ---------------------------------------------------------------------
; unsigned char x16_fio_open(void)
;   0 on success, else the KERNAL error code.
; ---------------------------------------------------------------------
_x16_fio_open:
        jsr     OPEN
        bcs     .err
        lda     #0
.err:
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fio_close(unsigned char lfn)
; ---------------------------------------------------------------------
_x16_fio_close:
        jmp     CLOSE

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_fio_chkin(unsigned char lfn)
; unsigned char __fastcall__ x16_fio_chkout(unsigned char lfn)
;   0 on success, else the KERNAL error code (3 = file not open).
; ---------------------------------------------------------------------
_x16_fio_chkin:
        tax
        jsr     CHKIN
        bcs     .err
        lda     #0
.err:
        ldx     #0
        rts

_x16_fio_chkout:
        tax
        jsr     CHKOUT
        bcs     .err
        lda     #0
.err:
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void x16_fio_clrchn(void)
; ---------------------------------------------------------------------
_x16_fio_clrchn:
        jmp     CLRCHN

; ---------------------------------------------------------------------
; unsigned char x16_fio_chrin(void)   -- one byte from the input channel
; void __fastcall__ x16_fio_chrout(unsigned char b)
; unsigned char x16_fio_readst(void)  -- status byte (bit 6 = end of file)
; unsigned char x16_fio_getin(void)   -- one byte, 0 if nothing is waiting
; ---------------------------------------------------------------------
_x16_fio_chrin:
        jsr     CHRIN
        ldx     #0
        rts

_x16_fio_chrout:
        jmp     CHROUT

_x16_fio_readst:
        jsr     READST
        ldx     #0
        rts

_x16_fio_getin:
        jsr     GETIN
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void x16_fio_close_all(void)                     -- every logical file
; void __fastcall__ x16_fio_close_device(unsigned char device)
; ---------------------------------------------------------------------
_x16_fio_close_all:
        jmp     CLALL

_x16_fio_close_device:
        jmp     CLOSE_ALL

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_fio_open_named(const char *name,
;                             unsigned char len, unsigned char lfn,
;                             unsigned char device, unsigned char secondary)
;   SETNAM + SETLFS + OPEN in one call. 0 on success, else the KERNAL
;   error code. open_read/open_write add the CHKIN/CHKOUT.
; ---------------------------------------------------------------------
_x16_fio_open_named:
        jsr     open_marshal
        jsr     fio_open_named
        bcs     .err
        lda     #0
.err:
        ldx     #0
        rts

_x16_fio_open_read:
        jsr     open_marshal
        jsr     fio_open_read
        bcs     .err
        lda     #0
.err:
        ldx     #0
        rts

_x16_fio_open_write:
        jsr     open_marshal
        jsr     fio_open_write
        bcs     .err
        lda     #0
.err:
        ldx     #0
        rts

; the five arguments into X16_P0..P5, popped right to left
; in:  r0/r1 = name, r2 = len, r4 = lfn, r6 = device, (sp)+0 = secondary
;      -- five arguments, so the last one spilled past r0..r7
; out: X16_P0/P1 = name, P2 = len, P3 = lfn, P4 = device, P5 = secondary
open_marshal:
        lda     r0
        sta     X16_P0                  ; name
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; len
        lda     r4
        sta     X16_P3                  ; lfn
        lda     r6
        sta     X16_P4                  ; device
        ldy     #0
        lda     (sp),y
        sta     X16_P5                  ; secondary
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fio_close_named(unsigned char lfn)
;   CLRCHN + CLOSE.
; ---------------------------------------------------------------------
_x16_fio_close_named:
        sta     X16_P3
        jmp     fio_close_named

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; fio_open_named -- SETNAM + SETLFS + OPEN from X16_P0..P5
;   out: carry follows OPEN (set = failed, A = KERNAL error code)
; ---------------------------------------------------------------------
fio_open_named:
        jsr     fileio_setup
        jmp     OPEN

; ---------------------------------------------------------------------
; fio_open_read -- open, then select the logical file for input
;   out: carry set if OPEN or CHKIN failed
; ---------------------------------------------------------------------
fio_open_read:
        jsr     fio_open_named
        bcs     .done
        ldx     X16_P3
        jmp     CHKIN
.done:
        rts

; ---------------------------------------------------------------------
; fio_open_write -- open, then select the logical file for output
;   out: carry set if OPEN or CHKOUT failed
; ---------------------------------------------------------------------
fio_open_write:
        jsr     fio_open_named
        bcs     .done
        ldx     X16_P3
        jmp     CHKOUT
.done:
        rts

; ---------------------------------------------------------------------
; fio_close_named -- CLRCHN + CLOSE for X16_P3
; ---------------------------------------------------------------------
fio_close_named:
        jsr     CLRCHN
        lda     X16_P3
        jmp     CLOSE

fileio_setup:
        lda     X16_P2
        ldx     X16_P0
        ldy     X16_P1
        jsr     SETNAM
        lda     X16_P3
        ldx     X16_P4
        ldy     X16_P5
        jmp     SETLFS
