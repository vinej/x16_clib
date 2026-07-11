// =====================================================================
// x16clib :: x16/mem.c -- KERNAL block memory operations
// =====================================================================
// Thin wrappers over MEMORY_FILL / COPY / CRC / DECOMPRESS in the $FExx
// jump table; no bank switching needed. Same code as
// src_ca65/storage/mem.s.
//
// ONE PROPERTY MAKES THESE SPECIAL: addresses in $9F00-$9FFF are NOT
// incremented during the operation. Point a VERA data port somewhere
// and pass $9F23 (VERA_DATA0) as the source or target, and these stream
// straight into or out of VRAM at the port's own increment.
//
// The pointers here are only handed to the KERNAL by value, never
// indirected by this code, so none of them needs a zero-page slot.
// =====================================================================

#include <x16/mem.h>

void x16_mem_fill(void *dst, unsigned int count,
                  unsigned char value) {
    __asm {
        lda count                       /* a zero count fills nothing */
        ora count+1
        beq mf_done
        lda dst
        sta 0x02                        /* r0L */
        lda dst+1
        sta 0x03                        /* r0H */
        lda count
        sta 0x04                        /* r1L */
        lda count+1
        sta 0x05                        /* r1H */
        lda value
        jsr 0xfee4                      /* MEMORY_FILL */
    mf_done:
    }
}

// The regions may overlap. Source or target in $9F00-$9FFF is not
// incremented: uploads to VRAM (dst = X16_VERA_DATA0), downloads from
// it, or copies VRAM to VRAM port to port.
void x16_mem_copy(const void *src, void *dst, unsigned int count) {
    __asm {
        lda count                       /* a zero count copies nothing */
        ora count+1
        beq mc_done
        lda src
        sta 0x02                        /* r0L */
        lda src+1
        sta 0x03                        /* r0H */
        lda dst
        sta 0x04                        /* r1L */
        lda dst+1
        sta 0x05                        /* r1H */
        lda count
        sta 0x06                        /* r2L */
        lda count+1
        sta 0x07                        /* r2H */
        jsr 0xfee7                      /* MEMORY_COPY */
    mc_done:
    }
}

// CRC-16/IBM-3740. An empty block gives the algorithm's init value,
// $FFFF.
unsigned int x16_mem_crc(const void *addr, unsigned int count) {
    return __asm {
        lda count
        ora count+1
        bne crc_go
        lda #0xff                        /* empty: the 0xFFFF init value */
        sta accu
        sta accu + 1
        jmp crc_done
    crc_go:
        lda addr
        sta 0x02                        /* r0L */
        lda addr+1
        sta 0x03                        /* r0H */
        lda count
        sta 0x04                        /* r1L */
        lda count+1
        sta 0x05                        /* r1H */
        jsr 0xfeea                      /* MEMORY_CRC */
        lda 0x06                        /* r2L */
        sta accu
        lda 0x07                        /* r2H */
        sta accu + 1
    crc_done:
    };
}

// ---------------------------------------------------------------------
// Raw LZSA2 block (lzsa -r -f2). Returns one past the last output byte.
// Cannot decompress in place. A target in $9F00-$9FFF is not
// incremented: point port 0 at VRAM and pass X16_VERA_DATA0 to unpack
// assets straight into video memory.
// ---------------------------------------------------------------------
void * x16_mem_decompress(const void *src, void *dst) {
    return (void*)__asm {
        lda src
        sta 0x02                        /* r0L */
        lda src+1
        sta 0x03                        /* r0H */
        lda dst
        sta 0x04                        /* r1L */
        lda dst+1
        sta 0x05                        /* r1H */
        jsr 0xfeed                      /* MEMORY_DECOMPRESS */
        lda 0x04                 /* the KERNAL leaves r1 one past (r1L) */
        sta accu                         /* the end */
        lda 0x05                        /* r1H */
        sta accu + 1
    };
}
