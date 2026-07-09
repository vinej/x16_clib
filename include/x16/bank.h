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

#ifndef X16_BANK_H
#define X16_BANK_H

#define X16_BANK_SIZE   8192

/* Safe from an ISR: neither touches the shared scratch block. */
void __fastcall__ x16_bank_set (unsigned char bank);
unsigned char x16_bank_get (void);

unsigned char __fastcall__ x16_bank_peek (unsigned char bank,
                                          unsigned int offset);
void __fastcall__ x16_bank_poke (unsigned char bank, unsigned int offset,
                                 unsigned char value);

/* Low RAM -> banked RAM, and back. */
void __fastcall__ x16_mem_to_bank (const void *src, unsigned char bank,
                                   unsigned int offset, unsigned int count);
void __fastcall__ x16_bank_to_mem (unsigned char bank, unsigned int offset,
                                   void *dst, unsigned int count);

#endif /* X16_BANK_H */
