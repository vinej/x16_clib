// =====================================================================
// x16clib :: x16/sort.c -- in-place sorting of arrays
// =====================================================================
// The same insertion sort as src_ca65/util/sort.s, in plain C. The ca65
// version drives one engine through a comparator vector to share code;
// here each typed entry carries its comparison inline -- what the asm's
// sort_cmp_u8/s8/u16/s16 did -- and x16_sort() is the 2-byte-element
// engine calling the C comparator through a real function pointer
// (proven to work with true runtime indirection).
//
// All variants sort ascending, in place, and are stable: the shift only
// happens while the earlier element is strictly greater.
// =====================================================================

#include <x16/sort.h>

// The element being inserted by x16_sort, addressable so the comparator
// can be handed a pointer to it (the asm's srt_key).
volatile unsigned int x16__srt_key;

void x16_sort_u8(unsigned char *arr, unsigned int count) {
    unsigned int i;
    unsigned int j;
    unsigned char key;
    unsigned char aj;
    if (count < 2) {                    // nothing to do
        return;
    }
    for (i = 1; i < count; ++i) {
        key = arr[i];
        j = i - 1;
        for (;;) {
            aj = arr[j];
            if (aj <= key) {            // arr[j] <= key: stop shifting
                arr[j + 1] = key;
                break;
            }
            arr[j + 1] = aj;
            if (j == 0) {               // key belongs at arr[0]
                arr[0] = key;
                break;
            }
            --j;
        }
    }
}

void x16_sort_s8(signed char *arr, unsigned int count) {
    unsigned int i;
    unsigned int j;
    signed char key;
    signed char aj;
    /* No signed-compare fragment is relied on at either width, so the
    ** loop compares the operands biased by their sign bit: as
    ** unsigned, x ^ SIGN orders exactly as x does signed. */
    unsigned char bkey;
    unsigned char baj;
    if (count < 2) {
        return;
    }
    for (i = 1; i < count; ++i) {
        key = arr[i];
        bkey = (unsigned char)key ^ 0x80;
        j = i - 1;
        for (;;) {
            aj = arr[j];
            baj = (unsigned char)aj ^ 0x80;
            if (baj <= bkey) {
                arr[j + 1] = key;
                break;
            }
            arr[j + 1] = aj;
            if (j == 0) {
                arr[0] = key;
                break;
            }
            --j;
        }
    }
}

void x16_sort_u16(unsigned int *arr, unsigned int count) {
    unsigned int i;
    unsigned int j;
    unsigned int key;
    unsigned int aj;
    if (count < 2) {
        return;
    }
    for (i = 1; i < count; ++i) {
        key = arr[i];
        j = i - 1;
        for (;;) {
            aj = arr[j];
            if (aj <= key) {
                arr[j + 1] = key;
                break;
            }
            arr[j + 1] = aj;
            if (j == 0) {
                arr[0] = key;
                break;
            }
            --j;
        }
    }
}

void x16_sort_s16(int *arr, unsigned int count) {
    unsigned int i;
    unsigned int j;
    int key;
    int aj;
    /* No signed-compare fragment is relied on at either width, so the
    ** loop compares the operands biased by their sign bit: as
    ** unsigned, x ^ SIGN orders exactly as x does signed. */
    unsigned int bkey;
    unsigned int baj;
    if (count < 2) {
        return;
    }
    for (i = 1; i < count; ++i) {
        key = arr[i];
        bkey = (unsigned int)key ^ 0x8000;
        j = i - 1;
        for (;;) {
            aj = arr[j];
            baj = (unsigned int)aj ^ 0x8000;
            if (baj <= bkey) {
                arr[j + 1] = key;
                break;
            }
            arr[j + 1] = aj;
            if (j == 0) {
                arr[0] = key;
                break;
            }
            --j;
        }
    }
}

// ---------------------------------------------------------------------
// The generic engine: 2-byte elements handled as words (the asm's
// sort_ptr), ordered by the caller's comparator. The comparator gets
// the ADDRESSES of two elements and answers nonzero iff element a must
// sort after element b.
// ---------------------------------------------------------------------
void x16_sort(void *base, unsigned int count, x16_sort_cmp_t cmp) {
    unsigned int *arr = (unsigned int *)base;
    unsigned int i;
    unsigned int j;
    unsigned char v;
    if (count < 2) {
        return;
    }
    for (i = 1; i < count; ++i) {
        x16__srt_key = arr[i];          // key = arr[i]
        j = i - 1;
        for (;;) {
            v = cmp((void *)&arr[j], (void *)&x16__srt_key);
            if (v == 0) {               // arr[j] <= key: stop shifting
                arr[j + 1] = x16__srt_key;
                break;
            }
            arr[j + 1] = arr[j];
            if (j == 0) {
                arr[0] = x16__srt_key;
                break;
            }
            --j;
        }
    }
}
