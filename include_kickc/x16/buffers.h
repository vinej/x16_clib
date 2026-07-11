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
** KickC build. The API is identical to the cc65 build's; what differs is
** the delivery. KickC has no linker and no archive format -- it compiles
** the whole program from source and strips what goes unused -- so the
** KickC port is a SOURCE distribution. Include this header; the matching
** implementation in src_kickc/x16/ is compiled in automatically when the
** library path points there:
**
**     kickc -p cx16 -a -I include_kickc -L src_kickc yourprog.c
** --------------------------------------------------------------------- */

#ifndef X16_BUFFERS_H
#define X16_BUFFERS_H

#include <x16/zpsafe.h>

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
