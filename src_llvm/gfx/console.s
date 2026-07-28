; =====================================================================
; x16clib :: gfx/console.s -- the KERNAL console API
; =====================================================================
; A proportional-font terminal that renders through GRAPH: word wrap,
; paging, inline images, line input. Deliberately its own module so a
; program can use GRAPH without linking the console, and vice versa.
;
; x16_con_init() calls GRAPH_init internally when needed, but the usual
; sequence is graph_init -> con_init -> put_char/get_char.
;
; Characters are ISO/ASCII here, not PETSCII -- the GRAPH font's
; encoding. The CON_ATTR_* codes in x16/console.h restyle the pen.
; =====================================================================

        .include        "macros.inc"

; (import dropped: popa, popax)

        .globl  x16_con_init
        .globl  x16_con_put_char
        .globl  x16_con_get_char
        .globl  x16_con_set_paging_message
        .globl  x16_con_disable_paging
        .globl  x16_con_put_image

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void __fastcall__ x16_con_init(unsigned int x, unsigned int y,
;                                unsigned int width, unsigned int height)
;
; Open a console in the given rectangle. All zeroes uses the full
; screen. Clears the area.
; ---------------------------------------------------------------------
x16_con_init:
        pha                             ; A and X hold the first
        phx                             ; argument; the loads below
                                        ; clobber both, so park them
        lda     __rc7
        sta     r3H
        lda     __rc6
        sta     r3L                ; height (rightmost arg: A/X)
        lda     __rc5
        sta     r2H
        lda     __rc4
        sta     r2L                ; width
        lda     __rc3
        sta     r1H
        lda     __rc2
        sta     r1L                ; y
        plx
        pla
        sta     r0L                ; x
        stx     r0H
        jmp     CONSOLE_INIT

; ---------------------------------------------------------------------
; void __fastcall__ x16_con_put_char(unsigned char c,
;                                    unsigned char wrap)
;
; wrap 0 breaks lines mid-word (character wrap); nonzero buffers each
; word and breaks between them (word wrap). Scrolls, and pages if a
; paging message is set.
; ---------------------------------------------------------------------
x16_con_put_char:
                                        ; c in A, wrap flag in X already
        cpx     #1                      ; carry set iff wrap != 0
        jmp     CONSOLE_PUT_CHAR

; ---------------------------------------------------------------------
; unsigned char x16_con_get_char(void)
;
; Line input: BLOCKS until the user finishes a line with RETURN, then
; hands it back one character per call, CR last. Editing is limited to
; backspace.
; ---------------------------------------------------------------------
x16_con_get_char:
        jsr     CONSOLE_GET_CHAR
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_con_set_paging_message(const char *msg)
;
; After a full page of output the console shows `msg` (NUL-terminated)
; and waits for a key before scrolling on.
; ---------------------------------------------------------------------
x16_con_set_paging_message:
                                        ; msg already sits in r0
        jmp     CONSOLE_SET_PAGING_MESSAGE

; ---------------------------------------------------------------------
; void x16_con_disable_paging(void)
;   scroll freely, never prompt (the power-on state)
; ---------------------------------------------------------------------
x16_con_disable_paging:
        stz     r0L
        stz     r0H
        jmp     CONSOLE_SET_PAGING_MESSAGE

; ---------------------------------------------------------------------
; void __fastcall__ x16_con_put_image(const unsigned char *image,
;                                     unsigned int width,
;                                     unsigned int height)
;
; Inline a GRAPH_draw_image-format bitmap at the cursor, like an
; oversized character. Fails the line if it cannot fit.
; ---------------------------------------------------------------------
x16_con_put_image:
        pha                             ; A and X hold the first
        phx                             ; argument; the loads below
                                        ; clobber both, so park them
        lda     __rc5
        sta     r2H
        lda     __rc4
        sta     r2L                ; height (rightmost arg: A/X)
        lda     __rc3
        sta     r1H
        lda     __rc2
        sta     r1L                ; width
        plx
        pla
        sta     r0L                ; image
        stx     r0H
        jmp     CONSOLE_PUT_IMAGE
