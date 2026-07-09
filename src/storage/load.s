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
