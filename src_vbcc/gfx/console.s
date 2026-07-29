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

        include        "macros.inc"
        include        "x16zp.inc"

        zpage	r0
        zpage	r2


        global	_x16_con_init
        global	_x16_con_put_char
        global	_x16_con_get_char
        global	_x16_con_set_paging_message
        global	_x16_con_disable_paging
        global	_x16_con_put_image

        section text

; ---------------------------------------------------------------------
; void __fastcall__ x16_con_init(unsigned int x, unsigned int y,
;                                unsigned int width, unsigned int height)
;
; Open a console in the given rectangle. All zeroes uses the full
; screen. Clears the area.
; ---------------------------------------------------------------------
_x16_con_init:
        jmp     CONSOLE_INIT            ; vbcc's r0/r1, r2/r3, r4/r5 and
                                        ; r6/r7 ARE the KERNAL's r0..r3, in
                                        ; that order: nothing to marshal

; ---------------------------------------------------------------------
; void __fastcall__ x16_con_put_char(unsigned char c,
;                                    unsigned char wrap)
;
; wrap 0 breaks lines mid-word (character wrap); nonzero buffers each
; word and breaks between them (word wrap). Scrolls, and pages if a
; paging message is set.
; ---------------------------------------------------------------------
_x16_con_put_char:
        lda     r2                      ; wrap
        cmp     #1                      ; carry set iff wrap != 0
        lda     r0                      ; A = c, and lda leaves the carry
        jmp     CONSOLE_PUT_CHAR

; ---------------------------------------------------------------------
; unsigned char x16_con_get_char(void)
;
; Line input: BLOCKS until the user finishes a line with RETURN, then
; hands it back one character per call, CR last. Editing is limited to
; backspace.
; ---------------------------------------------------------------------
_x16_con_get_char:
        jsr     CONSOLE_GET_CHAR
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_con_set_paging_message(const char *msg)
;
; After a full page of output the console shows `msg` (NUL-terminated)
; and waits for a key before scrolling on.
; ---------------------------------------------------------------------
_x16_con_set_paging_message:
        jmp     CONSOLE_SET_PAGING_MESSAGE      ; msg already rides r0/r1

; ---------------------------------------------------------------------
; void x16_con_disable_paging(void)
;   scroll freely, never prompt (the power-on state)
; ---------------------------------------------------------------------
_x16_con_disable_paging:
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
_x16_con_put_image:
        jmp     CONSOLE_PUT_IMAGE       ; image, width and height already sit
                                        ; in the KERNAL's r0, r1 and r2
