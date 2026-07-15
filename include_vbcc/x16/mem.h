/* =====================================================================
 * x16clib :: x16/mem.h -- KERNAL block memory operations
 * =====================================================================
 * Wrappers over the KERNAL's MEMORY_FILL, MEMORY_COPY, MEMORY_CRC and
 * MEMORY_DECOMPRESS. The last one is the reason to care: an LZSA2
 * depacker sitting in ROM, free.
 *
 * ONE PROPERTY MAKES THESE SPECIAL. Addresses in $9F00-$9FFF are NOT
 * incremented during the operation. So passing X16_VERA_DATA0 as a
 * source or target streams straight into or out of VRAM, at whatever
 * increment the data port is set to:
 *
 *      x16_vera_addr0(X16_INC_1, X16_VRAM_SPRITE_DATA);
 *      x16_mem_decompress(tiles_lzsa, X16_VERA_DATA0);
 *
 * That unpacks a compressed asset directly into video memory -- no
 * staging buffer, no second copy.
 * =====================================================================
 */

#ifndef X16_MEM_H
#define X16_MEM_H

/* VERA's data ports, as memory-operation endpoints. Point the port with
** x16_vera_addr0()/x16_vera_addr1() first; these addresses do not
** increment, so the port's own increment does the walking. */
#define X16_VERA_DATA0  ((void *)0x9F23)
#define X16_VERA_DATA1  ((void *)0x9F24)

/* Set `count` bytes at `dst` to `value`. A count of 0 fills nothing. */
void x16_mem_fill(__reg("r0/r1") void *dst, __reg("r2/r3") unsigned int count,
                  __reg("r4") unsigned char value);

/* Copy `count` bytes. The regions may overlap. A count of 0 copies
** nothing. */
void x16_mem_copy(__reg("r0/r1") const void *src, __reg("r2/r3") void *dst,
                  __reg("r4/r5") unsigned int count);

/* CRC-16/IBM-3740 of a block. An empty block returns the algorithm's
** initial value, 0xFFFF. */
unsigned int x16_mem_crc(__reg("r0/r1") const void *addr, __reg("r2/r3") unsigned int count);

/* Decompress a raw LZSA2 block. Returns one past the last output byte, so
** the unpacked length is the return value minus `dst`.
**
** Compress with:  lzsa -r -f2 <original> <compressed>
** The -r matters: a raw block, with no frame header.
**
** Cannot decompress in place. The input may live in banked RAM (map the
** bank yourself; the 8 KB window is the limit). The output may be
** X16_VERA_DATA0, which is the whole point -- see above. */
void *x16_mem_decompress(__reg("r0/r1") const void *src, __reg("r2/r3") void *dst);

#endif /* X16_MEM_H */
