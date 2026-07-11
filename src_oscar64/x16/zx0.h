/* =====================================================================
 * x16clib :: x16/zx0.h -- ZX0 decompression (Einar Saukas's format)
 * =====================================================================
 * The ROM's LZSA2 (x16_mem_decompress) is free and fast; ZX0 packs
 * tighter. This decodes the MODERN ZX0 v2 stream -- what `zx0` and
 * `salvador` emit by default, not their -classic mode.
 *
 *      salvador data.bin data.zx0
 *
 * RAM to RAM only (the match copier reads the output back). Cannot
 * decompress in place.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** Oscar64 build. The API is identical to the cc65 build's; what differs
** is the delivery. Oscar64 compiles the whole program at once and strips
** what goes unused, so this port is a SOURCE distribution: headers and
** implementations sit side by side in src_oscar64/x16/, and the
** `#pragma compile` at the bottom of this header pulls the matching .c
** in automatically:
**
**     oscar64 -tm=x16 -n -i=src_oscar64 -o=YOURPROG.PRG yourprog.c
** --------------------------------------------------------------------- */

#ifndef X16_ZX0_H
#define X16_ZX0_H

/* Returns one past the last output byte, so the unpacked length is the
** return value minus `dst`.
*/
void * x16_zx0_decompress (const void *src, void *dst);

/* pulls the implementation in with this header */
#pragma compile("zx0.c")

#endif /* X16_ZX0_H */
