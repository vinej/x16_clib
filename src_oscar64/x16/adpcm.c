// =====================================================================
// x16clib :: x16/adpcm.c -- IMA ADPCM decoding (4:1 compression)
// =====================================================================
// The canonical IMA/DVI algorithm (the one in WAV files, LOW nibble of
// each byte first). Same decoder as src_ca65/audio/adpcm.s; the block
// loop became plain C driving the asm nibble decoder -- the ca65 loop's
// only job was pointer walking, which C does as well.
//
// Decoder state is exposed: the predictor and the step index. IMA WAV
// block headers carry initial values for both; store them before
// decoding a block's payload.
// =====================================================================

#include <x16/adpcm.h>

const char x16__ad_idxtab[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 2, 4, 6, 8 };

const unsigned int x16__ad_steps[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544,
    598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707,
    1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
    5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

volatile char x16__ad_pred0 = 0;  // the predictor (signed 16-bit)
volatile char x16__ad_pred1 = 0;
volatile char x16__ad_index = 0;  // step table index 0-88

volatile char x16__ad_n;
volatile char x16__ad_sh[2];
volatile char x16__ad_diff[2];
volatile char x16__ad_p[3];

// predictor 0, step index 0
void x16_adpcm_init(void) {
    x16__ad_pred0 = 0;
    x16__ad_pred1 = 0;
    x16__ad_index = 0;
}

void x16_adpcm_set_state(int predictor, unsigned char index) {
    __asm {
        lda predictor
        sta x16__ad_pred0
        lda predictor+1
        sta x16__ad_pred1
        lda index
        sta x16__ad_index
    }
}

int x16_adpcm_predictor(void) {
    return __asm {
        lda x16__ad_pred0
        sta accu
        lda x16__ad_pred1
        sta accu + 1
    };
}

unsigned char x16_adpcm_index(void) {
    return x16__ad_index;
}

// ---------------------------------------------------------------------
// Decode one 4-bit code. Returns the signed 16-bit sample, which is
// also left in the predictor.
// ---------------------------------------------------------------------
int x16_adpcm_nibble(unsigned char code) {
    return __asm {
        lda code
        sta x16__ad_n
        lda x16__ad_index               /* step = steptab[index] */
        asl
        tay
        lda x16__ad_steps,y
        sta x16__ad_sh
        lda x16__ad_steps+1,y
        sta x16__ad_sh+1

        /* diff = step>>3 (+ step if bit2) (+ step>>1 if bit1) */
        /* (+ step>>2 if bit0); max 1.875 * 32767, fits 16 bits unsigned */
        lda #0
        sta x16__ad_diff
        sta x16__ad_diff+1
        lda x16__ad_n
        and #4
        beq an_no4
        lda x16__ad_sh
        sta x16__ad_diff
        lda x16__ad_sh+1
        sta x16__ad_diff+1
    an_no4:
        lsr x16__ad_sh+1
        ror x16__ad_sh
        lda x16__ad_n
        and #2
        beq an_no2
        jsr an_add_sh
    an_no2:
        lsr x16__ad_sh+1
        ror x16__ad_sh
        lda x16__ad_n
        and #1
        beq an_no1
        jsr an_add_sh
    an_no1:
        lsr x16__ad_sh+1
        ror x16__ad_sh
        jsr an_add_sh                   /* the unconditional step>>3 */

        /* predictor +/- diff, in 24 bits, saturated to 16 */
        lda x16__ad_pred0               /* sign-extend the predictor */
        sta x16__ad_p
        lda #0                          /* zero the top BEFORE the load */
        sta x16__ad_p+2                 /* whose N flag bpl tests */
        lda x16__ad_pred1
        sta x16__ad_p+1
        bpl an_ext_ok
        dec x16__ad_p+2                 /* 0xFF */
    an_ext_ok:
        lda x16__ad_n
        and #8
        bne an_minus
        clc
        lda x16__ad_p
        adc x16__ad_diff
        sta x16__ad_p
        lda x16__ad_p+1
        adc x16__ad_diff+1
        sta x16__ad_p+1
        lda x16__ad_p+2
        adc #0
        sta x16__ad_p+2
        jmp an_clamp
    an_minus:
        sec
        lda x16__ad_p
        sbc x16__ad_diff
        sta x16__ad_p
        lda x16__ad_p+1
        sbc x16__ad_diff+1
        sta x16__ad_p+1
        lda x16__ad_p+2
        sbc #0
        sta x16__ad_p+2

    an_clamp:
        /* a legal 16-bit value has p+2 = 0x00 with p+1 bit7 clear, or */
        /* p+2 = 0xFF with p+1 bit7 set; anything else saturates */
        lda x16__ad_p+2
        beq an_maybe_pos
        cmp #0xff
        beq an_maybe_neg
        jmp an_sat                      /* way out of range */
    an_maybe_pos:
        lda x16__ad_p+1
        bpl an_in_range
        jmp an_sat_pos
    an_maybe_neg:
        lda x16__ad_p+1
        bmi an_in_range
        jmp an_sat_neg
    an_sat:
        lda x16__ad_p+2
        bmi an_sat_neg
    an_sat_pos:
        lda #0xff
        sta x16__ad_p
        lda #0x7f
        sta x16__ad_p+1
        jmp an_in_range
    an_sat_neg:
        lda #0
        sta x16__ad_p
        lda #0x80
        sta x16__ad_p+1
    an_in_range:
        lda x16__ad_p
        sta x16__ad_pred0
        lda x16__ad_p+1
        sta x16__ad_pred1

        /* index += indextab[n & 7], clamped to 0..88 */
        lda x16__ad_n
        and #7
        tay
        lda x16__ad_index
        clc
        adc x16__ad_idxtab,y
        bpl an_not_neg
        lda #0
    an_not_neg:
        cmp #89
        bcc an_idx_ok
        lda #88
    an_idx_ok:
        sta x16__ad_index

        lda x16__ad_pred0
        sta accu
        lda x16__ad_pred1
        sta accu + 1
        jmp an_end

    an_add_sh:
        clc
        lda x16__ad_diff
        adc x16__ad_sh
        sta x16__ad_diff
        lda x16__ad_diff+1
        adc x16__ad_sh+1
        sta x16__ad_diff+1
        rts

    an_end:
    };
}

// ---------------------------------------------------------------------
// Decode a run of bytes to 16-bit little-endian samples. `count` is the
// SOURCE byte count: two samples, four bytes, come out for every byte
// in. Low nibble first, as in IMA WAV blocks. State carries across
// calls, so feeding a block in slices is fine.
// ---------------------------------------------------------------------
void x16_adpcm_block(const void *src, void *dst, unsigned int count) {
    const char *s = (char*)src;
    int *d = (int*)dst;
    unsigned int i;
    unsigned char b;

    for (i = 0; i < count; i++) {
        b = s[i];
        *d = x16_adpcm_nibble(b & 0x0F);        // low nibble first
        d++;
        *d = x16_adpcm_nibble(b >> 4);
        d++;
    }
}
