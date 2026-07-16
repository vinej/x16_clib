/* =====================================================================
 * x16clib :: test_llvm/runner2.c -- the ABI half of the llvm-mos suite
 * =====================================================================
 * The internal routines in src_llvm are mechanical translations of
 * src_ca65, and 158 tests on the cc65 side already prove them. What is
 * new here, and what nothing else covers, is the ~130 hand-written C
 * entry points: llvm-mos places arguments and return values nothing like
 * cc65 does, and a shim that reads the wrong register does not crash --
 * it quietly computes the wrong thing.
 *
 * So every test below is chosen to fail if ANY argument is transposed:
 * the values are distinct, none is zero, and where a routine writes
 * memory the cell past the end is poisoned.
 *
 * Three placement rules are under test throughout:
 *   - integer bytes fill A, X, then __rc2 upward;
 *   - a POINTER takes a whole aligned __rc pair and consumes no A/X;
 *   - a POINTER RETURN comes back in __rc2/__rc3, not A/X.
 * ===================================================================== */

#include "testlib.h"
#include <cx16.h>
#include <x16/x16.h>

#define TESTVRAM    0x08000UL

/* The byte in the 320x240x256 bitmap plane at (x, y). */
#define PIXEL(x, y) (X16_VRAM_BITMAP + (unsigned long)(y) * 320 + (x))

/* ------------------------------------------------------------------ */
/* wide integer argument lists                                         */
/* ------------------------------------------------------------------ */

/* Eight byte arguments: A, X, __rc2 .. __rc7. Boxes chosen so that any
** transposition of any pair flips the answer.
*/
static void test_collide8_argorder(void)
{
    /* a = (10,20,5,5)  b = (12,22,5,5): overlap on both axes */
    unsigned char hit  = x16_collide8(10, 20, 5, 5, 12, 22, 5, 5);
    /* b moved right so only the y axis overlaps */
    unsigned char miss = x16_collide8(10, 20, 5, 5, 40, 22, 5, 5);
    /* edges that merely touch do not overlap */
    unsigned char edge = x16_collide8(10, 20, 5, 5, 15, 20, 5, 5);

    t_check(hit == 1 && miss == 0 && edge == 0, "ABI_COLLIDE8_8ARGS");
}

/* Two pointers -> __rc2/__rc3 and __rc4/__rc5, no A/X. */
static void test_collide16_pointers(void)
{
    static const x16_box16 a = { 300, 400, 50, 50 };
    static const x16_box16 b = { 320, 420, 50, 50 };
    static const x16_box16 c = { 900, 400, 50, 50 };

    t_check(x16_collide16(&a, &b) == 1 && x16_collide16(&a, &c) == 0,
            "ABI_COLLIDE16_PTRS");
}

/* Seven integer bytes: src_bank -> A, src_offset -> X/__rc2,
** dst_bank -> __rc3, dst_offset -> __rc4/__rc5, count -> __rc6/__rc7.
** Every one is distinct, and the byte past the run is poisoned.
*/
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

    for (i = 0; i < 4; ++i) {
        if (x16_bank_peek(2, 0x0200 + i) != (unsigned char)(0xA0 + i)) { ok = 0; }
    }
    if (x16_bank_peek(2, 0x0204) != 0x5E) { ok = 0; }

    t_check(ok, "ABI_BANK_COPY_FAR_7ARGS");
}

/* bank_peek(bank, offset) and bank_poke(bank, offset, value): the bank,
** the two offset bytes and the value are all different, so a shim that
** confuses any of them writes somewhere else.
*/
static void test_bank_peek_poke_argorder(void)
{
    x16_bank_poke(3, 0x0102, 0x7B);
    x16_bank_poke(4, 0x0102, 0x3C);             /* same offset, other bank */
    x16_bank_poke(3, 0x0201, 0x99);             /* offset bytes swapped */

    t_check(x16_bank_peek(3, 0x0102) == 0x7B &&
            x16_bank_peek(4, 0x0102) == 0x3C &&
            x16_bank_peek(3, 0x0201) == 0x99,
            "ABI_BANK_PEEK_POKE");
}

/* ------------------------------------------------------------------ */
/* pointer arguments                                                   */
/* ------------------------------------------------------------------ */

/* mem_fill(dst, count, value): dst is a pointer -> __rc2/__rc3, so the
** integers fill A, X and then __rc4 -- NOT __rc2. Getting that wrong
** fills with the pointer's high byte.
*/
static void test_mem_fill_argorder(void)
{
    static unsigned char buf[6];

    buf[4] = 0x11;
    buf[5] = 0x22;                              /* poison */
    x16_mem_fill(buf, 5, 0xD7);

    t_check(buf[0] == 0xD7 && buf[4] == 0xD7 && buf[5] == 0x22,
            "ABI_MEM_FILL_PTR_INT_CHAR");
}

/* mem_copy(src, dst, count): two pointers then one integer. */
static void test_mem_copy_argorder(void)
{
    static const unsigned char src[4] = { 0x31, 0x41, 0x59, 0x26 };
    static unsigned char dst[5];

    dst[4] = 0x8A;                              /* poison */
    x16_mem_copy(src, dst, 4);

    t_check(dst[0] == 0x31 && dst[3] == 0x26 && dst[4] == 0x8A,
            "ABI_MEM_COPY_2PTR");
}

/* mem_crc returns an int: A = low, X = high. CRC-16/IBM-3740 of an empty
** block is the algorithm's init value, $FFFF -- a nonzero high byte, so a
** shim that drops X is caught.
*/
static void test_mem_crc_int_return(void)
{
    static const unsigned char data[1] = { 0 };
    unsigned int empty = x16_mem_crc(data, 0);
    unsigned int one   = x16_mem_crc(data, 1);

    t_check(empty == 0xFFFF && one != 0xFFFF && (one >> 8) != 0,
            "ABI_MEM_CRC_INT_RETURN");
}

/* ------------------------------------------------------------------ */
/* pointer RETURNS -- these come back in __rc2/__rc3, not A/X          */
/* ------------------------------------------------------------------ */

/* The phrase both depackers below reproduce, four times over. These are
** the same verified vectors the cc65 suite uses -- do not hand-write a
** compressed block, an invalid stream sends the depacker off the rails
** rather than failing cleanly.
*/
static const unsigned char lzsa_packed[] = {
    0x3f, 0xf4, 0x06, 0x58, 0x31, 0x36, 0x4c, 0x49, 0x42, 0x2d, 0x44, 0x45,
    0x43, 0x4f, 0x4d, 0x50, 0x52, 0x45, 0x53, 0x53, 0x2d, 0x54, 0x45, 0x53,
    0x54, 0x21, 0x21, 0xff, 0x30, 0xe7, 0xe8
};
static const unsigned char zx0_packed[] = {
    0x15, 0xb8, 0x58, 0x31, 0x36, 0x4c, 0x49, 0x42, 0x2d, 0x44, 0x45, 0x43,
    0x4f, 0x4d, 0x50, 0x52, 0x45, 0x53, 0x53, 0x2d, 0x54, 0x45, 0x53, 0x54,
    0x21, 0xd0, 0x15, 0xd5, 0x55, 0x60
};
static const unsigned char phrase[24] = {
    'X','1','6','L','I','B','-','D','E','C','O','M','P','R','E','S','S',
    '-','T','E','S','T','!','!'
};
static unsigned char unpacked[97];

/* The LZSA2 depacker returns one past the last output byte. If the shim
** left the address in A/X the caller reads whatever __rc2 held, which is
** usually a plausible-looking pointer into the same page -- so compare
** against the exact expected address, not merely "non-NULL".
*/
static void test_mem_decompress_ptr_return(void)
{
    unsigned char *end;
    unsigned char i, r, ok = 1;

    unpacked[96] = 0x5C;                        /* poison, one past the output */
    end = (unsigned char *)x16_mem_decompress(lzsa_packed, unpacked);

    for (r = 0; r < 4; ++r) {
        for (i = 0; i < 24; ++i) {
            if (unpacked[r * 24 + i] != phrase[i]) { ok = 0; }
        }
    }
    t_check(ok && end == unpacked + 96 && unpacked[96] == 0x5C,
            "ABI_MEM_DECOMPRESS_PTR_RETURN");
}

/* x16_dos_msg() returns const char *. Its address is a link-time
** constant inside the library, so the only way to check it is that the
** pointer is sane AND the bytes behind it are the drive's reply. Ask for
** the status first so there is something there.
*/
static void test_dos_msg_ptr_return(void)
{
    const char *msg;
    unsigned char code;

    code = x16_dos_status();
    msg  = x16_dos_msg();

    /* The reply always begins with two decimal digits: "00,OK,00,00". */
    t_check(msg != 0 &&
            msg[0] >= '0' && msg[0] <= '9' &&
            msg[1] >= '0' && msg[1] <= '9' &&
            msg[2] == ',' &&
            code < 20,
            "ABI_DOS_MSG_PTR_RETURN");
}

/* ------------------------------------------------------------------ */
/* 32-bit returns: A, X, __rc2, __rc3                                  */
/* ------------------------------------------------------------------ */

/* 65535 * 65535 = 0xFFFE0001. All four bytes differ, so a shim that
** misplaces any of them is caught. cc65 put the top half in its sreg.
*/
static void test_umul16_long_return(void)
{
    unsigned long big   = x16_umul16(65535u, 65535u);
    unsigned long mixed = x16_umul16(0x1234u, 0x5678u);   /* = 0x06260060 */

    t_check(big == 0xFFFE0001UL && mixed == 0x06260060UL,
            "ABI_UMUL16_LONG_RETURN");
}

/* mul88 is 8.8 fixed point and returns an int: 2.5 * 2.0 = 5.0. */
static void test_mul88_int_return(void)
{
    int r = x16_mul88(0x0280, 0x0200);

    t_check(r == 0x0500, "ABI_MUL88_INT_RETURN");
}

/* ------------------------------------------------------------------ */
/* int returns that must sign-extend, and -1 sentinels                 */
/* ------------------------------------------------------------------ */

/* rb_get() returns int, -1 when empty. A shim that forgets the high byte
** returns 255, which compares unequal to -1 -- and equal to a stored 255.
*/
static void test_rb_int_minus_one(void)
{
    int empty, got;

    x16_rb_init();
    empty = x16_rb_get();

    x16_rb_put(0xFF);                           /* a byte that LOOKS like -1 */
    got = x16_rb_get();

    t_check(empty == -1 && got == 255, "ABI_RB_GET_MINUS_ONE");
}

static void test_stk_int_minus_one(void)
{
    int empty, got;

    x16_stk_init();
    empty = x16_stk_pop();
    x16_stk_push(0xFF);
    got = x16_stk_pop();

    t_check(empty == -1 && got == 255, "ABI_STK_POP_MINUS_ONE");
}

/* sin8 returns signed char. sin8(192) is -127; a shim that returns it
** unsigned gives 129.
*/
static void test_sin8_signed_return(void)
{
    signed char q1 = x16_sin8(64);              /* +127 */
    signed char q3 = x16_sin8(192);             /* -127 */
    int promoted   = x16_sin8(192);

    t_check(q1 == 127 && q3 == -127 && promoted == -127,
            "ABI_SIN8_SIGNED_RETURN");
}

/* ------------------------------------------------------------------ */
/* out-parameters                                                      */
/* ------------------------------------------------------------------ */

/* sprite_pos then sprite_get_pos: two pointers out, in __rc2/__rc3 and
** __rc4/__rc5. x and y differ, and both differ from their sentinels.
*/
static void test_sprite_pos_roundtrip(void)
{
    unsigned int x = 0xEEEE, y = 0xEEEE;

    x16_sprite_init_all();
    x16_sprite_pos(3, 300, 200);
    x16_sprite_get_pos(3, &x, &y);

    t_check(x == 300 && y == 200, "ABI_SPRITE_POS_ROUNDTRIP");
}

/* Both out-params must be written, through DIFFERENT pointers. */
static void test_sprite_get_pos_both(void)
{
    unsigned int x = 1, y = 2;

    x16_sprite_pos(4, 0x0155, 0x00AA);
    x16_sprite_get_pos(4, &x, &y);

    t_check(x == 0x0155 && y == 0x00AA && x != y, "ABI_SPRITE_GET_POS_BOTH");
}

/* sprite_image(sprite, mode, addr): A, X, then a 32-bit address in
** __rc2..__rc5. The record stores address bits 16:5.
*/
static void test_sprite_image_argorder(void)
{
    unsigned char lo, hi;

    x16_sprite_setptr(5, 0);
    x16_sprite_image(5, X16_SPRITE_8BPP, 0x14000UL);

    /* Read the two address bytes back through VERA's own port. */
    x16_sprite_setptr(5, 0);
    lo = VERA.data0;
    hi = VERA.data0;

    /* $14000 >> 5 = $0A00: low byte $00, high byte $0A, plus the 8bpp bit. */
    t_check(lo == 0x00 && (hi & 0x0F) == 0x0A && (hi & 0x80) != 0,
            "ABI_SPRITE_IMAGE_ARGORDER");
}

/* ------------------------------------------------------------------ */
/* the pointer that hides among integers                               */
/* ------------------------------------------------------------------ */

/* gfx_text(x, y, color, s): `s` is last in the declaration but FIRST in
** the __rc space -- it takes __rc2/__rc3, pushing y and color out to
** __rc4 and __rc5. This is the single most confusing shim in the port.
*/
static void test_gfx_text_ptr_last(void)
{
    unsigned char i, a = 0, b = 0;

    /* Same setup as the cc65 suite: no mode switch, just a cleared
    ** bitmap plane, so this test isolates the shim and nothing else.
    */
    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 12800);

    x16_gfx_text(100, 8, 0x96, "AB");

    for (i = 0; i < 8; ++i) {
        unsigned char j;
        for (j = 0; j < 8; ++j) {
            if (vpeek(PIXEL(100 + j, 8 + i)) == 0x96) { ++a; }
            if (vpeek(PIXEL(108 + j, 8 + i)) == 0x96) { ++b; }
        }
    }
    /* Two different glyphs, both with lit pixels. A shim that mixed up
    ** y and colour would draw nothing here; one that mixed up the string
    ** pointer would draw garbage identically in both cells.
    */
    t_check(a > 4 && b > 4 && a != b, "ABI_GFX_TEXT_PTR_LAST");
}

/* ------------------------------------------------------------------ */
/* three pointers plus a byte                                          */
/* ------------------------------------------------------------------ */

/* fx_triangle(a, b, c, color): the three vertices take __rc2/__rc3,
** __rc4/__rc5 and __rc6/__rc7; the colour is the only integer, so it is
** in A. Requires VERA FX.
*/
static void test_fx_triangle_3ptr(void)
{
    static const x16_point a = { 10, 10 };
    static const x16_point b = { 60, 10 };
    static const x16_point c = { 10, 40 };
    unsigned char inside, outside;

    if (!x16_vera_has_fx()) { t_skip("ABI_FX_TRIANGLE_3PTR"); return; }
    if (!x16_gfx_init())    { t_skip("ABI_FX_TRIANGLE_3PTR"); return; }

    x16_gfx_clear(0);
    x16_fx_triangle(&a, &b, &c, 0x2A);

    inside  = vpeek(X16_VRAM_BITMAP + 15UL * 320 + 15);   /* well inside  */
    outside = vpeek(X16_VRAM_BITMAP + 35UL * 320 + 55);   /* past the hypotenuse */

    x16_screen_reset();
    t_check(inside == 0x2A && outside == 0, "ABI_FX_TRIANGLE_3PTR");
}

/* fx_copy(src, dst, count): ten integer bytes, so `count` spills into
** __rc8/__rc9 -- past everything the other shims touch.
*/
static void test_fx_copy_rc8(void)
{
    unsigned char i, ok = 1;

    if (!x16_vera_has_fx()) { t_skip("ABI_FX_COPY_RC8"); return; }

    for (i = 0; i < 8; ++i) {
        t_vpoke(0x60 + i, TESTVRAM + i);
        t_vpoke(0x00,     TESTVRAM + 0x80 + i);
    }
    t_vpoke(0x4D, TESTVRAM + 0x88);             /* poison */

    x16_fx_copy(TESTVRAM, TESTVRAM + 0x80, 8);

    for (i = 0; i < 8; ++i) {
        if (vpeek(TESTVRAM + 0x80 + i) != (unsigned char)(0x60 + i)) { ok = 0; }
    }
    if (vpeek(TESTVRAM + 0x88) != 0x4D) { ok = 0; }

    t_check(ok, "ABI_FX_COPY_RC8");
}

/* ------------------------------------------------------------------ */
/* six arguments, two of them pointers with a gap                      */
/* ------------------------------------------------------------------ */

/* fs_save then fs_load. fs_load's `sa` byte lands in __rc4 and so splits
** the pointer pairs: dest goes to __rc6/__rc7 and end to __rc8/__rc9.
** `end` also proves the out-parameter: it must receive one past the last
** byte loaded.
*/
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

    t_check(ok, "ABI_FS_LOAD_6ARGS_END");
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
            "ABI_FS_LOAD_NULL_END");
}

/* ------------------------------------------------------------------ */
/* struct block copies through a pointer                               */
/* ------------------------------------------------------------------ */

static void test_bmx_info_roundtrip(void)
{
    x16_bmx_info in, back;

    in.width    = 320;
    in.height   = 240;
    in.bpp      = 8;
    in.palstart = 17;
    in.palcount = 200;
    in.border   = 5;
    in.stride   = 320;
    x16_bmx_set_info(&in);

    back.width = 0; back.height = 0; back.bpp = 0; back.palstart = 0;
    back.palcount = 0; back.border = 0; back.stride = 0;
    x16_bmx_get_info(&back);

    t_check(back.width == 320 && back.height == 240 && back.bpp == 8 &&
            back.palstart == 17 && back.palcount == 200 &&
            back.border == 5 && back.stride == 320,
            "ABI_BMX_INFO_ROUNDTRIP");
}

/* clip_set takes four ints -- eight bytes, A through __rc7 -- and
** clip_line takes a pointer to a four-int struct and writes it back.
*/
static void test_clip_argorder(void)
{
    x16_line seg;
    unsigned char visible;

    x16_clip_set(10, 20, 100, 200);

    seg.x0 = -50; seg.y0 = 100;                 /* enters from the left */
    seg.x1 =  50; seg.y1 = 100;
    visible = x16_clip_line(&seg);

    t_check(visible == 1 && seg.x0 == 10 && seg.y0 == 100 &&
            seg.x1 == 50 && seg.y1 == 100,
            "ABI_CLIP_SET_AND_LINE");
}

/* Entirely outside: the routine answers 0 and must not touch the struct. */
static void test_clip_outside(void)
{
    x16_line seg;
    unsigned char visible;

    x16_clip_set(10, 20, 100, 200);
    seg.x0 = 300; seg.y0 = 300;
    seg.x1 = 400; seg.y1 = 400;
    visible = x16_clip_line(&seg);

    t_check(visible == 0 && seg.x0 == 300 && seg.y0 == 300,
            "ABI_CLIP_OUTSIDE");
}

/* ------------------------------------------------------------------ */
/* the ROM float binding: every operand is a pointer                   */
/* ------------------------------------------------------------------ */

static void test_float_ptr_operands(void)
{
    x16_float a, b;
    char buf[X16_FP_STRLEN];
    unsigned char ok;

    x16_f_from_u8(200);                 /* fp_float is SIGNED: 200 must not be -56 */
    x16_f_store(a);

    x16_f_from_u8(50);
    x16_f_store(b);

    x16_f_load(a);
    x16_f_sub(b);                       /* 200 - 50, not 50 - 200 */
    x16_f_to_str_trim(buf);

    ok = buf[0] == '1' && buf[1] == '5' && buf[2] == '0' && buf[3] == 0;

    /* f_cmp returns a signed char: a > b is +1. */
    x16_f_load(a);
    ok = ok && (x16_f_cmp(b) == 1);
    x16_f_load(b);
    ok = ok && (x16_f_cmp(a) == -1);

    t_check(ok, "ABI_FLOAT_PTR_OPERANDS");
}

/* ------------------------------------------------------------------ */
/* zx0: two pointers in, a pointer out                                 */
/* ------------------------------------------------------------------ */

static void test_zx0_ptr_return(void)
{
    unsigned char *end;
    unsigned char i, r, ok = 1;

    unpacked[96] = 0x9D;                        /* poison */
    end = (unsigned char *)x16_zx0_decompress(zx0_packed, unpacked);

    for (r = 0; r < 4; ++r) {
        for (i = 0; i < 24; ++i) {
            if (unpacked[r * 24 + i] != phrase[i]) { ok = 0; }
        }
    }
    t_check(ok && end == unpacked + 96 && unpacked[96] == 0x9D,
            "ABI_ZX0_PTR_RETURN");
}

/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */
/* gfx2: every 640x480@2bpp shim, transposition-proof                  */
/* ------------------------------------------------------------------ */

/* A 2bpp framebuffer byte sits at y*160 + (x>>2). All values distinct
** and nonzero, so any swapped register lands somewhere it can be seen.
** x16_gfx2_init() reprograms the display and palette 0-3, so this block
** runs last.
*/

/* rect(4, 40, 8, 2, 1): quad_marshal + colour in __rc8. */
static void test_abi_g2_rect(void)
{
    unsigned char i;

    x16_gfx2_init();
    for (i = 0; i < 4; ++i) {
        t_vpoke(0x00, 40UL * 160 + i);
        t_vpoke(0x00, 41UL * 160 + i);
        t_vpoke(0x00, 42UL * 160 + i);
    }
    x16_gfx2_rect(4, 40, 8, 2, 1);
    t_check(vpeek(40UL * 160 + 0) == 0x00 &&
            vpeek(40UL * 160 + 1) == 0x55 &&
            vpeek(40UL * 160 + 2) == 0x55 &&
            vpeek(40UL * 160 + 3) == 0x00 &&
            vpeek(41UL * 160 + 1) == 0x55 &&
            vpeek(42UL * 160 + 1) == 0x00,
            "ABI_G2_RECT");
}

/* hline(5, 20, 13, 3): span_marshal + colour in __rc6. The head, middle
** and tail bytes only land right if x, y and len each reach their slot.
*/
static void test_abi_g2_hline(void)
{
    unsigned char i;

    for (i = 0; i < 6; ++i) {
        t_vpoke(0x00, 20UL * 160 + i);
    }
    x16_gfx2_hline(5, 20, 13, 3);
    t_check(vpeek(20UL * 160 + 0) == 0x00 &&
            vpeek(20UL * 160 + 1) == 0x3F &&
            vpeek(20UL * 160 + 2) == 0xFF &&
            vpeek(20UL * 160 + 3) == 0xFF &&
            vpeek(20UL * 160 + 4) == 0xF0 &&
            vpeek(20UL * 160 + 5) == 0x00,
            "ABI_G2_HLINE");
}

/* pset + read prove the x/y pairs in BOTH directions plus the returns:
** pset(5, 10, 2) puts colour 2 in byte 1 pixel 1 of row 10; read must
** find it at (5,10), see background at (6,10), and answer $FF off screen.
*/
static void test_abi_g2_pset_read(void)
{
    t_vpoke(0x00, 10UL * 160 + 1);
    x16_gfx2_pset(5, 10, 2);
    t_check(vpeek(10UL * 160 + 1) == 0x20 &&
            x16_gfx2_read(5, 10) == 2 &&
            x16_gfx2_read(6, 10) == 0 &&
            x16_gfx2_read(640, 10) == 0xFF,
            "ABI_G2_PSET_READ");
}

/* setptr(inc, x, y): the byte argument goes in A and the first int
** STRADDLES X and __rc2 -- the placement most worth proving. inc 0
** keeps the port still, so two writes land on the same byte.
*/
static void test_abi_g2_setptr(void)
{
    unsigned char phase;

    t_vpoke(0x00, 7UL * 160 + 80);
    phase = x16_gfx2_setptr(0, 322, 7);     /* byte 80 of row 7, pixel 2 */
    VERA.data0 = 0x5A;
    VERA.data0 = 0xA5;                      /* INC_0: same byte again */
    t_check(phase == 2 && vpeek(7UL * 160 + 80) == 0xA5,
            "ABI_G2_SETPTR");
}

/* line(0, 60, 7, 67, 3): four words through quad_marshal, colour after. */
static void test_abi_g2_line(void)
{
    unsigned char i;

    for (i = 60; i <= 67; ++i) {
        t_vpoke(0x00, (unsigned long)i * 160);
        t_vpoke(0x00, (unsigned long)i * 160 + 1);
    }
    x16_gfx2_line(0, 60, 7, 67, 3);
    t_check(vpeek(60UL * 160) == 0xC0 &&
            vpeek(63UL * 160) == 0x03 &&
            vpeek(64UL * 160 + 1) == 0xC0 &&
            vpeek(67UL * 160 + 1) == 0x03,
            "ABI_G2_LINE");
}

/* pattern_set(pattern, colors): the pointer hides in __rc2/__rc3 while
** the colour byte takes A. Pattern $F0 with fg 3 makes even bytes $FF
** and odd bytes $00 -- swap anything and the parity signature is gone.
*/
static const unsigned char g2_pat[8] = {
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0
};

static void test_abi_g2_pattern(void)
{
    unsigned char i;

    for (i = 0; i < 4; ++i) {
        t_vpoke(0x55, 70UL * 160 + i);
    }
    x16_gfx2_pattern_set(g2_pat, 0x03);
    x16_gfx2_pattern_rect(0, 70, 12, 1);
    t_check(vpeek(70UL * 160 + 0) == 0xFF &&
            vpeek(70UL * 160 + 1) == 0x00 &&
            vpeek(70UL * 160 + 2) == 0xFF &&
            vpeek(70UL * 160 + 3) == 0x55,      /* untouched */
            "ABI_G2_PATTERN");
}

/* blit(8, 80, 2, 2, img, 0) then XOR: six arguments -- two words, two
** single bytes, an aligned pointer pair in __rc6/__rc7 and op in __rc8.
*/
static const unsigned char g2_img[4] = { 0xDE, 0xAD, 0xBE, 0xEF };

static void test_abi_g2_blit(void)
{
    t_vpoke(0x00, 80UL * 160 + 2);
    t_vpoke(0x00, 80UL * 160 + 3);
    t_vpoke(0x00, 81UL * 160 + 2);
    t_vpoke(0x00, 81UL * 160 + 3);
    x16_gfx2_blit(8, 80, 2, 2, g2_img, 0);
    if (vpeek(80UL * 160 + 2) != 0xDE || vpeek(81UL * 160 + 3) != 0xEF) {
        t_check(0, "ABI_G2_BLIT");
        return;
    }
    x16_gfx2_blit(8, 80, 2, 2, g2_img, 3);
    t_check(vpeek(80UL * 160 + 2) == 0x00 &&
            vpeek(81UL * 160 + 3) == 0x00,
            "ABI_G2_BLIT");
}

/* blitm(12, 90, 4, 1, mcol): h and cols are the two singles before the
** pointer pair. (fb & $0F) | $50 over $FF reads $5F on every row.
*/
static const unsigned char g2_mcol[8] = {
    0x0F, 0x50, 0x0F, 0x50, 0x0F, 0x50, 0x0F, 0x50
};

static void test_abi_g2_blitm(void)
{
    unsigned char i;

    for (i = 90; i <= 94; ++i) {
        t_vpoke(0xFF, (unsigned long)i * 160 + 3);
    }
    x16_gfx2_blitm(12, 90, 4, 1, g2_mcol);
    t_check(vpeek(90UL * 160 + 3) == 0x5F &&
            vpeek(93UL * 160 + 3) == 0x5F &&
            vpeek(94UL * 160 + 3) == 0xFF,
            "ABI_G2_BLITM");
}

/* clear(2): one byte in A, and exactly 76,800 bytes covered. FX only. */
static void test_abi_g2_clear(void)
{
    t_vpoke(0x77, 76800UL);
    x16_gfx2_clear(2);
    t_check(vpeek(0UL) == 0xAA &&
            vpeek(76799UL) == 0xAA &&
            vpeek(76800UL) == 0x77,
            "ABI_G2_CLEAR");
}

int main(void)
{
    t_init();

    test_collide8_argorder();
    test_collide16_pointers();
    test_bank_peek_poke_argorder();
    test_bank_copy_far_argorder();

    test_mem_fill_argorder();
    test_mem_copy_argorder();
    test_mem_crc_int_return();
    test_mem_decompress_ptr_return();
    test_dos_msg_ptr_return();

    test_umul16_long_return();
    test_mul88_int_return();

    test_rb_int_minus_one();
    test_stk_int_minus_one();
    test_sin8_signed_return();

    test_sprite_pos_roundtrip();
    test_sprite_get_pos_both();
    test_sprite_image_argorder();

    test_gfx_text_ptr_last();
    test_fx_triangle_3ptr();
    test_fx_copy_rc8();

    test_fs_roundtrip_and_end();
    test_fs_load_null_end();

    test_bmx_info_roundtrip();
    test_clip_argorder();
    test_clip_outside();

    test_float_ptr_operands();
    test_zx0_ptr_return();

    /* Last: x16_gfx2_init() reprograms the display and palette 0-3. */
    test_abi_g2_rect();
    test_abi_g2_hline();
    test_abi_g2_pset_read();
    test_abi_g2_setptr();
    test_abi_g2_line();
    test_abi_g2_pattern();
    test_abi_g2_blit();
    test_abi_g2_blitm();
    if (x16_vera_has_fx()) {
        test_abi_g2_clear();
    } else {
        t_skip("ABI_G2_CLEAR");
    }

    t_done();
    return 0;
}
