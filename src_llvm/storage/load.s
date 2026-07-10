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

; llvm-mos argument placement, measured on the machine:
;   POINTERS take aligned __rc PAIRS -- __rc2/__rc3, __rc4/__rc5, ... in
;   order, skipping any pair already partly used by an integer.
;   INTEGER bytes take A, then X, then single __rc bytes from __rc2 up,
;   skipping any byte a pointer already claimed.
;
; x16_fs_load is the clearest case: `name` takes __rc2/__rc3, len and
; device take A and X, `sa` takes __rc4 -- which spoils the __rc4/__rc5
; pair -- so `dest` skips to __rc6/__rc7 and `end` lands in __rc8/__rc9.

        .globl  x16_fs_setname
        .globl  x16_fs_load
        .globl  x16_fs_save
        .globl  x16_fs_vload

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_fs_setname(const char *name, unsigned char len)
; ---------------------------------------------------------------------
; name is a pointer -> __rc2/__rc3; len is the only integer -> A.
x16_fs_setname:
        pha                             ; len
        lda     __rc2
        sta     X16_P0                  ; name
        lda     __rc3
        sta     X16_P1
        pla                             ; A = len
        jmp     fs_setname

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_fs_load(const char *name, unsigned char len,
;                                        unsigned char device, unsigned char sa,
;                                        void *dest, unsigned int *end)
;   returns 0 on success, else the KERNAL error code
;   *end receives the address one past the last byte loaded; end may be NULL
; ---------------------------------------------------------------------
; name -> __rc2/__rc3, len -> A, device -> X, sa -> __rc4,
; dest -> __rc6/__rc7, end -> __rc8/__rc9.
;
; The `end` pointer is parked in T6/T7 rather than cc65's ptr1: fs_load
; itself does not use those two, and they are adjacent, which is all an
; indirect store needs.
x16_fs_load:
        sta     X16_P2                  ; name length
        stx     X16_P3                  ; device
        lda     __rc2
        sta     X16_P0                  ; name
        lda     __rc3
        sta     X16_P1
        lda     __rc4
        sta     X16_P4                  ; secondary address
        lda     __rc6
        sta     X16_P5                  ; dest
        lda     __rc7
        sta     X16_P6
        lda     __rc8
        sta     X16_T6                  ; end* (may be NULL)
        lda     __rc9
        sta     X16_T7

        jsr     fs_load                 ; carry + A = error, X/Y = end
        ; fall through

; in:  carry + A from a KERNAL LOAD, X/Y = end address,
;      X16_T6/T7 = end* or NULL
; out: A = 0 on success else the error code
store_end_and_status:
        stx     X16_T0                  ; neither store touches the flags
        sty     X16_T1
        php                             ; the carry is the only success signal
        pha

        lda     X16_T6
        ora     X16_T7
        beq     .Lstore_end_and_status_no_out                 ; end == NULL: caller does not want it
        ldy     #0
        lda     X16_T0
        sta     (X16_T6),y
        iny
        lda     X16_T1
        sta     (X16_T6),y
.Lstore_end_and_status_no_out:
        pla                             ; A = KERNAL error code
        plp                             ; carry set = it failed
        bcs     .Lstore_end_and_status_failed
        lda     #0                      ; success
.Lstore_end_and_status_failed:
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_fs_save(const char *name, unsigned char len,
;                                        unsigned char device,
;                                        const void *start, const void *end)
;   returns 0 on success, else the KERNAL error code
;   `end` is exclusive: one past the last byte to write
; ---------------------------------------------------------------------
; name -> __rc2/__rc3, len -> A, device -> X, start -> __rc4/__rc5,
; end -> __rc6/__rc7. Nothing spoils a pair here, so the pointers land
; consecutively.
x16_fs_save:
        sta     X16_P2                  ; name length
        stx     X16_P3                  ; device
        lda     __rc2
        sta     X16_P0                  ; name
        lda     __rc3
        sta     X16_P1
        lda     __rc4
        sta     X16_P5                  ; start
        lda     __rc5
        sta     X16_P6
        lda     __rc6
        sta     X16_T6                  ; end, exclusive
        lda     __rc7
        sta     X16_T7

        jsr     fs_save                 ; carry + A = error
        bcs     .Lx16_fs_save_failed
        lda     #0
.Lx16_fs_save_failed:
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
; name -> __rc2/__rc3, len -> A, device -> X, vaddr -> __rc4..__rc7.
x16_fs_vload:
        sta     X16_P2                  ; name length
        stx     X16_P3                  ; device
        lda     __rc2
        sta     X16_P0                  ; name
        lda     __rc3
        sta     X16_P1
        lda     __rc4
        sta     X16_P5                  ; vaddr bits 0-7
        lda     __rc5
        sta     X16_P6                  ; vaddr bits 8-15
        lda     __rc6
        sta     X16_P4                  ; vaddr bit 16 -> VRAM bank

        jsr     fs_vload                ; carry + A = error
        bcs     .Lx16_fs_vload_failed
        lda     #0
.Lx16_fs_vload_failed:
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
