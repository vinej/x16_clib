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

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers, plus the C soft-stack pointer for the arguments
; that overflow r0..r7. See each shim for the exact placement.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r4
        zpage	r6
        zpage	r7
        zpage	sp
        zpage	btmp0

        global	_x16_fs_setname
        global	_x16_fs_load
        global	_x16_fs_save
        global	_x16_fs_vload

; A dedicated zero-page pointer for the fs_load end* out-param.
; store_end_and_status dereferences it with (zp),y AFTER fs_load has run,
; and fs_load fills the whole P block, so end* cannot live there. This is
; the module's own cell, placed by the linker clear of vbcc's registers.
        section zpage
fs_endp:  reserve 2

        section text

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void x16_fs_setname(__reg("r0/r1") const char *name, __reg("r2") unsigned char len)
;   fs_setname wants P0/P1 = name, A = len.
; ---------------------------------------------------------------------
_x16_fs_setname:
        lda     r0
        sta     X16_P0
        lda     r1
        sta     X16_P1
        lda     r2                      ; A = len
        jmp     fs_setname

; ---------------------------------------------------------------------
; unsigned char x16_fs_load(__reg("r0/r1") const char *name, __reg("r2") unsigned char len,
;                           __reg("r4") unsigned char device, __reg("r6") unsigned char sa,
;                           void *dest, unsigned int *end)
;   returns 0 on success, else the KERNAL error code. *end receives the
;   address one past the last byte loaded; end may be NULL.
;
; Six arguments: dest and end overflow r0..r7 and ride the C soft stack --
; dest at (sp)+0/1, end at (sp)+2/3. fs_load wants P0/P1 = name, P2 = len,
; P3 = device, P4 = sa, P5/P6 = dest; end* is stashed in fs_endp.
; ---------------------------------------------------------------------
_x16_fs_load:
        lda     r0
        sta     X16_P0                  ; name
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; name length
        lda     r4
        sta     X16_P3                  ; device
        lda     r6
        sta     X16_P4                  ; secondary address
        ldy     #0
        lda     (sp),y
        sta     X16_P5                  ; dest lo
        iny
        lda     (sp),y
        sta     X16_P6                  ; dest hi
        iny
        lda     (sp),y
        sta     fs_endp                 ; end* lo
        iny
        lda     (sp),y
        sta     fs_endp+1               ; end* hi

        jsr     fs_load                 ; carry + A = error, X/Y = end
        ; fall through

; in:  carry + A from a KERNAL LOAD, X/Y = end address, fs_endp = end* or NULL
; out: A = 0 on success else the error code, X = 0
store_end_and_status:
        stx     X16_T0                  ; neither store touches the flags
        sty     X16_T1
        php                             ; the carry is the only success signal
        pha

        lda     fs_endp
        ora     fs_endp+1
        beq     .no_out                 ; end == NULL: caller does not want it
        ldy     #0
        lda     X16_T0
        sta     (fs_endp),y
        iny
        lda     X16_T1
        sta     (fs_endp),y
.no_out:
        pla                             ; A = KERNAL error code
        plp                             ; carry set = it failed
        bcs     .failed
        lda     #0                      ; success
.failed:
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char x16_fs_save(__reg("r0/r1") const char *name, __reg("r2") unsigned char len,
;                           __reg("r4") unsigned char device,
;                           __reg("r6/r7") const void *start, const void *end)
;   returns 0 on success, else the KERNAL error code. `end` is exclusive.
;
; Five arguments: end overflows to the C soft stack at (sp)+0/1. fs_save
; wants P0/P1 = name, P2 = len, P3 = device, P5/P6 = start, T6/T7 = end.
; ---------------------------------------------------------------------
_x16_fs_save:
        lda     r0
        sta     X16_P0                  ; name
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; name length
        lda     r4
        sta     X16_P3                  ; device
        lda     r6
        sta     X16_P5                  ; start
        lda     r7
        sta     X16_P6
        ldy     #0
        lda     (sp),y
        sta     X16_T6                  ; end lo (stacked 5th arg)
        iny
        lda     (sp),y
        sta     X16_T7                  ; end hi

        jsr     fs_save                 ; carry + A = error
        bcs     .failed
        lda     #0
.failed:
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char x16_fs_vload(__reg("r0/r1") const char *name, __reg("r2") unsigned char len,
;                            __reg("r4") unsigned char device, unsigned long vaddr)
;   Load straight into VRAM. Returns 0 on success, else the error code.
;
; vaddr is a long in btmp0..btmp0+2; bit 16 picks the VRAM bank. fs_vload
; wants P0/P1 = name, P2 = len, P3 = device, P5/P6 = vaddr lo/mid,
; P4 = vaddr bit 16.
; ---------------------------------------------------------------------
_x16_fs_vload:
        lda     btmp0
        sta     X16_P5                  ; vaddr bits 0-7
        lda     btmp0+1
        sta     X16_P6                  ; vaddr bits 8-15
        lda     btmp0+2
        sta     X16_P4                  ; vaddr bit 16 -> VRAM bank
        lda     r0
        sta     X16_P0                  ; name
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; name length
        lda     r4
        sta     X16_P3                  ; device

        jsr     fs_vload                ; carry + A = error
        bcs     .failed
        lda     #0
.failed:
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
