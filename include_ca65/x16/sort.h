/* =====================================================================
 * x16clib :: x16/sort.h -- in-place sorting of arrays
 * =====================================================================
 * Insertion sort: O(n^2) but tiny and stable, which is right for the
 * modest arrays a 6502 sorts. All variants sort ascending, in place.
 *
 * cc65 has qsort(), but it drags in the full comparator-callback
 * machinery for every element width; these typed entries carry their
 * comparison inline and cost far less to call for the common cases.
 * =====================================================================
 */

#ifndef X16_SORT_H
#define X16_SORT_H

/* Byte and word elements, unsigned or signed, ascending. `count` is
** the ELEMENT count, not a byte size.
*/
void __fastcall__ x16_sort_u8  (unsigned char *arr, unsigned int count);
void __fastcall__ x16_sort_s8  (signed char *arr, unsigned int count);
void __fastcall__ x16_sort_u16 (unsigned int *arr, unsigned int count);
void __fastcall__ x16_sort_s16 (int *arr, unsigned int count);

/* The generic engine: 2-byte elements (pointers, pairs, 16-bit
** handles), ordered by your comparator. It receives the addresses of
** two ELEMENTS -- for an array of pointers that is a pointer to the
** pointer -- and returns nonzero iff element a must sort AFTER
** element b. Equal elements keep their original order (stable).
**
** The comparator runs inside the sort: it must not call x16_sort()
** itself, and must leave the library's zero-page block alone (any C
** function does; hand-written assembly must save X16_P4-P7).
*/
typedef unsigned char __fastcall__ (*x16_sort_cmp_t) (const void *a,
                                                      const void *b);

void __fastcall__ x16_sort (void *base, unsigned int count,
                            x16_sort_cmp_t cmp);

#endif /* X16_SORT_H */
