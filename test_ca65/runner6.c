/* =====================================================================
 * x16clib :: test/runner6.c -- the storage wave: dir, fileio, iec,
 *                              ringbuffer, stack, and buffers' counters
 * =====================================================================
 * Standalone suite: run it with
 *
 *      .\build_ca65.ps1 -Test -Source test_ca65\runner6.c
 *
 * Same independent-path principle as runner.c: drive the library one way
 * and verify the other way. A file written byte-by-byte through
 * x16_fio_chrout() is read back whole through x16_fs_load(); a file
 * scratched through raw IEC LISTEN/CIOUT is proven gone through the
 * loader; the banked ring and stack are checked against a pattern the
 * test computes independently.
 *
 * Everything on device 8 lands in the emulator's -fsroot scratch
 * directory, which build_ca65.ps1 empties before the run.
 * ===================================================================== */

#include "testlib.h"
#include <cbm.h>
#include <string.h>
#include <x16/dir.h>
#include <x16/fileio.h>
#include <x16/iec.h>
#include <x16/ringbuffer.h>
#include <x16/stack.h>
#include <x16/buffers.h>
#include <x16/load.h>
#include <x16/dos.h>
#include <x16/bank.h>

/* The bank tests in the main suite prove banks round-trip; these two are
** simply out of the way of everything else the suites touch.
*/
#define RING_BANK       6
#define STACK_BANK      5

/* ------------------------------------------------------------------ */
/* the banked ring buffer                                              */
/* ------------------------------------------------------------------ */

static void test_ring_fifo(void)
{
    unsigned char ok;

    x16_ring_init(RING_BANK);
    ok = x16_ring_isempty() && !x16_ring_isfull();
    ok = ok && (x16_ring_size() == 0);
    ok = ok && (x16_ring_free() == X16_RING_CAPACITY);

    x16_ring_put(10);
    x16_ring_put(20);
    x16_ring_put(30);
    ok = ok && (x16_ring_size() == 3);
    ok = ok && (x16_ring_free() == X16_RING_CAPACITY - 3);
    ok = ok && !x16_ring_isempty();

    ok = ok && (x16_ring_get() == 10);      /* FIFO: 10 comes out first */
    ok = ok && (x16_ring_get() == 20);
    ok = ok && (x16_ring_get() == 30);
    ok = ok && x16_ring_isempty();

    t_check(ok, "RING_FIFO");
}

/* Words interleave with bytes and keep their byte order. */
static void test_ring_word(void)
{
    unsigned char ok;

    x16_ring_init(RING_BANK);
    x16_ring_put(0xAA);
    x16_ring_putw(777);
    x16_ring_putw(258);

    ok = (x16_ring_size() == 5);
    ok = ok && (x16_ring_get() == 0xAA);
    ok = ok && (x16_ring_getw() == 777);    /* FIFO: 777 first */
    ok = ok && (x16_ring_getw() == 258);
    ok = ok && x16_ring_isempty();

    t_check(ok, "RING_WORD");
}

/* Fill all 8191 bytes, watch the bookkeeping, drain and compare against
** a pattern with no 256 periodicity. The caller's RAM_BANK must come
** back untouched.
*/
static void test_ring_fill_drain(void)
{
    unsigned int i;
    unsigned char ok = 1;

    x16_bank_set(3);
    x16_ring_init(RING_BANK);

    for (i = 0; i < X16_RING_CAPACITY; ++i) {
        x16_ring_put((unsigned char)(i ^ (i >> 8)));
    }
    ok = ok && (x16_ring_size() == X16_RING_CAPACITY);
    ok = ok && (x16_ring_free() == 0);
    ok = ok && x16_ring_isfull();
    ok = ok && !x16_ring_isempty();

    for (i = 0; i < X16_RING_CAPACITY; ++i) {
        if (x16_ring_get() != (unsigned char)(i ^ (i >> 8))) {
            ok = 0;
        }
    }
    ok = ok && x16_ring_isempty();
    ok = ok && !x16_ring_isfull();
    ok = ok && (x16_bank_get() == 3);

    t_check(ok, "RING_FILL_DRAIN");
}

/* After the fill/drain above head and tail sit at the top of the bank;
** two more puts and gets must cross the 8191 -> 0 seam intact.
*/
static void test_ring_wrap(void)
{
    unsigned char ok;

    x16_ring_put(0x5A);
    x16_ring_put(0xA5);
    ok = (x16_ring_get() == 0x5A);
    ok = ok && (x16_ring_get() == 0xA5);
    ok = ok && x16_ring_isempty();

    t_check(ok, "RING_WRAP");
}

/* ------------------------------------------------------------------ */
/* the banked stack                                                    */
/* ------------------------------------------------------------------ */

static void test_stack_lifo(void)
{
    unsigned char ok;

    x16_stack_init(STACK_BANK);
    ok = x16_stack_isempty() && !x16_stack_isfull();
    ok = ok && (x16_stack_size() == 0);
    ok = ok && (x16_stack_free() == X16_STACK_CAPACITY);

    x16_stack_push(42);
    x16_stack_push(7);
    x16_stack_push(99);
    ok = ok && (x16_stack_size() == 3);
    ok = ok && (x16_stack_free() == X16_STACK_CAPACITY - 3);
    ok = ok && !x16_stack_isempty();

    ok = ok && (x16_stack_pop() == 99);     /* LIFO: last in, first out */
    ok = ok && (x16_stack_pop() == 7);
    ok = ok && (x16_stack_pop() == 42);
    ok = ok && x16_stack_isempty();

    t_check(ok, "STACK_LIFO");
}

static void test_stack_word(void)
{
    unsigned char ok;

    x16_stack_init(STACK_BANK);
    x16_stack_pushw(1000);
    x16_stack_pushw(50);

    ok = (x16_stack_popw() == 50);          /* LIFO: 50 comes back first */
    ok = ok && (x16_stack_popw() == 1000);
    ok = ok && x16_stack_isempty();

    t_check(ok, "STACK_WORD");
}

static void test_stack_fill_drain(void)
{
    unsigned int i;
    unsigned char ok = 1;

    x16_bank_set(3);
    x16_stack_init(STACK_BANK);

    for (i = 0; i < X16_STACK_CAPACITY; ++i) {
        x16_stack_push((unsigned char)(i ^ (i >> 8)));
    }
    ok = ok && (x16_stack_size() == X16_STACK_CAPACITY);
    ok = ok && (x16_stack_free() == 0);
    ok = ok && !x16_stack_isempty();

    for (i = X16_STACK_CAPACITY; i-- > 0; ) {
        if (x16_stack_pop() != (unsigned char)(i ^ (i >> 8))) {
            ok = 0;
        }
    }
    ok = ok && x16_stack_isempty();
    ok = ok && (x16_bank_get() == 3);

    t_check(ok, "STACK_FILL_DRAIN");
}

/* The full flag. The module reports full once the pointer has wrapped
** below zero -- fill the whole 8 KB window and it must trip; empty it
** again and isempty must agree.
*/
static void test_stack_full_flag(void)
{
    unsigned int i;
    unsigned char ok;

    x16_stack_init(STACK_BANK);
    ok = !x16_stack_isfull();

    for (i = 0; i < 8192U; ++i) {
        x16_stack_push((unsigned char)i);
    }
    ok = ok && x16_stack_isfull();

    for (i = 0; i < 8192U; ++i) {
        x16_stack_pop();
    }
    ok = ok && x16_stack_isempty() && !x16_stack_isfull();

    t_check(ok, "STACK_FULL_FLAG");
}

/* ------------------------------------------------------------------ */
/* util/buffers: the rb_count / stk_depth additions                    */
/* ------------------------------------------------------------------ */

static void test_rb_count(void)
{
    unsigned char ok;

    x16_rb_init();
    ok = (x16_rb_count() == 0);
    x16_rb_put(5);
    x16_rb_put(6);
    x16_rb_put(7);
    ok = ok && (x16_rb_count() == 3);       /* puts minus gets */
    ok = ok && (x16_rb_get() == 5);
    ok = ok && (x16_rb_count() == 2);

    t_check(ok, "RB_COUNT");
}

static void test_stk_depth(void)
{
    unsigned char ok;

    x16_stk_init();
    ok = (x16_stk_depth() == 0);
    x16_stk_push(9);
    x16_stk_push(8);
    ok = ok && (x16_stk_depth() == 2);      /* pushes minus pops */
    ok = ok && (x16_stk_pop() == 8);
    ok = ok && (x16_stk_depth() == 1);

    t_check(ok, "STK_DEPTH");
}

/* ------------------------------------------------------------------ */
/* fileio                                                              */
/* ------------------------------------------------------------------ */

#define FIO_LEN 32

static unsigned char fio_pat(unsigned char i)
{
    return (unsigned char)(0xA5 ^ i);
}

/* Write a SEQ file one byte at a time through the channel API, then
** read it back whole through the loader -- an address path entirely
** independent of CHKOUT/CHROUT.
*/
static void test_fio_write(void)
{
    static const char wname[] = "FIOTEST.SEQ,S,W";
    static const char name[]  = "FIOTEST.SEQ";
    static unsigned char in[FIO_LEN];
    unsigned int end = 0;
    unsigned char i, err, ok;

    err = x16_fio_open_write(wname, sizeof wname - 1, 2, 8, 2);
    if (err) {
        t_check(0, "FIO_WRITE");
        return;
    }
    for (i = 0; i < FIO_LEN; ++i) {
        x16_fio_chrout(fio_pat(i));
    }
    x16_fio_close_named(2);

    err = x16_fs_load(name, sizeof name - 1, X16_DEVICE_SD, X16_SA_RAW,
                      in, &end);
    ok = (err == 0);
    for (i = 0; i < FIO_LEN; ++i) {
        if (in[i] != fio_pat(i)) {
            ok = 0;
        }
    }
    ok = ok && (end == (unsigned int)in + FIO_LEN);

    t_check(ok, "FIO_WRITE");
}

/* Read the same file back through CHRIN and GETIN (on a file channel
** GETIN reads like CHRIN), watching READST: clear mid-file, end-of-file
** bit on the last byte.
*/
static void test_fio_read_eof(void)
{
    static const char rname[] = "FIOTEST.SEQ,S,R";
    unsigned char i, err, ok;

    err = x16_fio_open_read(rname, sizeof rname - 1, 2, 8, 2);
    if (err) {
        t_check(0, "FIO_READ_EOF");
        return;
    }

    ok = 1;
    for (i = 0; i < FIO_LEN / 2; ++i) {
        if (x16_fio_chrin() != fio_pat(i)) {
            ok = 0;
        }
    }
    ok = ok && (x16_fio_readst() == 0);     /* mid-file: nothing to report */
    for (i = FIO_LEN / 2; i < FIO_LEN; ++i) {
        if (x16_fio_getin() != fio_pat(i)) {
            ok = 0;
        }
    }
    ok = ok && (x16_fio_readst() & X16_FIO_ST_EOF);
    x16_fio_close_named(2);

    t_check(ok, "FIO_READ_EOF");
}

/* The same open, decomposed into the raw SETNAM/SETLFS/OPEN/CHKIN
** wrappers.
*/
static void test_fio_raw_open(void)
{
    static const char rname[] = "FIOTEST.SEQ,S,R";
    unsigned char ok;

    x16_fio_set_name(rname, sizeof rname - 1);
    x16_fio_set_lfs(2, 8, 2);
    ok = (x16_fio_open() == 0);
    ok = ok && (x16_fio_chkin(2) == 0);
    ok = ok && (x16_fio_chrin() == fio_pat(0));
    x16_fio_clrchn();
    x16_fio_close(2);

    t_check(ok, "FIO_RAW_OPEN");
}

/* Selecting a never-opened logical file is KERNAL error 3, FILE NOT
** OPEN -- the error path of the carry-to-code shims.
*/
static void test_fio_chkin_unopened(void)
{
    t_check(x16_fio_chkin(7) == 3 && x16_fio_chkout(7) == 3,
            "FIO_CHKIN_UNOPENED");
}

/* close_all wipes the whole file table: a channel that selected fine a
** moment ago must now be FILE NOT OPEN.
*/
static void test_fio_close_all(void)
{
    static const char rname[] = "FIOTEST.SEQ,S,R";
    unsigned char ok;

    ok = (x16_fio_open_read(rname, sizeof rname - 1, 2, 8, 2) == 0);
    x16_fio_close_all();
    ok = ok && (x16_fio_chkin(2) != 0);

    t_check(ok, "FIO_CLOSE_ALL");
}

/* ------------------------------------------------------------------ */
/* iec                                                                 */
/* ------------------------------------------------------------------ */

/* Read the drive's status line the way the DOS wedge does: TALK 8,
** secondary $6F, ACPTR until CR. Returns its length; NUL-terminates.
** Reading it also clears the pending status.
*/
static unsigned char iec_read_status(char *buf, unsigned char max)
{
    unsigned char n = 0;
    unsigned char total = 0;
    unsigned char c;

    x16_iec_talk_channel(8, 15);
    for (;;) {
        c = x16_iec_acptr();
        if (c == 0x0D) {
            break;
        }
        if (n < max - 1) {
            buf[n++] = c;
        }
        if (x16_iec_readst()) {
            break;                      /* timeout/EOF: do not spin */
        }
        if (++total >= 80) {
            break;
        }
    }
    x16_iec_untalk();
    buf[n] = 0;
    return n;
}

/* The status line is always parsable: two digits, a comma, then text. */
static void test_iec_talk_status(void)
{
    static char buf[48];
    unsigned char n = iec_read_status(buf, sizeof buf);

    t_check(n >= 3 &&
            buf[0] >= '0' && buf[0] <= '9' &&
            buf[1] >= '0' && buf[1] <= '9' &&
            buf[2] == ',',
            "IEC_TALK_STATUS");
}

/* Scratch a file through the raw LISTEN path -- LISTEN 8, secondary
** $6F, the command bytes, UNLISTEN -- and prove it worked twice over:
** the drive answers a success code, and the file no longer loads.
*/
static void test_iec_listen_cmd(void)
{
    static const char name[] = "IECDEL.BIN";
    static const char cmd[]  = "S:IECDEL.BIN";
    static char buf[48];
    static unsigned char junk[4] = { 1, 2, 3, 4 };
    unsigned char i, n, code, ok;

    ok = (x16_fs_save(name, sizeof name - 1, X16_DEVICE_SD,
                      junk, junk + sizeof junk) == 0);
    iec_read_status(buf, sizeof buf);   /* clear whatever is pending */

    x16_iec_data_channel(8, 15);
    for (i = 0; i < sizeof cmd - 1; ++i) {
        x16_iec_ciout(cmd[i]);
    }
    x16_iec_unlisten();

    n = iec_read_status(buf, sizeof buf);
    code = (unsigned char)((buf[0] - '0') * 10 + (buf[1] - '0'));

    ok = ok && (n >= 3);
    ok = ok && (code < X16_DOS_OK_BELOW);   /* the drive said yes */
    ok = ok && (x16_fs_load(name, sizeof name - 1, X16_DEVICE_SD,
                            X16_SA_RAW, junk, (unsigned int *)0) != 0);

    t_check(ok, "IEC_LISTEN_CMD");
}

/* Block-read the fileio test file over MACPTR. */
static void test_iec_macptr(void)
{
    static const char rname[] = "FIOTEST.SEQ,S,R";
    static unsigned char in[FIO_LEN];
    unsigned char i, ok;
    int n;

    if (x16_fio_open_read(rname, sizeof rname - 1, 2, 8, 2) != 0) {
        t_check(0, "IEC_MACPTR");
        return;
    }
    n = x16_iec_macptr(FIO_LEN, in);
    x16_fio_close_named(2);

    ok = (n == FIO_LEN);
    for (i = 0; i < FIO_LEN; ++i) {
        if (in[i] != fio_pat(i)) {
            ok = 0;
        }
    }
    t_check(ok, "IEC_MACPTR");
}

/* Block-write a file over MCIOUT, verify through the loader. */
static void test_iec_mciout(void)
{
    static const char wname[] = "IECOUT.SEQ,S,W";
    static const char name[]  = "IECOUT.SEQ";
    static unsigned char out[16];
    static unsigned char in[16];
    unsigned char i, ok;
    int n;

    for (i = 0; i < 16; ++i) {
        out[i] = (unsigned char)(0x3C ^ i);
        in[i] = 0;
    }

    if (x16_fio_open_write(wname, sizeof wname - 1, 2, 8, 2) != 0) {
        t_check(0, "IEC_MCIOUT");
        return;
    }
    n = x16_iec_mciout(16, out);
    x16_fio_close_named(2);

    ok = (n == 16);
    ok = ok && (x16_fs_load(name, sizeof name - 1, X16_DEVICE_SD,
                            X16_SA_RAW, in, (unsigned int *)0) == 0);
    for (i = 0; i < 16; ++i) {
        if (in[i] != (unsigned char)(0x3C ^ i)) {
            ok = 0;
        }
    }
    t_check(ok, "IEC_MCIOUT");
}

/* ------------------------------------------------------------------ */
/* dir                                                                 */
/* ------------------------------------------------------------------ */

/* Walk the current directory and find a file this test wrote. The
** header line has to come back as HOST and the file as PRG -- proving
** the type is read, not assumed -- and the name has to match exactly,
** which means the quotes were stripped and the terminator placed.
*/
static void test_dir_walk(void)
{
    static const char name[] = "DIRA.BIN";
    static unsigned char src[16];
    static char entry[40];
    unsigned char found = 0;
    unsigned char ftype = 0xFF;
    unsigned char guard, ok;

    if (x16_fs_save(name, sizeof name - 1, X16_DEVICE_SD,
                    src, src + sizeof src) != 0) {
        t_check(0, "DIR_WALK");
        return;
    }

    if (!x16_dir_open("", 0, 8)) {      /* len 0: "$", the current dir */
        t_check(0, "DIR_WALK");
        return;
    }

    ok = x16_dir_next(entry, sizeof entry);         /* the header line */
    ok = ok && (x16_dir_type() == X16_DIR_TYPE_HOST);

    for (guard = 0; guard < 50; ++guard) {
        if (!x16_dir_next(entry, sizeof entry)) {
            break;                      /* the listing terminated */
        }
        if (strcmp(entry, name) == 0) {
            found = 1;
            ftype = x16_dir_type();
        }
    }
    x16_dir_close();

    ok = ok && (guard < 50);
    ok = ok && found && (ftype == X16_DIR_TYPE_PRG);

    t_check(ok, "DIR_WALK");
}

/* The block counts come from the listing's line numbers: an 18-byte
** file is one block under any rounding, a 1018-byte file four or five,
** and the plumbing would hand back garbage for either.
*/
static void test_dir_blocks(void)
{
    static const char small_name[] = "DIRA.BIN";
    static const char big_name[]   = "DIRBIG.BIN";
    static unsigned char big[1016];
    static char entry[40];
    unsigned int small_blocks = 0xFFFF;
    unsigned int big_blocks = 0xFFFF;
    unsigned char guard, ok;

    if (x16_fs_save(big_name, sizeof big_name - 1, X16_DEVICE_SD,
                    big, big + sizeof big) != 0) {
        t_check(0, "DIR_BLOCKS");
        return;
    }

    if (!x16_dir_open("", 0, 8)) {
        t_check(0, "DIR_BLOCKS");
        return;
    }
    for (guard = 0; guard < 50; ++guard) {
        if (!x16_dir_next(entry, sizeof entry)) {
            break;
        }
        if (strcmp(entry, small_name) == 0) {
            small_blocks = x16_dir_blocks();
        } else if (strcmp(entry, big_name) == 0) {
            big_blocks = x16_dir_blocks();
        }
    }
    x16_dir_close();

    ok = (small_blocks == 1);
    ok = ok && (big_blocks >= 4) && (big_blocks <= 5);

    t_check(ok, "DIR_BLOCKS");
}

/* A 4-byte buffer must yield at most 3 name characters plus the
** terminator, and never a byte beyond it.
*/
static void test_dir_trunc(void)
{
    static char entry[8];
    unsigned char guard, ok, saw_three;

    if (!x16_dir_open("", 0, 8)) {
        t_check(0, "DIR_TRUNC");
        return;
    }

    ok = 1;
    saw_three = 0;
    for (guard = 0; guard < 50; ++guard) {
        entry[4] = 0x7F;                /* sentinel just past the buffer */
        if (!x16_dir_next(entry, 4)) {
            break;
        }
        if (strlen(entry) > 3 || entry[4] != 0x7F) {
            ok = 0;
        }
        if (strlen(entry) == 3) {
            saw_three = 1;              /* something really was truncated */
        }
    }
    x16_dir_close();

    ok = ok && (guard < 50) && saw_three;

    t_check(ok, "DIR_TRUNC");
}

/* An explicit "$" takes the named-path branch: the pointer and length
** have to arrive intact for this to open at all.
*/
static void test_dir_named(void)
{
    static const char path[] = "$";
    static char entry[40];
    unsigned char ok;

    ok = x16_dir_open(path, sizeof path - 1, 8);
    if (ok) {
        ok = x16_dir_next(entry, sizeof entry) &&
             (x16_dir_type() == X16_DIR_TYPE_HOST);
        x16_dir_close();
    }

    t_check(ok, "DIR_NAMED");
}

/* A device that is not there cannot open a directory. Last of the dir
** tests: a failed OPEN can leave the logical file half-registered, so
** tidy it up rather than tripping a later test.
*/
static void test_dir_open_missing(void)
{
    unsigned char ok = !x16_dir_open("", 0, 30);

    x16_fio_close(3);                   /* tidy the LFN either way */
    t_check(ok, "DIR_OPEN_MISSING");
}

/* ------------------------------------------------------------------ */

int main(void)
{
    t_init();

    test_ring_fifo();
    test_ring_word();
    test_ring_fill_drain();
    test_ring_wrap();

    test_stack_lifo();
    test_stack_word();
    test_stack_fill_drain();
    test_stack_full_flag();

    test_rb_count();
    test_stk_depth();

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

    /* Leave the -fsroot scratch directory the way we found it. */
    x16_dos_delete("DIRA.BIN", 8);
    x16_dos_delete("DIRBIG.BIN", 10);
    x16_dos_delete("FIOTEST.SEQ", 11);
    x16_dos_delete("IECOUT.SEQ", 10);

    t_done();
    return 0;
}
