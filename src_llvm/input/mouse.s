; =====================================================================
; x16clib :: input/mouse.s -- the full KERNAL mouse surface
; =====================================================================
; Complements the compact helpers in input.s, which already wrap the
; common cases. Ported from x16_library input/mouse.asm; the overlap
; maps onto the existing API instead of being duplicated:
;
;       upstream mse_show      -> x16_mouse_show(cursor)  (x16/input.h)
;       upstream mse_show_keep -> x16_mouse_show(0xFF)
;       upstream mse_hide      -> x16_mouse_hide()
;       upstream mse_get       -> x16_mouse_get(&x, &y)   (buttons only)
;       upstream mse_get_to    -> not exposed: its argument is a zero-
;                                 page address, which C cannot provide;
;                                 x16_mse_get uses it internally
;
; New here: the raw MOUSE_CONFIG (show selector plus mouse-field bounds),
; MOUSE_SCAN, and a mse_get that also returns the scroll-wheel delta.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; (import dropped: popa, popax)
; cc65 lends ptr1/ptr2/ptr3 for indirection; llvm-mos has no such
; runtime, so these use the library's own scratch pairs.

        .globl  x16_mse_config
        .globl  x16_mse_scan
        .globl  x16_mse_get

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void __fastcall__ x16_mse_config(unsigned char show,
;                                  unsigned char width8,
;                                  unsigned char height8)
;
; `show` is $00 to hide the pointer, $FF to show it without changing the
; cursor sprite, or a sprite number n to show cursor n. `width8` and
; `height8` bound the mouse field in 8-pixel units; both 0 leaves the
; current bounds unchanged.
; ---------------------------------------------------------------------
x16_mse_config:
        ldy     mos8(__rc2)             ; Y = height8; A = show and
        jmp     MOUSE_CONFIG            ; X = width8 arrive in place

; ---------------------------------------------------------------------
; void x16_mse_scan(void)
;
; Sample the mouse once. The KERNAL's IRQ already does this every frame;
; you only need it if you have taken the interrupt over.
; ---------------------------------------------------------------------
x16_mse_scan:
        jmp     MOUSE_SCAN

; ---------------------------------------------------------------------
; signed char __fastcall__ x16_mse_get(unsigned int *x, unsigned int *y,
;                                      unsigned char *buttons)
;   returns the signed scroll-wheel delta since the last call
;
; The superset of x16_mouse_get(): position through the pointers,
; buttons through the third, and the wheel as the return value.
; ---------------------------------------------------------------------
x16_mse_get:
        lda     mos8(__rc2)             ; x*: the first pointer pair
        sta     mos8(X16_TPTR0)
        lda     mos8(__rc3)
        sta     mos8(X16_TPTR0+1)
        lda     mos8(__rc4)             ; y*
        sta     mos8(X16_TPTR1)
        lda     mos8(__rc5)
        sta     mos8(X16_TPTR1+1)
        lda     mos8(__rc6)             ; buttons*
        sta     mos8(X16_TPTR2)
        lda     mos8(__rc7)
        sta     mos8(X16_TPTR2+1)

        ldx     #<X16_P0
        jsr     MOUSE_GET               ; X16_P0..P3, A = buttons, X = wheel
        phx                             ; park the wheel

        ldy     #0
        sta     (X16_TPTR2),y                ; *buttons
        lda     mos8(X16_P0)
        sta     (X16_TPTR0),y
        lda     mos8(X16_P2)
        sta     (X16_TPTR1),y
        iny
        lda     mos8(X16_P1)
        sta     (X16_TPTR0),y
        lda     mos8(X16_P3)
        sta     (X16_TPTR1),y

        pla                             ; A = wheel delta
        ldx     #0                      ; sign-extend for int-promoting callers
        cmp     #$80
        bcc     .Lx16_mse_get_plus
        dex                             ; X = $FF
.Lx16_mse_get_plus:
        rts
