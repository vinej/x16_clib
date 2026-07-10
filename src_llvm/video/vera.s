; =====================================================================
; x16clib :: video/vera.s (llvm-mos) -- VRAM data-port access
; =====================================================================
; Register contract for the internal routines: A, X, Y and flags are
; clobbered unless a routine says otherwise. Scratch is X16_T0..T2.
;
; The C shims stage into X16_T3..T5, which the internal routines never
; touch.
;
; THE CALLING CONVENTION IS NOT cc65's. llvm-mos passes argument BYTES
; left to right in A, then X, then __rc2, __rc3, __rc4, ... and returns
; the same way. Nothing is pushed, so there is no popa/popax and no
; callee pop -- the entire class of "wrong pop width corrupts sp" bugs
; that the cc65 shims must guard against cannot occur here.
;
; A one-byte return goes in A alone. cc65 needed `ldx #0` beside it so an
; int-promoting caller read a clean high byte; llvm-mos knows the type
; and never reads X, so that store is gone.
;
; Cheap locals: ca65's @label is scoped to the preceding real label.
; LLVM has no such thing, so each one becomes .L<routine>_<name>.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void x16_vera_addr0(unsigned char inc, unsigned long addr)
; void x16_vera_addr1(unsigned char inc, unsigned long addr)
;
; Bytes land in declaration order: A = inc, X = addr bits 0-7,
; __rc2 = 8-15, __rc3 = 16-23, __rc4 = 24-31. Only bit 16 of addr
; reaches the hardware -- VRAM is 17-bit -- so __rc4 is ignored.
;
; cc65 had to put addr last so that a 32-bit value arrived in registers
; rather than on the stack. Here every argument is in a register anyway,
; and the order is simply the declared one.
; ---------------------------------------------------------------------
        .globl  x16_vera_addr0
x16_vera_addr0:
        jsr     addr_marshal
        jmp     vera_set_addr0

        .globl  x16_vera_addr1
x16_vera_addr1:
        jsr     addr_marshal
        jmp     vera_set_addr1

; in:  A = inc, X = addr bits 0-7, __rc2 = 8-15, __rc3 = 16-23
; out: A = ADDR_L, X = ADDR_M, Y = ADDR_H
addr_marshal:
        pha                             ; inc, for the DECR test below
        stx     X16_T3                  ; addr bits 0-7
        lda     __rc2
        sta     X16_T4                  ; addr bits 8-15

        lda     __rc3                   ; addr bits 16-23
        and     #VERA_ADDR_H_BANK       ; ...only bit 16 exists
        sta     X16_T5                  ; ADDR_H under construction

        pla                             ; A = inc | optional X16_DECR
        pha
        and     #$0F                    ; the increment index
        asl     a
        asl     a
        asl     a
        asl     a                       ; into ADDR_H bits 7:4
        ora     X16_T5
        sta     X16_T5
        pla
        and     #X16_DECR
        beq     .Laddr_ascending
        lda     #VERA_ADDR_H_DECR
        ora     X16_T5
        sta     X16_T5
.Laddr_ascending:
        lda     X16_T3
        ldx     X16_T4
        ldy     X16_T5
        rts

; ---------------------------------------------------------------------
; void x16_vera_fill(unsigned char value, unsigned int count)
;   A = value, X = count lo, __rc2 = count hi
; ---------------------------------------------------------------------
        .globl  x16_vera_fill
x16_vera_fill:
        ldy     __rc2                   ; count hi
        jmp     vera_fill               ; A = value, X = count lo already

; ---------------------------------------------------------------------
; void x16_vera_copy(unsigned int count)
;   A = count lo, X = count hi -- but vera_copy wants X = lo, Y = hi.
; ---------------------------------------------------------------------
        .globl  x16_vera_copy
x16_vera_copy:
        stx     X16_T3                  ; stash hi; X is about to be lo
        tax                             ; X = count lo
        ldy     X16_T3                  ; Y = count hi
        jmp     vera_copy

; ---------------------------------------------------------------------
; unsigned char x16_vera_has_fx(void)
;
; 1 if VERA's firmware carries the FX register set, else 0. The internal
; routine also leaves the major version in A, but that is useless as a
; return value: FX first shipped in VERA v0.3.1, whose major version is
; zero, so it could not be told apart from "absent".
; ---------------------------------------------------------------------
        .globl  x16_vera_has_fx
x16_vera_has_fx:
        jsr     vera_has_fx             ; carry = present
        lda     #0
        rol     a                       ; carry -> bit 0
        rts

; =====================================================================
; Internal routines -- other modules call these directly
; =====================================================================

; ---------------------------------------------------------------------
; vera_set_addr0 / vera_set_addr1
;   in:  A = ADDR_L, X = ADDR_M, Y = ADDR_H (bank | DECR | incr<<4)
;   out: the chosen port points at that address
; ---------------------------------------------------------------------
        .globl  vera_set_addr0
vera_set_addr0:
        pha
        lda     #VERA_CTRL_ADDRSEL
        trb     VERA_CTRL               ; ADDRSEL = 0
        pla
        sta     VERA_ADDR_L
        stx     VERA_ADDR_M
        sty     VERA_ADDR_H
        rts

        .globl  vera_set_addr1
vera_set_addr1:
        pha
        lda     #VERA_CTRL_ADDRSEL
        tsb     VERA_CTRL               ; ADDRSEL = 1
        pla
        sta     VERA_ADDR_L
        stx     VERA_ADDR_M
        sty     VERA_ADDR_H
        rts

; ---------------------------------------------------------------------
; vera_fill
;   in:  A = byte value
;        X = count low, Y = count high   (16-bit, 0 means write nothing)
;   pre: caller has pointed port 0 at the destination, with the
;        increment it wants.
; ---------------------------------------------------------------------
        .globl  vera_fill
vera_fill:
        sta     X16_T0                  ; value
        stx     X16_T1                  ; count lo
        sty     X16_T2                  ; count hi

        txa
        ora     X16_T2
        beq     .Lfill_done             ; count == 0

        ldx     X16_T1
        ldy     X16_T2
        txa
        beq     .Lfill_full             ; low byte 0 -> exactly hi*256 bytes
        iny                             ; otherwise one extra partial page
.Lfill_full:
        lda     X16_T0
.Lfill_loop:
        sta     VERA_DATA0
        dex
        bne     .Lfill_loop
        dey
        bne     .Lfill_loop
.Lfill_done:
        rts

; ---------------------------------------------------------------------
; vera_copy
;   in:  X = count low, Y = count high
;   pre: port 0 points at the SOURCE (read), port 1 at the DESTINATION
;        (write), each with its own increment.
;
; DATA0 always reads port 0 and DATA1 always writes port 1, whatever
; ADDRSEL says -- so the inner loop never touches CTRL.
; ---------------------------------------------------------------------
        .globl  vera_copy
vera_copy:
        stx     X16_T1
        sty     X16_T2

        txa
        ora     X16_T2
        beq     .Lcopy_done

        ldx     X16_T1
        ldy     X16_T2
        txa
        beq     .Lcopy_loop
        iny
.Lcopy_loop:
        lda     VERA_DATA0
        sta     VERA_DATA1
        dex
        bne     .Lcopy_loop
        dey
        bne     .Lcopy_loop
.Lcopy_done:
        rts

; ---------------------------------------------------------------------
; vera_has_fx
;   out: carry set if VERA firmware supports the FX register set
;        A = major version (only meaningful when carry is set)
;
; Probes DCSEL=63, where DC_VER0 reads back ASCII 'V' on FX-capable
; VERA. Restores DCSEL to 0 on the way out.
; ---------------------------------------------------------------------
        .globl  vera_has_fx
vera_has_fx:
        vera_dcsel VERA_DCSEL_FX_VERSION
        lda     VERA_DC_VER0
        cmp     #VERA_VERSION_MAGIC
        bne     .Lfx_no
        lda     VERA_DC_VER1            ; major release
        pha
        vera_dcsel 0
        pla
        sec
        rts
.Lfx_no:
        vera_dcsel 0
        lda     #0
        clc
        rts
