// =====================================================================
// x16clib :: x16/zx0.c -- ZX0 decompression (Einar Saukas's format)
// =====================================================================
// Decodes the MODERN ZX0 v2 stream -- what `zx0` and `salvador` emit by
// default, not their -classic mode. RAM to RAM only (the match copier
// reads the output back); cannot decompress in place.
//
// The same hand-written 6502 as src_ca65/util/zx0.s, in one asm body.
// The cc65 version streamed through the X16_P0..P3 zero-page block;
// here `src` and `dst` are the function's own parameters, which
// Oscar64 keeps in zero page, indirected with Y held at 0.
// =====================================================================

#include <x16/zx0.h>

volatile char x16__zx_bits;
volatile char x16__zx_last;
volatile char x16__zx_bt;
volatile char x16__zx_inv;
volatile char x16__zx_val[2];
volatile char x16__zx_off[2];
volatile char x16__zx_t;

// The match copier's read pointer patches its own absolute load
// (self-modifying code, like the original): it is the only stream
// pointer that is not simply a parameter. The cc65 build used T6/T7
// for the same job.

void* x16_zx0_decompress(const void* src, void* dst) {
    return (void*)__asm {
        ldy #0                  /* Y stays 0: every indirect is (p),y */
        lda #0
        sta x16__zx_bits /* empty bit buffer: first use refills */
        sta x16__zx_bt
        sta x16__zx_off+1       /* (A is still 0 here) */
        lda #1                  /* the initial offset is 1 */
        sta x16__zx_off

    zx_literals:
        jsr zx_gamma_n          /* literal run length */
    zx_lit_byte:
        jsr zx_getbyte
        sta (dst),y
        inc dst
        bne zx_lit_dec
        inc dst+1
    zx_lit_dec:
        jsr zx_dec_len
        bne zx_lit_byte

        jsr zx_getbit
        bcs zx_new_offset

    zx_last_offset:
        jsr zx_gamma_n          /* match length, offset unchanged */
        jsr zx_copy
        jsr zx_getbit
        bcc zx_literals

    zx_new_offset:
        jsr zx_gamma_i          /* the offset MSB, inverted gamma (v2) */
        lda x16__zx_val+1       /* 256 is the end-of-stream marker */
        beq zx_not_end
        lda x16__zx_val
        bne zx_not_end
        lda dst         /* done: hand back the output end */
        sta accu
        lda dst+1
        sta accu + 1
        jmp zx_done
    zx_not_end:
        lda x16__zx_val         /* offset = MSB*128 - (next byte >> 1) */
        lsr
        sta x16__zx_off+1
        lda #0
        ror
        sta x16__zx_off
        jsr zx_getbyte          /* ...which also latches zx_last */
        lsr
        sta x16__zx_t
        sec
        lda x16__zx_off
        sbc x16__zx_t
        sta x16__zx_off
        lda x16__zx_off+1
        sbc #0
        sta x16__zx_off+1
        lda #1                  /* that byte's low bit is the FIRST bit */
        sta x16__zx_bt          /* of the coming length gamma */
        jsr zx_gamma_n
        inc x16__zx_val         /* new-offset match lengths are +1 */
        bne zx_len_ok
        inc x16__zx_val+1
    zx_len_ok:
        jsr zx_copy
        jsr zx_getbit
        bcs zx_new_offset
        jmp zx_literals

    /* --- plumbing --------------------------------------------------- */

    /* copy zx_val bytes from (output - zx_off) to the output */
    zx_copy:
        sec
        lda dst
        sbc x16__zx_off
        sta zxc_ld+1
        lda dst+1
        sbc x16__zx_off+1
        sta zxc_ld+2
    zxc_byte:
    zxc_ld:
        lda 0xffff              /* self-modified: dst - offset */
        sta (dst),y
        inc zxc_ld+1
        bne zxc_dst
        inc zxc_ld+2
    zxc_dst:
        inc dst
        bne zxc_count
        inc dst+1
    zxc_count:
        jsr zx_dec_len
        bne zxc_byte
        rts

    /* zx_val -= 1; Z set when it reaches zero (val >= 1 on entry) */
    zx_dec_len:
        lda x16__zx_val
        bne zxd_lo
        dec x16__zx_val+1
    zxd_lo:
        dec x16__zx_val
        lda x16__zx_val
        ora x16__zx_val+1
        rts

    /* interlaced Elias gamma into zx_val: normal and inverted data bits */
    zx_gamma_i:
        lda #1
        jmp zx_gamma
    zx_gamma_n:
        lda #0
    zx_gamma:
        sta x16__zx_inv
        lda #0
        sta x16__zx_val+1
        lda #1
        sta x16__zx_val
    zxg_more:
        jsr zx_getbit
        bcs zxg_done            /* a 1 control bit ends the number */
        jsr zx_getbit
        lda #0
        rol                     /* A = the data bit */
        eor x16__zx_inv
        lsr                     /* ...back into the carry */
        rol x16__zx_val
        rol x16__zx_val+1
        jmp zxg_more
    zxg_done:
        rts

    /* next bit into the carry. The buffer keeps a sentinel 1 in bit 0, */
    /* so a zero buffer after the shift means "that carry was the */
    /* sentinel": refill and take bit 7 of the fresh byte instead. */
    zx_getbit:
        lda x16__zx_bt
        beq zxb_stream
        lda #0
        sta x16__zx_bt /* backtrack: the offset byte's low bit */
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
        rol                     /* carry = bit 7, sentinel into bit 0 */
        sta x16__zx_bits
        rts

    zx_getbyte:
        lda (src),y
        sta x16__zx_last
        inc src
        bne zxgb_ok
        inc src+1
    zxgb_ok:
        lda x16__zx_last
        rts

    zx_done:
    };
}
