/* =====================================================================
 * x16clib :: x16/sort.h -- in-place sorting of arrays (vbcc)
 * =====================================================================
 * Insertion sort: O(n^2) but tiny and stable, which is right for the
 * modest arrays a 6502 sorts. All variants sort ascending, in place.
 *
 * vbcc has qsort(), but it drags in the full comparator-callback
 * machinery for every element width; these typed entries carry their
 * comparison inline and cost far less to call for the common cases.
 * =====================================================================
 */

#ifndef X16_SORT_H
#define X16_SORT_H

/* Byte and word elements, unsigned or signed, ascending. `count` is
** the ELEMENT count, not a byte size. */
void x16_sort_u8(__reg("r0/r1") unsigned char *arr,
                 __reg("r2/r3") unsigned int count);
void x16_sort_s8(__reg("r0/r1") signed char *arr,
                 __reg("r2/r3") unsigned int count);
void x16_sort_u16(__reg("r0/r1") unsigned int *arr,
                  __reg("r2/r3") unsigned int count);
void x16_sort_s16(__reg("r0/r1") int *arr,
                  __reg("r2/r3") unsigned int count);

/* The generic engine: 2-byte elements (pointers, pairs, 16-bit
** handles), ordered by your comparator. It receives the addresses of
** two ELEMENTS -- for an array of pointers that is a pointer to the
** pointer -- and returns nonzero iff element a must sort AFTER
** element b. Equal elements keep their original order (stable).
**
** The comparator runs inside the sort: it must not call x16_sort()
** itself, and must leave the library's zero-page block alone (any C
** function does; hand-written assembly must save X16_P4-P7). Write
** your comparator with EXACTLY these __reg() annotations -- the
** trampoline delivers the element addresses in r0/r1 and r2/r3. */
typedef unsigned char (*x16_sort_cmp_t)(__reg("r0/r1") const void *a,
                                        __reg("r2/r3") const void *b);

void x16_sort(__reg("r0/r1") void *base,
              __reg("r2/r3") unsigned int count,
              __reg("r4/r5") x16_sort_cmp_t cmp);

#endif /* X16_SORT_H */
