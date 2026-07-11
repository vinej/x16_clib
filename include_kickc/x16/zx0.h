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
** KickC build. The API is identical to the cc65 build's; what differs is
** the delivery. KickC has no linker and no archive format -- it compiles
** the whole program from source and strips what goes unused -- so the
** KickC port is a SOURCE distribution. Include this header; the matching
** implementation in src_kickc/x16/ is compiled in automatically when the
** library path points there:
**
**     kickc -p cx16 -a -I include_kickc -L src_kickc yourprog.c
** --------------------------------------------------------------------- */

#ifndef X16_ZX0_H
#define X16_ZX0_H

#include <x16/zpsafe.h>

/* Returns one past the last output byte, so the unpacked length is the
** return value minus `dst`.
*/
void * x16_zx0_decompress (const void *src, void *dst);

#endif /* X16_ZX0_H */
