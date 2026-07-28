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

; THE C ENTRY POINTS ARE NOT cc65's. llvm-mos passes argument bytes left to
; right in A, then X, then __rc2, __rc3, ... and returns a byte in A alone.
; Nothing is pushed, so nothing is popped, and the `ldx #0` cc65 needs
; beside a one-byte return is gone. Several entry points that had to
; marshal under cc65 are now pure fall-throughs.

        .include        "macros.inc"
        .include        "x16zp.inc"

        .globl  x16_screen_set_mode
        .globl  x16_screen_get_mode
        .globl  x16_screen_get_size
        .globl  x16_screen_reset
        .globl  x16_screen_cls
        .globl  x16_screen_chrout
        .globl  x16_screen_color
        .globl  x16_screen_border
        .globl  x16_screen_locate
        .globl  x16_screen_get_cursor
        .globl  x16_screen_charset
        .globl  x16_screen_puts
        .globl  x16_screen_addr
        .globl  x16_screen_addr1
        .globl  x16_screen_scode
        .globl  x16_screen_blit
        .globl  x16_screen_blitfill
        .globl  x16_screen_scroll

; Cross-module: gfx/bitmap8l.s switches to bitmap mode through this.
        .globl  screen_set_mode

; Cross-module: ui/filepick.s saves and restores the screen with these.
        .globl  screen_charset
        .globl  screen_get_mode

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; unsigned char x16_screen_set_mode(unsigned char mode)
;   returns 1 on success, 0 if the mode is unsupported
;
; KERNAL SCREEN_MODE reports failure in the carry, and takes carry clear
; to mean "set". `mode` is the only argument, so it is already in A.
; ---------------------------------------------------------------------
x16_screen_set_mode:
        jsr     screen_set_mode         ; carry set = unsupported
        lda     #0
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
x16_screen_get_mode:
        jmp     screen_get_mode         ; the mode comes straight back in A

screen_get_mode:
        vera_addrsel 0
        sec
        jmp     SCREEN_MODE

; ---------------------------------------------------------------------
; void x16_screen_get_size(unsigned char *cols, unsigned char *rows)
;   The LIVE text grid, after whatever x16_screen_set_mode() left behind
;   -- not the 80x60 default.
;
; Two pointers, so cols* is in __rc2/__rc3 and rows* in __rc4/__rc5, and
; A/X carry nothing -- exactly as x16_screen_get_cursor() above, and for
; the same reason. SCREEN answers in X and Y, both of which the store
; loop needs, so stash them first.
; ---------------------------------------------------------------------
x16_screen_get_size:
        lda     mos8(__rc2)
        sta     mos8(X16_T3)            ; cols* lo
        lda     mos8(__rc3)
        sta     mos8(X16_T4)            ; cols* hi
        lda     mos8(__rc4)
        sta     mos8(X16_T5)            ; rows* lo
        lda     mos8(__rc5)
        sta     mos8(X16_T6)            ; rows* hi

        jsr     screen_get_size         ; X = columns, Y = rows
        stx     mos8(X16_T0)
        sty     mos8(X16_T1)

        ldy     #0
        lda     mos8(X16_T0)
        sta     (X16_T3),y
        lda     mos8(X16_T1)
        sta     (X16_T5),y
        rts

screen_get_size:
        jmp     SCREEN

; ---------------------------------------------------------------------
; void x16_screen_reset(void) -- restore the default text mode (CINT)
; ---------------------------------------------------------------------
x16_screen_reset:
screen_reset:
        vera_addrsel 0
        jmp     CINT

; ---------------------------------------------------------------------
; void x16_screen_cls(void) -- clear the text screen
; ---------------------------------------------------------------------
x16_screen_cls:
screen_cls:
        vera_addrsel 0
        lda     #PETSCII_CLS
        jmp     CHROUT

; ---------------------------------------------------------------------
; void x16_screen_chrout(unsigned char c)
;   CHROUT with the ADDRSEL precondition established.
;
; The only argument, so it already sits in A: the C entry point is the
; assembly routine, with no shim at all.
; ---------------------------------------------------------------------
x16_screen_chrout:
screen_chrout:
        pha
        vera_addrsel 0
        pla
        jmp     CHROUT

; ---------------------------------------------------------------------
; void x16_screen_color(unsigned char fg, unsigned char bg)
;
; A = fg, X = bg -- precisely the internal routine's contract. cc65 had to
; transpose them, because its rightmost argument is the one that arrives
; in A.
; ---------------------------------------------------------------------
x16_screen_color:
        ; fall through

; screen_color
;   in:  A = foreground (0-15), X = background (0-15)
;
; Sets the colour used by every subsequent CHROUT. Writes the KERNAL's
; editor colour byte directly -- there is no jump-table entry for this.
; Touches no VERA state.
screen_color:
        and     #$0F
        sta     mos8(X16_T0)
        txa
        and     #$0F
        asl     a
        asl     a
        asl     a
        asl     a                       ; background into the high nibble
        ora     mos8(X16_T0)
        sta     KERNAL_COLOR
        rts

; ---------------------------------------------------------------------
; void x16_screen_border(unsigned char color)
;
; DC_BORDER is only visible when DCSEL = 0, so select that bank first.
; Does not enter the KERNAL. Single argument: no shim.
; ---------------------------------------------------------------------
x16_screen_border:
screen_border:
        pha
        vera_dcsel 0
        pla
        sta     VERA_DC_BORDER
        rts

; ---------------------------------------------------------------------
; void x16_screen_locate(unsigned char row, unsigned char col)
;
; A = row, X = col; screen_locate wants X = row, Y = col. Rotate them
; through the stack: the 65C02 has phx/ply, so this costs no memory. cc65
; had to park `col` in scratch because its popa clobbers Y.
; ---------------------------------------------------------------------
x16_screen_locate:
        phx                             ; col
        tax                             ; X = row
        ply                             ; Y = col
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
; void x16_screen_get_cursor(unsigned char *row, unsigned char *col)
;
; POINTERS DO NOT ARRIVE IN A/X. llvm-mos gives every pointer argument a
; whole __rc pair -- only zero page can be indirected through -- so
; row* is in __rc2/__rc3 and col* in __rc4/__rc5, and A/X carry nothing.
;
; cc65 borrowed its runtime's ptr1/ptr2 for the two destinations; here they
; go in the library's own scratch, which the internal routine never
; touches. T3/T4 and T5/T6 are adjacent because core/x16zp.s defines the
; whole block in one object, in that order.
;
; PLOT hands the answer back in X and Y, both of which the store loop
; needs, so stash them before touching either.
; ---------------------------------------------------------------------
x16_screen_get_cursor:
        lda     mos8(__rc2)
        sta     mos8(X16_T3)            ; row* lo
        lda     mos8(__rc3)
        sta     mos8(X16_T4)            ; row* hi
        lda     mos8(__rc4)
        sta     mos8(X16_T5)            ; col* lo
        lda     mos8(__rc5)
        sta     mos8(X16_T6)            ; col* hi

        jsr     screen_get_cursor       ; X = row, Y = col
        stx     mos8(X16_T0)
        sty     mos8(X16_T1)

        ldy     #0
        lda     mos8(X16_T0)
        sta     (X16_T3),y
        lda     mos8(X16_T1)
        sta     (X16_T5),y
        rts

screen_get_cursor:
        sec
        jmp     PLOT

; ---------------------------------------------------------------------
; void x16_screen_charset(unsigned char charset)
;   1 = ISO, 2 = PET upper/graphics, 3 = PET upper/lower, ... 12 Katakana
; ---------------------------------------------------------------------
x16_screen_charset:
screen_charset:
        pha
        vera_addrsel 0
        pla
        jmp     SCREEN_SET_CHARSET

; ---------------------------------------------------------------------
; void x16_screen_puts(const char *s)
;   Prints a NUL-terminated string. Truncated at 255 bytes.
;
; The pointer arrives in __rc2/__rc3, not in A/X: llvm-mos allocates every
; pointer argument an __rc pair. cc65 handed it over in A/X, which is what
; the internal routine still expects, so the C entry point needs one line
; each way. It cannot be a bare fall-through as it is on the cc65 side.
; ---------------------------------------------------------------------
x16_screen_puts:
        lda     mos8(__rc2)
        ldx     mos8(__rc3)
        ; fall through

screen_puts:
        sta     mos8(X16_TPTR0)
        stx     mos8(X16_TPTR0+1)
        vera_addrsel 0
        ldy     #0
.Lscreen_puts_loop:
        lda     (X16_TPTR0),y
        beq     .Lscreen_puts_done
        jsr     CHROUT
        iny
        bne     .Lscreen_puts_loop
.Lscreen_puts_done:
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
; =====================================================================

; ---------------------------------------------------------------------
; void x16_screen_addr(unsigned char row, unsigned char col)
; void x16_screen_addr1(unsigned char row, unsigned char col)
;   Point VERA port 0 (or port 1) at a character cell.
;
; Two integers, so row -> A and col -> X, left to right. screen_addr
; wants them the other way round: X = row, Y = column.
; ---------------------------------------------------------------------
x16_screen_addr:
        pha                             ; row
        txa
        tay                             ; Y = col
        plx                             ; X = row
        jmp     screen_addr

x16_screen_addr1:
        pha
        txa
        tay
        plx
        jmp     screen_addr1

; ---------------------------------------------------------------------
; unsigned char x16_screen_scode(unsigned char petscii)
;   PETSCII to screen code. One integer in A, one byte back in A: the C
;   entry point is the assembly routine.
; ---------------------------------------------------------------------
x16_screen_scode:
        jmp     screen_scode

; ---------------------------------------------------------------------
; void x16_screen_blit(const char *text, unsigned char count,
;                      unsigned char color)
;   Write a run of characters, all one colour. Port 0 must already point
;   at the first cell; it is left just past the last one.
;
; text -> __rc2/__rc3 (pointers take a pair), count -> A, color -> X.
; screen_blit wants P0/P1 = source, A = count, X = colour, which is the
; same A and X -- only the pointer has to be moved.
; ---------------------------------------------------------------------
x16_screen_blit:
        pha                             ; count
        lda     mos8(__rc2)
        sta     mos8(X16_P0)            ; source
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        pla                             ; A = count, X still = colour
        jmp     screen_blit

; ---------------------------------------------------------------------
; void x16_screen_blitfill(unsigned char count, unsigned char color,
;                          unsigned char ch)
;   Write a run of one repeated character.
;
; Three integers: count -> A, color -> X, ch -> __rc2. screen_blitfill
; wants A = count, X = colour, Y = character.
; ---------------------------------------------------------------------
x16_screen_blitfill:
        ldy     mos8(__rc2)             ; Y = character
        jmp     screen_blitfill

; ---------------------------------------------------------------------
; void x16_screen_scroll(unsigned char top, unsigned char left,
;                        unsigned char height, unsigned char width,
;                        unsigned char distance, unsigned char down)
;   Slide a rectangle of the text screen up (down = 0) or down (1).
;
; Six integers: top -> A, left -> X, then height, width, distance and
; down in __rc2..__rc5. screen_scroll wants P0 = top, P1 = left,
; P2 = height, P3 = width, P4 = distance, A = direction.
; ---------------------------------------------------------------------
x16_screen_scroll:
        sta     mos8(X16_P0)            ; top
        stx     mos8(X16_P1)            ; left
        lda     mos8(__rc2)
        sta     mos8(X16_P2)            ; height
        lda     mos8(__rc3)
        sta     mos8(X16_P3)            ; width
        lda     mos8(__rc4)
        sta     mos8(X16_P4)            ; distance
        lda     mos8(__rc5)             ; A = direction
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
        lda     mos8(X16_T0)
        sta     VERA_ADDR_L
        lda     mos8(X16_T1)
        sta     VERA_ADDR_M
        lda     mos8(X16_T2)
        and     #VERA_ADDR_H_BANK       ; bit 16 of the address
        ora     #$10                    ; increment 1
        sta     VERA_ADDR_H
        rts

; address of (X = row, Y = column) into X16_T0/T1/T2, port untouched
screen_addr_calc:
        sty     mos8(X16_T5)            ; column
        stx     mos8(X16_T6)            ; row

        lda     VERA_L1_MAPBASE         ; map base = MAPBASE << 9
        asl     a                       ; carry = bit 16
        sta     mos8(X16_T1)            ; mid
        lda     #0
        rol     a
        sta     mos8(X16_T2)            ; high
        stz     mos8(X16_T0)            ; low

        lda     VERA_L1_CONFIG          ; MAP_WIDTH: 0=32 1=64 2=128 3=256
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        and     #3
        clc
        adc     #6                      ; bytes per row = 2 << (5 + width)
        tay

        lda     mos8(X16_T6)            ; row << Y
        sta     mos8(X16_T3)
        stz     mos8(X16_T4)
.Lscreen_addr_calc_shift:
        asl     mos8(X16_T3)
        rol     mos8(X16_T4)
        dey
        bne     .Lscreen_addr_calc_shift

        clc                             ; base += row * stride
        lda     mos8(X16_T0)
        adc     mos8(X16_T3)
        sta     mos8(X16_T0)
        lda     mos8(X16_T1)
        adc     mos8(X16_T4)
        sta     mos8(X16_T1)
        bcc     .Lscreen_addr_calc_nocarry1
        inc     mos8(X16_T2)
.Lscreen_addr_calc_nocarry1:
        lda     mos8(X16_T5)            ; base += column * 2
        asl     a
        tax
        lda     #0
        rol     a
        tay
        txa
        clc
        adc     mos8(X16_T0)
        sta     mos8(X16_T0)
        tya
        adc     mos8(X16_T1)
        sta     mos8(X16_T1)
        bcc     .Lscreen_addr_calc_nocarry2
        inc     mos8(X16_T2)
.Lscreen_addr_calc_nocarry2:
        rts

; ---------------------------------------------------------------------
; screen_scode -- PETSCII to screen code
;   in:  A = PETSCII, out: A = screen code
;
; The standard CBM folding.
; ---------------------------------------------------------------------
screen_scode:
        cmp     #$20
        bcc     .Lscreen_scode_plus80                 ; $00-$1F
        cmp     #$40
        bcc     .Lscreen_scode_same                   ; $20-$3F
        cmp     #$60
        bcc     .Lscreen_scode_minus40                ; $40-$5F
        cmp     #$80
        bcc     .Lscreen_scode_minus20                ; $60-$7F
        cmp     #$A0
        bcc     .Lscreen_scode_plus40                 ; $80-$9F
        cmp     #$C0
        bcc     .Lscreen_scode_minus40                ; $A0-$BF
.Lscreen_scode_minus80:                               ; $C0-$FF
        sec
        sbc     #$80
.Lscreen_scode_same:
        rts
.Lscreen_scode_plus80:
        clc
        adc     #$80
        rts
.Lscreen_scode_minus40:
        sec
        sbc     #$40
        rts
.Lscreen_scode_minus20:
        sec
        sbc     #$20
        rts
.Lscreen_scode_plus40:
        clc
        adc     #$40
        rts

; ---------------------------------------------------------------------
; screen_blit -- write a run of characters, all one colour
;   in:  X16_P0/P1 = source, A = count (1-255), X = colour byte
; ---------------------------------------------------------------------
screen_blit:
        sta     mos8(X16_T7)            ; count
        stx     mos8(X16_T3)            ; colour
        ldy     #0
.Lscreen_blit_loop:
        lda     (X16_P0),y
        jsr     screen_scode
        sta     VERA_DATA0
        lda     mos8(X16_T3)
        sta     VERA_DATA0
        iny
        cpy     mos8(X16_T7)
        bne     .Lscreen_blit_loop
        rts

; ---------------------------------------------------------------------
; screen_blitfill -- write a run of one repeated character
;   in:  A = count (1-255), X = colour byte, Y = character (PETSCII)
; ---------------------------------------------------------------------
screen_blitfill:
        sta     mos8(X16_T7)            ; count
        stx     mos8(X16_T3)            ; colour
        tya
        jsr     screen_scode
        sta     mos8(X16_T4)            ; screen code, converted once
        ldy     #0
.Lscreen_blitfill_loop:
        lda     mos8(X16_T4)
        sta     VERA_DATA0
        lda     mos8(X16_T3)
        sta     VERA_DATA0
        iny
        cpy     mos8(X16_T7)
        bne     .Lscreen_blitfill_loop
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
        sta     mos8(X16_P7)            ; direction
        lda     mos8(X16_P4)
        beq     .Lscreen_scroll_done                   ; nothing to do
        cmp     mos8(X16_P2)
        bcs     .Lscreen_scroll_done                   ; nothing survives: caller repaints

        sec
        lda     mos8(X16_P2)
        sbc     mos8(X16_P4)
        sta     mos8(X16_P5)            ; rows to copy
        stz     mos8(X16_P6)            ; index
.Lscreen_scroll_loop:
        lda     mos8(X16_P7)
        bne     .Lscreen_scroll_down
        lda     mos8(X16_P0)            ; up: dst = top + i, src = dst + dist
        clc
        adc     mos8(X16_P6)
        sta     mos8(X16_T7)
        clc
        adc     mos8(X16_P4)
        tax
        bra     .Lscreen_scroll_move
.Lscreen_scroll_down:
        lda     mos8(X16_P0)            ; down: dst = bottom - i, src = dst - dist
        clc
        adc     mos8(X16_P2)
        sec
        sbc     #1
        sec
        sbc     mos8(X16_P6)
        sta     mos8(X16_T7)
        sec
        sbc     mos8(X16_P4)
        tax
.Lscreen_scroll_move:
        phx                             ; port 1 = destination
        ldx     mos8(X16_T7)
        ldy     mos8(X16_P1)
        jsr     screen_addr1
        plx                             ; port 0 = source
        ldy     mos8(X16_P1)
        jsr     screen_addr
        lda     mos8(X16_P3)            ; width in cells -> bytes
        asl     a
        tax
        lda     #0
        rol     a
        tay
        jsr     vera_copy
        inc     mos8(X16_P6)
        lda     mos8(X16_P6)
        cmp     mos8(X16_P5)
        bne     .Lscreen_scroll_loop
.Lscreen_scroll_done:
        rts
