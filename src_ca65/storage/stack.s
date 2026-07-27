; =====================================================================
; x16clib :: storage/stack.s -- an 8 KB LIFO stack in a HIRAM bank
; =====================================================================
; A last-in-first-out stack whose 8 KB of storage is one whole banked-RAM
; bank ($A000-$BFFF). Tell it which bank to own with stack_init, then push
; and pop bytes or words. It grows downward from the top of the bank; the
; stack POINTER and the free/size counters live in low RAM, so only the
; data itself sits in the bank. There are no over/underflow guards -- the
; capacity is 8191 bytes, check stack_isfull / stack_isempty yourself.
;
; Every routine saves and restores RAM_BANK, so a stack in bank 5 and your
; own use of bank 7 in between never trip over each other. The small
; 256-byte stack that does not need a bank is x16_stk_* in util/buffers.s.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .export         _x16_stack_init
        .export         _x16_stack_push
        .export         _x16_stack_pushw
        .export         _x16_stack_pop
        .export         _x16_stack_popw
        .export         _x16_stack_size
        .export         _x16_stack_free
        .export         _x16_stack_isempty
        .export         _x16_stack_isfull

STACK_TOP = 8191                ; top offset of the bank window (0..8191)

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_stack_init(unsigned char bank)
; void __fastcall__ x16_stack_push(unsigned char b)
; void __fastcall__ x16_stack_pushw(unsigned int w)
;   cc65 hands the int over as A = low, X = high -- exactly the internal
;   contract, so these pass straight through.
; ---------------------------------------------------------------------
_x16_stack_init:
        jmp     stack_init

_x16_stack_push:
        jmp     stack_push

_x16_stack_pushw:
        jmp     stack_pushw

; ---------------------------------------------------------------------
; unsigned char x16_stack_pop(void)
; unsigned int x16_stack_popw(void)
; unsigned int x16_stack_size(void)  -- bytes stored
; unsigned int x16_stack_free(void)  -- bytes free
; ---------------------------------------------------------------------
_x16_stack_pop:
        jsr     stack_pop
        ldx     #0
        rts

_x16_stack_popw:
        jmp     stack_popw              ; already A = low, X = high

_x16_stack_size:
        jmp     stack_size

_x16_stack_free:
        jmp     stack_free

; ---------------------------------------------------------------------
; unsigned char x16_stack_isempty(void) -- 1 if empty
; unsigned char x16_stack_isfull(void)  -- 1 if full (no room for a word)
; ---------------------------------------------------------------------
_x16_stack_isempty:
        jsr     stack_isempty
        lda     #0
        ldx     #0
        rol     a
        rts

_x16_stack_isfull:
        jsr     stack_isfull
        lda     #0
        ldx     #0
        rol     a
        rts

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; stack_init -- claim a bank and empty the stack.
;   in: A = HIRAM bank number
; ---------------------------------------------------------------------
stack_init:
        sta     stack_bank
        lda     #<STACK_TOP
        sta     stack_sp
        lda     #>STACK_TOP
        sta     stack_sp+1
        rts

; ---------------------------------------------------------------------
; stack_push -- push one byte.  in: A = byte
; ---------------------------------------------------------------------
stack_push:
        sta     X16_T2
        lda     RAM_BANK
        sta     X16_T3
        lda     stack_bank
        sta     RAM_BANK
        jsr     stack_sptr
        lda     X16_T2
        sta     (X16_T0)                ; buffer[sp] = value
        jsr     stack_spdec
        lda     X16_T3
        sta     RAM_BANK
        rts

; ---------------------------------------------------------------------
; stack_pushw -- push one word (low byte first, then high).
;   in: A = low, X = high
; ---------------------------------------------------------------------
stack_pushw:
        sta     X16_T2
        stx     X16_T4
        lda     RAM_BANK
        sta     X16_T3
        lda     stack_bank
        sta     RAM_BANK
        jsr     stack_sptr
        lda     X16_T2
        sta     (X16_T0)                ; buffer[sp] = low
        jsr     stack_spdec
        jsr     stack_sptr
        lda     X16_T4
        sta     (X16_T0)                ; buffer[sp] = high
        jsr     stack_spdec
        lda     X16_T3
        sta     RAM_BANK
        rts

; ---------------------------------------------------------------------
; stack_pop -- pop one byte.  out: A = byte
; ---------------------------------------------------------------------
stack_pop:
        lda     RAM_BANK
        sta     X16_T3
        lda     stack_bank
        sta     RAM_BANK
        jsr     stack_spinc
        jsr     stack_sptr
        lda     (X16_T0)
        tay
        lda     X16_T3
        sta     RAM_BANK
        tya
        rts

; ---------------------------------------------------------------------
; stack_popw -- pop one word.  out: A = low, X = high
; The high byte was pushed last, so it comes off first.
; ---------------------------------------------------------------------
stack_popw:
        lda     RAM_BANK
        sta     X16_T3
        lda     stack_bank
        sta     RAM_BANK
        jsr     stack_spinc
        jsr     stack_sptr
        lda     (X16_T0)
        sta     X16_T4                  ; high
        jsr     stack_spinc
        jsr     stack_sptr
        lda     (X16_T0)
        sta     X16_T2                  ; low
        lda     X16_T3
        sta     RAM_BANK
        lda     X16_T2
        ldx     X16_T4
        rts

; ---------------------------------------------------------------------
; stack_size -- out: A = low, X = high  (bytes stored = STACK_TOP - sp)
; ---------------------------------------------------------------------
stack_size:
        sec
        lda     #<STACK_TOP
        sbc     stack_sp
        pha
        lda     #>STACK_TOP
        sbc     stack_sp+1
        tax
        pla
        rts

; ---------------------------------------------------------------------
; stack_free -- out: A = low, X = high  (bytes free = sp)
; ---------------------------------------------------------------------
stack_free:
        lda     stack_sp
        ldx     stack_sp+1
        rts

; ---------------------------------------------------------------------
; stack_isempty -- out: carry set if empty (sp == STACK_TOP)
; ---------------------------------------------------------------------
stack_isempty:
        lda     stack_sp
        cmp     #<STACK_TOP
        bne     stack_notempty
        lda     stack_sp+1
        cmp     #>STACK_TOP
        bne     stack_notempty
        sec
        rts
stack_notempty:
        clc
        rts

; ---------------------------------------------------------------------
; stack_isfull -- out: carry set if less than 2 bytes remain
; (sp == 0, or sp has wrapped below 0 to > STACK_TOP)
;
; Ported as upstream wrote it. Note that the sp < 2 leg is unreachable:
; after `cmp #$20 : bcs` has excluded sp_hi >= $20, Z can never be set,
; so `bne stack_notfull` always branches. In practice fullness is
; reported one push late, once sp has wrapped to $FFFF.
; ---------------------------------------------------------------------
stack_isfull:
        lda     stack_sp+1
        cmp     #$20                    ; sp >= $2000: wrapped past the bottom
        bcs     stack_full
        bne     stack_notfull
        lda     stack_sp
        cmp     #2                      ; 0 or 1 byte free is full for pushw
        bcc     stack_full
stack_notfull:
        clc
        rts
stack_full:
        sec
        rts

; --- helpers ---------------------------------------------------------
; T0/T1 = $A000 + stack_sp
stack_sptr:
        lda     stack_sp
        sta     X16_T0
        lda     stack_sp+1
        clc
        adc     #$A0                    ; $A000's high byte; sp_hi <= $1F, no carry
        sta     X16_T1
        rts

; sp-- (16-bit)
stack_spdec:
        lda     stack_sp
        bne     stack_spdec_lo
        dec     stack_sp+1
stack_spdec_lo:
        dec     stack_sp
        rts

; sp++ (16-bit)
stack_spinc:
        inc     stack_sp
        bne     stack_spinc_hi
        inc     stack_sp+1
stack_spinc_hi:
        rts

        .segment        "BSS"

stack_bank: .res 1              ; the HIRAM bank the stack owns
stack_sp:   .res 2              ; 16-bit offset; grows down from STACK_TOP
