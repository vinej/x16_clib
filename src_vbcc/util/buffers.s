; =====================================================================
; x16clib :: util/buffers.s -- byte ring buffer and byte stack
; =====================================================================
; The two structures an input queue and an audio refiller keep
; reinventing. One static instance of each, 256 bytes of storage, 8-bit
; indices so wrap-around is free. Capacity is 255 (a count byte
; distinguishes full from empty).
;
; Single-producer/single-consumer safe across an interrupt boundary is
; NOT promised: put and get both touch the count. If one side runs in an
; IRQ, wrap the other side's call in a critical section.
;
; The C bindings return -1 for "nothing there", like getchar(), which is
; how the carry flag crosses the boundary.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

        global	_x16_rb_init
        global	_x16_rb_put
        global	_x16_rb_get
        global	_x16_rb_count
        global	_x16_stk_init
        global	_x16_stk_push
        global	_x16_stk_pop
        global	_x16_stk_depth

        section text

; ---------------------------------------------------------------------
; void x16_rb_init(void)
; unsigned char __fastcall__ x16_rb_put(unsigned char b) -- 1 stored, 0 full
; int x16_rb_get(void)                                   -- -1 when empty
; unsigned char x16_rb_count(void)
; ---------------------------------------------------------------------
_x16_rb_init:
rb_init:
        stz     rb_head
        stz     rb_tail
        stz     rb_len
        rts

_x16_rb_put:
        jsr     rb_put                  ; carry set = full
        lda     #0
        ldx     #0
        rol     a
        eor     #1                      ; report stored, not full
        rts

rb_put:
        pha
        lda     rb_len
        cmp     #255
        bcs     .full
        pla
        phx
        ldx     rb_head
        sta     rb_data,x
        inc     rb_head
        inc     rb_len
        plx
        clc
        rts
.full:
        pla
        sec
        rts

_x16_rb_get:
        jsr     rb_get                  ; carry set = empty
        bcs     .empty
        ldx     #0
        rts
.empty:
        lda     #$FF                    ; -1
        tax
        rts

rb_get:
        lda     rb_len
        beq     .empty
        phx
        ldx     rb_tail
        lda     rb_data,x
        inc     rb_tail
        dec     rb_len
        plx
        clc
        rts
.empty:
        sec
        rts

_x16_rb_count:
        lda     rb_len
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; void x16_stk_init(void)
; unsigned char __fastcall__ x16_stk_push(unsigned char b) -- 0 if full
; int x16_stk_pop(void)                                    -- -1 when empty
; unsigned char x16_stk_depth(void)
; ---------------------------------------------------------------------
_x16_stk_init:
stk_init:
        stz     stk_sp
        rts

_x16_stk_push:
        jsr     stk_push                ; carry set = full
        lda     #0
        ldx     #0
        rol     a
        eor     #1
        rts

stk_push:
        pha
        lda     stk_sp
        cmp     #255
        bcs     .full
        pla
        phx
        ldx     stk_sp
        sta     stk_data,x
        inc     stk_sp
        plx
        clc
        rts
.full:
        pla
        sec
        rts

_x16_stk_pop:
        jsr     stk_pop                 ; carry set = empty
        bcs     .empty
        ldx     #0
        rts
.empty:
        lda     #$FF                    ; -1
        tax
        rts

stk_pop:
        lda     stk_sp
        beq     .empty
        phx
        dec     stk_sp
        ldx     stk_sp
        lda     stk_data,x
        plx
        clc
        rts
.empty:
        sec
        rts

_x16_stk_depth:
        lda     stk_sp
        ldx     #0
        rts

        section bss

rb_head:  reserve 1
rb_tail:  reserve 1
rb_len:   reserve 1
rb_data:  reserve 256
stk_sp:   reserve 1
stk_data: reserve 256
