/* =====================================================================
 * x16clib :: x16/stack.h -- an 8 KB LIFO stack in a HIRAM bank
 * =====================================================================
 * A last-in-first-out stack whose 8 KB of storage is one whole
 * banked-RAM bank ($A000-$BFFF). Tell it which bank to own with
 * x16_stack_init(), then push and pop bytes or words:
 *
 *      x16_stack_init(5);              // take bank 5 for the stack
 *      x16_stack_push(42);
 *      x16_stack_pushw(1000);
 *      w = x16_stack_popw();           // 1000 -- LIFO order
 *      b = x16_stack_pop();            // 42
 *
 * It grows downward from the top of the bank. The stack pointer lives in
 * low RAM, so only the data itself sits in the bank, and every call
 * saves and restores RAM_BANK -- a stack in bank 5 and your own use of
 * bank 7 in between never trip over each other.
 *
 * There are NO over/underflow guards: the capacity is 8191 bytes, and
 * checking x16_stack_isfull()/x16_stack_isempty() is on you. One stack
 * exists; init again (same or another bank) to reset it.
 *
 * The small 256-byte stack that needs no bank is x16_stk_* in
 * x16/buffers.h.
 * =====================================================================
 */

#ifndef X16_STACK_H
#define X16_STACK_H

#define X16_STACK_CAPACITY      8191

/* Claim a bank and empty the stack. */
void __fastcall__ x16_stack_init (unsigned char bank);

/* Push a byte, or a word. */
void __fastcall__ x16_stack_push (unsigned char b);
void __fastcall__ x16_stack_pushw (unsigned int w);

/* Pop them again, newest first. */
unsigned char x16_stack_pop (void);
unsigned int x16_stack_popw (void);

unsigned int x16_stack_size (void);     /* bytes stored */
unsigned int x16_stack_free (void);     /* bytes free */

unsigned char x16_stack_isempty (void); /* 1 if empty */
unsigned char x16_stack_isfull (void);  /* 1 if no room for a word */

#endif /* X16_STACK_H */
