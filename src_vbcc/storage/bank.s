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

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc argument registers, plus the C soft-stack pointer for the fifth
; argument of bank_copy_far. Each shim reads its args out of these into
; the P block BEFORE the internal routine stuffs KERNAL banks/registers.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3
        zpage	r4
        zpage	r5
        zpage	r6
        zpage	r7
        zpage	sp

        global	_x16_bank_set
        global	_x16_bank_get
        global	_x16_bank_peek
        global	_x16_bank_poke
        global	_x16_mem_to_bank
        global	_x16_bank_to_mem
        global	_x16_bank_copy_far

BANK_WINDOW     = $A000
BANK_WINDOW_END = $C000
BANK_SIZE       = BANK_WINDOW_END - BANK_WINDOW    ; 8192

BANK_BOUNCE_SIZE = 128

        section text

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_bank_set(unsigned char bank)
; unsigned char x16_bank_get(void)
;   Both safe from an ISR: neither touches the shared scratch block.
; ---------------------------------------------------------------------
_x16_bank_set:
        sta     RAM_BANK
        rts

_x16_bank_get:
        lda     RAM_BANK
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; unsigned char x16_bank_peek(__reg("r0") unsigned char bank,
;                             __reg("r2/r3") unsigned int offset)
;   bank_peek wants A = bank, P0/P1 = offset.
; ---------------------------------------------------------------------
_x16_bank_peek:
        lda     r2
        sta     X16_P0                  ; offset lo
        lda     r3
        sta     X16_P1                  ; offset hi
        lda     r0                      ; A = bank
        jsr     bank_peek
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void x16_bank_poke(__reg("r0") unsigned char bank,
;                    __reg("r2/r3") unsigned int offset,
;                    __reg("r4") unsigned char value)
;   bank_poke wants A = value, X = bank, P0/P1 = offset.
; ---------------------------------------------------------------------
_x16_bank_poke:
        lda     r2
        sta     X16_P0                  ; offset lo
        lda     r3
        sta     X16_P1                  ; offset hi
        ldx     r0                      ; X = bank
        lda     r4                      ; A = value
        jmp     bank_poke

; ---------------------------------------------------------------------
; void x16_mem_to_bank(__reg("r0/r1") const void *src, __reg("r2") unsigned char bank,
;                      __reg("r4/r5") unsigned int offset, __reg("r6/r7") unsigned int count)
;   mem_to_bank wants P0/P1 = src, P2 = bank, P3/P4 = offset, P5/P6 = count.
; ---------------------------------------------------------------------
_x16_mem_to_bank:
        lda     r0
        sta     X16_P0                  ; src
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2                  ; bank
        lda     r4
        sta     X16_P3                  ; offset
        lda     r5
        sta     X16_P4
        lda     r6
        sta     X16_P5                  ; count
        lda     r7
        sta     X16_P6
        jmp     mem_to_bank

; ---------------------------------------------------------------------
; void x16_bank_to_mem(__reg("r0") unsigned char bank, __reg("r2/r3") unsigned int offset,
;                      __reg("r4/r5") void *dst, __reg("r6/r7") unsigned int count)
;   bank_to_mem wants P0 = bank, P1/P2 = offset, P3/P4 = dst, P5/P6 = count.
; ---------------------------------------------------------------------
_x16_bank_to_mem:
        lda     r0
        sta     X16_P0                  ; bank
        lda     r2
        sta     X16_P1                  ; offset
        lda     r3
        sta     X16_P2
        lda     r4
        sta     X16_P3                  ; dst
        lda     r5
        sta     X16_P4
        lda     r6
        sta     X16_P5                  ; count
        lda     r7
        sta     X16_P6
        jmp     bank_to_mem

; ---------------------------------------------------------------------
; void x16_bank_copy_far(__reg("r0") unsigned char src_bank,
;                        __reg("r2/r3") unsigned int src_offset,
;                        __reg("r4") unsigned char dst_bank,
;                        __reg("r6/r7") unsigned int dst_offset,
;                        unsigned int count)
;
; Five arguments overflow the r0..r7 file: the last one, count, spills to
; the C soft stack at (sp)+0..1 (with no frame of our own, the caller's
; push leaves it there). bank_copy_far wants P0 = src_bank, P1/P2 =
; src_offset, P3 = dst_bank, P4/P5 = dst_offset, P6/P7 = count.
; ---------------------------------------------------------------------
_x16_bank_copy_far:
        lda     r0
        sta     X16_P0                  ; src bank
        lda     r2
        sta     X16_P1                  ; src offset
        lda     r3
        sta     X16_P2
        lda     r4
        sta     X16_P3                  ; dst bank
        lda     r6
        sta     X16_P4                  ; dst offset
        lda     r7
        sta     X16_P5
        ldy     #0
        lda     (sp),y
        sta     X16_P6                  ; count lo (stacked 5th arg)
        iny
        lda     (sp),y
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

.seg_out:
        jsr     seg_span                ; T6/T7 = bytes until the bank edge
        beq     .out_done
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
        bra     .seg_out
.out_done:
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

.seg_in:
        jsr     seg_span
        beq     .in_done
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
        bra     .seg_in
.in_done:
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
        bne     .count
        stz     X16_T2
        stz     X16_T3
        inc     RAM_BANK
.count:
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

.far_loop:
        lda     X16_P6
        ora     X16_P7
        bne     .far_more
        jmp     .far_done               ; out of branch range from here
.far_more:

        ; chunk = min(count, bounce size, source bank space, dest space)
        ldx     #BANK_BOUNCE_SIZE
        lda     X16_P7
        bne     .far_src_cap            ; count >= 256: the buffer is the cap
        lda     X16_P6
        cmp     #BANK_BOUNCE_SIZE
        bcs     .far_src_cap
        tax                             ; count < buffer: count is the cap
.far_src_cap:
        ; Space to the end of a bank only matters when the offset is in the
        ; window's last page: below that, more than a full chunk remains.
        sec
        lda     #<BANK_SIZE
        sbc     X16_P1
        sta     X16_T0
        lda     #>BANK_SIZE
        sbc     X16_P2
        bne     .far_dst_cap            ; >= 256 bytes left in the source bank
        txa
        cmp     X16_T0
        bcc     .far_dst_cap
        ldx     X16_T0
.far_dst_cap:
        sec
        lda     #<BANK_SIZE
        sbc     X16_P4
        sta     X16_T0
        lda     #>BANK_SIZE
        sbc     X16_P5
        bne     .far_go
        txa
        cmp     X16_T0
        bcc     .far_go
        ldx     X16_T0
.far_go:
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
        bne     .far_adv_dst
        stz     X16_P1
        stz     X16_P2
        inc     X16_P0
.far_adv_dst:
        clc
        lda     X16_P4
        adc     X16_T7
        sta     X16_P4
        lda     X16_P5
        adc     #0
        sta     X16_P5
        cmp     #>BANK_SIZE
        bne     .far_count
        stz     X16_P4
        stz     X16_P5
        inc     X16_P3
.far_count:
        sec
        lda     X16_P6
        sbc     X16_T7
        sta     X16_P6
        lda     X16_P7
        sbc     #0
        sta     X16_P7
        jmp     .far_loop

.far_done:
        pla
        sta     RAM_BANK
        rts

; The bounce buffer must live in low RAM, never in the banked window.
; cc65's cx16.cfg puts BSS between the program and the C stack, so it
; always does.
        section bss

bounce: reserve    BANK_BOUNCE_SIZE
