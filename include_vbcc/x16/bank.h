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
 * =====================================================================
 */

#ifndef X16_BANK_H
#define X16_BANK_H

#define X16_BANK_SIZE   8192

/* Safe from an ISR: neither touches the shared scratch block. */
void x16_bank_set(__reg("a") unsigned char bank);
unsigned char x16_bank_get(void);

unsigned char x16_bank_peek(__reg("r0") unsigned char bank,
                            __reg("r2/r3") unsigned int offset);
void x16_bank_poke(__reg("r0") unsigned char bank, __reg("r2/r3") unsigned int offset,
                   __reg("r4") unsigned char value);

/* Low RAM -> banked RAM, and back. Both auto-advance across bank edges:
** a run that reaches the end of a bank continues at offset 0 of the next.
** The KERNAL's MEMORY_COPY does the work, a bank-segment per call. */
void x16_mem_to_bank(__reg("r0/r1") const void *src, __reg("r2") unsigned char bank,
                     __reg("r4/r5") unsigned int offset, __reg("r6/r7") unsigned int count);
void x16_bank_to_mem(__reg("r0") unsigned char bank, __reg("r2/r3") unsigned int offset,
                     __reg("r4/r5") void *dst, __reg("r6/r7") unsigned int count);

/* Banked RAM -> banked RAM. Only one bank fits in the window at a time,
** so this bounces through a 128-byte low-RAM buffer. Both sides
** auto-advance across bank edges. (Five arguments: count spills to the C
** soft stack, which the entry reads directly.) */
void x16_bank_copy_far(__reg("r0") unsigned char src_bank,
                       __reg("r2/r3") unsigned int src_offset,
                       __reg("r4") unsigned char dst_bank,
                       __reg("r6/r7") unsigned int dst_offset,
                       unsigned int count);

/* ---------------------------------------------------------------------
 * Whole-bank allocator. A bitmap over banks 1-255; hands out bank NUMBERS
 * and never touches RAM_BANK itself.
 *
 *      x16_bank_alloc_init(1, 63);         // a 512K machine, minus bank 0
 *      b = x16_bank_alloc();               // 0 means exhausted
 *      ...
 *      x16_bank_free(b);
 * ------------------------------------------------------------------ */

/* Define the pool. first <= last, both inclusive. Calling it again resets
** the pool: every bank in range becomes free and nothing is remembered. */
void x16_bank_alloc_init(__reg("r0") unsigned char first, __reg("r2") unsigned char last);

/* The lowest free bank, or 0 when the pool is exhausted. */
unsigned char x16_bank_alloc(void);

/* Give a bank back. Freeing one that was never allocated quietly marks it
** allocatable -- there is no ownership record, so don't. */
void x16_bank_free(__reg("a") unsigned char bank);

/* Claim a specific bank. Returns 1 if it was free and is now yours, 0 if
** it was already taken or is outside the pool. */
unsigned char x16_bank_reserve(__reg("a") unsigned char bank);

#endif /* X16_BANK_H */
