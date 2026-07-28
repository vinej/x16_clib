; =====================================================================
; x16clib :: storage/ringbuffer.s -- an 8 KB FIFO ring in a HIRAM bank
; =====================================================================
; A first-in-first-out queue whose 8 KB of storage is one whole banked-RAM
; bank ($A000-$BFFF). Tell it which bank to own with ring_init, then put
; and get bytes or words. The head, tail and fill counters live in low
; RAM; only the queued data sits in the bank. There are no over/underflow
; guards -- the capacity is 8191 bytes; check ring_isfull / ring_isempty.
;
; Every routine saves and restores RAM_BANK. The small 256-byte ring that
; needs no bank is x16_rb_* in util/buffers.s; the bank NUMBER can come
; from anywhere, including storage/bankalloc.s's x16_bank_alloc().
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .globl  x16_ring_init
        .globl  x16_ring_put
        .globl  x16_ring_putw
        .globl  x16_ring_get
        .globl  x16_ring_getw
        .globl  x16_ring_size
        .globl  x16_ring_free
        .globl  x16_ring_isempty
        .globl  x16_ring_isfull

RING_CAP = 8192                 ; the bank window is 8192 bytes (0..8191)

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_ring_init(unsigned char bank)
; void __fastcall__ x16_ring_put(unsigned char b)
; void __fastcall__ x16_ring_putw(unsigned int w)
;   cc65 hands the int over as A = low, X = high -- exactly the internal
;   contract, so these pass straight through.
; ---------------------------------------------------------------------
x16_ring_init:
        jmp     ring_init

x16_ring_put:
        jmp     ring_put

x16_ring_putw:
        jmp     ring_putw

; ---------------------------------------------------------------------
; unsigned char x16_ring_get(void)
; unsigned int x16_ring_getw(void)
; unsigned int x16_ring_size(void)   -- bytes queued
; unsigned int x16_ring_free(void)   -- usable bytes free
; ---------------------------------------------------------------------
x16_ring_get:
        jsr     ring_get
        ldx     #0
        rts

x16_ring_getw:
        jmp     ring_getw               ; already A = low, X = high

x16_ring_size:
        jmp     ring_size

x16_ring_free:
        jmp     ring_free

; ---------------------------------------------------------------------
; unsigned char x16_ring_isempty(void) -- 1 if empty
; unsigned char x16_ring_isfull(void)  -- 1 if full (no room for a word)
; ---------------------------------------------------------------------
x16_ring_isempty:
        jsr     ring_isempty
        lda     #0
        ldx     #0
        rol     a
        rts

x16_ring_isfull:
        jsr     ring_isfull
        lda     #0
        ldx     #0
        rol     a
        rts

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; ring_init -- claim a bank and empty the queue.
;   in: A = HIRAM bank number
; ---------------------------------------------------------------------
ring_init:
        sta     ring_bank
        stz     ring_fill
        stz     ring_fill+1
        stz     ring_head
        stz     ring_head+1
        lda     #<(RING_CAP-1)          ; tail starts at the top; the first get's
        sta     ring_tail               ; inc_tail wraps it to 0, where head began
        lda     #>(RING_CAP-1)
        sta     ring_tail+1
        rts

; ---------------------------------------------------------------------
; ring_put -- enqueue one byte.  in: A = byte
; ---------------------------------------------------------------------
ring_put:
        sta     X16_T2
        lda     RAM_BANK
        sta     X16_T3
        lda     ring_bank
        sta     RAM_BANK
        jsr     ringbuffer_rhptr
        lda     X16_T2
        sta     (X16_T0)                ; buffer[head] = value
        lda     X16_T3
        sta     RAM_BANK
        jsr     ringbuffer_inchead
        jsr     ringbuffer_fillinc
        rts

; ---------------------------------------------------------------------
; ring_putw -- enqueue one word (low byte first).
;   in: A = low, X = high
; ---------------------------------------------------------------------
ring_putw:
        sta     X16_T2
        stx     X16_T4
        lda     RAM_BANK
        sta     X16_T3
        lda     ring_bank
        sta     RAM_BANK
        jsr     ringbuffer_rhptr
        lda     X16_T2
        sta     (X16_T0)                ; buffer[head] = low
        jsr     ringbuffer_inchead
        jsr     ringbuffer_rhptr
        lda     X16_T4
        sta     (X16_T0)                ; buffer[head] = high
        lda     X16_T3
        sta     RAM_BANK
        jsr     ringbuffer_inchead
        jsr     ringbuffer_fillinc
        jsr     ringbuffer_fillinc
        rts

; ---------------------------------------------------------------------
; ring_get -- dequeue one byte.  out: A = byte
; ---------------------------------------------------------------------
ring_get:
        jsr     ringbuffer_filldec
        jsr     ringbuffer_inctail
        lda     RAM_BANK
        sta     X16_T3
        lda     ring_bank
        sta     RAM_BANK
        jsr     ringbuffer_rtptr
        lda     (X16_T0)
        tay
        lda     X16_T3
        sta     RAM_BANK
        tya
        rts

; ---------------------------------------------------------------------
; ring_getw -- dequeue one word.  out: A = low, X = high
; ---------------------------------------------------------------------
ring_getw:
        jsr     ringbuffer_filldec
        jsr     ringbuffer_filldec
        lda     RAM_BANK
        sta     X16_T3
        lda     ring_bank
        sta     RAM_BANK
        jsr     ringbuffer_inctail
        jsr     ringbuffer_rtptr
        lda     (X16_T0)
        sta     X16_T2                  ; low
        jsr     ringbuffer_inctail
        jsr     ringbuffer_rtptr
        lda     (X16_T0)
        sta     X16_T4                  ; high
        lda     X16_T3
        sta     RAM_BANK
        lda     X16_T2
        ldx     X16_T4
        rts

; ---------------------------------------------------------------------
; ring_size -- out: A = low, X = high  (bytes queued = fill)
; ---------------------------------------------------------------------
ring_size:
        lda     ring_fill
        ldx     ring_fill+1
        rts

; ---------------------------------------------------------------------
; ring_free -- out: A = low, X = high  (usable bytes free)
; ---------------------------------------------------------------------
ring_free:
        sec
        lda     #<(RING_CAP-1)
        sbc     ring_fill
        pha
        lda     #>(RING_CAP-1)
        sbc     ring_fill+1
        tax
        pla
        rts

; ---------------------------------------------------------------------
; ring_isempty -- out: carry set if empty (fill == 0)
; ---------------------------------------------------------------------
ring_isempty:
        lda     ring_fill
        ora     ring_fill+1
        bne     ringbuffer_notempty
        sec
        rts
ringbuffer_notempty:
        clc
        rts

; ---------------------------------------------------------------------
; ring_isfull -- out: carry set if less than 2 bytes remain (fill >= 8191)
; ---------------------------------------------------------------------
ring_isfull:
        lda     ring_fill+1
        cmp     #>(RING_CAP-1)          ; $1F
        bcc     ringbuffer_notfull
        bne     ringbuffer_full
        lda     ring_fill
        cmp     #<(RING_CAP-1)          ; $FF
        bcc     ringbuffer_notfull
ringbuffer_full:
        sec
        rts
ringbuffer_notfull:
        clc
        rts

; --- helpers ---------------------------------------------------------
; T0/T1 = $A000 + ring_head
ringbuffer_rhptr:
        lda     ring_head
        sta     X16_T0
        lda     ring_head+1
        clc
        adc     #$A0
        sta     X16_T1
        rts

; T0/T1 = $A000 + ring_tail
ringbuffer_rtptr:
        lda     ring_tail
        sta     X16_T0
        lda     ring_tail+1
        clc
        adc     #$A0
        sta     X16_T1
        rts

; head++, wrapping to 0 when it reaches RING_CAP (8192)
ringbuffer_inchead:
        inc     ring_head
        bne     ringbuffer_inchead_hi
        inc     ring_head+1
ringbuffer_inchead_hi:
        lda     ring_head+1
        cmp     #>RING_CAP              ; $20
        bne     ringbuffer_inchead_done
        stz     ring_head
        stz     ring_head+1
ringbuffer_inchead_done:
        rts

; tail++, wrapping to 0 when it reaches RING_CAP
ringbuffer_inctail:
        inc     ring_tail
        bne     ringbuffer_inctail_hi
        inc     ring_tail+1
ringbuffer_inctail_hi:
        lda     ring_tail+1
        cmp     #>RING_CAP
        bne     ringbuffer_inctail_done
        stz     ring_tail
        stz     ring_tail+1
ringbuffer_inctail_done:
        rts

; fill++ / fill-- (16-bit)
ringbuffer_fillinc:
        inc     ring_fill
        bne     ringbuffer_fillinc_done
        inc     ring_fill+1
ringbuffer_fillinc_done:
        rts

ringbuffer_filldec:
        lda     ring_fill
        bne     ringbuffer_filldec_lo
        dec     ring_fill+1
ringbuffer_filldec_lo:
        dec     ring_fill
        rts

        .section .bss,"aw",@nobits

ring_bank: .zero  1              ; the HIRAM bank the queue owns
ring_fill: .zero  2              ; bytes currently queued
ring_head: .zero  2              ; where the next put goes
ring_tail: .zero  2              ; one before where the next get comes from
