; =====================================================================
; x16clib :: storage/bankalloc.s -- whole-bank RAM allocator
; =====================================================================
; Banked RAM is 8 KB pages at $A000, selected by RAM_BANK. The natural
; allocation unit IS the bank: sample sets, level maps, decompression
; buffers. This is a bitmap allocator over banks 1-255 (bank 0 belongs
; to the KERNAL).
;
; The allocator hands out BANK NUMBERS; it never touches RAM_BANK itself.
; Combine with x16_bank_peek/poke, x16_mem_to_bank/bank_to_mem and
; x16_bank_copy_far from storage/bank.s.
;
; ba_map lives in BSS, which cc65's crt0 zeroes, so before
; x16_bank_alloc_init() every bank reads as "not free" and
; x16_bank_alloc() fails cleanly rather than handing out bank 0.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa

        .export         _x16_bank_alloc_init
        .export         _x16_bank_alloc
        .export         _x16_bank_free
        .export         _x16_bank_reserve

        .segment        "CODE"

; ---------------------------------------------------------------------
; void __fastcall__ x16_bank_alloc_init(unsigned char first,
;                                       unsigned char last)
;   first <= last, both inclusive. Call again to reset the pool.
; ---------------------------------------------------------------------
_x16_bank_alloc_init:
        tax                             ; X = last (rightmost arg, in A)
        jsr     popa                    ; A = first
        ; fall through

; bank_alloc_init -- in: A = first bank, X = last bank (inclusive)
;
; Banks outside the range are never handed out.
bank_alloc_init:
        sta     X16_T0                  ; first
        stx     X16_T1                  ; last

        ldx     #31                     ; everything starts out un-ownable
@clear:
        stz     ba_map,x
        dex
        bpl     @clear

        lda     X16_T0
@mark:
        jsr     set_bit                 ; mark free
        lda     X16_T0
        cmp     X16_T1
        beq     @done
        inc     X16_T0
        lda     X16_T0
        bra     @mark
@done:
        rts

; ---------------------------------------------------------------------
; unsigned char x16_bank_alloc(void)
;   The lowest free bank, or 0 when the pool is exhausted.
;
; Bank 0 is the KERNAL's and can never be in the pool, so 0 is an
; unambiguous failure value -- no need to surface the carry.
; ---------------------------------------------------------------------
_x16_bank_alloc:
        jsr     bank_alloc              ; carry set = exhausted
        bcc     @ok
        lda     #0
@ok:
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; bank_alloc -- out: carry clear, A = bank number; or carry set: exhausted
;               Allocates the lowest free bank first.
bank_alloc:
        ldx     #0
@scan:
        lda     ba_map,x
        bne     @found
        inx
        cpx     #32
        bne     @scan
        sec                             ; nothing free
        rts
@found:
        ldy     #0
@bit:
        lda     ba_map,x
        and     bittab,y
        bne     @take
        iny
        bra     @bit                    ; must hit: the byte was nonzero
@take:
        lda     ba_map,x                ; clear the bit: bank is now in use
        eor     bittab,y
        sta     ba_map,x
        txa                             ; bank = byte index * 8 + bit index
        asl     a
        asl     a
        asl     a
        sta     X16_T0
        tya
        ora     X16_T0
        clc
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_bank_free(unsigned char bank)
;
; Freeing a bank that is already free, or that was never in the pool,
; quietly marks it allocatable -- there is no ownership record to check
; against, so don't do that.
; ---------------------------------------------------------------------
_x16_bank_free:
bank_free:
        ; fall through to set_bit

; mark bank A's bit in the map. Clobbers A, X, Y.
set_bit:
        pha
        lsr     a
        lsr     a
        lsr     a
        tax                             ; byte index
        pla
        and     #$07
        tay
        lda     ba_map,x
        ora     bittab,y
        sta     ba_map,x
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_bank_reserve(unsigned char bank)
;   1 if it was free and is now yours; 0 if it was already taken, or
;   outside the pool.
; ---------------------------------------------------------------------
_x16_bank_reserve:
        jsr     bank_reserve            ; carry set = already taken
        lda     #0
        ldx     #0
        rol     a
        eor     #1                      ; report success, not failure
        rts

; bank_reserve -- in: A = bank; out: carry clear if claimed
bank_reserve:
        pha
        lsr     a
        lsr     a
        lsr     a
        tax
        pla
        and     #$07
        tay
        lda     ba_map,x
        and     bittab,y
        beq     @taken
        lda     ba_map,x                ; it was free: clear the bit
        eor     bittab,y
        sta     ba_map,x
        clc
        rts
@taken:
        sec
        rts

        .segment        "RODATA"

bittab: .byte   $01, $02, $04, $08, $10, $20, $40, $80

; ---------------------------------------------------------------------
; One bit per bank; set = FREE. Zeroed by crt0, so nothing is
; allocatable until x16_bank_alloc_init() says so.
; ---------------------------------------------------------------------
        .segment        "BSS"

ba_map: .res    32
