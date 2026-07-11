// =====================================================================
// x16clib :: x16/float.c -- floating point, via the ROM's FP library
// =====================================================================
// The X16 ROM carries a complete C128/C65-compatible floating point
// library in BANK_BASIC ($04), reachable through a stable jump table at
// $FE00. This is a binding, not a reimplementation. Everything works on
// FAC, the floating accumulator at zp $C3-$D1 -- a range Oscar64's
// runtime never touches. BASIC is
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

// The FBUFFR copies patch their own load operands (self-modifying
// code): the ROM hands the string back in A/Y, and no zero page is
// spent on a walker. The destination is the `buf` parameter itself.

void x16_f_zero(void) {
    __asm {
        jsr 0xff6e
        byt 0x72, 0xfe, 0x04             /* fp_zerofc, BANK_BASIC */
    }
}

// fp_negop is the true unary minus. fp_negfac, despite its name, is an
// internal helper of the add/subtract path that denormalises FAC.
void x16_f_neg(void) {
    __asm {
        jsr 0xff6e
        byt 0x33, 0xfe, 0x04             /* fp_negop */
    }
}

void x16_f_abs(void) {
    __asm {
        jsr 0xff6e
        byt 0x4e, 0xfe, 0x04             /* fp_abs */
    }
}

void x16_f_int(void) {
    __asm {
        jsr 0xff6e
        byt 0x2d, 0xfe, 0x04             /* fp_int */
    }
}

void x16_f_sqrt(void) {
    __asm {
        jsr 0xff6e
        byt 0x30, 0xfe, 0x04             /* fp_sqr */
    }
}

void x16_f_ln(void) {
    __asm {
        jsr 0xff6e
        byt 0x2a, 0xfe, 0x04             /* fp_log */
    }
}

void x16_f_exp(void) {
    __asm {
        jsr 0xff6e
        byt 0x3c, 0xfe, 0x04             /* fp_exp */
    }
}

void x16_f_sin(void) {
    __asm {
        jsr 0xff6e
        byt 0x42, 0xfe, 0x04             /* fp_sin */
    }
}

void x16_f_cos(void) {
    __asm {
        jsr 0xff6e
        byt 0x3f, 0xfe, 0x04             /* fp_cos */
    }
}

void x16_f_tan(void) {
    __asm {
        jsr 0xff6e
        byt 0x45, 0xfe, 0x04             /* fp_tan */
    }
}

void x16_f_atan(void) {
    __asm {
        jsr 0xff6e
        byt 0x48, 0xfe, 0x04             /* fp_atn */
    }
}

// -1 if FAC < 0, 0 if zero, 1 if positive.
signed char x16_f_sgn(void) {
    return __asm {
        jsr 0xff6e
        byt 0x51, 0xfe, 0x04             /* fp_sign */
        sta accu
    };
}

// Through givayf with a zero high byte: the ROM's fp_float converts a
// SIGNED byte, so 200 through it would come out as -56.
void x16_f_from_u8(unsigned char v) {
    __asm {
        ldy v                           /* Y = low byte */
        lda #0                          /* A = high byte: zero-extend */
        jsr 0xff6e
        byt 0x03, 0xfe, 0x04             /* fp_givayf */
    }
}

// fp_givayf wants the high byte in A and the low byte in Y.
void x16_f_from_s16(int v) {
    __asm {
        ldy v                           /* Y = low */
        lda v+1                         /* A = high */
        jsr 0xff6e
        byt 0x03, 0xfe, 0x04             /* fp_givayf */
    }
}

// Rounds toward zero. fp_ayint leaves the result big-endian in FACMO
// (high) and FACLO (low).
int x16_f_to_s16(void) {
    return __asm {
        jsr 0xff6e
        byt 0x00, 0xfe, 0x04             /* fp_ayint */
        lda 0xc7                        /* FP_FACLO */
        sta accu
        lda 0xc6                        /* FP_FACMO */
        sta accu + 1
    };
}

// FAC = *m. The ROM wants A = low, Y = high.
void x16_f_load(const unsigned char *m) {
    __asm {
        lda m
        ldy m+1
        jsr 0xff6e
        byt 0x60, 0xfe, 0x04             /* fp_movfm */
    }
}

// *m = round(FAC). fp_movmf takes its pointer in X/Y; only this one.
void x16_f_store(unsigned char *m) {
    __asm {
        ldx m
        ldy m+1
        jsr 0xff6e
        byt 0x66, 0xfe, 0x04             /* fp_movmf */
    }
}

// FAC = FAC + m and FAC = FAC * m. Both commute.
void x16_f_add(const unsigned char *m) {
    __asm {
        lda m
        ldy m+1
        jsr 0xff6e
        byt 0x18, 0xfe, 0x04             /* fp_fadd */
    }
}

void x16_f_mul(const unsigned char *m) {
    __asm {
        lda m
        ldy m+1
        jsr 0xff6e
        byt 0x1e, 0xfe, 0x04             /* fp_fmult */
    }
}

// ---------------------------------------------------------------------
// FAC = FAC - m, FAC / m, FAC ^ m. The ROM only offers mem-first, so:
// ARG = FAC (movef), FAC = mem (movfm), then the ARG-first entry.
// Three bank crossings -- the price of the intuitive direction.
// ---------------------------------------------------------------------
void x16_f_sub(const unsigned char *m) {
    __asm {
        jsr 0xff6e
        byt 0x81, 0xfe, 0x04             /* fp_movef: ARG = FAC */
        lda m
        ldy m+1
        jsr 0xff6e
        byt 0x60, 0xfe, 0x04             /* fp_movfm: FAC = mem */
        jsr 0xff6e
        byt 0x15, 0xfe, 0x04             /* fp_fsubt: FAC = ARG - FAC */
    }
}

void x16_f_div(const unsigned char *m) {
    __asm {
        jsr 0xff6e
        byt 0x81, 0xfe, 0x04             /* fp_movef */
        lda m
        ldy m+1
        jsr 0xff6e
        byt 0x60, 0xfe, 0x04             /* fp_movfm */
        jsr 0xff6e
        byt 0x27, 0xfe, 0x04             /* fp_fdivt: FAC = ARG / FAC */
    }
}

void x16_f_pow(const unsigned char *m) {
    __asm {
        jsr 0xff6e
        byt 0x81, 0xfe, 0x04             /* fp_movef */
        lda m
        ldy m+1
        jsr 0xff6e
        byt 0x60, 0xfe, 0x04             /* fp_movfm */
        jsr 0xff6e
        byt 0x39, 0xfe, 0x04             /* fp_fpwrt: FAC = ARG ^ FAC */
    }
}

// FAC = m - FAC, m / FAC, m ^ FAC: the ROM's native order, one
// crossing. f_rdiv is the reciprocal form, 1/x.
void x16_f_rsub(const unsigned char *m) {
    __asm {
        lda m
        ldy m+1
        jsr 0xff6e
        byt 0x12, 0xfe, 0x04             /* fp_fsub */
    }
}

void x16_f_rdiv(const unsigned char *m) {
    __asm {
        lda m
        ldy m+1
        jsr 0xff6e
        byt 0x24, 0xfe, 0x04             /* fp_fdiv */
    }
}

void x16_f_rpow(const unsigned char *m) {
    __asm {
        lda m
        ldy m+1
        jsr 0xff6e
        byt 0x36, 0xfe, 0x04             /* fp_fpwr */
    }
}

// -1 if FAC < m, 0 if equal, 1 if FAC > m.
signed char x16_f_cmp(const unsigned char *m) {
    return __asm {
        lda m
        ldy m+1
        jsr 0xff6e
        byt 0x54, 0xfe, 0x04             /* fp_fcomp */
        sta accu
    };
}

// ---------------------------------------------------------------------
// The ROM formats into FP_FBUFFR ($0100, the bottom of the stack page),
// which would not survive the next deep call -- so copy it out at once.
// Positive numbers get a leading space, exactly as BASIC's PRINT shows
// them; the _trim form skips it.
// ---------------------------------------------------------------------
void x16_f_to_str(char *buf) {
    __asm {
        jsr 0xff6e
        byt 0x06, 0xfe, 0x04             /* fp_fout: A = low, Y = high */
        sta fts_ld+1
        sty fts_ld+2
        ldy #0
    fts_loop:
    fts_ld:
        lda 0xffff,y                    /* self-modified above */
        sta (buf),y
        beq fts_done
        iny
        bne fts_loop
    fts_done:
    }
}

void x16_f_to_str_trim(char *buf) {
    __asm {
        jsr 0xff6e
        byt 0x06, 0xfe, 0x04             /* fp_fout */
        sta ftt_pk+1
        sta ftt_ld+1
        sty ftt_pk+2
        sty ftt_ld+2
    ftt_pk:
        lda 0xffff                      /* self-modified: the first char */
        cmp #32                         /* a leading sign space */
        bne ftt_copy
        inc ftt_ld+1                    /* skip it: bump the copy base */
        bne ftt_copy
        inc ftt_ld+2
    ftt_copy:
        ldy #0
    ftt_loop:
    ftt_ld:
        lda 0xffff,y                    /* self-modified above */
        sta (buf),y
        beq ftt_done
        iny
        bne ftt_loop
    ftt_done:
    }
}

// fp_val wants X = address LOW, Y = address high, A = length.
void x16_f_from_str(const char *s, unsigned char len) {
    __asm {
        ldx s
        ldy s+1
        lda len
        jsr 0xff6e
        byt 0x09, 0xfe, 0x04             /* fp_val */
    }
}
