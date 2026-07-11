// =====================================================================
// x16clib :: x16/buffers.c -- byte ring buffer and byte stack
// =====================================================================
// Same logic as src_ca65/util/buffers.s. The cc65 version juggled the
// carry flag across a jsr and preserved X with phx/plx for fastcall;
// with KickC's memory parameters neither dance is needed, so each entry
// point is the internal routine and its shim fused into one asm body.
// =====================================================================

#include <x16/buffers.h>

// The counters are explicitly zeroed: cc65's crt0 zeroes BSS before
// main, KickC loads only initialised data, and a ring buffer whose
// count starts as leftover RAM would "contain" garbage. The 256-byte
// stores stay uninitialised -- their contents are meaningless until
// indexed by the counters -- so they cost the PRG nothing.
__mem volatile char x16__rb_head = 0;
__mem volatile char x16__rb_tail = 0;
__mem volatile char x16__rb_len = 0;
__mem volatile char x16__rb_data[256];

__mem volatile char x16__stk_sp = 0;
__mem volatile char x16__stk_data[256];

void x16_rb_init(void) {
    asm {
        stz x16__rb_head
        stz x16__rb_tail
        stz x16__rb_len
    }
}

unsigned char x16_rb_put(__mem unsigned char b) {
    __mem char r;
    asm {
        lda x16__rb_len
        cmp #255
        bcs rbp_full            // full: the byte is dropped
        ldx x16__rb_head
        lda b
        sta x16__rb_data,x
        inc x16__rb_head        // 8-bit index: wrap is free
        inc x16__rb_len
        lda #1
        sta r
        bra rbp_done
    rbp_full:
        stz r
    rbp_done:
    }
    return r;
}

int x16_rb_get(void) {
    __mem int r;
    asm {
        lda x16__rb_len
        beq rbg_empty
        ldx x16__rb_tail
        lda x16__rb_data,x
        inc x16__rb_tail
        dec x16__rb_len
        sta r
        stz r+1
        bra rbg_done
    rbg_empty:
        lda #$ff                // -1
        sta r
        sta r+1
    rbg_done:
    }
    return r;
}

unsigned char x16_rb_count(void) {
    return x16__rb_len;
}

void x16_stk_init(void) {
    asm {
        stz x16__stk_sp
    }
}

unsigned char x16_stk_push(__mem unsigned char b) {
    __mem char r;
    asm {
        lda x16__stk_sp
        cmp #255
        bcs stp_full
        ldx x16__stk_sp
        lda b
        sta x16__stk_data,x
        inc x16__stk_sp
        lda #1
        sta r
        bra stp_done
    stp_full:
        stz r
    stp_done:
    }
    return r;
}

int x16_stk_pop(void) {
    __mem int r;
    asm {
        lda x16__stk_sp
        beq stg_empty
        dec x16__stk_sp
        ldx x16__stk_sp
        lda x16__stk_data,x
        sta r
        stz r+1
        bra stg_done
    stg_empty:
        lda #$ff                // -1
        sta r
        sta r+1
    stg_done:
    }
    return r;
}

unsigned char x16_stk_depth(void) {
    return x16__stk_sp;
}
