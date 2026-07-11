/* =====================================================================
 * x16clib :: test_kickc/runner2.c -- the compute half of the suite
 * =====================================================================
 * THE SUITE IS BUILT TWICE, like the cc65 suite -- there for program
 * RAM, here for zero page. KickC never coalesces a variable that inline
 * asm references, and the full test set plus the whole library holds
 * more of those than the $22-$7F user window (see x16/zpsafe.h). Past
 * the window's edge KickC 0.8.6 does not fail; it silently allocates
 * into the reserved ranges, and the KERNAL corrupts whatever lands
 * there. Two smaller programs keep both compiles comfortably inside.
 *
 * runner.c holds the hardware-facing tests; this file holds the pure
 * compute: fixed point, math, buffers, clipping, collision, ZX0.
 * =====================================================================
 */

#include "testlib.h"
#include <x16/x16.h>

/* ------------------------------------------------------------------ */
/* fixed point                                                         */
/* ------------------------------------------------------------------ */

void test_mul88(void) {
    /* 1.5 * 2.0 == 3.0 */
    t_check((x16_mul88(0x0180, 0x0200) == 0x0300) ? 1 : 0, "MUL88");
}

void test_mul88_negative(void) {
    /* -1.5 * 2.0 == -3.0, and -1.5 * -2.0 == 3.0 */
    t_check((x16_mul88(-384, 512) == -768 &&
            x16_mul88(-384, -512) == 768) ? 1 : 0,
            "MUL88_NEGATIVE");
}

void test_umul16(void) {
    t_check((x16_umul16(1000, 1000) == 1000000 &&
            x16_umul16(0xFFFF, 0xFFFF) == 0xFFFE0001) ? 1 : 0,
            "UMUL16");
}

/* ------------------------------------------------------------------ */
/* game math                                                           */
/* ------------------------------------------------------------------ */

/* The tables are precomputed literals; pin the values that would move
** if anyone regenerated them wrong: the four quarter points of the
** sine, and the bias of the unsigned variants.
*/
void test_math_tables(void) {
    t_check((x16_sin8(0) == 0 &&
            x16_sin8(64) == 127 &&              /* +90 degrees */
            x16_sin8(128) == 0 &&
            x16_sin8(192) == -127 &&            /* -90 degrees */
            x16_cos8(0) == 127 &&               /* cos(a) = sin(a+64) */
            x16_cos8(64) == 0 &&
            x16_sin8u(64) == 255 &&             /* biased by 128 */
            x16_sin8u(192) == 1) ? 1 : 0,
            "MATH_TABLES");
}

/* xorshift has period 65535 and one fixed point at zero, which the seed
** routine must avoid. The same seed must replay the same sequence.
*/
void test_rnd(void) {
    unsigned int a;
    unsigned int b;
    unsigned int c;

    x16_rnd_seed(0x1234);
    a = x16_rnd16();
    b = x16_rnd16();

    x16_rnd_seed(0x1234);
    c = x16_rnd16();

    t_check((a == c && a != b) ? 1 : 0, "RND_REPEATABLE");
}

void test_rnd_zero_seed(void) {
    unsigned int a;
    unsigned int b;

    x16_rnd_seed(0);                    /* must be nudged off the fixed point */
    a = x16_rnd16();
    b = x16_rnd16();

    t_check((a != 0 && b != 0 && a != b) ? 1 : 0, "RND_ZERO_SEED");
}

/* Angle 0 is east (+x); 64 is south (+y), because the screen's y axis
** points down. A byte angle wraps for free, so 192 is north.
*/
void test_atan2(void) {
    t_check((x16_atan2(100, 0) == 0 &&           /* east */
            x16_atan2(0, 100) == 64 &&          /* south, down-screen */
            x16_atan2(-100, 0) == 128 &&        /* west */
            x16_atan2(0, -100) == 192 &&        /* north */
            x16_atan2(50, 50) == 32 &&          /* exactly 45 degrees */
            x16_atan2(0, 0) == 0) ? 1 : 0,               /* degenerate: call it east */
            "ATAN2");
}

/* atan2 must agree with the sine table it shares a convention with:
** walking one step along the returned heading moves toward the target.
*/
void test_atan2_roundtrip(void) {
    unsigned char a = x16_atan2(-60, 60);       /* south-west: 96 = 135 deg */

    t_check((a == 96 && x16_cos8(a) < 0 && x16_sin8(a) > 0) ? 1 : 0,
            "ATAN2_ROUNDTRIP");
}

/* Exact at both ends; the interior is a /256 approximation of /255. */
void test_lerp8(void) {
    t_check((x16_lerp8(10, 200, 0) == 10 &&
            x16_lerp8(10, 200, 255) == 200 &&
            x16_lerp8(200, 10, 0) == 200 &&     /* descending */
            x16_lerp8(200, 10, 255) == 10 &&
            x16_lerp8(0, 100, 128) == 50 &&     /* halfway, within one */
            x16_lerp8(77, 77, 128) == 77) ? 1 : 0,       /* a == b */
            "LERP8");
}

/* ------------------------------------------------------------------ */
/* ring buffer and stack                                               */
/* ------------------------------------------------------------------ */

void test_ringbuffer(void) {
    x16_rb_init();

    t_check((x16_rb_get() == -1 &&               /* empty reads -1 */
            x16_rb_count() == 0 &&
            x16_rb_put(0x11) == 1 &&
            x16_rb_put(0x22) == 1 &&
            x16_rb_count() == 2 &&
            x16_rb_get() == 0x11 &&             /* FIFO order */
            x16_rb_get() == 0x22 &&
            x16_rb_get() == -1 &&
            x16_rb_count() == 0) ? 1 : 0,
            "RINGBUFFER");
}

/* Capacity is 255, and the 8-bit head/tail wrap for free -- so pushing
** and draining twice as many bytes as the buffer holds must still come
** out in order.
*/
void test_ringbuffer_wrap(void) {
    unsigned char i;
    unsigned char e;
    unsigned char ok = 1;
    int got;
    int want;

    x16_rb_init();
    for (i = 0; i < 255; i++) {
        if (!x16_rb_put(i)) ok = 0;
    }
    if (x16_rb_put(0xFF) != 0) ok = 0;          /* the 256th is refused */
    if (x16_rb_count() != 255) ok = 0;

    for (i = 0; i < 200; i++) {                 /* drain most of it... */
        got = x16_rb_get();
        want = (int)i;
        if (got != want) ok = 0;
    }
    for (i = 0; i < 100; i++) {                 /* ...and refill past the wrap */
        e = 0xC0 + i;                           /* wraps: byte arithmetic */
        if (!x16_rb_put(e)) ok = 0;
    }
    for (i = 200; i < 255; i++) {
        got = x16_rb_get();
        want = (int)i;
        if (got != want) ok = 0;
    }
    for (i = 0; i < 100; i++) {
        got = x16_rb_get();
        e = 0xC0 + i;
        want = (int)e;
        if (got != want) ok = 0;
    }
    t_check((ok && x16_rb_get() == -1) ? 1 : 0, "RINGBUFFER_WRAP");
}

void test_stack(void) {
    x16_stk_init();

    t_check((x16_stk_pop() == -1 &&
            x16_stk_depth() == 0 &&
            x16_stk_push(0xAA) == 1 &&
            x16_stk_push(0xBB) == 1 &&
            x16_stk_depth() == 2 &&
            x16_stk_pop() == 0xBB &&            /* LIFO order */
            x16_stk_pop() == 0xAA &&
            x16_stk_pop() == -1) ? 1 : 0,
            "STACK");
}

/* A byte of 0xFF must not be mistaken for the -1 that means "empty". */
void test_buffers_ff_is_not_empty(void) {
    x16_rb_init();
    x16_stk_init();
    x16_rb_put(0xFF);
    x16_stk_push(0xFF);

    t_check((x16_rb_get() == 0x00FF && x16_stk_pop() == 0x00FF) ? 1 : 0,
            "BUFFERS_FF_NOT_EMPTY");
}

/* ------------------------------------------------------------------ */
/* line clipping                                                       */
/* ------------------------------------------------------------------ */

x16_line cl_seg;
x16_line cl_seg2;

void cl_load(int x0, int y0, int x1, int y1) {
    cl_seg.x0 = x0;
    cl_seg.y0 = y0;
    cl_seg.x1 = x1;
    cl_seg.y1 = y1;
}

/* Wholly inside: unchanged, and accepted. */
void test_clip_inside(void) {
    cl_load(10, 20, 300, 200);

    t_check((x16_clip_line(&cl_seg) == 1 &&
            cl_seg.x0 == 10 && cl_seg.y0 == 20 &&
            cl_seg.x1 == 300 && cl_seg.y1 == 200) ? 1 : 0,
            "CLIP_INSIDE");
}

/* Both endpoints share an outside half-plane: rejected without work. */
void test_clip_reject(void) {
    unsigned char ok;

    cl_load(-50, -10, 400, -5);                 /* wholly above */
    ok = (x16_clip_line(&cl_seg) == 0) ? 1 : 0;
    cl_load(400, 10, 500, 200);                 /* wholly right */
    t_check((ok && x16_clip_line(&cl_seg) == 0) ? 1 : 0, "CLIP_REJECT");
}

/* A horizontal line crossing both vertical edges: y is unchanged, and
** the ends land exactly on xmin and xmax (inclusive: 0 and 319).
*/
void test_clip_horizontal(void) {
    cl_load(-100, 120, 500, 120);

    t_check((x16_clip_line(&cl_seg) == 1 &&
            cl_seg.x0 == 0 && cl_seg.x1 == 319 &&
            cl_seg.y0 == 120 && cl_seg.y1 == 120) ? 1 : 0,
            "CLIP_HORIZONTAL");
}

/* A 45-degree diagonal from (-40,-40): it must enter the rectangle
** exactly at the origin, because x and y cross zero together.
*/
void test_clip_diagonal(void) {
    cl_load(-40, -40, 100, 100);

    t_check((x16_clip_line(&cl_seg) == 1 &&
            cl_seg.x0 == 0 && cl_seg.y0 == 0 &&
            cl_seg.x1 == 100 && cl_seg.y1 == 100) ? 1 : 0,
            "CLIP_DIAGONAL");
}

/* A user rectangle, and a segment clipped to it on both sides. */
void test_clip_set(void) {
    unsigned char ok;

    cl_load(0, 50, 319, 50);
    x16_clip_set(100, 40, 200, 60);
    ok = (x16_clip_line(&cl_seg) == 1 &&
          cl_seg.x0 == 100 && cl_seg.x1 == 200) ? 1 : 0;

    x16_clip_set(0, 0, 319, 239);       /* back to the full bitmap */
    t_check(ok, "CLIP_SET");
}

/* Reversing the endpoints must clip to the same segment, reversed --
** the algorithm moves whichever end is outside, and both can be.
*/
void test_clip_symmetry(void) {
    cl_load(-100, 120, 500, 120);
    cl_seg2.x0 = 500;
    cl_seg2.y0 = 120;
    cl_seg2.x1 = -100;
    cl_seg2.y1 = 120;

    t_check((x16_clip_line(&cl_seg) == 1 && x16_clip_line(&cl_seg2) == 1 &&
            cl_seg.x0 == cl_seg2.x1 && cl_seg.x1 == cl_seg2.x0) ? 1 : 0,
            "CLIP_SYMMETRY");
}

/* ------------------------------------------------------------------ */
/* collision                                                           */
/* ------------------------------------------------------------------ */

void test_collide_overlap(void) {
    t_check((x16_collide8(0, 0, 10, 10, 5, 5, 10, 10) == 1) ? 1 : 0, "COLLIDE_OVERLAP");
}

void test_collide_apart(void) {
    t_check((x16_collide8(0, 0, 10, 10, 20, 20, 5, 5) == 0) ? 1 : 0, "COLLIDE_APART");
}

/* Edges that merely touch do not overlap. Box A spans x[0,10), box B
** starts at x=10: adjacent, not colliding. Same on the y axis.
*/
void test_collide_touching(void) {
    t_check((x16_collide8(0, 0, 10, 10, 10, 0, 10, 10) == 0 &&
            x16_collide8(0, 0, 10, 10, 0, 10, 10, 10) == 0) ? 1 : 0,
            "COLLIDE_TOUCHING");
}

x16_box16 co_a;
x16_box16 co_b;
x16_box16 co_away;

void test_collide16(void) {
    /* Beyond x=255, where collide8 cannot reach. */
    co_a.x = 600;    co_a.y = 400;    co_a.w = 20; co_a.h = 4;
    co_b.x = 618;    co_b.y = 370;    co_b.w = 2;  co_b.h = 35;
    co_away.x = 1000; co_away.y = 400; co_away.w = 4; co_away.h = 4;

    t_check((x16_collide16(&co_a, &co_b) == 1 &&
            x16_collide16(&co_a, &co_away) == 0) ? 1 : 0,
            "COLLIDE16");
}

/* ------------------------------------------------------------------ */
/* ZX0                                                                 */
/* ------------------------------------------------------------------ */

/* A 24-byte phrase, four times over, packed by `salvador`. ZX0 v2, not
** -classic. 30 bytes against LZSA2's 31 for the same payload -- ZX0
** packs tighter, which is the reason to carry this code at all.
*/
const unsigned char zx0_packed[30] = {
    0x15, 0xb8, 0x58, 0x31, 0x36, 0x4c, 0x49, 0x42, 0x2d, 0x44, 0x45, 0x43,
    0x4f, 0x4d, 0x50, 0x52, 0x45, 0x53, 0x53, 0x2d, 0x54, 0x45, 0x53, 0x54,
    0x21, 0xd0, 0x15, 0xd5, 0x55, 0x60
};

const unsigned char zx0_phrase[24] = {
    'X', '1', '6', 'L', 'I', 'B', '-', 'D', 'E', 'C', 'O', 'M', 'P', 'R',
    'E', 'S', 'S', '-', 'T', 'E', 'S', 'T', '!', '!'
};

unsigned char zx0_out[97];

void test_zx0(void) {
    unsigned char *end;
    unsigned char i;
    unsigned char r;
    unsigned char ok = 1;

    zx0_out[96] = 0x77;                 /* guard, one past the output */
    end = (unsigned char *)x16_zx0_decompress(zx0_packed, zx0_out);

    if (end != zx0_out + 96 || zx0_out[96] != 0x77) {
        t_check(0, "ZX0");
        return;
    }
    for (r = 0; r < 4; r++) {
        for (i = 0; i < 24; i++) {
            if (zx0_out[r * 24 + i] != zx0_phrase[i]) ok = 0;
        }
    }
    t_check(ok, "ZX0");
}

/* ------------------------------------------------------------------ */
/* the ROM's floating point                                            */
/* ------------------------------------------------------------------ */

x16_float fa;
x16_float fb;
char fbuf[X16_FP_STRLEN];

/* KickC has no strcmp to lean on; three-byte answers at most. */
unsigned char f_streq(const char *p, const char *q) {
    unsigned char i;
    for (i = 0; i < 8; i++) {
        if (p[i] != q[i]) {
            return 0;
        }
        if (p[i] == 0) {
            return 1;
        }
    }
    return 0;
}

void test_f_roundtrip(void) {
    x16_f_from_s16(1234);
    x16_f_store(fa);
    x16_f_zero();
    x16_f_load(fa);

    t_check((x16_f_to_s16() == 1234) ? 1 : 0, "F_ROUNDTRIP");
}

void test_f_neg(void) {
    x16_f_from_s16(5);
    x16_f_neg();

    t_check((x16_f_to_s16() == -5) ? 1 : 0, "F_NEG");
}

/* 200 through the ROM's signed fp_float would come out as -56. */
void test_f_from_u8(void) {
    x16_f_from_u8(200);

    t_check((x16_f_to_s16() == 200) ? 1 : 0, "F_FROM_U8");
}

void test_f_sub_order(void) {
    x16_f_from_s16(4);
    x16_f_store(fb);

    x16_f_from_s16(10);
    x16_f_sub(fb);                      /* 10 - 4 */
    if (x16_f_to_s16() != 6) {
        t_check(0, "F_SUB_ORDER");
        return;
    }

    x16_f_from_s16(10);
    x16_f_rsub(fb);                     /* 4 - 10 */
    t_check((x16_f_to_s16() == -6) ? 1 : 0, "F_SUB_ORDER");
}

void test_f_div_order(void) {
    x16_f_from_s16(4);
    x16_f_store(fb);

    x16_f_from_s16(10);
    x16_f_div(fb);                      /* 10 / 4 */
    x16_f_to_str_trim(fbuf);
    if (!f_streq(fbuf, "2.5")) {
        t_check(0, "F_DIV_ORDER");
        return;
    }

    x16_f_from_s16(10);
    x16_f_rdiv(fb);                     /* 4 / 10 */
    x16_f_to_str_trim(fbuf);
    t_check(f_streq(fbuf, ".4"), "F_DIV_ORDER");
}

void test_f_sqrt(void) {
    x16_f_from_s16(16);
    x16_f_sqrt();

    t_check((x16_f_to_s16() == 4) ? 1 : 0, "F_SQRT");
}

/* Positive numbers carry BASIC's leading sign space; _trim drops it. */
void test_f_str(void) {
    unsigned char ok;

    x16_f_from_s16(42);
    x16_f_to_str(fbuf);
    ok = f_streq(fbuf, " 42");

    x16_f_from_s16(42);
    x16_f_to_str_trim(fbuf);
    ok = (ok && f_streq(fbuf, "42")) ? 1 : 0;

    x16_f_from_s16(-3);
    x16_f_to_str_trim(fbuf);
    ok = (ok && f_streq(fbuf, "-3")) ? 1 : 0;

    t_check(ok, "F_STR");
}

const char f_two_five[] = "2.5";

void test_f_from_str(void) {
    unsigned char ok;

    x16_f_from_str(f_two_five, 3);
    x16_f_store(fa);

    x16_f_from_s16(2);
    x16_f_store(fb);
    x16_f_load(fa);

    /* 2.5 > 2, and 2.5 * 2 == 5. */
    ok = (x16_f_cmp(fb) == 1) ? 1 : 0;
    x16_f_mul(fb);
    ok = (ok && x16_f_to_s16() == 5) ? 1 : 0;
    t_check(ok, "F_FROM_STR");
}

void test_f_cmp(void) {
    x16_f_from_s16(3);
    x16_f_store(fb);

    x16_f_from_s16(2);
    if (x16_f_cmp(fb) != -1) {
        t_check(0, "F_CMP");
        return;
    }
    x16_f_from_s16(3);
    if (x16_f_cmp(fb) != 0) {
        t_check(0, "F_CMP");
        return;
    }
    x16_f_from_s16(4);
    t_check((x16_f_cmp(fb) == 1) ? 1 : 0, "F_CMP");
}

/* ------------------------------------------------------------------ */

void main(void) {
    t_init();

    test_mul88();
    test_mul88_negative();
    test_umul16();

    test_math_tables();
    test_rnd();
    test_rnd_zero_seed();
    test_atan2();
    test_atan2_roundtrip();
    test_lerp8();

    test_ringbuffer();
    test_ringbuffer_wrap();
    test_stack();
    test_buffers_ff_is_not_empty();

    test_clip_inside();
    test_clip_reject();
    test_clip_horizontal();
    test_clip_diagonal();
    test_clip_set();
    test_clip_symmetry();

    test_collide_overlap();
    test_collide_apart();
    test_collide_touching();
    test_collide16();

    test_zx0();

    test_f_roundtrip();
    test_f_neg();
    test_f_from_u8();
    test_f_sub_order();
    test_f_div_order();
    test_f_sqrt();
    test_f_str();
    test_f_from_str();
    test_f_cmp();

    t_done();
}
