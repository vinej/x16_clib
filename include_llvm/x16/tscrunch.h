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
** llvm-mos build. The API is identical to the cc65 build's; only the
** calling convention differs, and llvm-mos expresses it in the compiler
** rather than in the declaration. So there is no __fastcall__ here.
** --------------------------------------------------------------------- */

#ifndef X16_TSCRUNCH_H
#define X16_TSCRUNCH_H

/* Returns one past the last output byte, so the unpacked length is the
** return value minus `dst`.
*/
void * x16_tsc_decompress (const void *src, void *dst);

#endif /* X16_TSCRUNCH_H */
