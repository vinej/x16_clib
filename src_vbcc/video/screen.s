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
        zpage	r2
        zpage	r3
        zpage	r4
        zpage	r5

        global	_x16_screen_set_mode
        global	_x16_screen_get_mode
        global	_x16_screen_get_size
        global	_x16_screen_reset
        global	_x16_screen_cls
        global	_x16_screen_chrout
        global	_x16_screen_color
        global	_x16_screen_border
        global	_x16_screen_locate
        global	_x16_screen_get_cursor
        global	_x16_screen_charset
        global	_x16_screen_puts
        global	_x16_screen_addr
        global	_x16_screen_addr1
        global	_x16_screen_scode
        global	_x16_screen_blit
        global	_x16_screen_blitfill
        global	_x16_screen_scroll

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
; void x16_screen_get_size(__reg("a/x") unsigned char *cols,
;                          __reg("r0/r1") unsigned char *rows)
;   The LIVE text grid, after whatever x16_screen_set_mode() left behind
;   -- not the 80x60 default.
;
; Two pointers, placed exactly as x16_screen_get_cursor's are. SCREEN
; answers in X and Y, both of which the store loop needs, so stash them
; before touching either.
; ---------------------------------------------------------------------
_x16_screen_get_size:
        sta     X16_PTR2                ; cols* = a/x
        stx     X16_PTR2+1
        lda     r0
        sta     X16_PTR3                ; rows* = r0/r1
        lda     r1
        sta     X16_PTR3+1

        jsr     screen_get_size         ; X = columns, Y = rows
        stx     X16_T3
        sty     X16_T4

        ldy     #0
        lda     X16_T3
        sta     (X16_PTR2),y
        lda     X16_T4
        sta     (X16_PTR3),y
        rts

screen_get_size:
        jmp     SCREEN

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

; =====================================================================
; Direct text-map access
; =====================================================================
; CHROUT costs several hundred cycles a character once the editor's
; scroll checks, colour handling and cursor bookkeeping are paid for. A
; program that repaints a whole text screen -- a spreadsheet, a file
; browser, any full-screen TUI -- cannot afford that, so these write
; VERA's tile map itself: screen_addr points port 0 at a cell with
; auto-increment 1, and each following pair of bytes is one character
; and its colour. The address walks the row on its own, so a whole line
; costs one set-up and two stores per column.
;
; The KERNAL is not involved and neither is its cursor: these do not
; scroll, do not wrap, and do not move the CHROUT cursor. Do not print
; past the end of a row.
;
; Text is PETSCII on the way in -- the same bytes you would give CHROUT
; -- and is folded to screen codes here, so the caller never has to know
; the difference.
;
; The colour byte is foreground | background << 4, the same layout
; x16_screen_color() builds.
;
; As everywhere else in this module, the arguments are pinned to single
; registers with __reg() rather than left to vbcc's even-slot rule --
; screen_scroll takes six chars, which would otherwise spill two of them
; onto the C soft stack for no gain.
; =====================================================================

; ---------------------------------------------------------------------
; void x16_screen_addr(__reg("r0") unsigned char row,
;                      __reg("r1") unsigned char col)
; void x16_screen_addr1(... the same ...)
;   the body wants X = row, Y = column.
; ---------------------------------------------------------------------
_x16_screen_addr:
        ldx     r0                      ; row
        ldy     r1                      ; column
        jmp     screen_addr

_x16_screen_addr1:
        ldx     r0
        ldy     r1
        jmp     screen_addr1

; ---------------------------------------------------------------------
; unsigned char x16_screen_scode(__reg("a") unsigned char petscii)
;   the body wants A = the byte and hands one back in A: no shim beyond
;   the int-promotion high byte.
; ---------------------------------------------------------------------
_x16_screen_scode:
        jsr     screen_scode
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; void x16_screen_blit(__reg("a/x") const char *text,
;                      __reg("r0") unsigned char count,
;                      __reg("r1") unsigned char color)
;   the body wants P0/P1 = source, A = count, X = colour.
; ---------------------------------------------------------------------
_x16_screen_blit:
        sta     X16_P0                  ; source (a/x)
        stx     X16_P1
        ldx     r1                      ; colour
        lda     r0                      ; count
        jmp     screen_blit

; ---------------------------------------------------------------------
; void x16_screen_blitfill(__reg("r0") unsigned char count,
;                          __reg("r1") unsigned char color,
;                          __reg("r2") unsigned char ch)
;   the body wants A = count, X = colour, Y = character.
; ---------------------------------------------------------------------
_x16_screen_blitfill:
        ldy     r2                      ; character
        ldx     r1                      ; colour
        lda     r0                      ; count
        jmp     screen_blitfill

; ---------------------------------------------------------------------
; void x16_screen_scroll(__reg("r0") unsigned char top,
;                        __reg("r1") unsigned char left,
;                        __reg("r2") unsigned char height,
;                        __reg("r3") unsigned char width,
;                        __reg("r4") unsigned char distance,
;                        __reg("r5") unsigned char down)
;   the body wants P0 = top, P1 = left, P2 = height, P3 = width,
;   P4 = distance, A = direction.
; ---------------------------------------------------------------------
_x16_screen_scroll:
        lda     r0
        sta     X16_P0                  ; top
        lda     r1
        sta     X16_P1                  ; left
        lda     r2
        sta     X16_P2                  ; height
        lda     r3
        sta     X16_P3                  ; width
        lda     r4
        sta     X16_P4                  ; distance
        lda     r5                      ; A = direction
        jmp     screen_scroll

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; screen_addr -- point VERA port 0 at a character cell
;   in:  X = row, Y = column
;
; Reads L1_MAPBASE and L1_CONFIG, so it follows whatever screen_set_mode
; left behind rather than assuming the 80x60 default. Leaves ADDRSEL = 0
; and the increment set to 1.
; ---------------------------------------------------------------------
screen_addr:
        jsr     screen_addr_calc
        vera_addrsel 0
        jmp     screen_addr_store

; ---------------------------------------------------------------------
; screen_addr1 -- the same, for VERA port 1
;   in:  X = row, Y = column
;
; Port 1 is what you point at the destination when moving text around
; with vera_copy; screen_scroll below is the usual reason to want it.
; ---------------------------------------------------------------------
screen_addr1:
        jsr     screen_addr_calc
        vera_addrsel 1
screen_addr_store:
        lda     X16_T0
        sta     VERA_ADDR_L
        lda     X16_T1
        sta     VERA_ADDR_M
        lda     X16_T2
        and     #VERA_ADDR_H_BANK       ; bit 16 of the address
        ora     #$10                    ; increment 1
        sta     VERA_ADDR_H
        rts

; address of (X = row, Y = column) into X16_T0/T1/T2, port untouched
screen_addr_calc:
        sty     X16_T5                  ; column
        stx     X16_T6                  ; row

        lda     VERA_L1_MAPBASE         ; map base = MAPBASE << 9
        asl     a                       ; carry = bit 16
        sta     X16_T1                  ; mid
        lda     #0
        rol     a
        sta     X16_T2                  ; high
        stz     X16_T0                  ; low

        lda     VERA_L1_CONFIG          ; MAP_WIDTH: 0=32 1=64 2=128 3=256
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        and     #3
        clc
        adc     #6                      ; bytes per row = 2 << (5 + width)
        tay

        lda     X16_T6                  ; row << Y
        sta     X16_T3
        stz     X16_T4
.shift:
        asl     X16_T3
        rol     X16_T4
        dey
        bne     .shift

        clc                             ; base += row * stride
        lda     X16_T0
        adc     X16_T3
        sta     X16_T0
        lda     X16_T1
        adc     X16_T4
        sta     X16_T1
        bcc     .nocarry1
        inc     X16_T2
.nocarry1:
        lda     X16_T5                  ; base += column * 2
        asl     a
        tax
        lda     #0
        rol     a
        tay
        txa
        clc
        adc     X16_T0
        sta     X16_T0
        tya
        adc     X16_T1
        sta     X16_T1
        bcc     .nocarry2
        inc     X16_T2
.nocarry2:
        rts

; ---------------------------------------------------------------------
; screen_scode -- PETSCII to screen code
;   in:  A = PETSCII, out: A = screen code
;
; The standard CBM folding.
; ---------------------------------------------------------------------
screen_scode:
        cmp     #$20
        bcc     .plus80                 ; $00-$1F
        cmp     #$40
        bcc     .same                   ; $20-$3F
        cmp     #$60
        bcc     .minus40                ; $40-$5F
        cmp     #$80
        bcc     .minus20                ; $60-$7F
        cmp     #$A0
        bcc     .plus40                 ; $80-$9F
        cmp     #$C0
        bcc     .minus40                ; $A0-$BF
.minus80:                               ; $C0-$FF
        sec
        sbc     #$80
.same:
        rts
.plus80:
        clc
        adc     #$80
        rts
.minus40:
        sec
        sbc     #$40
        rts
.minus20:
        sec
        sbc     #$20
        rts
.plus40:
        clc
        adc     #$40
        rts

; ---------------------------------------------------------------------
; screen_blit -- write a run of characters, all one colour
;   in:  X16_P0/P1 = source, A = count (1-255), X = colour byte
; ---------------------------------------------------------------------
screen_blit:
        sta     X16_T7                  ; count
        stx     X16_T3                  ; colour
        ldy     #0
.loop:
        lda     (X16_P0),y
        jsr     screen_scode
        sta     VERA_DATA0
        lda     X16_T3
        sta     VERA_DATA0
        iny
        cpy     X16_T7
        bne     .loop
        rts

; ---------------------------------------------------------------------
; screen_blitfill -- write a run of one repeated character
;   in:  A = count (1-255), X = colour byte, Y = character (PETSCII)
; ---------------------------------------------------------------------
screen_blitfill:
        sta     X16_T7                  ; count
        stx     X16_T3                  ; colour
        tya
        jsr     screen_scode
        sta     X16_T4                  ; screen code, converted once
        ldy     #0
.loop:
        lda     X16_T4
        sta     VERA_DATA0
        lda     X16_T3
        sta     VERA_DATA0
        iny
        cpy     X16_T7
        bne     .loop
        rts

; ---------------------------------------------------------------------
; screen_scroll -- slide a rectangle of the text screen up or down
;   in:  X16_P0 = top row of the region
;        X16_P1 = left column
;        X16_P2 = height, in rows
;        X16_P3 = width, in columns
;        X16_P4 = distance to move, in rows
;        A      = 0 to move the picture up (toward row 0), 1 for down
;
; The point is not to save typing: a full-screen program that re-renders
; its whole grid to scroll one line pays for every cell it draws, and for
; a spreadsheet or a directory listing most of that cost is formatting
; the contents, not the drawing. Moving the picture inside VRAM and
; rendering only the row that appears costs one row instead of a
; screenful, whatever the contents happen to be.
;
; Vertical only. Scrolling sideways would move a row onto itself, and
; vera_copy walks forward, so the two would overlap.
; ---------------------------------------------------------------------
screen_scroll:
        sta     X16_P7                  ; direction
        lda     X16_P4
        beq     .done                   ; nothing to do
        cmp     X16_P2
        bcs     .done                   ; nothing survives: caller repaints

        sec
        lda     X16_P2
        sbc     X16_P4
        sta     X16_P5                  ; rows to copy
        stz     X16_P6                  ; index
.loop:
        lda     X16_P7
        bne     .down
        lda     X16_P0                  ; up: dst = top + i, src = dst + dist
        clc
        adc     X16_P6
        sta     X16_T7
        clc
        adc     X16_P4
        tax
        bra     .move
.down:
        lda     X16_P0                  ; down: dst = bottom - i, src = dst - dist
        clc
        adc     X16_P2
        sec
        sbc     #1
        sec
        sbc     X16_P6
        sta     X16_T7
        sec
        sbc     X16_P4
        tax
.move:
        phx                             ; port 1 = destination
        ldx     X16_T7
        ldy     X16_P1
        jsr     screen_addr1
        plx                             ; port 0 = source
        ldy     X16_P1
        jsr     screen_addr
        lda     X16_P3                  ; width in cells -> bytes
        asl     a
        tax
        lda     #0
        rol     a
        tay
        jsr     vera_copy
        inc     X16_P6
        lda     X16_P6
        cmp     X16_P5
        bne     .loop
.done:
        rts
