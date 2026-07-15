/* =====================================================================
 * x16clib :: x16/zx0.h -- ZX0 decompression
 * =====================================================================
 * The ROM's LZSA2 depacker (x16_mem_decompress) is free and fast. ZX0
 * packs tighter, at the cost of carrying this code.
 *
 * Compress with `salvador`, or `zx0` in its default mode -- this decodes
 * the MODERN ZX0 v2 stream, not the -classic one:
 *
 *      salvador data.bin data.zx0
 *
 * RAM to RAM only: the match copier reads the output back, so unlike
 * x16_mem_decompress() this cannot write through VERA's data port, and
 * cannot decompress in place.
 * =====================================================================
 */

#ifndef X16_ZX0_H
#define X16_ZX0_H

/* Returns one past the last output byte, so the unpacked length is the
** return value minus `dst`. */
void *x16_zx0_decompress(__reg("r0/r1") const void *src, __reg("r2/r3") void *dst);

#endif /* X16_ZX0_H */
