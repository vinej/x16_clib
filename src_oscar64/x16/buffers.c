// =====================================================================
// x16clib :: x16/buffers.c -- byte ring buffer and byte stack
// =====================================================================
// Same logic as src_ca65/util/buffers.s. The cc65 version juggled the
// carry flag across a jsr and preserved X for fastcall; with named
// zero-page parameters neither dance is needed, so each entry
// point is the internal routine and its shim fused into one asm body.
// =====================================================================

#include <x16/buffers.h>

// The counters are explicitly zeroed: a PRG loads only initialised
// data, and a ring buffer whose
// count starts as leftover RAM would "contain" garbage. The 256-byte
// stores stay uninitialised -- their contents are meaningless until
// indexed by the counters -- so they cost the PRG nothing.
volatile char x16__rb_head = 0;
volatile char x16__rb_tail = 0;
volatile char x16__rb_len = 0;
volatile char x16__rb_data[256];

volatile char x16__stk_sp = 0;
volatile char x16__stk_data[256];

void x16_rb_init(void) {
    __asm {
        lda #0
        sta x16__rb_head
        sta x16__rb_tail
        sta x16__rb_len
    }
}

unsigned char x16_rb_put(unsigned char b) {
    return __asm {
        lda x16__rb_len
        cmp #255
        bcs rbp_full            /* full: the byte is dropped */
        ldx x16__rb_head
        lda b
        sta x16__rb_data,x
        inc x16__rb_head        /* 8-bit index: wrap is free */
        inc x16__rb_len
        lda #1
        sta accu
        jmp rbp_done
    rbp_full:
        lda #0
        sta accu
    rbp_done:
    };
}

int x16_rb_get(void) {
    return __asm {
        lda x16__rb_len
        beq rbg_empty
        ldx x16__rb_tail
        lda x16__rb_data,x
        inc x16__rb_tail
        dec x16__rb_len
        sta accu
        lda #0
        sta accu + 1
        jmp rbg_done
    rbg_empty:
        lda #0xff                /* -1 */
        sta accu
        sta accu + 1
    rbg_done:
    };
}

unsigned char x16_rb_count(void) {
    return x16__rb_len;
}

void x16_stk_init(void) {
    __asm {
        lda #0
        sta x16__stk_sp
    }
}

unsigned char x16_stk_push(unsigned char b) {
    return __asm {
        lda x16__stk_sp
        cmp #255
        bcs stp_full
        ldx x16__stk_sp
        lda b
        sta x16__stk_data,x
        inc x16__stk_sp
        lda #1
        sta accu
        jmp stp_done
    stp_full:
        lda #0
        sta accu
    stp_done:
    };
}

int x16_stk_pop(void) {
    return __asm {
        lda x16__stk_sp
        beq stg_empty
        dec x16__stk_sp
        ldx x16__stk_sp
        lda x16__stk_data,x
        sta accu
        lda #0
        sta accu + 1
        jmp stg_done
    stg_empty:
        lda #0xff                /* -1 */
        sta accu
        sta accu + 1
    stg_done:
    };
}

unsigned char x16_stk_depth(void) {
    return x16__stk_sp;
}
