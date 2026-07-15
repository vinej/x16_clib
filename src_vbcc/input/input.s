; =====================================================================
; x16clib :: input/input.s -- joystick, mouse, keyboard
; =====================================================================
; Thin wrappers over the KERNAL. Nothing here touches VERA directly
; except mouse_show, which the KERNAL handles for us.
;
; cc65 ships joystick and mouse drivers (<joystick.h>, <mouse.h>) that
; are portable across its targets. These are the X16's own calls: no
; driver to install, and they expose the SNES pad's full button set.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers. joy_get: joy in r0, present* in r2/r3.
; mouse_get: x* in r0/r1, y* in r2/r3. The output-pointer copies use the
; library's own X16_PTR2/PTR3 scratch, which mouse_get's P0..P3 do not
; touch.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3

        global	_x16_joy_scan
        global	_x16_joy_get
        global	_x16_mouse_show
        global	_x16_mouse_hide
        global	_x16_mouse_get
        global	_x16_key_get
        global	_x16_key_wait
        global	_x16_key_peek

; What JOYSTICK_GET returns in Y. Mirrored as X16_JOY_PRESENT in the
; header, though C callers see the boolean the shim derives from it.
JOY_PRESENT = $00
JOY_ABSENT  = $FF

        section text

; ---------------------------------------------------------------------
; void x16_joy_scan(void)
;
; Sample every joystick. The KERNAL's own IRQ already does this once a
; frame; you only need it if you have taken the interrupt over.
; ---------------------------------------------------------------------
_x16_joy_scan:
        jmp     JOYSTICK_SCAN

; ---------------------------------------------------------------------
; unsigned int __fastcall__ x16_joy_get(unsigned char joy,
;                                       unsigned char *present)
;   returns byte0 | byte1<<8, and sets *present to 1 or 0
;
; JOYSTICK_GET hands back exactly the A/X pair cc65 wants for a 16-bit
; return, plus presence in Y. So the shim's only work is unpacking Y.
;
; Bits are ACTIVE LOW: a pressed button reads 0.
; ---------------------------------------------------------------------
; x16_joy_get(__reg("r0") unsigned char joy, __reg("r2/r3") unsigned char *present)
_x16_joy_get:
        lda     r2
        sta     X16_PTR2                ; present*
        lda     r3
        sta     X16_PTR2+1
        lda     r0                      ; A = joy

        jsr     JOYSTICK_GET            ; A = byte0, X = byte1, Y = $00/$FF
        pha
        phx                             ; park the buttons; neither affects Y

        cpy     #JOY_PRESENT
        beq     .present
        lda     #0
        bra     .store
.present:
        lda     #1
.store:
        ldy     #0
        sta     (X16_PTR2),y

        plx
        pla
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_mouse_show(unsigned char cursor)
;   $FF shows without changing the cursor; n selects cursor sprite n.
;
; The screen size is left unchanged (MOUSE_CONFIG gets X = Y = 0). Call
; MOUSE_CONFIG yourself to resize the mouse field.
; ---------------------------------------------------------------------
_x16_mouse_show:
mouse_show:
        ldx     #0
        ldy     #0
        jmp     MOUSE_CONFIG

; ---------------------------------------------------------------------
; void x16_mouse_hide(void)
; ---------------------------------------------------------------------
_x16_mouse_hide:
mouse_hide:
        lda     #0
        ldx     #0
        ldy     #0
        jmp     MOUSE_CONFIG

; ---------------------------------------------------------------------
; unsigned char x16_mouse_get(__reg("r0/r1") unsigned int *x,
;                             __reg("r2/r3") unsigned int *y)
;   returns the button mask: bit 0 left, bit 1 right, bit 2 middle
;
; x*, y* go into X16_PTR2/PTR3; mouse_get writes its results into P0..P3,
; which those scratch pointers do not overlap.
; ---------------------------------------------------------------------
_x16_mouse_get:
        lda     r0
        sta     X16_PTR2                ; x*
        lda     r1
        sta     X16_PTR2+1
        lda     r2
        sta     X16_PTR3                ; y*
        lda     r3
        sta     X16_PTR3+1

        jsr     mouse_get               ; X16_P0..P3, A = buttons
        pha

        ldy     #0
        lda     X16_P0
        sta     (X16_PTR2),y
        lda     X16_P2
        sta     (X16_PTR3),y
        iny
        lda     X16_P1
        sta     (X16_PTR2),y
        lda     X16_P3
        sta     (X16_PTR3),y

        pla
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; mouse_get -- out: X16_P0/P1 = x, X16_P2/P3 = y, A = buttons
;
; The KERNAL writes the four position bytes to zero page starting at the
; address in X, which is why the results land in the parameter block.
mouse_get:
        ldx     #<X16_P0
        jmp     MOUSE_GET

; ---------------------------------------------------------------------
; unsigned char x16_key_get(void)
;   PETSCII code, or 0 if nothing is waiting. Non-blocking.
; ---------------------------------------------------------------------
_x16_key_get:
        jsr     GETIN
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char x16_key_wait(void)
;   Block until a key is pressed.
; ---------------------------------------------------------------------
_x16_key_wait:
.loop:
        jsr     GETIN
        beq     .loop
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned int x16_key_peek(void)
;   returns key | queued<<8, without consuming the key
;
; KBDBUF_PEEK returns the key in A and the queue depth in X -- again
; exactly cc65's 16-bit return convention, so there is nothing to do.
; ---------------------------------------------------------------------
_x16_key_peek:
        jmp     KBDBUF_PEEK
