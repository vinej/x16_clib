/* =====================================================================
 * x16clib :: x16/sort.h -- in-place sorting of arrays
 * =====================================================================
 * Insertion sort: O(n^2) but tiny and stable, which is right for the
 * modest arrays a 6502 sorts. All variants sort ascending, in place.
 *
 * The typed entries carry their comparison inline and cost far less to
 * call than a comparator-callback sort for the common cases.
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

#ifndef X16_SORT_H
#define X16_SORT_H

#include <x16/zpsafe.h>

/* Byte and word elements, unsigned or signed, ascending. `count` is
** the ELEMENT count, not a byte size.
*/
void x16_sort_u8  (unsigned char *arr, unsigned int count);
void x16_sort_s8  (signed char *arr, unsigned int count);
void x16_sort_u16 (unsigned int *arr, unsigned int count);
void x16_sort_s16 (int *arr, unsigned int count);

/* The generic engine: 2-byte elements (pointers, pairs, 16-bit
** handles), ordered by your comparator. It receives the addresses of
** two ELEMENTS -- for an array of pointers that is a pointer to the
** pointer -- and returns nonzero iff element a must sort AFTER
** element b. Equal elements keep their original order (stable).
**
** The comparator runs inside the sort: it must not call x16_sort()
** itself.
*/
typedef unsigned char (*x16_sort_cmp_t) (const void *a, const void *b);

void x16_sort (void *base, unsigned int count, x16_sort_cmp_t cmp);

#endif /* X16_SORT_H */
