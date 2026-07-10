/* =====================================================================
 * x16clib :: x16/buffers.h -- byte ring buffer and byte stack
 * =====================================================================
 * The two structures an input queue and an audio refiller keep
 * reinventing. One static instance of each, 256 bytes of storage, 8-bit
 * indices so wrap-around is free. Capacity is 255: a count byte is what
 * distinguishes full from empty.
 *
 * `get` and `pop` return -1 when there is nothing there, like getchar().
 *
 * NOT safe across an interrupt boundary, even single-producer /
 * single-consumer: put and get both touch the count. If one side runs in
 * an ISR, bracket the other side's call in a critical section.
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

#ifndef X16_BUFFERS_H
#define X16_BUFFERS_H

/* FIFO. */
void x16_rb_init (void);

/* 1 if stored, 0 if the buffer was full (the byte is dropped). */
unsigned char x16_rb_put (unsigned char b);

/* The next byte, or -1 when empty. */
int x16_rb_get (void);

unsigned char x16_rb_count (void);

/* LIFO. */
void x16_stk_init (void);

/* 1 if pushed, 0 if the stack was full (255 deep). */
unsigned char x16_stk_push (unsigned char b);

/* The top byte, or -1 when empty. */
int x16_stk_pop (void);

unsigned char x16_stk_depth (void);

#endif /* X16_BUFFERS_H */
