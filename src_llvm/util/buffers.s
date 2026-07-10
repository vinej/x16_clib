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

        .include        "macros.inc"
        .include        "x16zp.inc"

        .globl  x16_rb_init
        .globl  x16_rb_put
        .globl  x16_rb_get
        .globl  x16_rb_count
        .globl  x16_stk_init
        .globl  x16_stk_push
        .globl  x16_stk_pop
        .globl  x16_stk_depth

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void x16_rb_init(void)
; unsigned char __fastcall__ x16_rb_put(unsigned char b) -- 1 stored, 0 full
; int x16_rb_get(void)                                   -- -1 when empty
; unsigned char x16_rb_count(void)
; ---------------------------------------------------------------------
x16_rb_init:
rb_init:
        stz     rb_head
        stz     rb_tail
        stz     rb_len
        rts

x16_rb_put:
        jsr     rb_put                  ; carry set = full
        lda     #0
        rol     a
        eor     #1                      ; report stored, not full
        rts

rb_put:
        pha
        lda     rb_len
        cmp     #255
        bcs     .Lrb_put_full
        pla
        phx
        ldx     rb_head
        sta     rb_data,x
        inc     rb_head
        inc     rb_len
        plx
        clc
        rts
.Lrb_put_full:
        pla
        sec
        rts

x16_rb_get:
        jsr     rb_get                  ; carry set = empty
        bcs     .Lx16_rb_get_empty
        ldx     #0
        rts
.Lx16_rb_get_empty:
        lda     #$FF                    ; -1
        tax
        rts

rb_get:
        lda     rb_len
        beq     .Lrb_get_empty
        phx
        ldx     rb_tail
        lda     rb_data,x
        inc     rb_tail
        dec     rb_len
        plx
        clc
        rts
.Lrb_get_empty:
        sec
        rts

x16_rb_count:
        lda     rb_len                  ; a char return is A alone
        rts

; ---------------------------------------------------------------------
; void x16_stk_init(void)
; unsigned char __fastcall__ x16_stk_push(unsigned char b) -- 0 if full
; int x16_stk_pop(void)                                    -- -1 when empty
; unsigned char x16_stk_depth(void)
; ---------------------------------------------------------------------
x16_stk_init:
stk_init:
        stz     stk_sp
        rts

x16_stk_push:
        jsr     stk_push                ; carry set = full
        lda     #0
        rol     a
        eor     #1
        rts

stk_push:
        pha
        lda     stk_sp
        cmp     #255
        bcs     .Lstk_push_full
        pla
        phx
        ldx     stk_sp
        sta     stk_data,x
        inc     stk_sp
        plx
        clc
        rts
.Lstk_push_full:
        pla
        sec
        rts

x16_stk_pop:
        jsr     stk_pop                 ; carry set = empty
        bcs     .Lx16_stk_pop_empty
        ldx     #0
        rts
.Lx16_stk_pop_empty:
        lda     #$FF                    ; -1
        tax
        rts

stk_pop:
        lda     stk_sp
        beq     .Lstk_pop_empty
        phx
        dec     stk_sp
        ldx     stk_sp
        lda     stk_data,x
        plx
        clc
        rts
.Lstk_pop_empty:
        sec
        rts

x16_stk_depth:
        lda     stk_sp
        rts

        .section .bss,"aw",@nobits

rb_head:  .zero  1
rb_tail:  .zero  1
rb_len:   .zero  1
rb_data:  .zero  256
stk_sp:   .zero  1
stk_data: .zero  256
