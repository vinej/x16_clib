// =====================================================================
// x16clib :: x16/bank.c -- banked RAM ($A000-$BFFF window)
// =====================================================================
// Same code as src_ca65/storage/bank.s. RAM_BANK ($00) selects which
// 8 KB bank appears at $A000-$BFFF; every routine here saves and
// restores it. The heavy lifting is the KERNAL's MEMORY_COPY ($FEE7),
// one bank-segment per call. The KERNAL's r0-r2 argument registers at
// $02-$07 are exactly why x16/zpsafe.h keeps KickC off that range.
//
// The seg_span/advance helpers the ca65 build shared between its two
// bulk copies are duplicated into each asm body here: inline asm cannot
// jsr across functions, and thirty lines twice beats a C-call dance in
// the middle of a copy loop.
// =====================================================================

#include <x16/bank.h>

// peek/poke's window pointer, pinned in zero page (it is indirected;
// KickC ignores __zp on parameters).
__address(0x78) char* volatile x16__bk_ptr;

__mem volatile char x16__bk_save;       // the caller's RAM_BANK
__mem volatile char x16__bk_t0;         // low-RAM pointer / scratch
__mem volatile char x16__bk_t1;
__mem volatile char x16__bk_t2;         // offset within the window
__mem volatile char x16__bk_t3;
__mem volatile char x16__bk_t4;         // remaining count
__mem volatile char x16__bk_t5;
__mem volatile char x16__bk_t6;         // this segment's span
__mem volatile char x16__bk_t7;

// The bounce buffer must live in low RAM, never in the banked window.
// KickC places data after the code, so it always does.
__mem char x16__bk_bounce[128];

// Both safe from an ISR: neither touches shared scratch.
void x16_bank_set(__mem unsigned char bank) {
    asm {
        lda bank
        sta $00 /*RAM_BANK*/
    }
}

unsigned char x16_bank_get(void) {
    __mem char r;
    asm {
        lda $00 /*RAM_BANK*/
        sta r
    }
    return r;
}

unsigned char x16_bank_peek(__mem unsigned char bank,
                            __mem unsigned int offset) {
    __mem char r;
    asm {
        lda $00 /*RAM_BANK*/
        sta x16__bk_save
        lda bank
        sta $00 /*RAM_BANK*/
        lda offset                      // window pointer = $A000 + offset
        sta x16__bk_ptr                 // (<$A000 is 0: low byte unchanged)
        lda offset+1
        clc
        adc #$a0                        // >BANK_WINDOW
        sta x16__bk_ptr+1
        ldy #0
        lda (x16__bk_ptr),y
        sta r
        lda x16__bk_save
        sta $00 /*RAM_BANK*/
    }
    return r;
}

void x16_bank_poke(__mem unsigned char bank, __mem unsigned int offset,
                   __mem unsigned char value) {
    asm {
        lda $00 /*RAM_BANK*/
        sta x16__bk_save
        lda bank
        sta $00 /*RAM_BANK*/
        lda offset
        sta x16__bk_ptr
        lda offset+1
        clc
        adc #$a0                        // >BANK_WINDOW
        sta x16__bk_ptr+1
        ldy #0
        lda value
        sta (x16__bk_ptr),y
        lda x16__bk_save
        sta $00 /*RAM_BANK*/
    }
}

// ---------------------------------------------------------------------
// Copy low RAM into banked RAM. Auto-advances: a run that hits the end
// of a bank continues at offset 0 of the next. `src` is only handed to
// MEMORY_COPY by value, never indirected, so it needs no zp slot.
// ---------------------------------------------------------------------
void x16_mem_to_bank(const void *src, __mem unsigned char bank,
                     __mem unsigned int offset, __mem unsigned int count) {
    asm {
        lda $00 /*RAM_BANK*/
        pha
        lda bank
        sta $00 /*RAM_BANK*/

        lda src                         // t0/t1 = low-RAM side
        sta x16__bk_t0
        lda src+1
        sta x16__bk_t1
        lda offset                      // t2/t3 = offset within the window
        sta x16__bk_t2
        lda offset+1
        sta x16__bk_t3
        lda count                       // t4/t5 = remaining
        sta x16__bk_t4
        lda count+1
        sta x16__bk_t5

    mtb_seg:
        jsr mtb_span                    // t6/t7 = bytes to the bank edge
        beq mtb_done
        lda x16__bk_t0                  // source: low RAM
        sta $02 /*r0L*/
        lda x16__bk_t1
        sta $03 /*r0H*/
        lda x16__bk_t2                  // target: window + offset
        sta $04 /*r1L*/
        lda x16__bk_t3
        clc
        adc #$a0                        // >BANK_WINDOW
        sta $05 /*r1H*/
        lda x16__bk_t6
        sta $06 /*r2L*/
        lda x16__bk_t7
        sta $07 /*r2H*/
        jsr $fee7 /*MEMORY_COPY*/
        jsr mtb_advance
        bra mtb_seg
    mtb_done:
        pla
        sta $00 /*RAM_BANK*/
        jmp mtb_end

        // t6/t7 = min(remaining, space left in this bank). Z set when
        // nothing remains.
    mtb_span:
        lda x16__bk_t4
        ora x16__bk_t5
        beq mtb_span_done               // remaining == 0 (Z for the caller)
        sec                             // space = $2000 - offset
        lda #$00                        // <BANK_SIZE
        sbc x16__bk_t2
        sta x16__bk_t6
        lda #$20                        // >BANK_SIZE
        sbc x16__bk_t3
        sta x16__bk_t7
        lda x16__bk_t5                  // remaining < space? take remaining
        cmp x16__bk_t7
        bcc mtb_take_rem
        bne mtb_have
        lda x16__bk_t4
        cmp x16__bk_t6
        bcs mtb_have
    mtb_take_rem:
        lda x16__bk_t4
        sta x16__bk_t6
        lda x16__bk_t5
        sta x16__bk_t7
    mtb_have:
        lda #1                          // Z clear: there is work to do
    mtb_span_done:
        rts

        // consume t6/t7 bytes: advance the pointer and the window
        // offset (rolling into the next bank), shrink the remaining.
    mtb_advance:
        clc
        lda x16__bk_t0
        adc x16__bk_t6
        sta x16__bk_t0
        lda x16__bk_t1
        adc x16__bk_t7
        sta x16__bk_t1
        clc
        lda x16__bk_t2
        adc x16__bk_t6
        sta x16__bk_t2
        lda x16__bk_t3
        adc x16__bk_t7
        sta x16__bk_t3
        cmp #$20                        // offset reached $2000: next bank
        bne mtb_count
        stz x16__bk_t2
        stz x16__bk_t3
        inc $00 /*RAM_BANK*/
    mtb_count:
        sec
        lda x16__bk_t4
        sbc x16__bk_t6
        sta x16__bk_t4
        lda x16__bk_t5
        sbc x16__bk_t7
        sta x16__bk_t5
        rts

    mtb_end:
    }
}

// The inverse: banked RAM into low RAM. Same segment walk.
void x16_bank_to_mem(__mem unsigned char bank, __mem unsigned int offset,
                     void *dst, __mem unsigned int count) {
    asm {
        lda $00 /*RAM_BANK*/
        pha
        lda bank
        sta $00 /*RAM_BANK*/

        lda dst                         // t0/t1 = low-RAM side
        sta x16__bk_t0
        lda dst+1
        sta x16__bk_t1
        lda offset                      // t2/t3 = offset within the window
        sta x16__bk_t2
        lda offset+1
        sta x16__bk_t3
        lda count
        sta x16__bk_t4
        lda count+1
        sta x16__bk_t5

    btm_seg:
        jsr btm_span
        beq btm_done
        lda x16__bk_t2                  // source: window + offset
        sta $02 /*r0L*/
        lda x16__bk_t3
        clc
        adc #$a0                        // >BANK_WINDOW
        sta $03 /*r0H*/
        lda x16__bk_t0                  // target: low RAM
        sta $04 /*r1L*/
        lda x16__bk_t1
        sta $05 /*r1H*/
        lda x16__bk_t6
        sta $06 /*r2L*/
        lda x16__bk_t7
        sta $07 /*r2H*/
        jsr $fee7 /*MEMORY_COPY*/
        jsr btm_advance
        bra btm_seg
    btm_done:
        pla
        sta $00 /*RAM_BANK*/
        jmp btm_end

    btm_span:
        lda x16__bk_t4
        ora x16__bk_t5
        beq btm_span_done
        sec
        lda #$00                        // <BANK_SIZE
        sbc x16__bk_t2
        sta x16__bk_t6
        lda #$20                        // >BANK_SIZE
        sbc x16__bk_t3
        sta x16__bk_t7
        lda x16__bk_t5
        cmp x16__bk_t7
        bcc btm_take_rem
        bne btm_have
        lda x16__bk_t4
        cmp x16__bk_t6
        bcs btm_have
    btm_take_rem:
        lda x16__bk_t4
        sta x16__bk_t6
        lda x16__bk_t5
        sta x16__bk_t7
    btm_have:
        lda #1
    btm_span_done:
        rts

    btm_advance:
        clc
        lda x16__bk_t0
        adc x16__bk_t6
        sta x16__bk_t0
        lda x16__bk_t1
        adc x16__bk_t7
        sta x16__bk_t1
        clc
        lda x16__bk_t2
        adc x16__bk_t6
        sta x16__bk_t2
        lda x16__bk_t3
        adc x16__bk_t7
        sta x16__bk_t3
        cmp #$20
        bne btm_count
        stz x16__bk_t2
        stz x16__bk_t3
        inc $00 /*RAM_BANK*/
    btm_count:
        sec
        lda x16__bk_t4
        sbc x16__bk_t6
        sta x16__bk_t4
        lda x16__bk_t5
        sbc x16__bk_t7
        sta x16__bk_t5
        rts

    btm_end:
    }
}

// ---------------------------------------------------------------------
// Banked RAM to banked RAM. Only one bank fits the window at a time, so
// this bounces through a small low-RAM buffer, MEMORY_COPY both legs.
// Both sides auto-advance across bank boundaries; the parameters are
// consumed as working state, exactly like the ca65 build's P block.
// ---------------------------------------------------------------------
void x16_bank_copy_far(__mem unsigned char src_bank,
                       __mem unsigned int src_offset,
                       __mem unsigned char dst_bank,
                       __mem unsigned int dst_offset,
                       __mem unsigned int count) {
    asm {
        lda $00 /*RAM_BANK*/
        pha

    bcf_loop:
        lda count
        ora count+1
        bne bcf_more
        jmp bcf_done                    // out of branch range from here
    bcf_more:

        // chunk = min(count, bounce size, source bank space, dest space)
        ldx #128                        // BANK_BOUNCE_SIZE
        lda count+1
        bne bcf_src_cap                 // count >= 256: the buffer caps it
        lda count
        cmp #128
        bcs bcf_src_cap
        tax                             // count < buffer: count is the cap
    bcf_src_cap:
        // Space to the end of a bank only matters in the window's last
        // page: below that, more than a full chunk remains.
        sec
        lda #$00                        // <BANK_SIZE
        sbc src_offset
        sta x16__bk_t0
        lda #$20                        // >BANK_SIZE
        sbc src_offset+1
        bne bcf_dst_cap                 // >= 256 bytes left in the source
        txa
        cmp x16__bk_t0
        bcc bcf_dst_cap
        ldx x16__bk_t0
    bcf_dst_cap:
        sec
        lda #$00
        sbc dst_offset
        sta x16__bk_t0
        lda #$20
        sbc dst_offset+1
        bne bcf_go
        txa
        cmp x16__bk_t0
        bcc bcf_go
        ldx x16__bk_t0
    bcf_go:
        stx x16__bk_t7                  // t7 = chunk (1..128)

        lda src_bank                    // leg 1: source bank -> bounce
        sta $00 /*RAM_BANK*/
        lda src_offset
        sta $02 /*r0L*/
        lda src_offset+1
        clc
        adc #$a0                        // >BANK_WINDOW
        sta $03 /*r0H*/
        lda #<x16__bk_bounce
        sta $04 /*r1L*/
        lda #>x16__bk_bounce
        sta $05 /*r1H*/
        stx $06 /*r2L*/
        stz $07 /*r2H*/
        jsr $fee7 /*MEMORY_COPY*/

        lda dst_bank                    // leg 2: bounce -> destination
        sta $00 /*RAM_BANK*/
        lda #<x16__bk_bounce
        sta $02 /*r0L*/
        lda #>x16__bk_bounce
        sta $03 /*r0H*/
        lda dst_offset
        sta $04 /*r1L*/
        lda dst_offset+1
        clc
        adc #$a0
        sta $05 /*r1H*/
        lda x16__bk_t7
        sta $06 /*r2L*/
        stz $07 /*r2H*/
        jsr $fee7 /*MEMORY_COPY*/

        clc                             // advance the source (rolls at $2000)
        lda src_offset
        adc x16__bk_t7
        sta src_offset
        lda src_offset+1
        adc #0
        sta src_offset+1
        cmp #$20
        bne bcf_adv_dst
        stz src_offset
        stz src_offset+1
        inc src_bank
    bcf_adv_dst:
        clc
        lda dst_offset
        adc x16__bk_t7
        sta dst_offset
        lda dst_offset+1
        adc #0
        sta dst_offset+1
        cmp #$20
        bne bcf_count
        stz dst_offset
        stz dst_offset+1
        inc dst_bank
    bcf_count:
        sec
        lda count
        sbc x16__bk_t7
        sta count
        lda count+1
        sbc #0
        sta count+1
        jmp bcf_loop

    bcf_done:
        pla
        sta $00 /*RAM_BANK*/
    }
}

// =====================================================================
// x16clib :: x16/bankalloc.c -- whole-bank RAM allocator
// =====================================================================
// A bitmap allocator over banks 1-255; one bit per bank, set = FREE.
// Same code as src_ca65/storage/bankalloc.s. The map is explicitly
// zeroed (KickC has no zeroed BSS), so before x16_bank_alloc_init()
// every bank reads as "not free" and x16_bank_alloc() fails cleanly
// rather than handing out bank 0.
//
// The ca65 build's shared set_bit helper is folded into its two users;
// inline asm cannot jsr across functions.
// =====================================================================


const char x16__ba_bits[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

__mem volatile char x16__ba_map[32] = { 0, 0, 0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0, 0, 0, 0 };

__mem volatile char x16__ba_t0;
__mem volatile char x16__ba_t1;

// first <= last, both inclusive. Call again to reset the pool. Banks
// outside the range are never handed out.
void x16_bank_alloc_init(__mem unsigned char first, __mem unsigned char last) {
    asm {
        lda first
        sta x16__ba_t0
        lda last
        sta x16__ba_t1

        ldx #31                         // everything starts un-ownable
    bai_clear:
        stz x16__ba_map,x
        dex
        bpl bai_clear

    bai_mark:
        lda x16__ba_t0                  // mark bank t0 free (set_bit)
        lsr
        lsr
        lsr
        tax                             // byte index
        lda x16__ba_t0
        and #$07
        tay
        lda x16__ba_map,x
        ora x16__ba_bits,y
        sta x16__ba_map,x

        lda x16__ba_t0
        cmp x16__ba_t1
        beq bai_done
        inc x16__ba_t0
        bra bai_mark
    bai_done:
    }
}

// ---------------------------------------------------------------------
// The lowest free bank, or 0 when the pool is exhausted. Bank 0 is the
// KERNAL's and can never be in the pool, so 0 is unambiguous.
// ---------------------------------------------------------------------
unsigned char x16_bank_alloc(void) {
    __mem char r;
    asm {
        ldx #0
    ba_scan:
        lda x16__ba_map,x
        bne ba_found
        inx
        cpx #32
        bne ba_scan
        stz r                           // nothing free
        bra ba_end
    ba_found:
        ldy #0
    ba_bit:
        lda x16__ba_map,x
        and x16__ba_bits,y
        bne ba_take
        iny
        bra ba_bit                      // must hit: the byte was nonzero
    ba_take:
        lda x16__ba_map,x               // clear the bit: bank now in use
        eor x16__ba_bits,y
        sta x16__ba_map,x
        txa                             // bank = byte index * 8 + bit index
        asl
        asl
        asl
        sta x16__ba_t0
        tya
        ora x16__ba_t0
        sta r
    ba_end:
    }
    return r;
}

// ---------------------------------------------------------------------
// Freeing a bank that is already free, or that was never in the pool,
// quietly marks it allocatable -- there is no ownership record to check
// against, so don't do that.
// ---------------------------------------------------------------------
void x16_bank_free(__mem unsigned char bank) {
    asm {
        lda bank                        // set_bit
        lsr
        lsr
        lsr
        tax
        lda bank
        and #$07
        tay
        lda x16__ba_map,x
        ora x16__ba_bits,y
        sta x16__ba_map,x
    }
}

// 1 if it was free and is now yours; 0 if already taken or outside the
// pool.
unsigned char x16_bank_reserve(__mem unsigned char bank) {
    __mem char r;
    asm {
        lda bank
        lsr
        lsr
        lsr
        tax
        lda bank
        and #$07
        tay
        lda x16__ba_map,x
        and x16__ba_bits,y
        beq br_taken
        lda x16__ba_map,x               // it was free: clear the bit
        eor x16__ba_bits,y
        sta x16__ba_map,x
        lda #1
        sta r
        bra br_end
    br_taken:
        stz r
    br_end:
    }
    return r;
}
