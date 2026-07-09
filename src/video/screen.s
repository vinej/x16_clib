; =====================================================================
; x16clib :: video/screen.s -- screen mode, text output, cursor
; =====================================================================
;
; ---------------------------------------------------------------------
; THE KERNAL REQUIRES ADDRSEL = 0.
;
; Several KERNAL screen routines write VERA_ADDR_L/M/H *before* they set
; ADDRSEL, taking it on faith that port 0 is already selected. The screen
; scroller is the clearest case (x16-rom-r49 kernal/drivers/x16/screen.s):
;
;       lda pnt : sta VERA_ADDR_L   ; destination -- ADDRSEL assumed 0
;       ...
;       lda #1  : sta VERA_CTRL     ; only now switch to port 1
;       lda sal : sta VERA_ADDR_L   ; source
;
; Call that with ADDRSEL = 1 and the destination lands in port 1, where
; the source promptly overwrites it. The screen corrupts.
;
; screen_set_char is worse still: it writes all three ADDR registers and
; then `sta VERA_DATA0` without ever touching VERA_CTRL. With ADDRSEL = 1
; the address goes to port 1 while the character goes out of port 0, at
; whatever stale address port 0 happened to hold.
;
; So every routine here that enters a KERNAL routine which touches VERA
; forces ADDRSEL = 0 first. If you call CHROUT / CINT yourself after
; touching port 1 -- and x16_vera_addr1() and x16_vera_copy() both leave
; it selected -- either go through x16_screen_chrout(), or clear ADDRSEL
; beforehand.
;
; Note also that the KERNAL leaves DCSEL = 0, so do not expect a DCSEL
; selection to survive a call into it.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa, popax
        .importzp       ptr1, ptr2

        .export         _x16_screen_set_mode
        .export         _x16_screen_get_mode
        .export         _x16_screen_reset
        .export         _x16_screen_cls
        .export         _x16_screen_chrout
        .export         _x16_screen_color
        .export         _x16_screen_border
        .export         _x16_screen_locate
        .export         _x16_screen_get_cursor
        .export         _x16_screen_charset
        .export         _x16_screen_puts

; Cross-module: gfx/bitmap.s switches to bitmap mode through this.
        .export         screen_set_mode

        .segment        "CODE"

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_screen_set_mode(unsigned char mode)
;   returns 1 on success, 0 if the mode is unsupported
;
; KERNAL SCREEN_MODE reports failure in the carry, and takes carry clear
; to mean "set".
; ---------------------------------------------------------------------
_x16_screen_set_mode:
        jsr     screen_set_mode         ; carry set = unsupported
        lda     #0
        ldx     #0                      ; high byte, for int-promoting callers
        rol     a                       ; carry -> bit 0
        eor     #1                      ; ...report success, not failure
        rts

screen_set_mode:
        pha
        vera_addrsel 0
        pla
        clc
        jmp     SCREEN_MODE

; ---------------------------------------------------------------------
; unsigned char x16_screen_get_mode(void)
; ---------------------------------------------------------------------
_x16_screen_get_mode:
        jsr     screen_get_mode
        ldx     #0
        rts

screen_get_mode:
        vera_addrsel 0
        sec
        jmp     SCREEN_MODE

; ---------------------------------------------------------------------
; void x16_screen_reset(void) -- restore the default text mode (CINT)
; ---------------------------------------------------------------------
_x16_screen_reset:
screen_reset:
        vera_addrsel 0
        jmp     CINT

; ---------------------------------------------------------------------
; void x16_screen_cls(void) -- clear the text screen
; ---------------------------------------------------------------------
_x16_screen_cls:
screen_cls:
        vera_addrsel 0
        lda     #PETSCII_CLS
        jmp     CHROUT

; ---------------------------------------------------------------------
; void __fastcall__ x16_screen_chrout(unsigned char c)
;   CHROUT with the ADDRSEL precondition established.
;
; The argument is the rightmost (and only) one, so it already sits in A:
; the C entry point is the assembly routine, with no shim at all.
; ---------------------------------------------------------------------
_x16_screen_chrout:
screen_chrout:
        pha
        vera_addrsel 0
        pla
        jmp     CHROUT

; ---------------------------------------------------------------------
; void __fastcall__ x16_screen_color(unsigned char fg, unsigned char bg)
; ---------------------------------------------------------------------
_x16_screen_color:
        tax                             ; X = bg (rightmost arg, in A)
        jsr     popa                    ; A = fg
        ; fall through

; screen_color
;   in:  A = foreground (0-15), X = background (0-15)
;
; Sets the colour used by every subsequent CHROUT. Writes the KERNAL's
; editor colour byte directly -- there is no jump-table entry for this.
; Touches no VERA state.
screen_color:
        and     #$0F
        sta     X16_T0
        txa
        and     #$0F
        asl     a
        asl     a
        asl     a
        asl     a                       ; background into the high nibble
        ora     X16_T0
        sta     KERNAL_COLOR
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_screen_border(unsigned char color)
;
; DC_BORDER is only visible when DCSEL = 0, so select that bank first.
; Does not enter the KERNAL. Single argument: no shim.
; ---------------------------------------------------------------------
_x16_screen_border:
screen_border:
        pha
        vera_dcsel 0
        pla
        sta     VERA_DC_BORDER
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_screen_locate(unsigned char row, unsigned char col)
;
; popa clobbers Y, so `col` has to be parked in memory rather than held
; there across the pop.
; ---------------------------------------------------------------------
_x16_screen_locate:
        sta     X16_T3                  ; col (rightmost arg, in A)
        jsr     popa                    ; A = row
        tax                             ; X = row
        ldy     X16_T3                  ; Y = col
        ; fall through

; screen_locate -- move the text cursor
;   in:  X = row, Y = column
;
; KERNAL PLOT takes carry clear to mean "set".
;
; No ADDRSEL guard here: PLOT only moves the cursor variables (it lands
; in screen_set_position, which just writes `pnt`) and never touches
; VERA. Adding one would cost a clobbered A for nothing.
screen_locate:
        clc
        jmp     PLOT

; ---------------------------------------------------------------------
; void __fastcall__ x16_screen_get_cursor(unsigned char *row,
;                                         unsigned char *col)
;
; PLOT hands the answer back in X and Y, both of which the store loop
; needs, so stash them before touching either.
; ---------------------------------------------------------------------
_x16_screen_get_cursor:
        sta     ptr2                    ; col* (rightmost arg, in A/X)
        stx     ptr2+1
        jsr     popax                   ; row*
        sta     ptr1
        stx     ptr1+1

        jsr     screen_get_cursor       ; X = row, Y = col
        stx     X16_T3
        sty     X16_T4

        ldy     #0
        lda     X16_T3
        sta     (ptr1),y
        lda     X16_T4
        sta     (ptr2),y
        rts

screen_get_cursor:
        sec
        jmp     PLOT

; ---------------------------------------------------------------------
; void __fastcall__ x16_screen_charset(unsigned char charset)
;   1 = ISO, 2 = PET upper/graphics, 3 = PET upper/lower, ... 12 Katakana
; ---------------------------------------------------------------------
_x16_screen_charset:
screen_charset:
        pha
        vera_addrsel 0
        pla
        jmp     SCREEN_SET_CHARSET

; ---------------------------------------------------------------------
; void __fastcall__ x16_screen_puts(const char *s)
;   Prints a NUL-terminated string. Truncated at 255 bytes.
;
; A pointer arrives in A (low) / X (high), which is exactly what the
; assembly routine wants: no shim.
; ---------------------------------------------------------------------
_x16_screen_puts:
screen_puts:
        sta     X16_TPTR0
        stx     X16_TPTR0+1
        vera_addrsel 0
        ldy     #0
@loop:
        lda     (X16_TPTR0),y
        beq     @done
        jsr     CHROUT
        iny
        bne     @loop
@done:
        rts
