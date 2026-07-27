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

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa, popax

        .export         _x16_fio_set_lfs
        .export         _x16_fio_set_name
        .export         _x16_fio_open
        .export         _x16_fio_close
        .export         _x16_fio_chkin
        .export         _x16_fio_chkout
        .export         _x16_fio_clrchn
        .export         _x16_fio_chrin
        .export         _x16_fio_chrout
        .export         _x16_fio_readst
        .export         _x16_fio_getin
        .export         _x16_fio_close_all
        .export         _x16_fio_close_device
        .export         _x16_fio_open_named
        .export         _x16_fio_open_read
        .export         _x16_fio_open_write
        .export         _x16_fio_close_named

        .segment        "CODE"

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
        pha                             ; secondary (rightmost arg, in A)
        jsr     popa
        tax                             ; X = device
        jsr     popa                    ; A = lfn
        ply                             ; Y = secondary
        jmp     SETLFS

; ---------------------------------------------------------------------
; void __fastcall__ x16_fio_set_name(const char *name, unsigned char len)
; ---------------------------------------------------------------------
_x16_fio_set_name:
        pha                             ; len (rightmost arg, in A)
        jsr     popax                   ; name: A = low, X = high
        phx
        tax                             ; X = name low
        ply                             ; Y = name high
        pla                             ; A = len
        jmp     SETNAM

; ---------------------------------------------------------------------
; unsigned char x16_fio_open(void)
;   0 on success, else the KERNAL error code.
; ---------------------------------------------------------------------
_x16_fio_open:
        jsr     OPEN
        bcs     @err
        lda     #0
@err:
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
        bcs     @err
        lda     #0
@err:
        ldx     #0
        rts

_x16_fio_chkout:
        tax
        jsr     CHKOUT
        bcs     @err
        lda     #0
@err:
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
        bcs     @err
        lda     #0
@err:
        ldx     #0
        rts

_x16_fio_open_read:
        jsr     open_marshal
        jsr     fio_open_read
        bcs     @err
        lda     #0
@err:
        ldx     #0
        rts

_x16_fio_open_write:
        jsr     open_marshal
        jsr     fio_open_write
        bcs     @err
        lda     #0
@err:
        ldx     #0
        rts

; the five arguments into X16_P0..P5, popped right to left
open_marshal:
        sta     X16_P5                  ; secondary (rightmost arg, in A)
        jsr     popa
        sta     X16_P4                  ; device
        jsr     popa
        sta     X16_P3                  ; lfn
        jsr     popa
        sta     X16_P2                  ; len
        jsr     popax
        sta     X16_P0                  ; name
        stx     X16_P1
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
        bcs     @done
        ldx     X16_P3
        jmp     CHKIN
@done:
        rts

; ---------------------------------------------------------------------
; fio_open_write -- open, then select the logical file for output
;   out: carry set if OPEN or CHKOUT failed
; ---------------------------------------------------------------------
fio_open_write:
        jsr     fio_open_named
        bcs     @done
        ldx     X16_P3
        jmp     CHKOUT
@done:
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
