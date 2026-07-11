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

void x16_mem_fill(void *dst, __mem unsigned int count,
                  __mem unsigned char value) {
    asm {
        lda count                       // a zero count fills nothing
        ora count+1
        beq mf_done
        lda dst
        sta $02 /*r0L*/
        lda dst+1
        sta $03 /*r0H*/
        lda count
        sta $04 /*r1L*/
        lda count+1
        sta $05 /*r1H*/
        lda value
        jsr $fee4 /*MEMORY_FILL*/
    mf_done:
    }
}

// The regions may overlap. Source or target in $9F00-$9FFF is not
// incremented: uploads to VRAM (dst = X16_VERA_DATA0), downloads from
// it, or copies VRAM to VRAM port to port.
void x16_mem_copy(const void *src, void *dst, __mem unsigned int count) {
    asm {
        lda count                       // a zero count copies nothing
        ora count+1
        beq mc_done
        lda src
        sta $02 /*r0L*/
        lda src+1
        sta $03 /*r0H*/
        lda dst
        sta $04 /*r1L*/
        lda dst+1
        sta $05 /*r1H*/
        lda count
        sta $06 /*r2L*/
        lda count+1
        sta $07 /*r2H*/
        jsr $fee7 /*MEMORY_COPY*/
    mc_done:
    }
}

// CRC-16/IBM-3740. An empty block gives the algorithm's init value,
// $FFFF.
unsigned int x16_mem_crc(const void *addr, __mem unsigned int count) {
    __mem unsigned int r;
    asm {
        lda count
        ora count+1
        bne crc_go
        lda #$ff                        // empty: the $FFFF init value
        sta r
        sta r+1
        bra crc_done
    crc_go:
        lda addr
        sta $02 /*r0L*/
        lda addr+1
        sta $03 /*r0H*/
        lda count
        sta $04 /*r1L*/
        lda count+1
        sta $05 /*r1H*/
        jsr $feea /*MEMORY_CRC*/
        lda $06 /*r2L*/
        sta r
        lda $07 /*r2H*/
        sta r+1
    crc_done:
    }
    return r;
}

// ---------------------------------------------------------------------
// Raw LZSA2 block (lzsa -r -f2). Returns one past the last output byte.
// Cannot decompress in place. A target in $9F00-$9FFF is not
// incremented: point port 0 at VRAM and pass X16_VERA_DATA0 to unpack
// assets straight into video memory.
// ---------------------------------------------------------------------
void * x16_mem_decompress(const void *src, void *dst) {
    __mem void* end;
    asm {
        lda src
        sta $02 /*r0L*/
        lda src+1
        sta $03 /*r0H*/
        lda dst
        sta $04 /*r1L*/
        lda dst+1
        sta $05 /*r1H*/
        jsr $feed /*MEMORY_DECOMPRESS*/
        lda $04 /*r1L*/                 // the KERNAL leaves r1 one past
        sta end                         // the end
        lda $05 /*r1H*/
        sta end+1
    }
    return end;
}
