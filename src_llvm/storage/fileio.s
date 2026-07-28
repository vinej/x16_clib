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

; (import dropped: popa, popax)

        .globl  x16_fio_set_lfs
        .globl  x16_fio_set_name
        .globl  x16_fio_open
        .globl  x16_fio_close
        .globl  x16_fio_chkin
        .globl  x16_fio_chkout
        .globl  x16_fio_clrchn
        .globl  x16_fio_chrin
        .globl  x16_fio_chrout
        .globl  x16_fio_readst
        .globl  x16_fio_getin
        .globl  x16_fio_close_all
        .globl  x16_fio_close_device
        .globl  x16_fio_open_named
        .globl  x16_fio_open_read
        .globl  x16_fio_open_write
        .globl  x16_fio_close_named

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_fio_set_lfs(unsigned char lfn, unsigned char device,
;                                   unsigned char secondary)
;
; llvm-mos delivers three byte arguments in A, X and __rc2 -- the same
; order SETLFS wants them, so only the secondary needs moving.
; ---------------------------------------------------------------------
x16_fio_set_lfs:
        ldy     __rc2                   ; Y = secondary
        jmp     SETLFS                  ; A = lfn, X = device already

; ---------------------------------------------------------------------
; void __fastcall__ x16_fio_set_name(const char *name, unsigned char len)
; ---------------------------------------------------------------------
x16_fio_set_name:
        ldx     __rc2                   ; X = name low
        ldy     __rc3                   ; Y = name high
        jmp     SETNAM                  ; A already holds len

; ---------------------------------------------------------------------
; unsigned char x16_fio_open(void)
;   0 on success, else the KERNAL error code.
; ---------------------------------------------------------------------
x16_fio_open:
        jsr     OPEN
        bcs     .Lx16_fio_open_err
        lda     #0
.Lx16_fio_open_err:
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fio_close(unsigned char lfn)
; ---------------------------------------------------------------------
x16_fio_close:
        jmp     CLOSE

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_fio_chkin(unsigned char lfn)
; unsigned char __fastcall__ x16_fio_chkout(unsigned char lfn)
;   0 on success, else the KERNAL error code (3 = file not open).
; ---------------------------------------------------------------------
x16_fio_chkin:
        tax
        jsr     CHKIN
        bcs     .Lx16_fio_chkin_err
        lda     #0
.Lx16_fio_chkin_err:
        ldx     #0
        rts

x16_fio_chkout:
        tax
        jsr     CHKOUT
        bcs     .Lx16_fio_chkout_err
        lda     #0
.Lx16_fio_chkout_err:
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void x16_fio_clrchn(void)
; ---------------------------------------------------------------------
x16_fio_clrchn:
        jmp     CLRCHN

; ---------------------------------------------------------------------
; unsigned char x16_fio_chrin(void)   -- one byte from the input channel
; void __fastcall__ x16_fio_chrout(unsigned char b)
; unsigned char x16_fio_readst(void)  -- status byte (bit 6 = end of file)
; unsigned char x16_fio_getin(void)   -- one byte, 0 if nothing is waiting
; ---------------------------------------------------------------------
x16_fio_chrin:
        jsr     CHRIN
        ldx     #0
        rts

x16_fio_chrout:
        jmp     CHROUT

x16_fio_readst:
        jsr     READST
        ldx     #0
        rts

x16_fio_getin:
        jsr     GETIN
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void x16_fio_close_all(void)                     -- every logical file
; void __fastcall__ x16_fio_close_device(unsigned char device)
; ---------------------------------------------------------------------
x16_fio_close_all:
        jmp     CLALL

x16_fio_close_device:
        jmp     CLOSE_ALL

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_fio_open_named(const char *name,
;                             unsigned char len, unsigned char lfn,
;                             unsigned char device, unsigned char secondary)
;   SETNAM + SETLFS + OPEN in one call. 0 on success, else the KERNAL
;   error code. open_read/open_write add the CHKIN/CHKOUT.
; ---------------------------------------------------------------------
x16_fio_open_named:
        jsr     open_marshal
        jsr     fio_open_named
        bcs     .Lx16_fio_open_named_err
        lda     #0
.Lx16_fio_open_named_err:
        ldx     #0
        rts

x16_fio_open_read:
        jsr     open_marshal
        jsr     fio_open_read
        bcs     .Lx16_fio_open_read_err
        lda     #0
.Lx16_fio_open_read_err:
        ldx     #0
        rts

x16_fio_open_write:
        jsr     open_marshal
        jsr     fio_open_write
        bcs     .Lx16_fio_open_write_err
        lda     #0
.Lx16_fio_open_write_err:
        ldx     #0
        rts

; The five arguments into X16_P0..P5. The pointer claims __rc2/__rc3,
; so the four bytes land in A, X, __rc4 and __rc5.
open_marshal:
        sta     X16_P2                  ; len
        stx     X16_P3                  ; lfn
        lda     __rc4
        sta     X16_P4                  ; device
        lda     __rc5
        sta     X16_P5                  ; secondary
        lda     __rc2                   ; name
        sta     X16_P0
        lda     __rc3
        sta     X16_P1
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fio_close_named(unsigned char lfn)
;   CLRCHN + CLOSE.
; ---------------------------------------------------------------------
x16_fio_close_named:
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
        bcs     .Lfio_open_read_done
        ldx     X16_P3
        jmp     CHKIN
.Lfio_open_read_done:
        rts

; ---------------------------------------------------------------------
; fio_open_write -- open, then select the logical file for output
;   out: carry set if OPEN or CHKOUT failed
; ---------------------------------------------------------------------
fio_open_write:
        jsr     fio_open_named
        bcs     .Lfio_open_write_done
        ldx     X16_P3
        jmp     CHKOUT
.Lfio_open_write_done:
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
