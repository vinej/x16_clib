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

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X, then whatever __rc bytes the pointers
;   left free.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_joy_scan
        .globl  x16_joy_get
        .globl  x16_mouse_show
        .globl  x16_mouse_hide
        .globl  x16_mouse_get
        .globl  x16_key_get
        .globl  x16_key_wait
        .globl  x16_key_peek

; What JOYSTICK_GET returns in Y. Mirrored as X16_JOY_PRESENT in the
; header, though C callers see the boolean the shim derives from it.
JOY_PRESENT = $00
JOY_ABSENT  = $FF

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void x16_joy_scan(void)
;
; Sample every joystick. The KERNAL's own IRQ already does this once a
; frame; you only need it if you have taken the interrupt over.
; ---------------------------------------------------------------------
x16_joy_scan:
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
; joy is a byte -> A. present is a pointer -> __rc2/__rc3. The pointer has
; to be copied somewhere indirectable that JOYSTICK_GET will not disturb;
; input.s uses no T bytes, so T0/T1 is free.
;
; JOYSTICK_GET hands back exactly the A/X pair llvm-mos wants for a 16-bit
; return, plus presence in Y, so the shim's only work is unpacking Y.
x16_joy_get:
        pha                             ; joy
        lda     __rc2
        sta     X16_T0                  ; present*
        lda     __rc3
        sta     X16_T1
        pla                             ; A = joy

        jsr     JOYSTICK_GET            ; A = byte0, X = byte1, Y = $00/$FF
        pha
        phx                             ; park the buttons; neither affects Y

        cpy     #JOY_PRESENT
        beq     .Lx16_joy_get_present
        lda     #0
        bra     .Lx16_joy_get_store
.Lx16_joy_get_present:
        lda     #1
.Lx16_joy_get_store:
        ldy     #0
        sta     (X16_T0),y

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
x16_mouse_show:
mouse_show:
        ldx     #0
        ldy     #0
        jmp     MOUSE_CONFIG

; ---------------------------------------------------------------------
; void x16_mouse_hide(void)
; ---------------------------------------------------------------------
x16_mouse_hide:
mouse_hide:
        lda     #0
        ldx     #0
        ldy     #0
        jmp     MOUSE_CONFIG

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_mouse_get(unsigned int *x, unsigned int *y)
;   returns the button mask: bit 0 left, bit 1 right, bit 2 middle
; ---------------------------------------------------------------------
; Two pointers: x* in __rc2/__rc3, y* in __rc4/__rc5. mouse_get answers
; through X16_P0..P3 -- the KERNAL writes them itself -- so the
; destinations must live elsewhere: T0/T1 and T2/T3.
x16_mouse_get:
        lda     __rc2
        sta     X16_T0                  ; x*
        lda     __rc3
        sta     X16_T1
        lda     __rc4
        sta     X16_T2                  ; y*
        lda     __rc5
        sta     X16_T3

        jsr     mouse_get               ; X16_P0..P3, A = buttons
        pha

        ldy     #0
        lda     X16_P0
        sta     (X16_T0),y
        lda     X16_P2
        sta     (X16_T2),y
        iny
        lda     X16_P1
        sta     (X16_T0),y
        lda     X16_P3
        sta     (X16_T2),y

        pla                             ; a char return is A alone
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
x16_key_get:
        jmp     GETIN                   ; a char return is A alone

; ---------------------------------------------------------------------
; unsigned char x16_key_wait(void)
;   Block until a key is pressed.
; ---------------------------------------------------------------------
x16_key_wait:
.Lx16_key_wait_loop:
        jsr     GETIN
        beq     .Lx16_key_wait_loop
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned int x16_key_peek(void)
;   returns key | queued<<8, without consuming the key
;
; KBDBUF_PEEK returns the key in A and the queue depth in X -- again
; exactly cc65's 16-bit return convention, so there is nothing to do.
; ---------------------------------------------------------------------
x16_key_peek:
        jmp     KBDBUF_PEEK
