// =====================================================================
// x16clib :: x16/tscrunch.c -- TSCrunch decompression
// =====================================================================
// TSCrunch (Antonio Savona) is a byte-aligned LZ+RLE built to maximise
// 6502 decode speed. This is the ca65 port's decoder
// (src_ca65/util/tscrunch.s, itself a 65C02 port of the reference
// decrunch_small.asm) re-expressed in C, token for token:
//
//   $00-$1F  literal: the token is the byte count, the bytes follow
//   $20      end of stream
//   $21-$7F  LZ2: a two-byte match at offset (127 - token)
//   bit 7 set, bit 0 set   RLE: (field+1) bytes of the following byte;
//                          field 0 is the one-token zero run whose
//                          length-1 is the stream's header byte
//   bit 7 set, bit 0 clear LZ: bit 1 picks a one-byte offset (set) or
//                          a 15-bit offset whose top bit extends the
//                          length field (clear)
//
// RAM to RAM, forward only, cannot decompress in place. Copyright for
// the original algorithm and decruncher: Antonio Savona.
// =====================================================================

#include <x16/tscrunch.h>

void *x16_tsc_decompress(const void *src, void *dst) {
    const unsigned char *get = (unsigned char *)src;
    unsigned char *put = (unsigned char *)dst;
    const unsigned char *back;
    unsigned int woff;
    unsigned char optlen;
    unsigned char token;
    unsigned char t;
    unsigned char lenop;
    unsigned char hi;
    unsigned char lo;
    unsigned char fill;
    unsigned char cnt;
    unsigned char i;

    optlen = get[0];                    // the one-token zero-run length - 1
    ++get;

    for (;;) {
        token = get[0];
        if ((token & 0x80) == 0) {
            if (token < 0x20) {
                // --- literal: token = count, the bytes follow --------
                ++get;                  // step past the token
                i = token;
                while (i != 0) {
                    --i;
                    put[i] = get[i];
                }
                put += token;
                get += token;
                continue;
            }
            if (token == 0x20) {        // the end-of-stream marker
                return (void *)put;
            }
            // --- LZ2: a two-byte match with a one-byte token ---------
            back = put - (unsigned char)(127 - token);
            put[0] = back[0];
            put[1] = back[1];
            put += 2;
            ++get;
            continue;
        }
        // --- RLE or LZ (token bit 7 set) -----------------------------
        t = (unsigned char)(token & 0x7f);
        if ((t & 1) != 0) {
            // RLE: write (field + 1) copies
            lenop = (unsigned char)(t >> 1);
            if (lenop == 0) {           // the one-token zero run
                cnt = optlen;
                fill = 0;
                ++get;
            } else {
                cnt = lenop;
                fill = get[1];
                get += 2;
            }
            i = cnt;                    // cnt+1 bytes: [cnt] down to [0]
            put[i] = fill;
            while (i != 0) {
                --i;
                put[i] = fill;
            }
            put += cnt;
            ++put;
            continue;
        }
        // LZ match: copy (lenop + 1) bytes, FORWARD, from inside the
        // output (matches may overlap their destination).
        lenop = (unsigned char)(t >> 2);
        if ((t & 2) != 0) {
            lo = get[1];                // lifted: KickC has no
            back = put - lo;            // fragment for ptr - arr[i]
            get += 2;
        } else {
            // long: 15-bit offset; its top bit is one more length bit.
            // Every get[] byte used in arithmetic is lifted into a local
            // first: KickC has no fragments for shifting or subtracting a
            // pointer-indexed byte in place, and the build fails outright
            // rather than miscompiling.
            hi = get[2];
            woff = ((unsigned int)(hi | 0x80)) << 8;
            lo = get[1];
            woff += (unsigned int)lo;
            back = put + woff;          // wraps mod 64K = put - (32K - off)
            lenop = (unsigned char)((lenop << 1) | (hi >> 7));
            get += 3;
        }
        put[0] = back[0];
        i = 0;
        while (i != lenop) {
            ++i;
            put[i] = back[i];
        }
        put += lenop;
        ++put;
    }
}
