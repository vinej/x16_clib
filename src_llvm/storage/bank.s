; =====================================================================
; x16clib :: storage/bank.s -- banked RAM ($A000-$BFFF window)
; =====================================================================
; RAM_BANK ($00) selects which 8 KB bank appears at $A000-$BFFF.
; Bank 0 holds KERNAL variables; banks 1..255 are yours.
;
; Offsets are 0..8191 into the window. The bulk copies auto-advance
; across bank boundaries, so a run may start near the end of one bank
; and finish in the next.
;
; All routines here save and restore RAM_BANK, so they are safe to call
; without disturbing whatever bank the caller had mapped in.
;
; The heavy lifting is the KERNAL's MEMORY_COPY ($FEE7), one bank-segment
; per call, rather than a hand-rolled byte loop.
;
; cc65's BANK_RAM[] in <cx16.h> gives raw access to the window once you
; have set RAM_BANK yourself. These add the save/restore, the offset
; arithmetic, and the boundary crossing.
;
; The ACME original named its shared helpers `.segment` and `.advance`.
; `.segment` is a ca65 directive, so the first is `seg_span` here.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   POINTERS take __rc pairs, in order: __rc2/__rc3, then __rc4/__rc5.
;   INTEGER bytes fill A, then X, then whatever __rc bytes the pointers
;   left free.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_bank_set
        .globl  x16_bank_get
        .globl  x16_bank_peek
        .globl  x16_bank_poke
        .globl  x16_mem_to_bank
        .globl  x16_bank_to_mem
        .globl  x16_bank_copy_far

BANK_WINDOW     = $A000
BANK_WINDOW_END = $C000
BANK_SIZE       = BANK_WINDOW_END - BANK_WINDOW    ; 8192

BANK_BOUNCE_SIZE = 128

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_bank_set(unsigned char bank)
; unsigned char x16_bank_get(void)
;   Both safe from an ISR: neither touches the shared scratch block.
; ---------------------------------------------------------------------
x16_bank_set:
        sta     RAM_BANK
        rts

x16_bank_get:
        lda     RAM_BANK                ; a char return is A alone
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_bank_peek(unsigned char bank,
;                                          unsigned int offset)
; ---------------------------------------------------------------------
; A = bank, X = offset lo, __rc2 = offset hi.
x16_bank_peek:
        stx     X16_P0                  ; offset lo
        pha                             ; bank
        lda     __rc2
        sta     X16_P1                  ; offset hi
        pla                             ; A = bank
        jmp     bank_peek               ; the byte comes back in A

; ---------------------------------------------------------------------
; void __fastcall__ x16_bank_poke(unsigned char bank, unsigned int offset,
;                                 unsigned char value)
; ---------------------------------------------------------------------
; A = bank, X = offset lo, __rc2 = offset hi, __rc3 = value.
; bank_poke wants A = value, X = bank, X16_P0/P1 = offset.
x16_bank_poke:
        stx     X16_P0                  ; offset lo
        tax                             ; X = bank
        lda     __rc2
        sta     X16_P1                  ; offset hi
        lda     __rc3                   ; A = value
        jmp     bank_poke

; ---------------------------------------------------------------------
; void __fastcall__ x16_mem_to_bank(const void *src, unsigned char bank,
;                                   unsigned int offset, unsigned int count)
; ---------------------------------------------------------------------
; src is a pointer -> __rc2/__rc3. The integers then fill A, X, __rc4, ...
; in declaration order: bank -> A, offset lo -> X, offset hi -> __rc4,
; count lo -> __rc5, count hi -> __rc6.
x16_mem_to_bank:
        sta     X16_P2                  ; bank
        stx     X16_P3                  ; offset lo
        lda     __rc4
        sta     X16_P4                  ; offset hi
        lda     __rc5
        sta     X16_P5                  ; count lo
        lda     __rc6
        sta     X16_P6                  ; count hi
        lda     __rc2
        sta     X16_P0                  ; src
        lda     __rc3
        sta     X16_P1
        jmp     mem_to_bank

; ---------------------------------------------------------------------
; void __fastcall__ x16_bank_to_mem(unsigned char bank, unsigned int offset,
;                                   void *dst, unsigned int count)
; ---------------------------------------------------------------------
; dst is a pointer -> __rc2/__rc3. Integers fill A, X, __rc4, ...:
; bank -> A, offset lo -> X, offset hi -> __rc4, count lo -> __rc5,
; count hi -> __rc6.
x16_bank_to_mem:
        sta     X16_P0                  ; bank
        stx     X16_P1                  ; offset lo
        lda     __rc4
        sta     X16_P2                  ; offset hi
        lda     __rc5
        sta     X16_P5                  ; count lo
        lda     __rc6
        sta     X16_P6                  ; count hi
        lda     __rc2
        sta     X16_P3                  ; dst
        lda     __rc3
        sta     X16_P4
        jmp     bank_to_mem

; ---------------------------------------------------------------------
; void __fastcall__ x16_bank_copy_far(unsigned char src_bank,
;                                     unsigned int src_offset,
;                                     unsigned char dst_bank,
;                                     unsigned int dst_offset,
;                                     unsigned int count)
; ---------------------------------------------------------------------
; Seven integer bytes, no pointers: src_bank -> A, src_offset -> X/__rc2,
; dst_bank -> __rc3, dst_offset -> __rc4/__rc5, count -> __rc6/__rc7.
x16_bank_copy_far:
        sta     X16_P0                  ; src bank
        stx     X16_P1                  ; src offset lo
        lda     __rc2
        sta     X16_P2                  ; src offset hi
        lda     __rc3
        sta     X16_P3                  ; dst bank
        lda     __rc4
        sta     X16_P4                  ; dst offset lo
        lda     __rc5
        sta     X16_P5                  ; dst offset hi
        lda     __rc6
        sta     X16_P6                  ; count lo
        lda     __rc7
        sta     X16_P7                  ; count hi
        jmp     bank_copy_far

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; bank_peek -- in:  A = bank, X16_P0/P1 = offset (0..8191)
;              out: A = byte
; bank_poke -- in:  A = byte, X = bank, X16_P0/P1 = offset
; ---------------------------------------------------------------------
bank_peek:
        ldx     RAM_BANK
        phx
        sta     RAM_BANK
        jsr     window_ptr
        ldy     #0
        lda     (X16_T0),y
        plx
        stx     RAM_BANK
        rts

bank_poke:
        pha                             ; [byte]
        lda     RAM_BANK
        pha                             ; [byte][caller bank]
        stx     RAM_BANK
        jsr     window_ptr
        ldy     #0
        pla
        tax                             ; X = caller bank
        pla                             ; A = byte to store
        sta     (X16_T0),y
        stx     RAM_BANK
        rts

; T0/T1 = BANK_WINDOW + offset. Preserves A.
;
; A shared helper, so it gets a plain label rather than a cheap local: a
; cheap local only reaches from one plain label to the next, and bank_peek
; could not then see a helper defined after bank_poke.
window_ptr:
        pha
        lda     X16_P0
        clc
        adc     #<BANK_WINDOW
        sta     X16_T0
        lda     X16_P1
        adc     #>BANK_WINDOW
        sta     X16_T1
        pla
        rts

; ---------------------------------------------------------------------
; mem_to_bank -- copy low RAM into banked RAM
;   in:  X16_P0/P1 = source address
;        X16_P2    = destination bank
;        X16_P3/P4 = destination offset (0..8191)
;        X16_P5/P6 = byte count
;
; bank_to_mem -- the inverse
;   in:  X16_P0    = source bank
;        X16_P1/P2 = source offset
;        X16_P3/P4 = destination address
;        X16_P5/P6 = byte count
;
; Both auto-advance: a run that hits the end of a bank continues at
; offset 0 of the next.
; ---------------------------------------------------------------------
mem_to_bank:
        lda     RAM_BANK
        pha
        lda     X16_P2
        sta     RAM_BANK

        lda     X16_P0                  ; T0/T1 = low-RAM side
        sta     X16_T0
        lda     X16_P1
        sta     X16_T1
        lda     X16_P3                  ; T2/T3 = offset within the window
        sta     X16_T2
        lda     X16_P4
        sta     X16_T3
        lda     X16_P5                  ; T4/T5 = remaining
        sta     X16_T4
        lda     X16_P6
        sta     X16_T5

.Lmem_to_bank_seg_out:
        jsr     seg_span                ; T6/T7 = bytes until the bank edge
        beq     .Lmem_to_bank_out_done
        lda     X16_T0                  ; source: low RAM
        sta     r0L
        lda     X16_T1
        sta     r0H
        lda     X16_T2                  ; target: window + offset
        sta     r1L
        lda     X16_T3
        clc
        adc     #>BANK_WINDOW
        sta     r1H
        lda     X16_T6
        sta     r2L
        lda     X16_T7
        sta     r2H
        jsr     MEMORY_COPY
        jsr     advance
        bra     .Lmem_to_bank_seg_out
.Lmem_to_bank_out_done:
        pla
        sta     RAM_BANK
        rts

bank_to_mem:
        lda     RAM_BANK
        pha
        lda     X16_P0
        sta     RAM_BANK

        lda     X16_P3                  ; T0/T1 = low-RAM side
        sta     X16_T0
        lda     X16_P4
        sta     X16_T1
        lda     X16_P1                  ; T2/T3 = offset within the window
        sta     X16_T2
        lda     X16_P2
        sta     X16_T3
        lda     X16_P5
        sta     X16_T4
        lda     X16_P6
        sta     X16_T5

.Lbank_to_mem_seg_in:
        jsr     seg_span
        beq     .Lbank_to_mem_in_done
        lda     X16_T2                  ; source: window + offset
        sta     r0L
        lda     X16_T3
        clc
        adc     #>BANK_WINDOW
        sta     r0H
        lda     X16_T0                  ; target: low RAM
        sta     r1L
        lda     X16_T1
        sta     r1H
        lda     X16_T6
        sta     r2L
        lda     X16_T7
        sta     r2H
        jsr     MEMORY_COPY
        jsr     advance
        bra     .Lbank_to_mem_seg_in
.Lbank_to_mem_in_done:
        pla
        sta     RAM_BANK
        rts

; --- shared helpers --------------------------------------------------

; T6/T7 = min(remaining, space left in this bank). Z set when nothing
; remains.
seg_span:
        lda     X16_T4
        ora     X16_T5
        beq     seg_done                ; remaining == 0 (Z set for the caller)

        sec                             ; space = $2000 - offset
        lda     #<BANK_SIZE
        sbc     X16_T2
        sta     X16_T6
        lda     #>BANK_SIZE
        sbc     X16_T3
        sta     X16_T7

        lda     X16_T5                  ; remaining < space? then take remaining
        cmp     X16_T7
        bcc     seg_take_rem
        bne     seg_have
        lda     X16_T4
        cmp     X16_T6
        bcs     seg_have
seg_take_rem:
        lda     X16_T4
        sta     X16_T6
        lda     X16_T5
        sta     X16_T7
seg_have:
        lda     #1                      ; Z clear: there is work to do
seg_done:
        rts

; consume T6/T7 bytes: advance the low-RAM pointer and the window offset
; (rolling into the next bank), shrink the remaining count.
advance:
        clc
        lda     X16_T0
        adc     X16_T6
        sta     X16_T0
        lda     X16_T1
        adc     X16_T7
        sta     X16_T1

        clc
        lda     X16_T2
        adc     X16_T6
        sta     X16_T2
        lda     X16_T3
        adc     X16_T7
        sta     X16_T3
        cmp     #>BANK_SIZE             ; offset reached $2000: next bank
        bne     .Ladvance_count
        stz     X16_T2
        stz     X16_T3
        inc     RAM_BANK
.Ladvance_count:
        sec
        lda     X16_T4
        sbc     X16_T6
        sta     X16_T4
        lda     X16_T5
        sbc     X16_T7
        sta     X16_T5
        rts

; ---------------------------------------------------------------------
; bank_copy_far -- copy banked RAM to banked RAM
;   in:  X16_P0    = source bank,      X16_P1/P2 = source offset
;        X16_P3    = destination bank, X16_P4/P5 = destination offset
;        X16_P6/P7 = byte count
;
; Only one bank fits in the $A000 window at a time, so this bounces
; through a small low-RAM buffer, MEMORY_COPY on both legs. Both sides
; auto-advance across bank boundaries. The parameter block is consumed.
; ---------------------------------------------------------------------
bank_copy_far:
        lda     RAM_BANK
        pha

.Lbank_copy_far_far_loop:
        lda     X16_P6
        ora     X16_P7
        bne     .Lbank_copy_far_far_more
        jmp     .Lbank_copy_far_far_done               ; out of branch range from here
.Lbank_copy_far_far_more:

        ; chunk = min(count, bounce size, source bank space, dest space)
        ldx     #BANK_BOUNCE_SIZE
        lda     X16_P7
        bne     .Lbank_copy_far_far_src_cap            ; count >= 256: the buffer is the cap
        lda     X16_P6
        cmp     #BANK_BOUNCE_SIZE
        bcs     .Lbank_copy_far_far_src_cap
        tax                             ; count < buffer: count is the cap
.Lbank_copy_far_far_src_cap:
        ; Space to the end of a bank only matters when the offset is in the
        ; window's last page: below that, more than a full chunk remains.
        sec
        lda     #<BANK_SIZE
        sbc     X16_P1
        sta     X16_T0
        lda     #>BANK_SIZE
        sbc     X16_P2
        bne     .Lbank_copy_far_far_dst_cap            ; >= 256 bytes left in the source bank
        txa
        cmp     X16_T0
        bcc     .Lbank_copy_far_far_dst_cap
        ldx     X16_T0
.Lbank_copy_far_far_dst_cap:
        sec
        lda     #<BANK_SIZE
        sbc     X16_P4
        sta     X16_T0
        lda     #>BANK_SIZE
        sbc     X16_P5
        bne     .Lbank_copy_far_far_go
        txa
        cmp     X16_T0
        bcc     .Lbank_copy_far_far_go
        ldx     X16_T0
.Lbank_copy_far_far_go:
        stx     X16_T7                  ; T7 = chunk (1..BANK_BOUNCE_SIZE)

        lda     X16_P0                  ; leg 1: source bank -> bounce buffer
        sta     RAM_BANK
        lda     X16_P1
        sta     r0L
        lda     X16_P2
        clc
        adc     #>BANK_WINDOW
        sta     r0H
        lda     #<bounce
        sta     r1L
        lda     #>bounce
        sta     r1H
        stx     r2L
        stz     r2H
        jsr     MEMORY_COPY

        lda     X16_P3                  ; leg 2: bounce buffer -> destination
        sta     RAM_BANK
        lda     #<bounce
        sta     r0L
        lda     #>bounce
        sta     r0H
        lda     X16_P4
        sta     r1L
        lda     X16_P5
        clc
        adc     #>BANK_WINDOW
        sta     r1H
        lda     X16_T7
        sta     r2L
        stz     r2H
        jsr     MEMORY_COPY

        clc                             ; advance the source (rolls at $2000)
        lda     X16_P1
        adc     X16_T7
        sta     X16_P1
        lda     X16_P2
        adc     #0
        sta     X16_P2
        cmp     #>BANK_SIZE
        bne     .Lbank_copy_far_far_adv_dst
        stz     X16_P1
        stz     X16_P2
        inc     X16_P0
.Lbank_copy_far_far_adv_dst:
        clc
        lda     X16_P4
        adc     X16_T7
        sta     X16_P4
        lda     X16_P5
        adc     #0
        sta     X16_P5
        cmp     #>BANK_SIZE
        bne     .Lbank_copy_far_far_count
        stz     X16_P4
        stz     X16_P5
        inc     X16_P3
.Lbank_copy_far_far_count:
        sec
        lda     X16_P6
        sbc     X16_T7
        sta     X16_P6
        lda     X16_P7
        sbc     #0
        sta     X16_P7
        jmp     .Lbank_copy_far_far_loop

.Lbank_copy_far_far_done:
        pla
        sta     RAM_BANK
        rts

; The bounce buffer must live in low RAM, never in the banked window.
; cc65's cx16.cfg puts BSS between the program and the C stack, so it
; always does.
        .section .bss,"aw",@nobits

bounce: .zero  BANK_BOUNCE_SIZE
