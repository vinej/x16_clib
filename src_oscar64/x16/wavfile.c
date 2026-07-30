// =====================================================================
// x16clib :: x16/wavfile.c -- parse a WAV/RIFF header out of RAM
// =====================================================================
// A RIFF file is "RIFF" <size> "WAVE" followed by 8-byte-headed chunks.
// The "fmt " chunk carries the format, the "data" chunk the samples; the
// walker steps chunk to chunk until it finds data, then reports where the
// samples start relative to the buffer.
//
// The magic is compared as EXPLICIT BYTES. What is on disk is ASCII, and
// a character literal is at the mercy of whatever encoding the including
// program set -- `p[0] != 'R'` would silently compare against a PETSCII
// byte and never match a real WAV file.
// =====================================================================

#include <x16/wavfile.h>

static x16_wav_info wav_state;                // what the last parse published

// A little-endian 32-bit field at q[0..3], assembled by shifting a local
// down from the top byte.
//
// NOT written as the obvious OR chain
//      (unsigned long)q[0] | ((unsigned long)q[1] << 8) | ...
// which Oscar64 1.32.272 miscompiles in this translation unit: the three
// shifted terms come out correct and the unshifted low-byte term is lost,
// so every value read back is missing its low byte -- a 22050 Hz rate
// arrives as 0x5600. It is not the function or the expression on their
// own (both are fine in isolation, and in a file that does not also
// compile this module); something about the combination sets it off,
// which is why the working form is pinned here rather than explained.
//
// test_oscar64/runner11.c checks rate and data_len byte for byte. If this
// is ever rewritten, run it.
static unsigned long wav_l32(const unsigned char *q) {
    unsigned long v = q[3];

    v = (v << 8) | q[2];
    v = (v << 8) | q[1];
    v = (v << 8) | q[0];
    return v;
}

// ---------------------------------------------------------------------
// Walk the chunks. 0 if the buffer is not RIFF/WAVE, if a data chunk
// arrives before any fmt chunk, or if a kilobyte of chunks goes by
// without one -- a header that long is malformed, and the bound is what
// keeps a corrupt size field from walking off into memory.
// ---------------------------------------------------------------------
unsigned char x16_wav_parse_header(const void *buf) {
    const unsigned char *p = (const unsigned char *)buf;
    unsigned int cur = 12;              // first chunk starts at offset 12
    unsigned char seen_fmt = 0;
    unsigned long sz;
    unsigned int adv;

    // "RIFF"
    if (p[0] != 0x52 || p[1] != 0x49 || p[2] != 0x46 || p[3] != 0x46) {
        return 0;
    }
    // "WAVE"
    if (p[8] != 0x57 || p[9] != 0x41 || p[10] != 0x56 || p[11] != 0x45) {
        return 0;
    }
    p += 12;

    for (;;) {
        // "fmt "
        if (p[0] == 0x66 && p[1] == 0x6D && p[2] == 0x74 && p[3] == 0x20) {
            wav_state.format   = p[8];        // the body starts at +8
            wav_state.channels = p[10];
            wav_state.rate     = wav_l32(p + 12);
            wav_state.bits     = p[22];
            seen_fmt = 1;
        }
        // "data"
        else if (p[0] == 0x64 && p[1] == 0x61 && p[2] == 0x74 && p[3] == 0x61) {
            if (seen_fmt == 0) {
                return 0;               // data before fmt: malformed
            }
            wav_state.data_len = wav_l32(p + 4);
            wav_state.data_off = cur + 8;     // the samples follow the header
            return 1;
        }

        sz = wav_l32(p + 4);            // this chunk's size
        if ((sz & 1) != 0) {
            ++sz;                       // chunks pad to an even length
        }
        // 16 bits is plenty for anything before the data chunk.
        adv = (unsigned int)sz + 8;
        p   += adv;
        cur += adv;

        if (cur >= 1024) {
            return 0;
        }
    }
}

// ---------------------------------------------------------------------
// What the last successful parse found.
// ---------------------------------------------------------------------
void x16_wav_get_info(x16_wav_info *out) {
    out->format   = wav_state.format;
    out->channels = wav_state.channels;
    out->rate     = wav_state.rate;
    out->bits     = wav_state.bits;
    out->data_off = wav_state.data_off;
    out->data_len = wav_state.data_len;
}

unsigned long x16_wav_rate(void) {
    return wav_state.rate;
}

unsigned long x16_wav_data_len(void) {
    return wav_state.data_len;
}
