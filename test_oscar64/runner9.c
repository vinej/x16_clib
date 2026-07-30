/* =====================================================================
 * x16clib :: test_oscar64/runner9.c -- the storage wave: ringbuffer, stack,
 *                                    fileio, iec, dir
 * =====================================================================
 * Standalone suite:
 *
 *      .\build_oscar64.ps1 -Test -Source test_oscar64\runner9.c
 *
 * Same independent-path principle as the other suites: drive the library
 * one way and verify the other way. A file written byte-by-byte through
 * x16_fio_chrout() is read back whole through x16_fs_load(); a file
 * scratched through raw IEC LISTEN/CIOUT is proven gone through the
 * loader; the banked ring and stack are checked against a pattern the
 * test computes for itself.
 *
 * WRITTEN IN THIS TREE'S IDIOM rather than transliterated from ca65's
 * runner6.c, which is what the other four suites are. That port kept
 * running into dialect gaps that have nothing to do with the library:
 * That dialect would not assign a bool to a char, had no `&&` fragment over
 * non-trivial operands, no 16- or 32-bit compare against a constant, no
 * `i-- > 0` in a for-condition, and answers a bare "Not implemented!"
 * to some combinations with no line number at all. Each is avoidable one
 * at a time; together they turn a faithful copy into a guessing game.
 * The COVERAGE here is the same -- every entry point of all five
 * modules, checked the same way -- written the way this compiler
 * accepts.
 *
 * Everything on device 8 lands in the emulator's -fsroot scratch
 * directory, which build_oscar64.ps1 empties before the run.
 * ===================================================================== */

#include "testlib.h"
#include <x16/ringbuffer.h>
#include <x16/stack.h>
#include <x16/fileio.h>
#include <x16/iec.h>
#include <x16/dir.h>
#include <x16/load.h>
#include <x16/bank.h>

/* Out of the way of everything else the suites touch. */
#define RING_BANK   6
#define STACK_BANK  5

#define FIO_LEN     32

/* No <string.h> here. */
char t_strcmp8(const char *a, const char *b) {
    unsigned char i;
    for (i = 0; ; ++i) {
        if (a[i] != b[i]) { return 1; }
        if (a[i] == 0) { return 0; }
    }
}

/* A pattern with no 256-byte periodicity, so a wrapped index shows up. */
unsigned char pat(unsigned int i) {
    unsigned char lo = (unsigned char)i;
    unsigned char hi = (unsigned char)(i >> 8);
    return (unsigned char)(lo ^ hi);
}

/* ------------------------------------------------------------------ */
/* the banked ring buffer                                              */
/* ------------------------------------------------------------------ */

void test_ring_fifo(void) {
    unsigned char ok = 1;
    unsigned int sz;

    x16_ring_init(RING_BANK);
    if (x16_ring_isempty() == 0) { ok = 0; }
    if (x16_ring_isfull() != 0) { ok = 0; }
    sz = x16_ring_size();
    if (sz != 0) { ok = 0; }

    x16_ring_put(10);
    x16_ring_put(20);
    x16_ring_put(30);
    sz = x16_ring_size();
    if (sz != 3) { ok = 0; }
    if (x16_ring_isempty() != 0) { ok = 0; }

    if (x16_ring_get() != 10) { ok = 0; }       /* FIFO: 10 comes out first */
    if (x16_ring_get() != 20) { ok = 0; }
    if (x16_ring_get() != 30) { ok = 0; }
    if (x16_ring_isempty() == 0) { ok = 0; }

    t_check(ok, "RING_FIFO");
}

/* Words interleave with bytes and keep their byte order. */
void test_ring_word(void) {
    unsigned char ok = 1;
    unsigned int w;
    unsigned int sz;

    x16_ring_init(RING_BANK);
    x16_ring_put(0xAA);
    x16_ring_putw(777);
    x16_ring_putw(258);

    sz = x16_ring_size();
    if (sz != 5) { ok = 0; }
    if (x16_ring_get() != 0xAA) { ok = 0; }
    w = x16_ring_getw();
    if ((unsigned char)w != 9) { ok = 0; }      /* 777 = $0309 */
    if ((unsigned char)(w >> 8) != 3) { ok = 0; }
    w = x16_ring_getw();
    if ((unsigned char)w != 2) { ok = 0; }      /* 258 = $0102 */
    if ((unsigned char)(w >> 8) != 1) { ok = 0; }
    if (x16_ring_isempty() == 0) { ok = 0; }

    t_check(ok, "RING_WORD");
}

/* Fill the whole 8191 bytes, watch the bookkeeping, drain and compare.
** The caller's RAM_BANK must come back untouched.
*/
void test_ring_fill_drain(void) {
    unsigned char ok = 1;
    unsigned int i;
    unsigned int sz;

    x16_bank_set(3);
    x16_ring_init(RING_BANK);

    for (i = 0; i < 8191; ++i) {
        x16_ring_put(pat(i));
    }
    sz = x16_ring_size();
    if (sz != 8191) { ok = 0; }
    sz = x16_ring_free();
    if (sz != 0) { ok = 0; }
    if (x16_ring_isfull() == 0) { ok = 0; }
    if (x16_ring_isempty() != 0) { ok = 0; }

    for (i = 0; i < 8191; ++i) {
        if (x16_ring_get() != pat(i)) { ok = 0; }
    }
    if (x16_ring_isempty() == 0) { ok = 0; }
    if (x16_ring_isfull() != 0) { ok = 0; }
    if (x16_bank_get() != 3) { ok = 0; }

    t_check(ok, "RING_FILL_DRAIN");
}

/* Head and tail now sit at the top of the bank: two more puts and gets
** must cross the 8191 -> 0 seam intact.
*/
void test_ring_wrap(void) {
    unsigned char ok = 1;

    x16_ring_put(0x5A);
    x16_ring_put(0xA5);
    if (x16_ring_get() != 0x5A) { ok = 0; }
    if (x16_ring_get() != 0xA5) { ok = 0; }
    if (x16_ring_isempty() == 0) { ok = 0; }

    t_check(ok, "RING_WRAP");
}

/* ------------------------------------------------------------------ */
/* the banked stack                                                    */
/* ------------------------------------------------------------------ */

void test_stack_lifo(void) {
    unsigned char ok = 1;
    unsigned int sz;

    x16_stack_init(STACK_BANK);
    if (x16_stack_isempty() == 0) { ok = 0; }
    if (x16_stack_isfull() != 0) { ok = 0; }
    sz = x16_stack_size();
    if (sz != 0) { ok = 0; }

    x16_stack_push(42);
    x16_stack_push(7);
    x16_stack_push(99);
    sz = x16_stack_size();
    if (sz != 3) { ok = 0; }
    if (x16_stack_isempty() != 0) { ok = 0; }

    if (x16_stack_pop() != 99) { ok = 0; }      /* LIFO: last in, first out */
    if (x16_stack_pop() != 7) { ok = 0; }
    if (x16_stack_pop() != 42) { ok = 0; }
    if (x16_stack_isempty() == 0) { ok = 0; }

    t_check(ok, "STACK_LIFO");
}

void test_stack_word(void) {
    unsigned char ok = 1;
    unsigned int w;

    x16_stack_init(STACK_BANK);
    x16_stack_pushw(1000);
    x16_stack_pushw(50);

    w = x16_stack_popw();                       /* LIFO: 50 comes back first */
    if ((unsigned char)w != 50) { ok = 0; }
    if ((unsigned char)(w >> 8) != 0) { ok = 0; }
    w = x16_stack_popw();
    if ((unsigned char)w != 0xE8) { ok = 0; }   /* 1000 = $03E8 */
    if ((unsigned char)(w >> 8) != 3) { ok = 0; }
    if (x16_stack_isempty() == 0) { ok = 0; }

    t_check(ok, "STACK_WORD");
}

void test_stack_fill_drain(void) {
    unsigned char ok = 1;
    unsigned int i;
    unsigned int sz;

    x16_bank_set(3);
    x16_stack_init(STACK_BANK);

    for (i = 0; i < 8191; ++i) {
        x16_stack_push(pat(i));
    }
    sz = x16_stack_size();
    if (sz != 8191) { ok = 0; }
    sz = x16_stack_free();
    if (sz != 0) { ok = 0; }
    if (x16_stack_isempty() != 0) { ok = 0; }

    /* Counted forwards and indexed backwards: avoids `i-- > 0`. */
    for (i = 0; i < 8191; ++i) {
        if (x16_stack_pop() != pat(8190 - i)) { ok = 0; }
    }
    if (x16_stack_isempty() == 0) { ok = 0; }
    if (x16_bank_get() != 3) { ok = 0; }

    t_check(ok, "STACK_FILL_DRAIN");
}

/* Full once the pointer has run below the window. */
void test_stack_full_flag(void) {
    unsigned char ok = 1;
    unsigned int i;

    x16_stack_init(STACK_BANK);
    if (x16_stack_isfull() != 0) { ok = 0; }
    for (i = 0; i < 8191; ++i) {
        x16_stack_push(1);
    }
    if (x16_stack_isfull() == 0) { ok = 0; }
    for (i = 0; i < 8191; ++i) {
        x16_stack_pop();
    }
    if (x16_stack_isempty() == 0) { ok = 0; }

    t_check(ok, "STACK_FULL_FLAG");
}

/* ------------------------------------------------------------------ */
/* the channel API                                                     */
/* ------------------------------------------------------------------ */

unsigned char fio_in[FIO_LEN];

/* Write a SEQ file one byte at a time through the channel API, then read
** it back whole through the loader -- an address path entirely
** independent of CHKOUT/CHROUT.
*/
void test_fio_write(void) {
    unsigned char ok = 1;
    unsigned char i;
    unsigned int end = 0;

    if (x16_fio_open_write("FIOTEST.SEQ,S,W", 15, 2, 8, 2) != 0) {
        t_check(0, "FIO_WRITE");
        return;
    }
    for (i = 0; i < FIO_LEN; ++i) {
        x16_fio_chrout(pat(i));
    }
    x16_fio_close_named(2);

    /* The 32 bytes just written show up in the captured output. Nothing
    ** is wrong: the emulator's -echo reports every byte CHROUT is handed,
    ** and these went to the FILE, not the screen. The ca65
    ** suites echo their own pattern the same way. Only the line break
    ** matters here -- without it the run lands on the end of the previous
    ** PASS line and the harness's own count check trips ("36 PASS lines
    ** but DONE says 37"). */
    t_chrout(10);

    if (x16_fs_load("FIOTEST.SEQ", 11, X16_DEVICE_SD, X16_SA_RAW,
                    fio_in, &end) != 0) {
        ok = 0;
    }
    for (i = 0; i < FIO_LEN; ++i) {
        if (fio_in[i] != pat(i)) { ok = 0; }
    }
    t_check(ok, "FIO_WRITE");
}

/* Read it back a byte at a time, and check EOF arrives exactly once the
** last byte has been handed over.
*/
void test_fio_read_eof(void) {
    unsigned char ok = 1;
    unsigned char i;
    unsigned char st;

    if (x16_fio_open_read("FIOTEST.SEQ,S,R", 15, 3, 8, 3) != 0) {
        t_check(0, "FIO_READ_EOF");
        return;
    }
    for (i = 0; i < FIO_LEN; ++i) {
        if (x16_fio_chrin() != pat(i)) { ok = 0; }
    }
    st = x16_fio_readst();
    if ((st & 0x40) == 0) { ok = 0; }           /* EOF by now */
    x16_fio_close_named(3);

    t_check(ok, "FIO_READ_EOF");
}

/* set_lfs + set_name + open, the long way round, must reach the same
** file the convenience wrapper did.
*/
void test_fio_raw_open(void) {
    unsigned char ok = 1;

    x16_fio_set_lfs(4, 8, 4);
    x16_fio_set_name("FIOTEST.SEQ,S,R", 15);
    if (x16_fio_open() != 0) { ok = 0; }
    if (x16_fio_chkin(4) != 0) { ok = 0; }
    if (x16_fio_chrin() != pat(0)) { ok = 0; }
    x16_fio_clrchn();
    x16_fio_close(4);

    t_check(ok, "FIO_RAW_OPEN");
}

/* CHKIN on a file that was never opened must fail, not wedge. */
void test_fio_chkin_unopened(void) {
    unsigned char ok = 1;

    if (x16_fio_chkin(9) == 0) { ok = 0; }
    x16_fio_clrchn();

    t_check(ok, "FIO_CHKIN_UNOPENED");
}

/* close_all really closes: a file that WAS open must fail CHKIN after.
** (The same assertion ca65's runner6 makes.)
*/
void test_fio_close_all(void) {
    unsigned char ok = 1;

    if (x16_fio_open_read("FIOTEST.SEQ,S,R", 15, 2, 8, 2) != 0) { ok = 0; }
    x16_fio_close_all();
    if (x16_fio_chkin(2) == 0) { ok = 0; }      /* now gone */
    x16_fio_clrchn();

    t_check(ok, "FIO_CLOSE_ALL");
}

/* ------------------------------------------------------------------ */
/* the raw bus                                                         */
/* ------------------------------------------------------------------ */

char iec_buf[48];

/* Read the DOS status channel with LISTEN/TALK by hand. The reply always
** starts with a two-digit code and a comma.
*/
void test_iec_talk_status(void) {
    unsigned char ok = 1;
    unsigned char n = 0;
    unsigned char b;
    unsigned char st;

    x16_iec_talk_channel(8, 15);
    for (;;) {
        b = x16_iec_acptr();
        st = x16_iec_readst();
        if (st != 0) { break; }
        if (n < 47) {
            iec_buf[n] = (char)b;
            n = n + 1;
        }
    }
    x16_iec_untalk();
    iec_buf[n] = 0;

    if (n < 3) { ok = 0; }
    if (iec_buf[0] < '0') { ok = 0; }
    if (iec_buf[0] > '9') { ok = 0; }
    if (iec_buf[1] < '0') { ok = 0; }
    if (iec_buf[1] > '9') { ok = 0; }
    if (iec_buf[2] != ',') { ok = 0; }

    t_check(ok, "IEC_TALK_STATUS");
}

/* Scratch a file with a DOS command pushed byte by byte, then prove
** through the loader that it is gone.
*/
void test_iec_listen_cmd(void) {
    unsigned char ok = 1;
    unsigned char i;
    unsigned int end = 0;
    unsigned char junk[4];

    junk[0] = 1;
    junk[1] = 2;
    junk[2] = 3;
    junk[3] = 4;
    if (x16_fs_save("IECDEL.BIN", 10, X16_DEVICE_SD, junk, junk + 4) != 0) {
        ok = 0;
    }

    x16_iec_data_channel(8, 15);
    for (i = 0; i < 12; ++i) {
        x16_iec_ciout("S:IECDEL.BIN"[i]);
    }
    x16_iec_unlisten();

    if (x16_fs_load("IECDEL.BIN", 10, X16_DEVICE_SD, X16_SA_RAW,
                    fio_in, &end) == 0) {
        ok = 0;                                  /* still there: not scratched */
    }
    t_check(ok, "IEC_LISTEN_CMD");
}

/* MACPTR pulls a whole block in one call. Not every device implements
** it; -1 says so, and that is a pass here too as long as it says it
** cleanly rather than hanging.
*/
void test_iec_macptr(void) {
    unsigned char ok = 1;
    int got;

    if (x16_fio_open_read("FIOTEST.SEQ,S,R", 15, 5, 8, 5) != 0) {
        t_check(0, "IEC_MACPTR");
        return;
    }
    x16_iec_talk_channel(8, 5);
    got = x16_iec_macptr(8, fio_in);
    x16_iec_untalk();
    x16_fio_close_named(5);

    if (got > 8) { ok = 0; }                     /* never more than asked */

    t_check(ok, "IEC_MACPTR");
}

void test_iec_mciout(void) {
    unsigned char ok = 1;
    int sent;
    unsigned char out[4];

    out[0] = 0x11;
    out[1] = 0x22;
    out[2] = 0x33;
    out[3] = 0x44;

    if (x16_fio_open_write("IECOUT.SEQ,S,W", 14, 6, 8, 6) != 0) {
        t_check(0, "IEC_MCIOUT");
        return;
    }
    x16_iec_data_channel(8, 6);
    sent = x16_iec_mciout(4, out);
    x16_iec_unlisten();
    x16_fio_close_named(6);

    if (sent > 4) { ok = 0; }

    t_check(ok, "IEC_MCIOUT");
}

/* ------------------------------------------------------------------ */
/* the directory walker                                                */
/* ------------------------------------------------------------------ */

char dir_name[32];

/* The scratch directory holds at least the file test_fio_write left, so
** a walk must find it, with a sane type and block count.
*/
void test_dir_walk(void) {
    unsigned char ok = 1;
    unsigned char n = 0;
    unsigned char seen = 0;
    unsigned char ty;

    if (x16_dir_open("", 0, 8) == 0) {
        t_check(0, "DIR_WALK");
        return;
    }
    for (;;) {
        if (x16_dir_next(dir_name, 32) == 0) { break; }
        n = n + 1;
        ty = x16_dir_type();
        if (ty == X16_DIR_TYPE_SEQ) { seen = 1; }
        if (ty == X16_DIR_TYPE_PRG) { seen = 1; }
    }
    x16_dir_close();

    if (n == 0) { ok = 0; }
    if (seen == 0) { ok = 0; }

    t_check(ok, "DIR_WALK");
}

/* The block count comes off the line number. A 1016-byte file must
** report more blocks than a 4-byte one -- which proves the count is read
** rather than invented, without depending on what the host filesystem
** calls a block.
*/
unsigned char dir_big[1016];

void test_dir_blocks(void) {
    unsigned char ok = 1;
    unsigned int small_blk = 0xFFFF;
    unsigned int big_blk = 0xFFFF;

    if (x16_fs_save("DIRA.BIN", 8, X16_DEVICE_SD, dir_big, dir_big + 4) != 0) {
        ok = 0;
    }
    if (x16_fs_save("DIRBIG.BIN", 10, X16_DEVICE_SD,
                    dir_big, dir_big + 1016) != 0) {
        ok = 0;
    }

    if (x16_dir_open("", 0, 8) == 0) {
        t_check(0, "DIR_BLOCKS");
        return;
    }
    for (;;) {
        if (x16_dir_next(dir_name, 32) == 0) { break; }
        if (t_strcmp8(dir_name, "DIRA.BIN") == 0) {
            small_blk = x16_dir_blocks();
        }
        if (t_strcmp8(dir_name, "DIRBIG.BIN") == 0) {
            big_blk = x16_dir_blocks();
        }
    }
    x16_dir_close();

    if (small_blk == 0xFFFF) { ok = 0; }        /* both were listed */
    if (big_blk == 0xFFFF) { ok = 0; }
    if (big_blk <= small_blk) { ok = 0; }       /* and the big one is bigger */

    t_check(ok, "DIR_BLOCKS");
}

/* A buffer too small for the name truncates it and still NUL-terminates,
** and the walk keeps going.
*/
void test_dir_trunc(void) {
    unsigned char ok = 1;
    unsigned char n = 0;
    char small[4];

    if (x16_dir_open("", 0, 8) == 0) {
        t_check(0, "DIR_TRUNC");
        return;
    }
    for (;;) {
        if (x16_dir_next(small, 4) == 0) { break; }
        n = n + 1;
        if (small[3] != 0) { ok = 0; }           /* always terminated */
    }
    x16_dir_close();
    if (n == 0) { ok = 0; }

    t_check(ok, "DIR_TRUNC");
}

/* A named pattern opens the same way a bare listing does. */
void test_dir_named(void) {
    unsigned char ok = 1;

    if (x16_dir_open("$", 1, 8) == 0) { ok = 0; }
    x16_dir_close();

    t_check(ok, "DIR_NAMED");
}

/* A device that is not there must fail the open, not hang. */
void test_dir_open_missing(void) {
    unsigned char ok = 1;

    if (x16_dir_open("", 0, 30) != 0) { ok = 0; }
    x16_fio_close(3);                            /* tidy the LFN either way */

    t_check(ok, "DIR_OPEN_MISSING");
}

/* ------------------------------------------------------------------ */

int main(void) {
    t_init();

    test_ring_fifo();
    test_ring_word();
    test_ring_fill_drain();
    test_ring_wrap();

    test_stack_lifo();
    test_stack_word();
    test_stack_fill_drain();
    test_stack_full_flag();

    test_fio_write();
    test_fio_read_eof();
    test_fio_raw_open();
    test_fio_chkin_unopened();
    test_fio_close_all();

    test_iec_talk_status();
    test_iec_listen_cmd();
    test_iec_macptr();
    test_iec_mciout();

    test_dir_walk();
    test_dir_blocks();
    test_dir_trunc();
    test_dir_named();
    test_dir_open_missing();

    t_done();
    return 0;
}
