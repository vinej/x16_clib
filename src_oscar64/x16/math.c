// =====================================================================
// x16clib :: x16/math.c -- game math: PRNG, sine tables, atan2, lerp
// =====================================================================
// The internal routines are the same hand-written 6502 as
// src_ca65/util/math.s. The sign-extension shims (`ldx #0` for cc65's
// int promotion) have no equivalent here: results leave through the
// accu bytes at exactly their declared width.
//
// THE TABLES ARE PRECOMPUTED, NOT GENERATED. ACME evaluated
//      !for i, 0, 255 { !byte int(sin(i*pi/128)*127.0 + 128.5) - 128 }
// at assembly time; the C ports cannot, so these bytes are the
// literal output of exactly those expressions, byte-identical to the
// other two toolchains' tables. MATH_TABLES in the test suite pins the
// values that would move if anyone regenerated them wrong.
// =====================================================================

#include <x16/math.h>

//   x16__sintab[i]  = int(sin(i * pi/128) * 127.0 + 128.5) - 128
const char x16__sintab[256] = {
    0x00, 0x03, 0x06, 0x09, 0x0C, 0x10, 0x13, 0x16,
    0x19, 0x1C, 0x1F, 0x22, 0x25, 0x28, 0x2B, 0x2E,
    0x31, 0x33, 0x36, 0x39, 0x3C, 0x3F, 0x41, 0x44,
    0x47, 0x49, 0x4C, 0x4E, 0x51, 0x53, 0x55, 0x58,
    0x5A, 0x5C, 0x5E, 0x60, 0x62, 0x64, 0x66, 0x68,
    0x6A, 0x6B, 0x6D, 0x6F, 0x70, 0x71, 0x73, 0x74,
    0x75, 0x76, 0x78, 0x79, 0x7A, 0x7A, 0x7B, 0x7C,
    0x7D, 0x7D, 0x7E, 0x7E, 0x7E, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7E, 0x7E, 0x7E, 0x7D,
    0x7D, 0x7C, 0x7B, 0x7A, 0x7A, 0x79, 0x78, 0x76,
    0x75, 0x74, 0x73, 0x71, 0x70, 0x6F, 0x6D, 0x6B,
    0x6A, 0x68, 0x66, 0x64, 0x62, 0x60, 0x5E, 0x5C,
    0x5A, 0x58, 0x55, 0x53, 0x51, 0x4E, 0x4C, 0x49,
    0x47, 0x44, 0x41, 0x3F, 0x3C, 0x39, 0x36, 0x33,
    0x31, 0x2E, 0x2B, 0x28, 0x25, 0x22, 0x1F, 0x1C,
    0x19, 0x16, 0x13, 0x10, 0x0C, 0x09, 0x06, 0x03,
    0x00, 0xFD, 0xFA, 0xF7, 0xF4, 0xF0, 0xED, 0xEA,
    0xE7, 0xE4, 0xE1, 0xDE, 0xDB, 0xD8, 0xD5, 0xD2,
    0xCF, 0xCD, 0xCA, 0xC7, 0xC4, 0xC1, 0xBF, 0xBC,
    0xB9, 0xB7, 0xB4, 0xB2, 0xAF, 0xAD, 0xAB, 0xA8,
    0xA6, 0xA4, 0xA2, 0xA0, 0x9E, 0x9C, 0x9A, 0x98,
    0x96, 0x95, 0x93, 0x91, 0x90, 0x8F, 0x8D, 0x8C,
    0x8B, 0x8A, 0x88, 0x87, 0x86, 0x86, 0x85, 0x84,
    0x83, 0x83, 0x82, 0x82, 0x82, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x82, 0x82, 0x82, 0x83,
    0x83, 0x84, 0x85, 0x86, 0x86, 0x87, 0x88, 0x8A,
    0x8B, 0x8C, 0x8D, 0x8F, 0x90, 0x91, 0x93, 0x95,
    0x96, 0x98, 0x9A, 0x9C, 0x9E, 0xA0, 0xA2, 0xA4,
    0xA6, 0xA8, 0xAB, 0xAD, 0xAF, 0xB2, 0xB4, 0xB7,
    0xB9, 0xBC, 0xBF, 0xC1, 0xC4, 0xC7, 0xCA, 0xCD,
    0xCF, 0xD2, 0xD5, 0xD8, 0xDB, 0xDE, 0xE1, 0xE4,
    0xE7, 0xEA, 0xED, 0xF0, 0xF4, 0xF7, 0xFA, 0xFD
};

//   x16__atantab[i] = int(arctan(i/32.0) * 128.0/pi + 0.5)
const char x16__atantab[33] = {
    0x00, 0x01, 0x03, 0x04, 0x05, 0x06, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
    0x0D, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1D, 0x1E, 0x1F, 0x1F, 0x20
};

// The PRNG seed has a nonzero default: a zero state is xorshift's one
// fixed point.
volatile unsigned int x16__rnd_state = 0x2A56;

// atan2 scratch
volatile char x16__at_ax;
volatile char x16__at_ay;
volatile char x16__at_negx;
volatile char x16__at_negy;
volatile char x16__at_num[2];
volatile char x16__at_den;

// lerp scratch
volatile char x16__lp_t;
volatile char x16__lp_n;
volatile char x16__lp_d;

// ---------------------------------------------------------------------
// John Metcalf's 16-bit xorshift (shifts 7, 9, 8): period 65535 and a
// handful of cycles, cheap enough per frame per object. Internal -- one
// step of the generator, in place on x16__rnd_state.
// ---------------------------------------------------------------------
void x16__rnd_step(void) {
    __asm {
        lda x16__rnd_state+1
        lsr
        lda x16__rnd_state
        ror
        eor x16__rnd_state+1
        sta x16__rnd_state+1    /* x ^= x >> 9 */
        ror
        eor x16__rnd_state
        sta x16__rnd_state      /* x ^= x << 7 */
        eor x16__rnd_state+1
        sta x16__rnd_state+1    /* x ^= x << 8 */
    }
}

void x16_rnd_seed(unsigned int seed) {
    __asm {
        lda seed
        sta x16__rnd_state
        lda seed+1
        sta x16__rnd_state+1
        ora seed
        bne rs_ok
        inc x16__rnd_state      /* zero stays zero forever */
    rs_ok:
    }
}

unsigned char x16_rnd8(void) {
    x16__rnd_step();
    return (unsigned char)x16__rnd_state;
}

unsigned int x16_rnd16(void) {
    x16__rnd_step();
    return x16__rnd_state;
}

signed char x16_sin8(unsigned char angle) {
    return __asm {
        ldy angle
        lda x16__sintab,y
        sta accu
    };
}

signed char x16_cos8(unsigned char angle) {
    return __asm {
        lda angle
        clc
        adc #64                 /* cos(a) = sin(a + 90 degrees) */
        tay
        lda x16__sintab,y
        sta accu
    };
}

unsigned char x16_sin8u(unsigned char angle) {
    return __asm {
        ldy angle
        lda x16__sintab,y
        clc
        adc #128
        sta accu
    };
}

unsigned char x16_cos8u(unsigned char angle) {
    return __asm {
        lda angle
        clc
        adc #64
        tay
        lda x16__sintab,y
        clc
        adc #128
        sta accu
    };
}

// ---------------------------------------------------------------------
// The angle of a vector: 0 = east (+x), 64 = down-screen (+y).
// Octant reduction plus the 33-entry arctangent table; the only work is
// one 8-bit divide. atan2(0,0) answers 0.
// ---------------------------------------------------------------------
unsigned char x16_atan2(signed char dx, signed char dy) {
    return __asm {
        lda #0
        sta x16__at_negx
        lda dx                  /* |dx|, remembering the sign */
        bpl at_dx_pos
        inc x16__at_negx
        eor #0xff
        clc
        adc #1
    at_dx_pos:
        sta x16__at_ax
        lda #0                  /* zero the sign BEFORE the load */
        sta x16__at_negy        /* whose N flag bpl tests */
        lda dy                  /* |dy| */
        bpl at_dy_pos
        inc x16__at_negy
        eor #0xff
        clc
        adc #1
    at_dy_pos:
        sta x16__at_ay

        /* base angle 0..64 within the positive quadrant */
        cmp x16__at_ax
        beq at_diag
        bcc at_shallow
        lda x16__at_ax          /* steep: base = 64 - atan(ax/ay) */
        ldx x16__at_ay
        jsr at_ratio32
        tay
        sec
        lda #64
        sbc x16__atantab,y
        jmp at_quad
    at_diag:
        ora x16__at_ax
        bne at_is45
        lda #0                  /* atan2(0,0): call it east */
        jmp at_store
    at_is45:
        lda #32                 /* exactly 45 degrees */
        jmp at_quad
    at_shallow:
        lda x16__at_ay          /* shallow: base = atan(ay/ax) */
        ldx x16__at_ax
        jsr at_ratio32
        tay
        lda x16__atantab,y

    at_quad:
        /* fold the base angle into the right quadrant */
        ldy x16__at_negx
        beq at_dx_ok
        eor #0xff                /* dx < 0: angle = 128 - base */
        clc
        adc #129
    at_dx_ok:
        ldy x16__at_negy
        beq at_store
        eor #0xff                /* dy < 0: angle = -angle */
        clc
        adc #1
    at_store:
        sta accu
        jmp at_end

    /* A = (A * 32) / X, for A <= X and X nonzero. Result 0..32. */
    at_ratio32:
        stx x16__at_den
        sta x16__at_num+1       /* num = A * 256... */
        ldx #0
        stx x16__at_num
        ldx #3
    at_shift:
        lsr x16__at_num+1       /* ...then >> 3 = A * 32 */
        ror x16__at_num
        dex
        bne at_shift
        lda #0                  /* 16-bit / 8-bit restoring divide */
        ldx #16
    at_div:
        asl x16__at_num
        rol x16__at_num+1
        rol
        cmp x16__at_den
        bcc at_no
        sbc x16__at_den
        inc x16__at_num
    at_no:
        dex
        bne at_div
        lda x16__at_num         /* the quotient */
        rts

    at_end:
    };
}

// ---------------------------------------------------------------------
// Linear interpolation between two unsigned bytes: t=0 is exactly a,
// t=255 exactly b. Computes a +/- (|b-a| * (t+1)) / 256 -- at most one
// off from the ideal /255 midway, exact at both ends.
// ---------------------------------------------------------------------
unsigned char x16_lerp8(unsigned char a, unsigned char b, unsigned char t) {
    return __asm {
        lda t
        sta x16__lp_t
        lda b
        cmp a
        bcc lp_down
        sbc a                   /* carry set: a clean subtract */
        jsr lp_scale
        clc
        adc a
        jmp lp_store
    lp_down:
        lda a                   /* b < a: interpolate downwards */
        sec
        sbc b
        jsr lp_scale
        sta x16__lp_d
        sec
        lda a
        sbc x16__lp_d
        jmp lp_store

    /* A = (A * (lp_t + 1)) >> 8 */
    lp_scale:
        sta x16__lp_d
        lda x16__lp_t
        cmp #0xff
        beq lp_whole            /* t+1 = 256: the answer is d itself */
        adc #1                  /* n = t+1 (C=0: cmp #0xff cleared it, */
                                /* the beq took the only A>=0xff case) */
        sta x16__lp_n
        /* 8x8 multiply keeping only the high byte: per multiplier bit */
        /* (LSB first), optionally add d, then rotate the result right. */
        lda #0
        ldx #8
    lp_mul:
        lsr x16__lp_n
        bcc lp_skip
        clc
        adc x16__lp_d
    lp_skip:
        ror
        dex
        bne lp_mul
        rts
    lp_whole:
        lda x16__lp_d
        rts

    lp_store:
        sta accu
    };
}
