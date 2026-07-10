/* =====================================================================
 * x16clib :: x16/bank.h -- banked RAM, the $A000-$BFFF window
 * =====================================================================
 * RAM_BANK selects which 8 KB bank appears at $A000. Bank 0 holds KERNAL
 * variables; banks 1..255 are yours. Offsets are 0..8191 into the window.
 *
 * Every routine here saves and restores RAM_BANK, so none of them
 * disturbs whatever bank the caller had mapped in. The bulk copies
 * auto-advance across bank boundaries: a run may start near the end of
 * one bank and finish in the next.
 *
 * cc65's BANK_RAM[] in <cx16.h> gives raw access to the window once you
 * have set RAM_BANK yourself. These add the save/restore, the offset
 * arithmetic, and the boundary crossing.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** llvm-mos build. The API is identical to the cc65 build's; only the
** calling convention differs, and llvm-mos expresses it in the compiler
** rather than in the declaration. So there is no __fastcall__ here.
**
** llvm-mos passes byte arguments in A, then X, then __rc2, __rc3, ...
** and returns the same way. cc65 pushes all but the last argument on a
** software stack. Object code from the two toolchains cannot be mixed.
** --------------------------------------------------------------------- */

#ifndef X16_BANK_H
#define X16_BANK_H

#define X16_BANK_SIZE   8192

/* Safe from an ISR: neither touches the shared scratch block. */
void x16_bank_set (unsigned char bank);
unsigned char x16_bank_get (void);

unsigned char x16_bank_peek (unsigned char bank,
                                          unsigned int offset);
void x16_bank_poke (unsigned char bank, unsigned int offset,
                                 unsigned char value);

/* Low RAM -> banked RAM, and back. Both auto-advance across bank edges:
** a run that reaches the end of a bank continues at offset 0 of the next.
** The KERNAL's MEMORY_COPY does the work, a bank-segment per call.
*/
void x16_mem_to_bank (const void *src, unsigned char bank,
                                   unsigned int offset, unsigned int count);
void x16_bank_to_mem (unsigned char bank, unsigned int offset,
                                   void *dst, unsigned int count);

/* Banked RAM -> banked RAM. Only one bank fits in the window at a time,
** so this bounces through a 128-byte low-RAM buffer. Both sides
** auto-advance across bank edges.
*/
void x16_bank_copy_far (unsigned char src_bank,
                                     unsigned int src_offset,
                                     unsigned char dst_bank,
                                     unsigned int dst_offset,
                                     unsigned int count);

/* ---------------------------------------------------------------------
 * Whole-bank allocator.
 *
 * The natural allocation unit on this machine IS the bank: sample sets,
 * level maps, decompression buffers. This is a bitmap over banks 1-255
 * (bank 0 is the KERNAL's). It hands out bank NUMBERS and never touches
 * RAM_BANK itself.
 *
 *      x16_bank_alloc_init(1, 63);         // a 512K machine, minus bank 0
 *      b = x16_bank_alloc();               // 0 means exhausted
 *      ...
 *      x16_bank_free(b);
 *
 * Before x16_bank_alloc_init() nothing is allocatable, so a forgotten
 * init fails cleanly rather than handing out a bank someone else owns.
 * ------------------------------------------------------------------ */

/* Define the pool. first <= last, both inclusive. Calling it again
** resets the pool: every bank in range becomes free and nothing is
** remembered.
*/
void x16_bank_alloc_init (unsigned char first,
                                       unsigned char last);

/* The lowest free bank, or 0 when the pool is exhausted. */
unsigned char x16_bank_alloc (void);

/* Give a bank back. Freeing one that was never allocated quietly marks
** it allocatable -- there is no ownership record, so don't.
*/
void x16_bank_free (unsigned char bank);

/* Claim a specific bank. Returns 1 if it was free and is now yours,
** 0 if it was already taken or is outside the pool.
*/
unsigned char x16_bank_reserve (unsigned char bank);

#endif /* X16_BANK_H */
