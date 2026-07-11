// =====================================================================
// x16clib :: x16/zx0.c -- ZX0 decompression (Einar Saukas's format)
// =====================================================================
// Decodes the MODERN ZX0 v2 stream -- what `zx0` and `salvador` emit by
// default, not their -classic mode. RAM to RAM only (the match copier
// reads the output back); cannot decompress in place.
//
// The same hand-written 6502 as src_ca65/util/zx0.s, in one asm body.
// The cc65 version streamed through the X16_P0..P3 zero-page block;
// here `src` and `dst` are the function's own parameters, which KickC
// places in zero page because the code indirects through them.
// =====================================================================

#include <x16/zx0.h>

__mem volatile char x16__zx_bits;
__mem volatile char x16__zx_last;
__mem volatile char x16__zx_bt;
__mem volatile char x16__zx_inv;
__mem volatile char x16__zx_val[2];
__mem volatile char x16__zx_off[2];
__mem volatile char x16__zx_t;

// The stream pointers, all pinned in zero page: everything here is
// indirected through, KickC ignores __zp on parameters, and under
// pressure it silently spills a pointer parameter to main memory. These
// are the cc65 build's X16_P0/P1 (x16__zx_src), P2/P3 (x16__zx_dst) and T6/T7 (the
// match copier's read pointer).
__address(0x78) const char* volatile x16__zx_src;
__address(0x7a) char* volatile x16__zx_dst;
__address(0x7c) char* volatile x16__zx_cp;

void* x16_zx0_decompress(const void* src, void* dst) {
    __mem void* end;
    x16__zx_src = (char*)src;
    x16__zx_dst = (char*)dst;
    asm {
        stz x16__zx_bits        // empty bit buffer: first use refills
        stz x16__zx_bt
        lda #1                  // the initial offset is 1
        sta x16__zx_off
        stz x16__zx_off+1

    zx_literals:
        jsr zx_gamma_n          // literal run length
    zx_lit_byte:
        jsr zx_getbyte
        sta (x16__zx_dst)
        inc x16__zx_dst
        bne zx_lit_dec
        inc x16__zx_dst+1
    zx_lit_dec:
        jsr zx_dec_len
        bne zx_lit_byte

        jsr zx_getbit
        bcs zx_new_offset

    zx_last_offset:
        jsr zx_gamma_n          // match length, offset unchanged
        jsr zx_copy
        jsr zx_getbit
        bcc zx_literals

    zx_new_offset:
        jsr zx_gamma_i          // the offset MSB, inverted gamma (v2)
        lda x16__zx_val+1       // 256 is the end-of-stream marker
        beq zx_not_end
        lda x16__zx_val
        bne zx_not_end
        lda x16__zx_dst         // done: hand back the output end
        sta end
        lda x16__zx_dst+1
        sta end+1
        jmp zx_done
    zx_not_end:
        lda x16__zx_val         // offset = MSB*128 - (next byte >> 1)
        lsr
        sta x16__zx_off+1
        lda #0
        ror
        sta x16__zx_off
        jsr zx_getbyte          // ...which also latches zx_last
        lsr
        sta x16__zx_t
        sec
        lda x16__zx_off
        sbc x16__zx_t
        sta x16__zx_off
        lda x16__zx_off+1
        sbc #0
        sta x16__zx_off+1
        lda #1                  // that byte's low bit is the FIRST bit
        sta x16__zx_bt          // of the coming length gamma
        jsr zx_gamma_n
        inc x16__zx_val         // new-offset match lengths are +1
        bne zx_len_ok
        inc x16__zx_val+1
    zx_len_ok:
        jsr zx_copy
        jsr zx_getbit
        bcs zx_new_offset
        bra zx_literals

    // --- plumbing ---------------------------------------------------

    // copy zx_val bytes from (output - zx_off) to the output
    zx_copy:
        sec
        lda x16__zx_dst
        sbc x16__zx_off
        sta x16__zx_cp
        lda x16__zx_dst+1
        sbc x16__zx_off+1
        sta x16__zx_cp+1
    zxc_byte:
        lda (x16__zx_cp)
        sta (x16__zx_dst)
        inc x16__zx_cp
        bne zxc_dst
        inc x16__zx_cp+1
    zxc_dst:
        inc x16__zx_dst
        bne zxc_count
        inc x16__zx_dst+1
    zxc_count:
        jsr zx_dec_len
        bne zxc_byte
        rts

    // zx_val -= 1; Z set when it reaches zero (val >= 1 on entry)
    zx_dec_len:
        lda x16__zx_val
        bne zxd_lo
        dec x16__zx_val+1
    zxd_lo:
        dec x16__zx_val
        lda x16__zx_val
        ora x16__zx_val+1
        rts

    // interlaced Elias gamma into zx_val: normal and inverted data bits
    zx_gamma_i:
        lda #1
        bra zx_gamma
    zx_gamma_n:
        lda #0
    zx_gamma:
        sta x16__zx_inv
        lda #1
        sta x16__zx_val
        stz x16__zx_val+1
    zxg_more:
        jsr zx_getbit
        bcs zxg_done            // a 1 control bit ends the number
        jsr zx_getbit
        lda #0
        rol                     // A = the data bit
        eor x16__zx_inv
        lsr                     // ...back into the carry
        rol x16__zx_val
        rol x16__zx_val+1
        bra zxg_more
    zxg_done:
        rts

    // next bit into the carry. The buffer keeps a sentinel 1 in bit 0,
    // so a zero buffer after the shift means "that carry was the
    // sentinel": refill and take bit 7 of the fresh byte instead.
    zx_getbit:
        lda x16__zx_bt
        beq zxb_stream
        stz x16__zx_bt          // backtrack: the offset byte's low bit
        lda x16__zx_last
        lsr
        rts
    zxb_stream:
        asl x16__zx_bits
        beq zxb_refill
        rts
    zxb_refill:
        jsr zx_getbyte
        sec
        rol                     // carry = bit 7, sentinel into bit 0
        sta x16__zx_bits
        rts

    zx_getbyte:
        lda (x16__zx_src)
        sta x16__zx_last
        inc x16__zx_src
        bne zxgb_ok
        inc x16__zx_src+1
    zxgb_ok:
        lda x16__zx_last
        rts

    zx_done:
    }
    return end;
}
