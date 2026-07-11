// =====================================================================
// x16clib :: x16/float.c -- floating point, via the ROM's FP library
// =====================================================================
// The X16 ROM carries a complete C128/C65-compatible floating point
// library in BANK_BASIC ($04), reachable through a stable jump table at
// $FE00. This is a binding, not a reimplementation. Everything works on
// FAC, the floating accumulator at zp $C3-$D1 -- inside the range
// x16/zpsafe.h reserves, so KickC cannot collide with it. BASIC is
// dormant while a program runs, so disturbing FAC/ARG is safe.
//
// Every entry crosses a ROM bank with the KERNAL's JSRFAR ($FF6E),
// whose calling convention is inline data after the jsr:
//      jsr $ff6e
//      .byte <entry, >entry, $04
// Not free: for hot per-frame maths prefer x16_mul88().
//
// Same code as src_ca65/util/float.s, including its hard-won ROM lore:
// fp_negop (not fp_negfac) is the true unary minus; fp_fsub/fp_fdiv
// compute mem-FAC and mem/FAC so the intuitive forms stage FAC into ARG
// first; fp_val takes the address LOW byte in X (jumptab.s's comment
// says otherwise and is wrong).
// =====================================================================

#include <x16/float.h>

// The string copies are indirected: pinned zp slots (KickC ignores
// __zp on parameters; see x16/zpsafe.h).
__address(0x78) const char* volatile x16__f_src;
__address(0x7a) char* volatile x16__f_dst;

void x16_f_zero(void) {
    asm {
        jsr $ff6e
        .byte $72, $fe, $04             // fp_zerofc, BANK_BASIC
    }
}

// fp_negop is the true unary minus. fp_negfac, despite its name, is an
// internal helper of the add/subtract path that denormalises FAC.
void x16_f_neg(void) {
    asm {
        jsr $ff6e
        .byte $33, $fe, $04             // fp_negop
    }
}

void x16_f_abs(void) {
    asm {
        jsr $ff6e
        .byte $4e, $fe, $04             // fp_abs
    }
}

void x16_f_int(void) {
    asm {
        jsr $ff6e
        .byte $2d, $fe, $04             // fp_int
    }
}

void x16_f_sqrt(void) {
    asm {
        jsr $ff6e
        .byte $30, $fe, $04             // fp_sqr
    }
}

void x16_f_ln(void) {
    asm {
        jsr $ff6e
        .byte $2a, $fe, $04             // fp_log
    }
}

void x16_f_exp(void) {
    asm {
        jsr $ff6e
        .byte $3c, $fe, $04             // fp_exp
    }
}

void x16_f_sin(void) {
    asm {
        jsr $ff6e
        .byte $42, $fe, $04             // fp_sin
    }
}

void x16_f_cos(void) {
    asm {
        jsr $ff6e
        .byte $3f, $fe, $04             // fp_cos
    }
}

void x16_f_tan(void) {
    asm {
        jsr $ff6e
        .byte $45, $fe, $04             // fp_tan
    }
}

void x16_f_atan(void) {
    asm {
        jsr $ff6e
        .byte $48, $fe, $04             // fp_atn
    }
}

// -1 if FAC < 0, 0 if zero, 1 if positive.
signed char x16_f_sgn(void) {
    __mem char r;
    asm {
        jsr $ff6e
        .byte $51, $fe, $04             // fp_sign
        sta r
    }
    return (signed char)r;
}

// Through givayf with a zero high byte: the ROM's fp_float converts a
// SIGNED byte, so 200 through it would come out as -56.
void x16_f_from_u8(__mem unsigned char v) {
    asm {
        ldy v                           // Y = low byte
        lda #0                          // A = high byte: zero-extend
        jsr $ff6e
        .byte $03, $fe, $04             // fp_givayf
    }
}

// fp_givayf wants the high byte in A and the low byte in Y.
void x16_f_from_s16(__mem int v) {
    asm {
        ldy v                           // Y = low
        lda v+1                         // A = high
        jsr $ff6e
        .byte $03, $fe, $04             // fp_givayf
    }
}

// Rounds toward zero. fp_ayint leaves the result big-endian in FACMO
// (high) and FACLO (low).
int x16_f_to_s16(void) {
    __mem int r;
    asm {
        jsr $ff6e
        .byte $00, $fe, $04             // fp_ayint
        lda $c7 /*FP_FACLO*/
        sta r
        lda $c6 /*FP_FACMO*/
        sta r+1
    }
    return r;
}

// FAC = *m. The ROM wants A = low, Y = high.
void x16_f_load(const unsigned char *m) {
    asm {
        lda m
        ldy m+1
        jsr $ff6e
        .byte $60, $fe, $04             // fp_movfm
    }
}

// *m = round(FAC). fp_movmf takes its pointer in X/Y; only this one.
void x16_f_store(unsigned char *m) {
    asm {
        ldx m
        ldy m+1
        jsr $ff6e
        .byte $66, $fe, $04             // fp_movmf
    }
}

// FAC = FAC + m and FAC = FAC * m. Both commute.
void x16_f_add(const unsigned char *m) {
    asm {
        lda m
        ldy m+1
        jsr $ff6e
        .byte $18, $fe, $04             // fp_fadd
    }
}

void x16_f_mul(const unsigned char *m) {
    asm {
        lda m
        ldy m+1
        jsr $ff6e
        .byte $1e, $fe, $04             // fp_fmult
    }
}

// ---------------------------------------------------------------------
// FAC = FAC - m, FAC / m, FAC ^ m. The ROM only offers mem-first, so:
// ARG = FAC (movef), FAC = mem (movfm), then the ARG-first entry.
// Three bank crossings -- the price of the intuitive direction.
// ---------------------------------------------------------------------
void x16_f_sub(const unsigned char *m) {
    asm {
        jsr $ff6e
        .byte $81, $fe, $04             // fp_movef: ARG = FAC
        lda m
        ldy m+1
        jsr $ff6e
        .byte $60, $fe, $04             // fp_movfm: FAC = mem
        jsr $ff6e
        .byte $15, $fe, $04             // fp_fsubt: FAC = ARG - FAC
    }
}

void x16_f_div(const unsigned char *m) {
    asm {
        jsr $ff6e
        .byte $81, $fe, $04             // fp_movef
        lda m
        ldy m+1
        jsr $ff6e
        .byte $60, $fe, $04             // fp_movfm
        jsr $ff6e
        .byte $27, $fe, $04             // fp_fdivt: FAC = ARG / FAC
    }
}

void x16_f_pow(const unsigned char *m) {
    asm {
        jsr $ff6e
        .byte $81, $fe, $04             // fp_movef
        lda m
        ldy m+1
        jsr $ff6e
        .byte $60, $fe, $04             // fp_movfm
        jsr $ff6e
        .byte $39, $fe, $04             // fp_fpwrt: FAC = ARG ^ FAC
    }
}

// FAC = m - FAC, m / FAC, m ^ FAC: the ROM's native order, one
// crossing. f_rdiv is the reciprocal form, 1/x.
void x16_f_rsub(const unsigned char *m) {
    asm {
        lda m
        ldy m+1
        jsr $ff6e
        .byte $12, $fe, $04             // fp_fsub
    }
}

void x16_f_rdiv(const unsigned char *m) {
    asm {
        lda m
        ldy m+1
        jsr $ff6e
        .byte $24, $fe, $04             // fp_fdiv
    }
}

void x16_f_rpow(const unsigned char *m) {
    asm {
        lda m
        ldy m+1
        jsr $ff6e
        .byte $36, $fe, $04             // fp_fpwr
    }
}

// -1 if FAC < m, 0 if equal, 1 if FAC > m.
signed char x16_f_cmp(const unsigned char *m) {
    __mem char r;
    asm {
        lda m
        ldy m+1
        jsr $ff6e
        .byte $54, $fe, $04             // fp_fcomp
        sta r
    }
    return (signed char)r;
}

// ---------------------------------------------------------------------
// The ROM formats into FP_FBUFFR ($0100, the bottom of the stack page),
// which would not survive the next deep call -- so copy it out at once.
// Positive numbers get a leading space, exactly as BASIC's PRINT shows
// them; the _trim form skips it.
// ---------------------------------------------------------------------
void x16_f_to_str(char *buf) {
    x16__f_dst = buf;
    asm {
        jsr $ff6e
        .byte $06, $fe, $04             // fp_fout: A = low, Y = high
        sta x16__f_src
        sty x16__f_src+1
        ldy #0
    fts_loop:
        lda (x16__f_src),y
        sta (x16__f_dst),y
        beq fts_done
        iny
        bne fts_loop
    fts_done:
    }
}

void x16_f_to_str_trim(char *buf) {
    x16__f_dst = buf;
    asm {
        jsr $ff6e
        .byte $06, $fe, $04             // fp_fout
        sta x16__f_src
        sty x16__f_src+1
        ldy #0
        lda (x16__f_src),y
        cmp #32                         // a leading sign space
        bne ftt_copy
        inc x16__f_src                  // skip it
        bne ftt_copy
        inc x16__f_src+1
    ftt_copy:
        ldy #0
    ftt_loop:
        lda (x16__f_src),y
        sta (x16__f_dst),y
        beq ftt_done
        iny
        bne ftt_loop
    ftt_done:
    }
}

// fp_val wants X = address LOW, Y = address high, A = length.
void x16_f_from_str(const char *s, __mem unsigned char len) {
    asm {
        ldx s
        ldy s+1
        lda len
        jsr $ff6e
        .byte $09, $fe, $04             // fp_val
    }
}
