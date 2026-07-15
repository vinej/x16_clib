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

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc's argument registers, read by the three shims that take more than
; one value (color, locate, get_cursor). r0/r1 are volatile; the library's
; own pointer scratch is X16_PTR2 (P4/P5), clear of vbcc's r-block.
        zpage	r0
        zpage	r1

        global	_x16_screen_set_mode
        global	_x16_screen_get_mode
        global	_x16_screen_reset
        global	_x16_screen_cls
        global	_x16_screen_chrout
        global	_x16_screen_color
        global	_x16_screen_border
        global	_x16_screen_locate
        global	_x16_screen_get_cursor
        global	_x16_screen_charset
        global	_x16_screen_puts

; Cross-module: gfx/bitmap.s switches to bitmap mode through this.
        global	screen_set_mode

        section text

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
; void x16_screen_color(__reg("a") unsigned char fg,
;                       __reg("r0") unsigned char bg)
;
; screen_color wants fg in A and bg in X. vbcc will not pass an argument
; in X, so the header routes bg through r0 and the shim loads it.
; ---------------------------------------------------------------------
_x16_screen_color:
        ldx     r0                      ; X = bg
        ; fall through (A already holds fg)

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
; void x16_screen_locate(__reg("r0") unsigned char row,
;                        __reg("r1") unsigned char col)
;
; screen_locate wants row in X and col in Y. Neither can be an argument
; register, so the header delivers row,col in r0,r1 and the shim moves
; them into X,Y.
; ---------------------------------------------------------------------
_x16_screen_locate:
        ldx     r0                      ; X = row
        ldy     r1                      ; Y = col
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
; void x16_screen_get_cursor(__reg("a/x") unsigned char *row,
;                            __reg("r0/r1") unsigned char *col)
;
; PLOT hands the answer back in X and Y, both of which the store loop
; needs, so stash them before touching either. The two destination
; pointers arrive in the a/x pair (row*) and r0/r1 (col*); the shim moves
; them into the library's own pointer scratch (X16_PTR2, X16_PTR3) so the
; (ind),y stores do not clash with vbcc's volatile r-block.
; ---------------------------------------------------------------------
_x16_screen_get_cursor:
        sta     X16_PTR2                ; row* = a/x
        stx     X16_PTR2+1
        lda     r0
        sta     X16_PTR3                ; col* = r0/r1
        lda     r1
        sta     X16_PTR3+1

        jsr     screen_get_cursor       ; X = row, Y = col
        stx     X16_T3
        sty     X16_T4

        ldy     #0
        lda     X16_T3
        sta     (X16_PTR2),y
        lda     X16_T4
        sta     (X16_PTR3),y
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
.loop:
        lda     (X16_TPTR0),y
        beq     .done
        jsr     CHROUT
        iny
        bne     .loop
.done:
        rts
