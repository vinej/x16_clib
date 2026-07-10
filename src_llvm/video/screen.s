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
        .globl  x16_screen_reset
        .globl  x16_screen_cls
        .globl  x16_screen_chrout
        .globl  x16_screen_color
        .globl  x16_screen_border
        .globl  x16_screen_locate
        .globl  x16_screen_get_cursor
        .globl  x16_screen_charset
        .globl  x16_screen_puts

; Cross-module: gfx/bitmap.s switches to bitmap mode through this.
        .globl  screen_set_mode

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
        lda     __rc2
        sta     X16_T3                  ; row* lo
        lda     __rc3
        sta     X16_T4                  ; row* hi
        lda     __rc4
        sta     X16_T5                  ; col* lo
        lda     __rc5
        sta     X16_T6                  ; col* hi

        jsr     screen_get_cursor       ; X = row, Y = col
        stx     X16_T0
        sty     X16_T1

        ldy     #0
        lda     X16_T0
        sta     (X16_T3),y
        lda     X16_T1
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
        lda     __rc2
        ldx     __rc3
        ; fall through

screen_puts:
        sta     X16_TPTR0
        stx     X16_TPTR0+1
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
