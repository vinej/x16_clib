/* =====================================================================
 * x16clib :: test_vbcc/runner.c -- the vbcc regression suite (modules)
 * =====================================================================
 * Same discipline as the cc65 and llvm-mos suites: drive the library one
 * way, verify through an INDEPENDENT path. VERA writes go through a
 * library call on port 0; every read back comes from t_vpeek(), which
 * sets up its own address. A bug in the address plumbing therefore cannot
 * hide behind itself.
 *
 * This file is filled in module by module as the port lands. For now it
 * carries the math and zero-page checks, which need no hardware state and
 * prove the harness end to end.
 * ===================================================================== */

#include "testlib.h"
#include <x16/math.h>
#include <x16/screen.h>
#include <x16/buffers.h>
#include <x16/vera.h>
#include <x16/fixed.h>
#include <x16/collide.h>
#include <x16/clip.h>
#include <x16/float.h>
#include <x16/zx0.h>
#include <x16/palette.h>
#include <x16/input.h>
#include <x16/mem.h>
#include <x16/bank.h>
#include <x16/sprite.h>
#include <x16/tile.h>
#include <x16/load.h>
#include <x16/dos.h>
#include <x16/bmx.h>
#include <x16/verafx.h>
#include <x16/bitmap.h>
#include <x16/bitmap2.h>
#include <x16/psg.h>

/* From x16/x16.h; declared here directly so this runner does not pull in
** the umbrella header (and every module header) before they all land. */
unsigned char x16_zp_base(void);

#define TESTVRAM    0x08000UL

/* ------------------------------------------------------------------ */
/* zero page                                                           */
/* ------------------------------------------------------------------ */

/* The linker places the scratch block; the value is not fixed. What must
** hold is that it landed in the free zero-page window ($02-$7F) and above
** the byte the KERNAL/vbcc registers occupy at the very bottom. */
static void test_zp_base(void)
{
    unsigned char base = x16_zp_base();
    t_check(base >= 0x02 && base <= 0x7F - 15, "ZP_BASE");
}

/* ------------------------------------------------------------------ */
/* math                                                                */
/* ------------------------------------------------------------------ */

static void test_math_sin(void)
{
    t_check(x16_sin8(0) == 0 && x16_sin8(64) == 127 &&
            x16_sin8(128) == 0 && (signed char)x16_sin8(192) == -127,
            "MATH_SIN");
}

static void test_math_sinu(void)
{
    t_check(x16_sin8u(0) == 128 && x16_sin8u(64) == 255, "MATH_SINU");
}

static void test_math_atan2(void)
{
    /* east=0, south(+y)=64, west(dx<0)=128, north(dy<0)=192 */
    t_check(x16_atan2(10, 0) == 0   && x16_atan2(0, 10) == 64 &&
            x16_atan2(-10, 0) == 128 && x16_atan2(0, -10) == 192,
            "MATH_ATAN2");
}

static void test_math_lerp(void)
{
    t_check(x16_lerp8(10, 20, 0) == 10 && x16_lerp8(10, 20, 255) == 20 &&
            x16_lerp8(0, 200, 128) == 100, "MATH_LERP");
}

static void test_math_rnd(void)
{
    unsigned int a, b;
    x16_rnd_seed(1);
    a = x16_rnd16();
    x16_rnd_seed(1);
    b = x16_rnd16();
    /* deterministic re-seed, and not stuck at zero */
    t_check(a == b && a != 0, "MATH_RND");
}

/* ------------------------------------------------------------------ */
/* screen                                                              */
/* ------------------------------------------------------------------ */

/* The KERNAL's active-colour byte, fg in the low nibble, bg in the high.
** Read it directly -- an independent path from the library's write. */
#define KERNAL_COLOR   (*(volatile unsigned char *)0x0376)

/* x16_screen_color(fg, bg): the shim wants fg in A and bg in X, and the
** header routes bg through r0. If the two ever swap, this catches it: 3
** and 12 are different and neither nibble is symmetric. */
static void test_screen_color(void)
{
    unsigned char saved = KERNAL_COLOR;

    x16_screen_color(3, 12);
    t_check(KERNAL_COLOR == (unsigned char)((12 << 4) | 3), "SCREEN_COLOR");

    KERNAL_COLOR = saved;
}

/* locate() then get_cursor() round-trip. Row and column differ and both
** are non-zero, so a transposed shim reports them the wrong way round.
** Two hand-written shims are under test at once: locate moves r0/r1 into
** X/Y, and get_cursor takes its second pointer from r0/r1. */
static void test_screen_cursor_roundtrip(void)
{
    unsigned char row = 0xEE, col = 0xEE;

    x16_screen_locate(7, 13);
    x16_screen_get_cursor(&row, &col);

    t_check(row == 7 && col == 13, "SCREEN_CURSOR_ROUNDTRIP");
}

/* Both out-params must be written. A shim that stored through one pointer
** twice would pass the round-trip above if the values happened to agree;
** here the sentinels differ from the answers and from each other. */
static void test_screen_get_cursor_both(void)
{
    unsigned char row = 0x5A, col = 0xA5;

    x16_screen_locate(2, 9);
    x16_screen_get_cursor(&row, &col);

    t_check(row == 2 && col == 9 && row != col, "SCREEN_GET_CURSOR_BOTH");
}

/* Mode 0 is the default text mode and is always supported; $FF is not a
** mode at all. The routine answers 1 / 0, not the KERNAL's carry. */
static void test_screen_mode(void)
{
    unsigned char ok  = x16_screen_set_mode(0);
    unsigned char bad = x16_screen_set_mode(0xFF);
    unsigned char now = x16_screen_get_mode();

    x16_screen_set_mode(0);
    t_check(ok == 1 && bad == 0 && now == 0, "SCREEN_MODE");
}

/* ------------------------------------------------------------------ */
/* buffers                                                             */
/* ------------------------------------------------------------------ */

/* rb_get()/stk_pop() return int, -1 when empty. A shim that forgets the
** high byte returns 255, which compares unequal to -1 -- and equal to a
** stored 255. So push a byte that LOOKS like -1 and prove it reads 255. */
static void test_rb_int_minus_one(void)
{
    int empty, got;

    x16_rb_init();
    empty = x16_rb_get();
    x16_rb_put(0xFF);
    got = x16_rb_get();

    t_check(empty == -1 && got == 255, "RB_GET_MINUS_ONE");
}

static void test_stk_int_minus_one(void)
{
    int empty, got;

    x16_stk_init();
    empty = x16_stk_pop();
    x16_stk_push(0xFF);
    got = x16_stk_pop();

    t_check(empty == -1 && got == 255, "STK_POP_MINUS_ONE");
}

/* FIFO vs LIFO order, plus put/push return and count/depth. */
static void test_buffers_order(void)
{
    unsigned char okrb, okstk;

    x16_rb_init();
    x16_rb_put(1); x16_rb_put(2); x16_rb_put(3);
    okrb = (x16_rb_count() == 3) &&
           (x16_rb_get() == 1) && (x16_rb_get() == 2) && (x16_rb_get() == 3);

    x16_stk_init();
    x16_stk_push(1); x16_stk_push(2); x16_stk_push(3);
    okstk = (x16_stk_depth() == 3) &&
            (x16_stk_pop() == 3) && (x16_stk_pop() == 2) && (x16_stk_pop() == 1);

    t_check(okrb && okstk, "BUFFERS_ORDER");
}

/* ------------------------------------------------------------------ */
/* vera                                                                */
/* ------------------------------------------------------------------ */

static void test_vera_fill(void)
{
    unsigned char i, ok = 1;

    for (i = 0; i < 8; ++i) t_vpoke(0x00, TESTVRAM + i);
    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0xA5, 8);
    for (i = 0; i < 8; ++i)
        if (t_vpeek(TESTVRAM + i) != 0xA5) ok = 0;

    t_check(ok, "VERA_FILL");
}

/* Exactly `count` bytes and not one more; the cell past the end is
** poisoned so the check cannot pass by reading an already-zero cell. */
static void test_vera_fill_count(void)
{
    t_vpoke(0x5A, TESTVRAM + 4);
    t_vpoke(0x5A, TESTVRAM + 5);

    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0xC3, 5);

    t_check(t_vpeek(TESTVRAM + 4) == 0xC3 &&
            t_vpeek(TESTVRAM + 5) == 0x5A, "VERA_FILL_COUNT");
}

/* A count of zero writes nothing; the naive dex/bne loop writes 65536. */
static void test_vera_fill_zero(void)
{
    t_vpoke(0x77, TESTVRAM);
    x16_vera_addr0(X16_INC_1, TESTVRAM);
    x16_vera_fill(0xEE, 0);
    t_check(t_vpeek(TESTVRAM) == 0x77, "VERA_FILL_ZERO");
}

static void test_vera_copy(void)
{
    unsigned char i, ok = 1;

    for (i = 0; i < 6; ++i) {
        t_vpoke(0x10 + i, TESTVRAM + i);            /* source */
        t_vpoke(0x00,     TESTVRAM + 0x40 + i);     /* destination, cleared */
    }
    x16_vera_addr0(X16_INC_1, TESTVRAM);            /* read from */
    x16_vera_addr1(X16_INC_1, TESTVRAM + 0x40);     /* write to  */
    x16_vera_copy(6);

    for (i = 0; i < 6; ++i)
        if (t_vpeek(TESTVRAM + 0x40 + i) != (unsigned char)(0x10 + i)) ok = 0;

    t_check(ok, "VERA_COPY");
}

/* addr0 marshals a 32-bit addr from btmp0 and inc from r0. A high byte
** dropped or transposed lands the fill somewhere other than TESTVRAM+0x100,
** so the poisoned cell there proves both address bytes reached ADDR_M/H. */
static void test_vera_addr_highbytes(void)
{
    t_vpoke(0x00, TESTVRAM + 0x100);
    x16_vera_addr0(X16_INC_1, TESTVRAM + 0x100);
    x16_vera_fill(0x3C, 1);
    t_check(t_vpeek(TESTVRAM + 0x100) == 0x3C, "VERA_ADDR_HIGHBYTES");
}

static void test_vera_has_fx(void)
{
    unsigned char fx = x16_vera_has_fx();
    t_check(fx == 0 || fx == 1, "VERA_HAS_FX");
}

/* ------------------------------------------------------------------ */
/* fixed point                                                         */
/* ------------------------------------------------------------------ */

/* umul16 returns a 32-bit long in btmp0. A shim that drops a high byte
** turns 0xFFFE0001 into something narrower. */
static void test_umul16_long_return(void)
{
    unsigned long big   = x16_umul16(65535u, 65535u);
    unsigned long mixed = x16_umul16(0x1234u, 0x5678u);   /* 0x06260060 */

    t_check(big == 0xFFFE0001UL && mixed == 0x06260060UL, "UMUL16_LONG");
}

/* mul88 is 8.8 fixed point, returns int: 2.5 * 2.0 = 5.0. */
static void test_mul88_int_return(void)
{
    t_check(x16_mul88(0x0280, 0x0200) == 0x0500, "MUL88_INT");
}

/* ------------------------------------------------------------------ */
/* collide                                                             */
/* ------------------------------------------------------------------ */

/* Eight char args: the entry reads four from r0/r2/r4/r6 and four off the
** soft stack, so a wrong slot lands a coordinate in the wrong axis. */
static void test_collide8_argorder(void)
{
    unsigned char hit  = x16_collide8(10, 20, 5, 5, 12, 22, 5, 5);
    unsigned char miss = x16_collide8(10, 20, 5, 5, 40, 22, 5, 5);
    unsigned char edge = x16_collide8(10, 20, 5, 5, 15, 20, 5, 5);

    t_check(hit == 1 && miss == 0 && edge == 0, "COLLIDE8_8ARGS");
}

static void test_collide16_pointers(void)
{
    static const x16_box16 a = { 300, 400, 50, 50 };
    static const x16_box16 b = { 320, 420, 50, 50 };
    static const x16_box16 c = { 900, 400, 50, 50 };

    t_check(x16_collide16(&a, &b) == 1 && x16_collide16(&a, &c) == 0,
            "COLLIDE16_PTRS");
}

/* ------------------------------------------------------------------ */
/* clip                                                                */
/* ------------------------------------------------------------------ */

/* clip_set takes four ints (r0/r1..r6/r7); clip_line one pointer. A
** transposed bound or endpoint moves the visible sub-segment. */
static void test_clip_argorder(void)
{
    x16_line seg;
    unsigned char visible;

    x16_clip_set(10, 20, 100, 200);
    seg.x0 = -50; seg.y0 = 100;                 /* enters from the left */
    seg.x1 =  50; seg.y1 = 100;
    visible = x16_clip_line(&seg);

    t_check(visible == 1 && seg.x0 == 10 && seg.y0 == 100 &&
            seg.x1 == 50 && seg.y1 == 100, "CLIP_SET_AND_LINE");
}

static void test_clip_outside(void)
{
    x16_line seg;
    unsigned char visible;

    x16_clip_set(10, 20, 100, 200);
    seg.x0 = 300; seg.y0 = 300;
    seg.x1 = 400; seg.y1 = 400;
    visible = x16_clip_line(&seg);

    t_check(visible == 0 && seg.x0 == 300 && seg.y0 == 300, "CLIP_OUTSIDE");
}

/* ------------------------------------------------------------------ */
/* float (ROM binding): every operand is a pointer                     */
/* ------------------------------------------------------------------ */

static void test_float_ptr_operands(void)
{
    x16_float a, b;
    char buf[X16_FP_STRLEN];
    unsigned char ok;

    x16_f_from_u8(200);                 /* fp_float is SIGNED: 200 != -56 */
    x16_f_store(a);
    x16_f_from_u8(50);
    x16_f_store(b);

    x16_f_load(a);
    x16_f_sub(b);                       /* 200 - 50, not 50 - 200 */
    x16_f_to_str_trim(buf);
    ok = buf[0] == '1' && buf[1] == '5' && buf[2] == '0' && buf[3] == 0;

    x16_f_load(a);
    ok = ok && (x16_f_cmp(b) == 1);
    x16_f_load(b);
    ok = ok && (x16_f_cmp(a) == -1);

    t_check(ok, "FLOAT_PTR_OPERANDS");
}

/* ------------------------------------------------------------------ */
/* zx0: two pointers in, a pointer out                                 */
/* ------------------------------------------------------------------ */

static const unsigned char zx0_packed[] = {
    0x15, 0xb8, 0x58, 0x31, 0x36, 0x4c, 0x49, 0x42, 0x2d, 0x44, 0x45, 0x43,
    0x4f, 0x4d, 0x50, 0x52, 0x45, 0x53, 0x53, 0x2d, 0x54, 0x45, 0x53, 0x54,
    0x21, 0xd0, 0x15, 0xd5, 0x55, 0x60
};
/* ASCII, spelled numerically: the packed stream decodes to raw ASCII
** bytes, but a char literal here would be charmapped to PETSCII by
** -cbmascii and never match. "X16LIB-DECOMPRESS-TEST!!" */
static const unsigned char zx0_phrase[24] = {
    0x58,0x31,0x36,0x4C,0x49,0x42,0x2D,0x44,0x45,0x43,0x4F,0x4D,
    0x50,0x52,0x45,0x53,0x53,0x2D,0x54,0x45,0x53,0x54,0x21,0x21
};
static unsigned char zx0_unpacked[97];

static void test_zx0_ptr_return(void)
{
    unsigned char *end;
    unsigned char i, r, ok = 1;

    zx0_unpacked[96] = 0x9D;                    /* poison, one past output */
    end = (unsigned char *)x16_zx0_decompress(zx0_packed, zx0_unpacked);

    for (r = 0; r < 4; ++r)
        for (i = 0; i < 24; ++i)
            if (zx0_unpacked[r * 24 + i] != zx0_phrase[i]) ok = 0;

    t_check(ok && end == zx0_unpacked + 96 && zx0_unpacked[96] == 0x9D,
            "ZX0_PTR_RETURN");
}

/* ------------------------------------------------------------------ */
/* palette                                                             */
/* ------------------------------------------------------------------ */

#define PAL_LO(i)   t_vpeek(X16_VRAM_PALETTE + (unsigned long)(i) * 2)
#define PAL_HI(i)   t_vpeek(X16_VRAM_PALETTE + (unsigned long)(i) * 2 + 1)

static void test_pal_set(void)
{
    x16_pal_set(5, 0x0A73);
    t_check(PAL_LO(5) == 0x73 && PAL_HI(5) == 0x0A, "PAL_SET");
}

/* Index 200 > 127: index*2 carries into the address high byte. */
static void test_pal_set_high_index(void)
{
    x16_pal_set(200, 0x0C41);
    t_check(PAL_LO(200) == 0x41 && PAL_HI(200) == 0x0C, "PAL_SET_HIGH_INDEX");
}

/* pal_load(src, first, count): src in r0/r1, first in r2, count in r4. */
static void test_pal_load(void)
{
    static const unsigned int src[3] = { 0x0111, 0x0222, 0x0333 };
    unsigned char ok;

    x16_pal_load(src, 100, 3);
    ok = PAL_LO(100) == 0x11 && PAL_HI(100) == 0x01 &&
         PAL_LO(101) == 0x22 && PAL_HI(101) == 0x02 &&
         PAL_LO(102) == 0x33 && PAL_HI(102) == 0x03;

    t_check(ok, "PAL_LOAD");
}

static void test_pal_load_zero(void)
{
    static const unsigned int src[1] = { 0xFFFF };

    x16_pal_set(150, 0x0555);
    x16_pal_load(src, 150, 0);
    t_check(PAL_LO(150) == 0x55 && PAL_HI(150) == 0x05, "PAL_LOAD_ZERO");
}

/* ------------------------------------------------------------------ */
/* mem: block ops                                                      */
/* ------------------------------------------------------------------ */

/* mem_fill(dst, count, value): dst pointer in r0/r1, count in r2/r3,
** value in r4. The poison byte one past the end must survive. */
static void test_mem_fill_argorder(void)
{
    static unsigned char buf[6];

    buf[4] = 0x11; buf[5] = 0x22;               /* poison */
    x16_mem_fill(buf, 5, 0xD7);

    t_check(buf[0] == 0xD7 && buf[4] == 0xD7 && buf[5] == 0x22,
            "MEM_FILL_PTR_INT_CHAR");
}

static void test_mem_copy_argorder(void)
{
    static const unsigned char src[4] = { 0x31, 0x41, 0x59, 0x26 };
    static unsigned char dst[5];

    dst[4] = 0x8A;                              /* poison */
    x16_mem_copy(src, dst, 4);

    t_check(dst[0] == 0x31 && dst[3] == 0x26 && dst[4] == 0x8A,
            "MEM_COPY_2PTR");
}

/* mem_crc returns int in a/x. Empty block is $FFFF (nonzero high byte, so
** a dropped X shows). */
static void test_mem_crc_int_return(void)
{
    static const unsigned char data[1] = { 0 };
    unsigned int empty = x16_mem_crc(data, 0);
    unsigned int one   = x16_mem_crc(data, 1);

    t_check(empty == 0xFFFF && one != 0xFFFF && (one >> 8) != 0,
            "MEM_CRC_INT_RETURN");
}

/* LZSA2 depacker: two pointers in, one past the output back in a/x. Uses
** the verified vector; the expected phrase is ASCII (numeric, so
** -cbmascii cannot corrupt it -- see zx0_phrase). */
static const unsigned char lzsa_packed[] = {
    0x3f, 0xf4, 0x06, 0x58, 0x31, 0x36, 0x4c, 0x49, 0x42, 0x2d, 0x44, 0x45,
    0x43, 0x4f, 0x4d, 0x50, 0x52, 0x45, 0x53, 0x53, 0x2d, 0x54, 0x45, 0x53,
    0x54, 0x21, 0x21, 0xff, 0x30, 0xe7, 0xe8
};
static unsigned char lzsa_unpacked[97];

static void test_mem_decompress_ptr_return(void)
{
    unsigned char *end;
    unsigned char i, r, ok = 1;

    lzsa_unpacked[96] = 0x5C;                   /* poison */
    end = (unsigned char *)x16_mem_decompress(lzsa_packed, lzsa_unpacked);

    for (r = 0; r < 4; ++r)
        for (i = 0; i < 24; ++i)
            if (lzsa_unpacked[r * 24 + i] != zx0_phrase[i]) ok = 0;

    t_check(ok && end == lzsa_unpacked + 96 && lzsa_unpacked[96] == 0x5C,
            "MEM_DECOMPRESS_PTR_RETURN");
}

/* ------------------------------------------------------------------ */
/* bank: banked-RAM access                                             */
/* ------------------------------------------------------------------ */

/* peek/poke(bank, offset, value): bank, both offset bytes and value all
** differ, so any confusion writes elsewhere. */
static void test_bank_peek_poke_argorder(void)
{
    x16_bank_poke(3, 0x0102, 0x7B);
    x16_bank_poke(4, 0x0102, 0x3C);             /* same offset, other bank */
    x16_bank_poke(3, 0x0201, 0x99);             /* offset bytes swapped */

    t_check(x16_bank_peek(3, 0x0102) == 0x7B &&
            x16_bank_peek(4, 0x0102) == 0x3C &&
            x16_bank_peek(3, 0x0201) == 0x99, "BANK_PEEK_POKE");
}

/* copy_far has FIVE args: the fifth (count) spills to the C soft stack. */
static void test_bank_copy_far_argorder(void)
{
    unsigned char i, ok = 1;

    x16_bank_set(1);
    for (i = 0; i < 4; ++i) {
        x16_bank_poke(1, 0x0100 + i, 0xA0 + i);
        x16_bank_poke(2, 0x0200 + i, 0x00);
    }
    x16_bank_poke(2, 0x0204, 0x5E);             /* poison */

    x16_bank_copy_far(1, 0x0100, 2, 0x0200, 4);

    for (i = 0; i < 4; ++i)
        if (x16_bank_peek(2, 0x0200 + i) != (unsigned char)(0xA0 + i)) ok = 0;
    if (x16_bank_peek(2, 0x0204) != 0x5E) ok = 0;

    t_check(ok, "BANK_COPY_FAR_5ARGS");
}

/* mem_to_bank(src, bank, offset, count) then bank_to_mem back. */
static void test_bank_mem_roundtrip(void)
{
    static const unsigned char src[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    static unsigned char dst[5];
    unsigned char ok;

    dst[4] = 0x77;                              /* poison */
    x16_mem_to_bank(src, 5, 0x0080, 4);
    x16_bank_to_mem(5, 0x0080, dst, 4);

    ok = dst[0] == 0xDE && dst[1] == 0xAD && dst[2] == 0xBE &&
         dst[3] == 0xEF && dst[4] == 0x77;
    t_check(ok, "BANK_MEM_ROUNDTRIP");
}

/* whole-bank allocator: lowest-first, free returns it, reserve claims. */
static void test_bank_alloc(void)
{
    unsigned char a, b, r1, r2;

    x16_bank_alloc_init(10, 12);
    a = x16_bank_alloc();               /* 10 */
    b = x16_bank_alloc();               /* 11 */
    x16_bank_free(a);                   /* 10 free again */
    r1 = x16_bank_reserve(11);          /* already taken -> 0 */
    r2 = x16_bank_reserve(10);          /* free -> 1 */

    t_check(a == 10 && b == 11 && r1 == 0 && r2 == 1, "BANK_ALLOC");
}

/* ------------------------------------------------------------------ */
/* sprite                                                              */
/* ------------------------------------------------------------------ */

/* pos(sprite, x, y) then get_pos: x and y differ and are both nonzero, so
** a transposed shim reports them the wrong way round. */
static void test_sprite_pos_roundtrip(void)
{
    unsigned int x = 0xEEEE, y = 0xEEEE;

    x16_sprite_init_all();
    x16_sprite_pos(3, 300, 200);
    x16_sprite_get_pos(3, &x, &y);

    t_check(x == 300 && y == 200, "SPRITE_POS_ROUNDTRIP");
}

/* Both out-params written, through DIFFERENT pointers. */
static void test_sprite_get_pos_both(void)
{
    unsigned int x = 1, y = 2;

    x16_sprite_pos(4, 0x0155, 0x00AA);
    x16_sprite_get_pos(4, &x, &y);

    t_check(x == 0x0155 && y == 0x00AA && x != y, "SPRITE_GET_POS_BOTH");
}

/* image(sprite, mode, addr): mode in r2, 32-bit addr in btmp0. The record
** stores address bits 16:5. Read the two bytes back via VERA_DATA0 (an
** independent path from the library's write -- but sprite_setptr points
** the port, so read it back through t_vpeek of the attr record). */
static void test_sprite_image_argorder(void)
{
    unsigned char lo, hi;

    x16_sprite_image(5, X16_SPRITE_8BPP, 0x14000UL);

    lo = t_vpeek(X16_VRAM_SPRITE_ATTR + 5 * 8 + 0);
    hi = t_vpeek(X16_VRAM_SPRITE_ATTR + 5 * 8 + 1);

    /* $14000 >> 5 = $0A00: low byte $00, high nibble $0A, plus 8bpp bit. */
    t_check(lo == 0x00 && (hi & 0x0F) == 0x0A && (hi & 0x80) != 0,
            "SPRITE_IMAGE_ARGORDER");
}

/* ------------------------------------------------------------------ */
/* tile                                                                */
/* ------------------------------------------------------------------ */

/* tile_put(col, row, code, attr) then tile_get: four distinct bytes, so a
** confused shim writes the wrong cell or the wrong halves. */
static void test_tile_put_get(void)
{
    unsigned int cell;

    x16_screen_set_mode(0);             /* known 80x60 text layout */
    x16_tile_put(10, 5, 0x41, 0x62);
    cell = x16_tile_get(10, 5);

    t_check(X16_TILE_CODE(cell) == 0x41 && X16_TILE_ATTR(cell) == 0x62,
            "TILE_PUT_GET");
}

/* A different cell, to prove col/row are not swapped: (5,10) must not
** read back what (10,5) held. */
static void test_tile_colrow(void)
{
    unsigned int a, b;

    x16_tile_put(10, 5, 0x11, 0x22);
    x16_tile_put(5, 10, 0x33, 0x44);
    a = x16_tile_get(10, 5);
    b = x16_tile_get(5, 10);

    t_check(X16_TILE_CODE(a) == 0x11 && X16_TILE_CODE(b) == 0x33,
            "TILE_COLROW");
}

/* ------------------------------------------------------------------ */
/* dos: command channel                                                */
/* ------------------------------------------------------------------ */

/* dos_msg() returns const char* (in a/x). The reply always begins with
** two decimal digits: "00,OK,00,00". */
static void test_dos_msg_ptr_return(void)
{
    const char *msg;
    unsigned char code;

    code = x16_dos_status();
    msg  = x16_dos_msg();

    t_check(msg != 0 &&
            msg[0] >= '0' && msg[0] <= '9' &&
            msg[1] >= '0' && msg[1] <= '9' &&
            msg[2] == ',' && code < 20, "DOS_MSG_PTR_RETURN");
}

/* ------------------------------------------------------------------ */
/* load/save: file round-trip on the SD image                          */
/* ------------------------------------------------------------------ */

/* save then load, checking the six-arg load and its *end out-param. */
static void test_fs_roundtrip_and_end(void)
{
    static const char name[] = "ABITEST.BIN";
    static unsigned char out[4] = { 0x11, 0x22, 0x33, 0x44 };
    static unsigned char in[6];
    unsigned int end = 0;
    unsigned char saved, loaded, ok;

    in[4] = 0x77;                               /* poison */

    saved  = x16_fs_save(name, sizeof name - 1, X16_DEVICE_SD, out, out + 4);
    loaded = x16_fs_load(name, sizeof name - 1, X16_DEVICE_SD, X16_SA_ADDR,
                         in, &end);
    x16_dos_delete(name, sizeof name - 1);

    ok = (saved == 0) && (loaded == 0) &&
         in[0] == 0x11 && in[3] == 0x44 && in[4] == 0x77 &&
         end == (unsigned int)(in + 4);

    t_check(ok, "FS_ROUNDTRIP_AND_END");
}

/* end may be NULL: the shim must not store through it. */
static void test_fs_load_null_end(void)
{
    static const char name[] = "ABITEST2.BIN";
    static unsigned char out[2] = { 0xA1, 0xB2 };
    static unsigned char in[2];
    unsigned char saved, loaded;

    saved  = x16_fs_save(name, sizeof name - 1, X16_DEVICE_SD, out, out + 2);
    loaded = x16_fs_load(name, sizeof name - 1, X16_DEVICE_SD, X16_SA_ADDR,
                         in, (unsigned int *)0);
    x16_dos_delete(name, sizeof name - 1);

    t_check(saved == 0 && loaded == 0 && in[0] == 0xA1 && in[1] == 0xB2,
            "FS_LOAD_NULL_END");
}

/* ------------------------------------------------------------------ */
/* bmx: struct block copy through a pointer                             */
/* ------------------------------------------------------------------ */

static void test_bmx_info_roundtrip(void)
{
    x16_bmx_info in, back;

    in.width = 320; in.height = 240; in.bpp = 8; in.palstart = 17;
    in.palcount = 200; in.border = 5; in.stride = 320;
    x16_bmx_set_info(&in);

    back.width = 0; back.height = 0; back.bpp = 0; back.palstart = 0;
    back.palcount = 0; back.border = 0; back.stride = 0;
    x16_bmx_get_info(&back);

    t_check(back.width == 320 && back.height == 240 && back.bpp == 8 &&
            back.palstart == 17 && back.palcount == 200 &&
            back.border == 5 && back.stride == 320, "BMX_INFO_ROUNDTRIP");
}

/* ------------------------------------------------------------------ */
/* verafx: hardware multiply and blits                                 */
/* ------------------------------------------------------------------ */

/* fx_mult(int a, int b) -> long. Signed 16x16->32; the result exercises
** the a/r0/r1, b/r2/r3 pairing and the long return in btmp0. */
static void test_fx_mult(void)
{
    long p, n;

    if (!x16_vera_has_fx()) { t_skip("FX_MULT"); return; }

    p = x16_fx_mult(1000, 1000);        /* 1000000 = 0x000F4240 */
    n = x16_fx_mult(-3, 100);           /* -300 = 0xFFFFFED4 */

    t_check(p == 1000000L && n == -300L, "FX_MULT");
}

/* fx_copy(long src, long dst, int count): two longs (btmp0/btmp1) plus an
** int (r0/r1). Byte-for-byte VRAM blit with a poison byte one past. */
static void test_fx_copy(void)
{
    unsigned char i, ok = 1;

    if (!x16_vera_has_fx()) { t_skip("FX_COPY"); return; }

    for (i = 0; i < 8; ++i) {
        t_vpoke(0x60 + i, TESTVRAM + i);
        t_vpoke(0x00,     TESTVRAM + 0x80 + i);
    }
    t_vpoke(0x4D, TESTVRAM + 0x88);             /* poison */

    x16_fx_copy(TESTVRAM, TESTVRAM + 0x80, 8);

    for (i = 0; i < 8; ++i)
        if (t_vpeek(TESTVRAM + 0x80 + i) != (unsigned char)(0x60 + i)) ok = 0;
    if (t_vpeek(TESTVRAM + 0x88) != 0x4D) ok = 0;

    t_check(ok, "FX_COPY");
}

/* fx_triangle(a*, b*, c*, color): three vertex pointers (r0/r1, r2/r3,
** r4/r5) plus a colour (r6). Requires FX and the bitmap mode. */
static void test_fx_triangle(void)
{
    static const x16_point a = { 10, 10 };
    static const x16_point b = { 60, 10 };
    static const x16_point c = { 10, 40 };
    unsigned char inside, outside;

    if (!x16_vera_has_fx()) { t_skip("FX_TRIANGLE"); return; }
    if (!x16_gfx_init())    { t_skip("FX_TRIANGLE"); return; }

    x16_gfx_clear(0);
    x16_fx_triangle(&a, &b, &c, 0x2A);

    inside  = t_vpeek(X16_VRAM_BITMAP + 15UL * 320 + 15);
    outside = t_vpeek(X16_VRAM_BITMAP + 35UL * 320 + 55);

    x16_screen_reset();
    t_check(inside == 0x2A && outside == 0, "FX_TRIANGLE");
}

/* ------------------------------------------------------------------ */
/* bitmap: 320x240x256 drawing                                         */
/* ------------------------------------------------------------------ */

/* pset(x, y, color): x in r0/r1, y in r2, color in r4. Written into the
** bitmap plane at $00000, read back via t_vpeek at y*320+x -- an
** independent path. No mode switch needed: only VRAM is touched. */
static void test_gfx_pset(void)
{
    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP + 20UL * 320 + 50);
    x16_vera_fill(0x00, 4);                     /* clear the target cells */

    x16_gfx_pset(50, 20, 0x8C);
    x16_gfx_pset(52, 20, 0x8C);

    t_check(t_vpeek(X16_VRAM_BITMAP + 20UL * 320 + 50) == 0x8C &&
            t_vpeek(X16_VRAM_BITMAP + 20UL * 320 + 51) == 0x00 &&
            t_vpeek(X16_VRAM_BITMAP + 20UL * 320 + 52) == 0x8C,
            "GFX_PSET");
}

/* gfx_text(x, y, color, s): the string pointer is the LAST arg (r6/r7).
** Two different glyphs, both with lit pixels; a swapped pointer draws the
** same in both cells, a swapped y/color draws nothing. */
#define PIXEL(px, py)   (X16_VRAM_BITMAP + (unsigned long)(py) * 320 + (px))
static void test_gfx_text(void)
{
    unsigned char i, j, a = 0, b = 0;
    static const char ab[] = { 0x41, 0x42, 0 };  /* "AB" in ASCII, numeric */

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 12800);

    x16_gfx_text(100, 8, 0x96, ab);

    for (i = 0; i < 8; ++i)
        for (j = 0; j < 8; ++j) {
            if (t_vpeek(PIXEL(100 + j, 8 + i)) == 0x96) ++a;
            if (t_vpeek(PIXEL(108 + j, 8 + i)) == 0x96) ++b;
        }

    t_check(a > 4 && b > 4 && a != b, "GFX_TEXT_PTR_LAST");
}

/* ------------------------------------------------------------------ */
/* psg: the shared marshalling helpers                                 */
/* ------------------------------------------------------------------ */

/* set_freq(voice, freq) writes the two-byte frequency to a voice's VRAM
** register; read it back through t_vpeek. voice in r0, freq in r2/r3. */
static void test_psg_set_freq(void)
{
    x16_psg_init();
    x16_psg_set_freq(3, 0x1234);

    /* Voice 3's frequency word is at $1F9C0 + 3*4, low byte first. */
    t_check(t_vpeek(0x1F9C0UL + 3 * 4 + 0) == 0x34 &&
            t_vpeek(0x1F9C0UL + 3 * 4 + 1) == 0x12, "PSG_SET_FREQ");
}

/* ------------------------------------------------------------------ */
/* gfx2: 640x480 at 2bpp                                              */
/* ------------------------------------------------------------------ */
/* A framebuffer byte sits at y*160 + (x>>2), 4 pixels per byte,
** MSB-first. Every gfx2 coordinate is a 16-bit int, so x fills r0/r1
** and y r2/r3; rect, frame, line and blit are wide enough to push their
** last argument onto the C soft stack, which is where a shim is most
** likely to be silently wrong -- so each of those is checked here.
** These run last: x16_gfx2_init() reprograms the display and palette.
*/
#define G2ROW(y)  ((unsigned long)(y) * 160)

static const unsigned char g2_pat_half[8] = {
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0
};
static const unsigned char g2_img[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
static const unsigned char g2_mcol[8] = {       /* (mask,data) x 4 rows */
    0x0F, 0x50, 0x0F, 0x50, 0x0F, 0x50, 0x0F, 0x50
};

static void test_g2_init(void)
{
    x16_gfx2_init();
    t_check(*(volatile unsigned char *)0x9F2D == 0x05 &&    /* bitmap|2bpp */
            *(volatile unsigned char *)0x9F2F == 0x01,      /* $00000/640 */
            "G2_INIT");
}

static void test_g2_pset_read(void)
{
    t_vpoke(0x00, G2ROW(10) + 1);
    x16_gfx2_pset(5, 10, 2);                    /* byte 1, pixel 1 */
    t_check(t_vpeek(G2ROW(10) + 1) == 0x20 &&
            x16_gfx2_read(5, 10) == 2 &&
            x16_gfx2_read(6, 10) == 0 &&
            x16_gfx2_read(640, 10) == 0xFF,     /* off screen */
            "G2_PSET_READ");
}

/* Unclipped, (640,0) would land at byte 160 and (0,480) at 76,800. */
static void test_g2_clip(void)
{
    t_vpoke(0x11, 160UL);
    t_vpoke(0x22, 76800UL);
    x16_gfx2_pset(640, 0, 3);
    x16_gfx2_pset(0, 480, 3);
    t_check(t_vpeek(160UL) == 0x11 && t_vpeek(76800UL) == 0x22, "G2_CLIP");
}

/* setptr's byte argument shares registers with a 16-bit pair, and
** increment 0 keeps the port still, so two writes hit one byte.
*/
static void test_g2_setptr(void)
{
    unsigned char phase;

    t_vpoke(0x00, G2ROW(7) + 80);
    phase = x16_gfx2_setptr(0, 322, 7);         /* byte 80 of row 7, px 2 */
    *(volatile unsigned char *)0x9F23 = 0x5A;
    *(volatile unsigned char *)0x9F23 = 0xA5;
    t_check(phase == 2 && t_vpeek(G2ROW(7) + 80) == 0xA5, "G2_SETPTR");
}

/* x=5 len=13: head = byte 1 pixels 1-3, middle bytes 2-3, tail = byte 4
** pixels 0-1. The bytes either side must survive.
*/
static void test_g2_hline(void)
{
    unsigned char i;

    for (i = 0; i < 6; ++i) {
        t_vpoke(0x00, G2ROW(20) + i);
    }
    x16_gfx2_hline(5, 20, 13, 3);
    t_check(t_vpeek(G2ROW(20) + 0) == 0x00 &&
            t_vpeek(G2ROW(20) + 1) == 0x3F &&
            t_vpeek(G2ROW(20) + 2) == 0xFF &&
            t_vpeek(G2ROW(20) + 3) == 0xFF &&
            t_vpeek(G2ROW(20) + 4) == 0xF0 &&
            t_vpeek(G2ROW(20) + 5) == 0x00,
            "G2_HLINE");
}

/* Colour 0 ink onto $FF: proves the column really is read-modify-write. */
static void test_g2_vline(void)
{
    unsigned char i;

    for (i = 30; i <= 34; ++i) {
        t_vpoke(0xFF, G2ROW(i) + 1);
    }
    x16_gfx2_vline(6, 30, 4, 0);                /* byte 1, pixel 2 */
    t_check(t_vpeek(G2ROW(30) + 1) == 0xF3 &&
            t_vpeek(G2ROW(33) + 1) == 0xF3 &&
            t_vpeek(G2ROW(34) + 1) == 0xFF,     /* one past the end */
            "G2_VLINE");
}

/* rect's colour is the fifth argument: it rides the soft stack. */
static void test_g2_rect(void)
{
    unsigned char i;

    for (i = 0; i < 4; ++i) {
        t_vpoke(0x00, G2ROW(40) + i);
        t_vpoke(0x00, G2ROW(41) + i);
        t_vpoke(0x00, G2ROW(42) + i);
    }
    x16_gfx2_rect(4, 40, 8, 2, 1);
    t_check(t_vpeek(G2ROW(40) + 0) == 0x00 &&
            t_vpeek(G2ROW(40) + 1) == 0x55 &&
            t_vpeek(G2ROW(40) + 2) == 0x55 &&
            t_vpeek(G2ROW(40) + 3) == 0x00 &&
            t_vpeek(G2ROW(41) + 1) == 0x55 &&
            t_vpeek(G2ROW(42) + 1) == 0x00,
            "G2_RECT");
}

static void test_g2_frame(void)
{
    unsigned char i;

    for (i = 0; i < 4; ++i) {
        t_vpoke(0x00, G2ROW(50) + i);
        t_vpoke(0x00, G2ROW(51) + i);
        t_vpoke(0x00, G2ROW(52) + i);
    }
    x16_gfx2_frame(0, 50, 16, 3, 3);
    t_check(t_vpeek(G2ROW(50) + 0) == 0xFF &&   /* top edge */
            t_vpeek(G2ROW(50) + 3) == 0xFF &&
            t_vpeek(G2ROW(51) + 0) == 0xC0 &&   /* left edge only */
            t_vpeek(G2ROW(51) + 1) == 0x00 &&   /* hollow */
            t_vpeek(G2ROW(51) + 3) == 0x03 &&   /* right edge only */
            t_vpeek(G2ROW(52) + 2) == 0xFF,     /* bottom edge */
            "G2_FRAME");
}

/* The 45-degree diagonal, down-right: four words in registers and the
** colour on the stack.
*/
static void test_g2_line(void)
{
    unsigned char i;

    for (i = 60; i <= 67; ++i) {
        t_vpoke(0x00, G2ROW(i));
        t_vpoke(0x00, G2ROW(i) + 1);
    }
    x16_gfx2_line(0, 60, 7, 67, 3);
    t_check(t_vpeek(G2ROW(60)) == 0xC0 &&
            t_vpeek(G2ROW(63)) == 0x03 &&
            t_vpeek(G2ROW(64) + 1) == 0xC0 &&
            t_vpeek(G2ROW(67) + 1) == 0x03,
            "G2_LINE");
}

/* Pattern $F0 with fg 3: even bytes $FF, odd bytes $00. Patterns anchor
** to the screen, so the x=2 fill gets a phase-2 head and the even byte
** again in its tail.
*/
static void test_g2_pattern(void)
{
    unsigned char i;

    for (i = 0; i < 5; ++i) {
        t_vpoke(0x55, G2ROW(70) + i);
    }
    for (i = 0; i < 4; ++i) {
        t_vpoke(0x00, G2ROW(74) + i);
    }
    x16_gfx2_pattern_set(g2_pat_half, 0x03);    /* bg 0, fg 3 */
    x16_gfx2_pattern_rect(0, 70, 16, 1);
    x16_gfx2_pattern_rect(2, 74, 8, 1);
    t_check(t_vpeek(G2ROW(70) + 0) == 0xFF &&
            t_vpeek(G2ROW(70) + 1) == 0x00 &&
            t_vpeek(G2ROW(70) + 2) == 0xFF &&
            t_vpeek(G2ROW(70) + 3) == 0x00 &&
            t_vpeek(G2ROW(70) + 4) == 0x55 &&   /* untouched */
            t_vpeek(G2ROW(74) + 0) == 0x0F &&   /* phase-2 head */
            t_vpeek(G2ROW(74) + 1) == 0x00 &&
            t_vpeek(G2ROW(74) + 2) == 0xF0,     /* tail */
            "G2_PATTERN");
}

/* blit's op is the sixth argument, on the soft stack: copy it down,
** then XOR the same image away again.
*/
static void test_g2_blit(void)
{
    t_vpoke(0x00, G2ROW(80) + 2);
    t_vpoke(0x00, G2ROW(80) + 3);
    t_vpoke(0x00, G2ROW(81) + 2);
    t_vpoke(0x00, G2ROW(81) + 3);
    x16_gfx2_blit(8, 80, 2, 2, g2_img, 0);
    if (t_vpeek(G2ROW(80) + 2) != 0xDE || t_vpeek(G2ROW(80) + 3) != 0xAD ||
        t_vpeek(G2ROW(81) + 2) != 0xBE || t_vpeek(G2ROW(81) + 3) != 0xEF) {
        t_check(0, "G2_BLIT");
        return;
    }
    x16_gfx2_blit(8, 80, 2, 2, g2_img, 3);
    t_check(t_vpeek(G2ROW(80) + 2) == 0x00 &&
            t_vpeek(G2ROW(81) + 3) == 0x00,
            "G2_BLIT");
}

/* Onto $FF: keep pixels 2-3 (mask $0F), ink pixels 0-1 with colour 1
** (data $50) -> every touched byte reads $5F.
*/
static void test_g2_blitm(void)
{
    unsigned char i;

    for (i = 90; i <= 94; ++i) {
        t_vpoke(0xFF, G2ROW(i) + 3);
    }
    x16_gfx2_blitm(12, 90, 4, 1, g2_mcol);
    t_check(t_vpeek(G2ROW(90) + 3) == 0x5F &&
            t_vpeek(G2ROW(93) + 3) == 0x5F &&
            t_vpeek(G2ROW(94) + 3) == 0xFF,     /* one past the end */
            "G2_BLITM");
}

/* Exactly the 76,800 framebuffer bytes and not one more. FX only. */
static void test_g2_clear(void)
{
    t_vpoke(0x77, 76800UL);
    x16_gfx2_clear(2);
    t_check(t_vpeek(0UL) == 0xAA &&
            t_vpeek(38400UL) == 0xAA &&         /* the second fill half */
            t_vpeek(76799UL) == 0xAA &&
            t_vpeek(76800UL) == 0x77,           /* the sentinel */
            "G2_CLEAR");
}

int main(void)
{
    t_init();

    test_zp_base();

    test_math_sin();
    test_math_sinu();
    test_math_atan2();
    test_math_lerp();
    test_math_rnd();

    test_screen_color();
    test_screen_cursor_roundtrip();
    test_screen_get_cursor_both();
    test_screen_mode();

    test_rb_int_minus_one();
    test_stk_int_minus_one();
    test_buffers_order();

    test_vera_fill();
    test_vera_fill_count();
    test_vera_fill_zero();
    test_vera_copy();
    test_vera_addr_highbytes();
    test_vera_has_fx();

    test_umul16_long_return();
    test_mul88_int_return();

    test_collide8_argorder();
    test_collide16_pointers();

    test_clip_argorder();
    test_clip_outside();

    test_float_ptr_operands();

    test_zx0_ptr_return();

    test_pal_set();
    test_pal_set_high_index();
    test_pal_load();
    test_pal_load_zero();

    test_mem_fill_argorder();
    test_mem_copy_argorder();
    test_mem_crc_int_return();
    test_mem_decompress_ptr_return();

    test_bank_peek_poke_argorder();
    test_bank_copy_far_argorder();
    test_bank_mem_roundtrip();
    test_bank_alloc();

    test_sprite_pos_roundtrip();
    test_sprite_get_pos_both();
    test_sprite_image_argorder();

    test_tile_put_get();
    test_tile_colrow();

    test_dos_msg_ptr_return();
    test_fs_roundtrip_and_end();
    test_fs_load_null_end();
    test_bmx_info_roundtrip();

    test_fx_mult();
    test_fx_copy();
    test_fx_triangle();

    test_gfx_pset();
    test_gfx_text();

    test_psg_set_freq();

    /* Last: x16_gfx2_init() reprograms the display and palette 0-3. */
    test_g2_init();
    test_g2_pset_read();
    test_g2_clip();
    test_g2_setptr();
    test_g2_hline();
    test_g2_vline();
    test_g2_rect();
    test_g2_frame();
    test_g2_line();
    test_g2_pattern();
    test_g2_blit();
    test_g2_blitm();
    if (x16_vera_has_fx()) {
        test_g2_clear();
    } else {
        t_skip("G2_CLEAR");
    }

    t_done();
    return 0;
}
