; =====================================================================
; x16clib :: storage/load.s -- load and save
; =====================================================================
; Device 8 is the SD card. Filenames are (address, length), not
; NUL-terminated.
;
; Two different registers steer a load, and they are easy to conflate:
;
;   SETLFS's secondary address says how to TREAT the file:
;     0  skip the 2-byte PRG header, load at the address you pass in X/Y
;     1  skip it, load at the address the header itself names
;     2  raw: no header to skip, load everything at your X/Y address
;
;   LOAD's own A register says WHERE memory-wise:
;     0  system RAM        1  verify only
;     2  VRAM bank 0       3  VRAM bank 1
;
; (Putting 2/3 into the secondary address does NOT reach VRAM -- it
; requests a raw header-included load into system RAM.)
;
; cc65's <cbm.h> has cbm_load() and cbm_save() for the system-RAM cases.
; fs_vload has no cc65 equivalent.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa, popax
        .importzp       ptr1, sreg

        .export         _x16_fs_setname
        .export         _x16_fs_load
        .export         _x16_fs_save
        .export         _x16_fs_vload
        .export         _x16_fs_prg_entry

; ---------------------------------------------------------------------
; ca65 -t cx16 translates character literals to PETSCII. Digits and the
; space survive that unchanged, but the bytes being compared here come
; off a disk, so they are written as their values rather than as literals
; -- the same rule storage/dos.s follows.
; ---------------------------------------------------------------------
CH_SPACE = $20
CH_ZERO  = $30                          ; '0'
CH_NINE1 = $3A                          ; one past '9'
TOK_SYS  = $9E                          ; BASIC's SYS token

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_fs_setname(const char *name, unsigned char len)
; ---------------------------------------------------------------------
_x16_fs_setname:
        pha                             ; len (rightmost arg, in A)
        jsr     popax                   ; name
        sta     X16_P0
        stx     X16_P1
        pla
        jmp     fs_setname

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_fs_load(const char *name, unsigned char len,
;                                        unsigned char device, unsigned char sa,
;                                        void *dest, unsigned int *end)
;   returns 0 on success, else the KERNAL error code
;   *end receives the address one past the last byte loaded; end may be NULL
; ---------------------------------------------------------------------
_x16_fs_load:
        sta     ptr1                    ; end* (rightmost arg: A/X)
        stx     ptr1+1
        jsr     popax
        sta     X16_P5                  ; dest
        stx     X16_P6
        jsr     popa
        sta     X16_P4                  ; secondary address
        jsr     popa
        sta     X16_P3                  ; device
        jsr     popa
        sta     X16_P2                  ; name length
        jsr     popax
        sta     X16_P0                  ; name
        stx     X16_P1

        jsr     fs_load                 ; carry + A = error, X/Y = end
        ; fall through

; in:  carry + A from a KERNAL LOAD, X/Y = end address, ptr1 = end* or NULL
; out: A = 0 on success else the error code, X = 0
store_end_and_status:
        stx     X16_T0                  ; neither store touches the flags
        sty     X16_T1
        php                             ; the carry is the only success signal
        pha

        lda     ptr1
        ora     ptr1+1
        beq     @no_out                 ; end == NULL: caller does not want it
        ldy     #0
        lda     X16_T0
        sta     (ptr1),y
        iny
        lda     X16_T1
        sta     (ptr1),y
@no_out:
        pla                             ; A = KERNAL error code
        plp                             ; carry set = it failed
        bcs     @failed
        lda     #0                      ; success
@failed:
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_fs_save(const char *name, unsigned char len,
;                                        unsigned char device,
;                                        const void *start, const void *end)
;   returns 0 on success, else the KERNAL error code
;   `end` is exclusive: one past the last byte to write
; ---------------------------------------------------------------------
_x16_fs_save:
        sta     X16_T6                  ; end lo (rightmost arg: A/X)
        stx     X16_T7                  ; end hi -- popa/popax leave T alone
        jsr     popax
        sta     X16_P5                  ; start
        stx     X16_P6
        jsr     popa
        sta     X16_P3                  ; device
        jsr     popa
        sta     X16_P2                  ; name length
        jsr     popax
        sta     X16_P0                  ; name
        stx     X16_P1

        jsr     fs_save                 ; carry + A = error
        bcs     @failed
        lda     #0
@failed:
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_fs_vload(const char *name, unsigned char len,
;                                         unsigned char device,
;                                         unsigned long vaddr)
;   Load straight into VRAM. Returns 0 on success, else the error code.
;
; vaddr goes last so cc65 passes all four bytes in registers, as with
; x16_vera_addr0(). Bit 16 picks the VRAM bank.
; ---------------------------------------------------------------------
_x16_fs_vload:
        sta     X16_P5                  ; vaddr bits 0-7
        stx     X16_P6                  ; vaddr bits 8-15
        lda     sreg
        sta     X16_P4                  ; vaddr bit 16 -> VRAM bank
        jsr     popa
        sta     X16_P3                  ; device
        jsr     popa
        sta     X16_P2                  ; name length
        jsr     popax
        sta     X16_P0                  ; name
        stx     X16_P1

        jsr     fs_vload                ; carry + A = error
        bcs     @failed
        lda     #0
@failed:
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned int __fastcall__ x16_fs_prg_entry(const char *name,
;                                            unsigned char len,
;                                            unsigned char device)
;   The SYS address out of a PRG's BASIC stub, read without loading the
;   file. 0 means the file could not be read or does not begin with one.
; ---------------------------------------------------------------------
_x16_fs_prg_entry:
        sta     X16_P3                  ; device (rightmost arg, in A)
        jsr     popa
        sta     X16_P2                  ; name length
        jsr     popax
        sta     X16_P0                  ; name
        stx     X16_P1

        jsr     fs_prg_entry            ; X = low, Y = high
        phy
        txa                             ; A = low
        plx                             ; X = high
        rts

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; fs_setname -- in: X16_P0/P1 = filename address, A = length
; ---------------------------------------------------------------------
fs_setname:
        ldx     X16_P0
        ldy     X16_P1
        jmp     SETNAM

; ---------------------------------------------------------------------
; fs_load -- load a file
;   in:  X16_P0/P1 = filename address
;        X16_P2    = filename length
;        X16_P3    = device (usually 8)
;        X16_P4    = secondary address (FS_SA_*)
;        X16_P5/P6 = destination address (ignored when SA = 1)
;   out: carry clear on success; carry set with A = KERNAL error code
;        X/Y = address one past the last byte loaded
; ---------------------------------------------------------------------
fs_load:
        lda     #0                      ; LOAD A = 0: into system RAM
        ; fall through

; in: A = LOAD's destination code (0 RAM, 2/3 VRAM); rest as fs_load
load_common:
        sta     X16_T3
        lda     X16_P2
        jsr     fs_setname

        lda     #1                      ; logical file number
        ldx     X16_P3                  ; device
        ldy     X16_P4                  ; secondary address
        jsr     SETLFS

        lda     X16_T3
        ldx     X16_P5
        ldy     X16_P6
        jmp     LOAD

; ---------------------------------------------------------------------
; fs_save -- save a block of memory as a PRG
;   in:  X16_P0/P1 = filename address
;        X16_P2    = filename length
;        X16_P3    = device
;        X16_P5/P6 = start address
;        X16_T6/T7 = end address, one past the last byte
;   out: carry clear on success; carry set with A = KERNAL error code
;
; Five 16-bit-ish things and the parameter block is eight bytes, so the
; end address goes in T6/T7 rather than squeezing P7. X16_T4/T5 is
; borrowed as the zero-page pointer KERNAL SAVE requires.
; ---------------------------------------------------------------------
fs_save:
        lda     X16_P2
        jsr     fs_setname

        lda     #1
        ldx     X16_P3
        ldy     #0                      ; secondary 0: no PRG-header relocation
        jsr     SETLFS

        lda     X16_P5                  ; SAVE takes the start address through a
        sta     X16_T4                  ; zero-page pointer, given by its address
        lda     X16_P6
        sta     X16_T5

        lda     #<X16_T4                ; A = zero-page offset of the pointer
        ldx     X16_T6                  ; X/Y = end address, exclusive
        ldy     X16_T7
        jmp     SAVE

; ---------------------------------------------------------------------
; fs_vload -- load straight into VRAM
;   in:  X16_P0/P1 = filename address
;        X16_P2    = filename length
;        X16_P3    = device
;        X16_P4    = VRAM bank (0 or 1)
;        X16_P5/P6 = VRAM address within that bank
;   out: as fs_load
;
; The bank turns into LOAD's A register (2 or 3); the secondary address
; is forced to 0 so the PRG header is skipped and X/Y is honoured.
; ---------------------------------------------------------------------
fs_vload:
        lda     X16_P4
        and     #$01
        clc
        adc     #2                      ; LOAD A: bank 0 -> 2, bank 1 -> 3
        stz     X16_P4                  ; SETLFS SA = 0 (does not disturb A)
        bra     load_common

; ---------------------------------------------------------------------
; fs_prg_entry -- a PRG's entry address, read without loading the file
;   in:  X16_P0/P1 = filename address
;        X16_P2    = filename length
;        X16_P3    = device (usually 8)
;   out: X/Y = the SYS address out of the file's BASIC stub, or $0000 if
;        the file cannot be read or does not begin with one
;
; A launcher has to know where to JSR before it hands the machine over,
; and loading the file to find out is the one thing it cannot do: the
; load would overwrite the launcher asking the question. So this reads
; the first few bytes off the disk and parses the stub where it lies:
;
;   two load-address bytes, two link bytes, two line-number bytes,
;   the SYS token ($9E), any spaces, then the address in ASCII.
;
; The address is read rather than assumed -- a compiler emitting
; "SYS 2071" today moves that number the moment the stub text changes.
; $0000 doubles as "no entry here", since no PRG can start there.
;
; Uses logical file 1, as fs_load does, on secondary address 2 so the
; bytes arrive raw rather than being treated as a program to relocate.
; ---------------------------------------------------------------------
FS_PRG_SKIP = 6                         ; load address, link, line number

fs_prg_entry:
        stz     X16_T0                  ; the result, built a digit at a time
        stz     X16_T1

        lda     X16_P2
        jsr     fs_setname
        lda     #1                      ; logical file
        ldx     X16_P3                  ; device
        ldy     #2                      ; a plain data channel
        jsr     SETLFS
        jsr     OPEN
        bcs     prg_quit
        ldx     #1
        jsr     CHKIN
        bcs     prg_quit

        lda     #FS_PRG_SKIP            ; CHRIN is free to clobber Y, so the
        sta     X16_T6                  ; count cannot live there
@skip:
        jsr     prg_getb
        bcs     prg_quit
        dec     X16_T6
        bne     @skip

        jsr     prg_getb                ; the SYS token
        bcs     prg_quit
        cmp     #TOK_SYS
        bne     prg_quit
@space:
        jsr     prg_getb
        bcs     prg_quit
        cmp     #CH_SPACE
        beq     @space
                                        ; A = the first character after them
@digit:
        cmp     #CH_ZERO
        bcc     prg_quit                ; a non-digit ends the number, and
        cmp     #CH_NINE1               ; ending it before it starts leaves 0
        bcs     prg_quit

        sec
        sbc     #CH_ZERO
        sta     X16_T2

        lda     X16_T0                  ; result = result * 10 + digit,
        sta     X16_T3                  ; taking *10 as ((r * 4) + r) * 2
        lda     X16_T1
        sta     X16_T4
        asl     X16_T0
        rol     X16_T1
        asl     X16_T0
        rol     X16_T1
        clc
        lda     X16_T0
        adc     X16_T3
        sta     X16_T0
        lda     X16_T1
        adc     X16_T4
        sta     X16_T1
        asl     X16_T0
        rol     X16_T1
        clc
        lda     X16_T0
        adc     X16_T2
        sta     X16_T0
        lda     X16_T1
        adc     #0
        sta     X16_T1

        jsr     prg_getb
        bcc     @digit                  ; ran out of file: keep what we have

prg_quit:
        jsr     CLRCHN
        lda     #1
        jsr     CLOSE
        ldx     X16_T0
        ldy     X16_T1
        rts

; one byte from the open channel; carry set if the file ended first
prg_getb:
        jsr     CHRIN
        sta     X16_T5
        jsr     READST
        cmp     #0
        bne     @end
        lda     X16_T5
        clc
        rts
@end:
        sec
        rts
