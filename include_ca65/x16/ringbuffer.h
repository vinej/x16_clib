/* =====================================================================
 * x16clib :: x16/ringbuffer.h -- an 8 KB FIFO ring in a HIRAM bank
 * =====================================================================
 * A first-in-first-out queue whose 8 KB of storage is one whole
 * banked-RAM bank ($A000-$BFFF). Tell it which bank to own with
 * x16_ring_init(), then put and get bytes or words:
 *
 *      x16_ring_init(6);               // take bank 6 for the queue
 *      x16_ring_put('H');
 *      x16_ring_putw(300);
 *      b = x16_ring_get();             // 'H' -- FIFO order
 *      w = x16_ring_getw();            // 300
 *
 * The head, tail and fill counters live in low RAM; only the queued data
 * sits in the bank, and every call saves and restores RAM_BANK. The bank
 * number can come from anywhere -- a constant, or x16_bank_alloc().
 *
 * There are NO over/underflow guards: the capacity is 8191 bytes, and
 * checking x16_ring_isfull()/x16_ring_isempty() is on you. One queue
 * exists; init again (same or another bank) to reset it.
 *
 * The small 256-byte ring that needs no bank is x16_rb_* in
 * x16/buffers.h.
 * =====================================================================
 */

#ifndef X16_RINGBUFFER_H
#define X16_RINGBUFFER_H

#define X16_RING_CAPACITY       8191

/* Claim a bank and empty the queue. */
void __fastcall__ x16_ring_init (unsigned char bank);

/* Enqueue a byte, or a word (low byte first). */
void __fastcall__ x16_ring_put (unsigned char b);
void __fastcall__ x16_ring_putw (unsigned int w);

/* Dequeue them again, oldest first. */
unsigned char x16_ring_get (void);
unsigned int x16_ring_getw (void);

unsigned int x16_ring_size (void);      /* bytes queued */
unsigned int x16_ring_free (void);      /* usable bytes free */

unsigned char x16_ring_isempty (void);  /* 1 if empty */
unsigned char x16_ring_isfull (void);   /* 1 if no room for a word */

#endif /* X16_RINGBUFFER_H */
