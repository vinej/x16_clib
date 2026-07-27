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

        .import         popa, popax
        .importzp       ptr1, ptr2, ptr3

        .export         _x16_mse_config
        .export         _x16_mse_scan
        .export         _x16_mse_get

        .segment        "CODE"

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
_x16_mse_config:
        pha                             ; height8 (rightmost arg, in A)
        jsr     popa                    ; width8
        pha
        jsr     popa                    ; A = show
        plx                             ; X = width8
        ply                             ; Y = height8
        jmp     MOUSE_CONFIG

; ---------------------------------------------------------------------
; void x16_mse_scan(void)
;
; Sample the mouse once. The KERNAL's IRQ already does this every frame;
; you only need it if you have taken the interrupt over.
; ---------------------------------------------------------------------
_x16_mse_scan:
        jmp     MOUSE_SCAN

; ---------------------------------------------------------------------
; signed char __fastcall__ x16_mse_get(unsigned int *x, unsigned int *y,
;                                      unsigned char *buttons)
;   returns the signed scroll-wheel delta since the last call
;
; The superset of x16_mouse_get(): position through the pointers,
; buttons through the third, and the wheel as the return value.
; ---------------------------------------------------------------------
_x16_mse_get:
        sta     ptr3                    ; buttons* (rightmost arg: A/X)
        stx     ptr3+1
        jsr     popax                   ; y*
        sta     ptr2
        stx     ptr2+1
        jsr     popax                   ; x*
        sta     ptr1
        stx     ptr1+1

        ldx     #<X16_P0
        jsr     MOUSE_GET               ; X16_P0..P3, A = buttons, X = wheel
        phx                             ; park the wheel

        ldy     #0
        sta     (ptr3),y                ; *buttons
        lda     X16_P0
        sta     (ptr1),y
        lda     X16_P2
        sta     (ptr2),y
        iny
        lda     X16_P1
        sta     (ptr1),y
        lda     X16_P3
        sta     (ptr2),y

        pla                             ; A = wheel delta
        ldx     #0                      ; sign-extend for int-promoting callers
        cmp     #$80
        bcc     @plus
        dex                             ; X = $FF
@plus:
        rts
