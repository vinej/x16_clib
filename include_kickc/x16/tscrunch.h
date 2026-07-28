/* =====================================================================
 * x16clib :: x16/tscrunch.h -- TSCrunch decompression
 * =====================================================================
 * TSCrunch (Antonio Savona) is a byte-aligned LZ+RLE built to maximise
 * 6502 decode speed -- the other end of the trade from ZX0: unpacks
 * markedly faster, packs a little looser. Crunch with:
 *
 *      tscrunch data.bin data.tsc      (plain memory crunch)
 *
 * RAM to RAM only: the match copier reads the output back, so this
 * cannot write through VERA's data port, and cannot decompress in
 * place (forward copies only).
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

#ifndef X16_TSCRUNCH_H
#define X16_TSCRUNCH_H

#include <x16/zpsafe.h>

/* Returns one past the last output byte, so the unpacked length is the
** return value minus `dst`.
*/
void * x16_tsc_decompress (const void *src, void *dst);

#endif /* X16_TSCRUNCH_H */
