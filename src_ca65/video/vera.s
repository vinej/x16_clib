; =====================================================================
; x16clib :: video/vera.s -- VRAM data-port access
; =====================================================================
; Register contract for the internal routines: A, X, Y and flags are
; clobbered unless a routine says otherwise. Scratch is X16_T0..T2.
;
; The C shims marshal into X16_T3..T5, which the internal routines never
; touch, so a shim can stage arguments across a popa without clobbering
; the routine it is about to call.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa
        .importzp       sreg

        .export         _x16_vera_addr0
        .export         _x16_vera_addr1
        .export         _x16_vera_fill
        .export         _x16_vera_copy
        .export         _x16_vera_has_fx

; Cross-module: gfx/bitmap.s and sprite/sprite.s stream through vera_fill.
; ACME textually included every module into one assembly, so a bare label
; sufficed; separate objects need a real export.
        .export         vera_fill

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_vera_addr0(unsigned char inc, unsigned long addr)
; void __fastcall__ x16_vera_addr1(unsigned char inc, unsigned long addr)
;
; addr is rightmost, so cc65 hands the whole 32-bit value over in
; registers: A = bits 0-7, X = 8-15, sreg = 16-23, sreg+1 = 24-31. Only
; bit 16 of it reaches the hardware (VRAM is 17-bit). That register-only
; path is why addr goes last -- the same trick cc65 plays with vpoke().
; `inc` is the one stack argument.
; ---------------------------------------------------------------------
_x16_vera_addr0:
        jsr     addr_marshal
        jmp     vera_set_addr0

_x16_vera_addr1:
        jsr     addr_marshal
        jmp     vera_set_addr1

; in:  A/X/sreg = addr, one byte (inc) on the C stack
; out: A = ADDR_L, X = ADDR_M, Y = ADDR_H
addr_marshal:
        sta     X16_T3                  ; addr bits 0-7
        stx     X16_T4                  ; addr bits 8-15

        lda     sreg                    ; addr bits 16-23
        and     #VERA_ADDR_H_BANK       ; ...only bit 16 exists
        sta     X16_T5                  ; ADDR_H under construction

        jsr     popa                    ; A = inc | optional X16_DECR
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
        beq     @ascending
        lda     #VERA_ADDR_H_DECR
        ora     X16_T5
        sta     X16_T5
@ascending:
        lda     X16_T3
        ldx     X16_T4
        ldy     X16_T5
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_vera_fill(unsigned char value, unsigned int count)
; ---------------------------------------------------------------------
_x16_vera_fill:
        sta     X16_T3                  ; count lo (rightmost arg: A/X)
        stx     X16_T4                  ; count hi
        jsr     popa                    ; A = value
        ldx     X16_T3
        ldy     X16_T4
        jmp     vera_fill

; ---------------------------------------------------------------------
; void __fastcall__ x16_vera_copy(unsigned int count)
; ---------------------------------------------------------------------
_x16_vera_copy:
        sta     X16_T3                  ; count lo
        txa
        tay                             ; Y = count hi
        ldx     X16_T3                  ; X = count lo
        jmp     vera_copy

; ---------------------------------------------------------------------
; unsigned char x16_vera_has_fx(void)
;
; 1 if VERA's firmware carries the FX register set, else 0. The internal
; routine also leaves the major version in A, but that is useless as a
; return value: FX first shipped in VERA v0.3.1, whose major version is
; zero, so it could not be told apart from "absent".
; ---------------------------------------------------------------------
_x16_vera_has_fx:
        jsr     vera_has_fx             ; carry = present
        lda     #0
        ldx     #0                      ; high byte, for int-promoting callers
        rol     a                       ; carry -> bit 0
        rts

; =====================================================================
; Internal routines -- unexported, callable from other library modules
; =====================================================================

; ---------------------------------------------------------------------
; vera_set_addr0 / vera_set_addr1
;   in:  A = ADDR_L, X = ADDR_M, Y = ADDR_H (bank | DECR | incr<<4)
;   out: the chosen port points at that address
;
; The runtime equivalent of the vera_addr macro, for addresses not known
; at assembly time.
; ---------------------------------------------------------------------
vera_set_addr0:
        pha
        lda     #VERA_CTRL_ADDRSEL
        trb     VERA_CTRL               ; ADDRSEL = 0
        pla
        sta     VERA_ADDR_L
        stx     VERA_ADDR_M
        sty     VERA_ADDR_H
        rts

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
;        increment it wants (VERA_INC_1 for a linear run, VERA_INC_320
;        to stripe down a bitmap column, etc.)
;
; The tight `sta VERA_DATA0` loop -- far faster than a per-byte address
; reload.
; ---------------------------------------------------------------------
vera_fill:
        sta     X16_T0                  ; value
        stx     X16_T1                  ; count lo
        sty     X16_T2                  ; count hi

        txa
        ora     X16_T2
        beq     @done                   ; count == 0

        ldx     X16_T1
        ldy     X16_T2
        txa
        beq     @full                   ; low byte 0 -> exactly hi*256 bytes
        iny                             ; otherwise one extra partial page
@full:
        lda     X16_T0
@loop:
        sta     VERA_DATA0
        dex
        bne     @loop
        dey
        bne     @loop
@done:
        rts

; ---------------------------------------------------------------------
; vera_copy
;   in:  X = count low, Y = count high
;   pre: port 0 points at the SOURCE (read), port 1 at the DESTINATION
;        (write), each with its own increment.
;
; DATA0 always reads port 0 and DATA1 always writes port 1, whatever
; ADDRSEL says -- so the inner loop never touches CTRL and never reloads
; an address. Two bytes per iteration, both auto-incrementing.
; ---------------------------------------------------------------------
vera_copy:
        stx     X16_T1
        sty     X16_T2

        txa
        ora     X16_T2
        beq     @done

        ldx     X16_T1
        ldy     X16_T2
        txa
        beq     @full
        iny
@full:
@loop:
        lda     VERA_DATA0
        sta     VERA_DATA1
        dex
        bne     @loop
        dey
        bne     @loop
@done:
        rts

; ---------------------------------------------------------------------
; vera_has_fx
;   out: carry set if VERA firmware supports the FX register set
;        A = major version (only meaningful when carry is set)
;
; Probes DCSEL=63, where DC_VER0 reads back ASCII 'V' on FX-capable
; VERA. Restores DCSEL to 0 on the way out.
; ---------------------------------------------------------------------
vera_has_fx:
        vera_dcsel VERA_DCSEL_FX_VERSION
        lda     VERA_DC_VER0
        cmp     #VERA_VERSION_MAGIC
        bne     @no
        lda     VERA_DC_VER1            ; major release
        pha
        vera_dcsel 0
        pla
        sec
        rts
@no:
        vera_dcsel 0
        lda     #0
        clc
        rts
