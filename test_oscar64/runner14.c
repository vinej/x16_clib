/* =====================================================================
 * x16clib :: test_oscar64/runner14.c -- the file browser, driven headless
 * =====================================================================
 * STANDALONE suite:
 *
 *      .\build_oscar64.ps1 -Test -Source test_oscar64\runner14.c
 *
 * Ported from test_ca65/runner11.c, check for check.
 *
 * The browser is modal -- x16_fp_open() does not return until the user
 * answers -- but its input loop drains the KERNAL keyboard buffer with
 * GETIN, and KBDBUF_PUT ($FEC3) appends to that same buffer. So each
 * test scripts its whole session BEFORE calling open: the injected keys
 * sit in the queue (10 deep, scripts here stay well under it) and the
 * loop plays them back one per poll. No key ever arrives "too early",
 * because the loop only reads the buffer, never the hardware.
 *
 * The scripts navigate by CLAMPING rather than counting: the listing is
 * directories first, then files, and pressing Down more times than
 * there are entries pins the selection to the LAST entry -- the last
 * file -- without the test having to know whether the host filesystem
 * listed ".." or what order readdir picked. One Up from there is the
 * last directory. Every target below is made unambiguous that way.
 *
 * Files are created through x16_fs_save()/x16_dos_mkdir() -- a path
 * entirely independent of the picker -- and the picker's answers are
 * verified against those names byte for byte (testlib.h sets
 * #pragma encoding(ascii), and the emulator's host filesystem stores
 * exactly the bytes it was given). The delete test closes the loop the
 * other way round: the picker deletes, then x16_fs_load() and
 * x16_dos_rmdir() prove the file is really gone.
 * ===================================================================== */

#include "testlib.h"

#include <x16/filepick.h>
#include <x16/fileio.h>
#include <x16/load.h>
#include <x16/dos.h>
#include <x16/dir.h>

/* The 80x60 text map: map base $1B000, 128-tile map width = 256-byte
** row stride, two bytes (char, colour) per cell. Bank 1 holds $1xxxx.
*/
#define TMAP_LOW        0xB000U
#define TMAP_BANK       1
#define TMAP_STRIDE     256U

/* The keys the scripts send, as the PETSCII bytes GETIN hands back:
** an unshifted letter is $41-$5A, the codes ASCII uses for capitals.
*/
#define K_STOP  0x03
#define K_RET   0x0D
#define K_DOWN  0x11
#define K_HOME  0x13
#define K_ESC   0x1B
#define K_A     0x41
#define K_D     0x44
#define K_H     0x48
#define K_N     0x4E
#define K_UP    0x91
#define K_X     0x58
#define K_Y     0x59

/* ------------------------------------------------------------------ */

void t_vsetaddr(unsigned char bank, unsigned int addr) {
    __asm {
        lda 0x9f25
        and #0xfe
        sta 0x9f25
        lda addr
        sta 0x9f20
        lda addr+1
        sta 0x9f21
        lda bank
        and #0x01
        sta 0x9f22                       /* increment index 0 */
    }
}

unsigned char t_vpeek(unsigned char bank, unsigned int addr) {
    t_vsetaddr(bank, addr);
    return __asm {
        lda 0x9f23
        sta accu
    };
}

void t_vpoke(unsigned char bank, unsigned int addr, unsigned char v) {
    t_vsetaddr(bank, addr);
    __asm {
        lda v
        sta 0x9f23
    }
}

/* KBDBUF_PUT: append one PETSCII key to the KERNAL keyboard buffer. */
static unsigned char kbd_byte;

static void kbd_put(unsigned char c)
{
    kbd_byte = c;
    __asm {
        lda kbd_byte
        jsr 0xfec3
    }
}

/* Drain whatever a previous script left behind: the KERNAL buffer holds
** ten keys, and a stale one would steer the next test.
*/
static void kbd_drain(void)
{
    while (x16_fio_getin() != 0) {
        /* discard */
    }
}

/* filepick polls GETIN in an unbounded loop -- it exits only when a key
** tells it to. Headless there is no one at the keyboard, so every script
** gets a STOP appended: if the intended keys already left the picker,
** the extra byte is drained before the next test; if they did not, STOP
** ends the loop instead of hanging the run forever.
*/
static void inject(const unsigned char *keys, unsigned char n)
{
    unsigned char i;

    kbd_drain();
    for (i = 0; i < n; ++i) {
        kbd_put(keys[i]);
    }
    kbd_put(K_STOP);
}

/* ------------------------------------------------------------------ */

static char pathbuf[64];
static char namebuf[64];
static char dirbuf[64];

static unsigned char t_streq(const char *a, const char *b)
{
    while (*a != 0 && *a == *b) {
        ++a;
        ++b;
    }
    if (*a == *b) {
        return 1;
    }
    return 0;
}

static unsigned char t_slen(const char *s)
{
    unsigned char n = 0;

    while (s[n] != 0) {
        ++n;
    }
    return n;
}

/* 1 if any cell in the panel's span of `row` holds a non-space
** character -- the picker really drew something there.
*/
static unsigned char row_has_ink(unsigned char row)
{
    unsigned char col;

    for (col = 6; col < 74; ++col) {
        if (t_vpeek(TMAP_BANK,
                    TMAP_LOW + row * TMAP_STRIDE + col * 2) != 0x20) {
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */

/* The suite shares one fsroot and this runner goes last, so earlier
** runners' files are still there, and each one shifts every navigation
** below it. Walk the root and delete every file this test does not own,
** so the picker sees exactly the listing the scripts were written
** against no matter what ran first. Directories are left alone --
** SUBDIR is part of the fixture.
*/
static void clear_strays(void)
{
    char entry[32];

    if (!x16_dir_open("$", 1, X16_DEVICE_SD)) {
        return;
    }
    while (x16_dir_next(entry, 32)) {
        /* Delete every FILE this test does not own, whatever its type.
        ** ca65's version of this only swept PRG and SEQ, which was
        ** enough for the order ITS suite runs in; this suite leaves
        ** different things behind, and with a "*.*" filter the picker
        ** lists all of them -- which shifts every navigation the scripts
        ** below depend on. Directories are still left alone: SUBDIR is
        ** part of the fixture.
        */
        if (x16_dir_type() == X16_DIR_TYPE_DIR ||
            x16_dir_type() == X16_DIR_TYPE_NONE) {
            continue;
        }
        if (t_streq(entry, "PICKA.BIN") || t_streq(entry, "PICKB.TXT")) {
            continue;                   /* the two files this test owns */
        }
        x16_dos_delete(entry, t_slen(entry));
    }
    x16_dir_close();
}

/* The directory tree the picker browses:
**
**      /PICKA.BIN          the only .bin at the root
**      /PICKB.TXT          data under a "*.bin" primary
**      /SUBDIR/NESTED.BIN  reached by descending, deleted by the picker
**
** Directories survive between runs, so SUBDIR (and its file) may be a
** stale leftover of an aborted run: everything here creates-or-replaces.
*/
static unsigned char payload[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

static unsigned char fs_setup(void)
{
    unsigned char ok = 1;

    x16_dos_chdir("/", 1);
    clear_strays();
    ok &= (x16_fs_save("PICKA.BIN", 9, X16_DEVICE_SD,
                       payload, payload + 8) == 0);
    ok &= (x16_fs_save("PICKB.TXT", 9, X16_DEVICE_SD,
                       payload, payload + 8) == 0);

    x16_dos_mkdir("SUBDIR", 6);                 /* may already exist */
    ok &= (x16_dos_chdir("SUBDIR", 6) < X16_DOS_OK_BELOW);
    x16_dos_delete("NESTED.BIN", 10);           /* may not exist */
    ok &= (x16_fs_save("NESTED.BIN", 10, X16_DEVICE_SD,
                       payload, payload + 8) == 0);
    x16_dos_chdir("/", 1);

    return ok;
}

/* ------------------------------------------------------------------ */

/* The matcher, before any panel is up: the same routine the listing
** filters with. Case folds both ways, ';' lists try every pattern,
** NULL matches everything, and "*." alone is not a pattern.
*/
static void test_match(void)
{
    unsigned char ok = 1;

    ok &= (x16_fp_match("game.prg", "*.prg") == 1);
    ok &= (x16_fp_match("game.prg", "*.txt") == 0);
    ok &= (x16_fp_match("GAME.PRG", "*.prg") == 1);     /* folds case */
    ok &= (x16_fp_match("pic.bmx", "*.bmx;*.png") == 1);
    ok &= (x16_fp_match("shot.png", "*.bmx;*.png") == 1);
    ok &= (x16_fp_match("shot.gif", "*.bmx;*.png") == 0);
    ok &= (x16_fp_match("anything", (const char *)0) == 1);
    ok &= (x16_fp_match("noext", "*.*") == 1);
    ok &= (x16_fp_match("noext", "*.prg") == 0);

    t_check(ok, "FP_MATCH");
}

/* The 80x60 defaults, and the fixed top row. Valid only once open() has
** sized the panel, so this runs after the first session.
*/
static void test_geometry(void)
{
    t_check((x16_fp_panel_top() == 3 &&
            x16_fp_panel_left() == 6 &&
            x16_fp_panel_width() == 68 &&
            x16_fp_panel_rows() == 40) ? 1 : 0,
            "FP_GEOMETRY");
}

/* Down past the end clamps on the last entry -- the only .bin file at
** the root -- and Enter picks it. All three accessors must agree.
*/
static void test_nav_pick(void)
{
    static const unsigned char script[] = {
        K_DOWN, K_DOWN, K_DOWN, K_DOWN, K_DOWN, K_DOWN, K_RET
    };
    unsigned char ret, plen, drew;

    x16_fp_filter("*.bin");
    x16_fp_start_dir("/");

    inject(script, 7);
    ret = x16_fp_open();

    drew = row_has_ink(3);              /* the header row, panel still up */

    plen = x16_fp_path(pathbuf, 64);
    x16_fp_name(namebuf, 64);
    x16_fp_dir(dirbuf, 64);
    x16_fp_close();

    t_check((ret == X16_FPK_PICK &&
            plen == 10 &&
            t_streq(pathbuf, "/PICKA.BIN") &&
            t_streq(namebuf, "PICKA.BIN") &&
            t_streq(dirbuf, "/")) ? 1 : 0,
            "FP_NAV_PICK");

    /* The vpeek spot-check: the picker really wrote the text map. */
    t_check(drew, "FP_DRAW");

    /* A 4-byte destination keeps 3 characters and the terminator. */
    t_check((x16_fp_path(pathbuf, 4) == 3 &&
            t_streq(pathbuf, "/PI")) ? 1 : 0,
            "FP_COPY_TRUNC");
}

/* Clamp to the last entry (a file), one Up to the last DIRECTORY --
** SUBDIR -- Enter descends, clamp again, Enter picks the nested file.
*/
static void test_nav_descend(void)
{
    static const unsigned char script[] = {
        K_DOWN, K_DOWN, K_DOWN, K_UP, K_RET,
        K_DOWN, K_DOWN, K_DOWN, K_RET
    };
    unsigned char ret;

    x16_fp_filter("*.bin");
    x16_fp_start_dir("/");

    inject(script, 9);
    ret = x16_fp_open();

    x16_fp_path(pathbuf, 64);
    x16_fp_dir(dirbuf, 64);
    x16_fp_close();

    t_check((ret == X16_FPK_PICK &&
            t_streq(pathbuf, "/SUBDIR/NESTED.BIN") &&
            t_streq(dirbuf, "/SUBDIR")) ? 1 : 0,
            "FP_NAV_DESCEND");
}

/* 'h' answers with the PLACE, not a file: FPK_HERE plus the directory
** the panel was showing. Also proves start_dir steered the open.
*/
static void test_here(void)
{
    static const unsigned char script[] = { K_H };
    unsigned char ret;

    x16_fp_filter("*.bin");
    x16_fp_start_dir("/SUBDIR");

    inject(script, 1);
    ret = x16_fp_open();

    x16_fp_dir(dirbuf, 64);
    x16_fp_close();

    t_check((ret == X16_FPK_HERE &&
            t_streq(dirbuf, "/SUBDIR")) ? 1 : 0,
            "FP_HERE");
}

/* ESC cancels. */
static void test_cancel(void)
{
    static const unsigned char script[] = { K_ESC };
    unsigned char ret;

    x16_fp_start_dir("/");

    inject(script, 1);
    ret = x16_fp_open();
    x16_fp_close();

    t_check((ret == X16_FPK_NONE) ? 1 : 0, "FP_CANCEL");
}

/* Save-under: a cell the panel covers is repainted while the panel is
** up and put back by close. Both halves matter -- "unchanged" alone
** would also pass if the panel never drew.
*/
static void test_saveunder(void)
{
    static const unsigned char script[] = { K_ESC };
    unsigned int cell = TMAP_LOW + 10 * TMAP_STRIDE + 10 * 2;
    unsigned char covered, restored;

    t_vpoke(TMAP_BANK, cell, 0x51);     /* a ball, not a space */

    x16_fp_saveunder(1, 0x14000UL);
    inject(script, 1);
    x16_fp_open();
    covered = (t_vpeek(TMAP_BANK, cell) != 0x51) ? 1 : 0;
    x16_fp_close();
    restored = (t_vpeek(TMAP_BANK, cell) == 0x51) ? 1 : 0;
    x16_fp_saveunder(0, 0x14000UL);

    t_check((covered && restored) ? 1 : 0, "FP_SAVEUNDER");
}

/* 'a' on a file is the ALT gesture; resume then puts the same panel
** back up, where ESC still cancels.
*/
static void test_alt_resume(void)
{
    static const unsigned char alt[] = {
        K_DOWN, K_DOWN, K_DOWN, K_DOWN, K_DOWN, K_DOWN, K_A
    };
    static const unsigned char esc[] = { K_ESC };
    unsigned char ret1, ret2;

    x16_fp_filter("*.bin");
    x16_fp_start_dir("/");

    inject(alt, 7);
    ret1 = x16_fp_open();
    x16_fp_name(namebuf, 64);

    inject(esc, 1);
    ret2 = x16_fp_resume();
    x16_fp_close();

    t_check((ret1 == X16_FPK_ALT &&
            t_streq(namebuf, "PICKA.BIN") &&
            ret2 == X16_FPK_NONE) ? 1 : 0,
            "FP_ALT_RESUME");
}

/* With a "*.*" filter and a "*.bin" primary, the primaries are listed
** before the data files: the LAST entry is the .txt (is_primary 0), the
** one above it the .bin (is_primary 1).
*/
static void test_primary(void)
{
    static const unsigned char last[] = {
        K_DOWN, K_DOWN, K_DOWN, K_DOWN, K_DOWN, K_RET
    };
    static const unsigned char above[] = {
        K_DOWN, K_DOWN, K_DOWN, K_DOWN, K_DOWN, K_UP, K_RET
    };
    unsigned char ret, prim;

    x16_fp_filter("*.*");
    x16_fp_primary("*.bin");
    x16_fp_start_dir("/");

    inject(last, 6);
    ret = x16_fp_open();
    x16_fp_name(namebuf, 64);
    prim = x16_fp_is_primary();
    x16_fp_close();

    t_check((ret == X16_FPK_PICK &&
            t_streq(namebuf, "PICKB.TXT") &&
            prim == 0) ? 1 : 0,
            "FP_PRIM_DATA");

    inject(above, 7);
    ret = x16_fp_open();
    x16_fp_name(namebuf, 64);
    prim = x16_fp_is_primary();
    x16_fp_close();

    t_check((ret == X16_FPK_PICK &&
            t_streq(namebuf, "PICKA.BIN") &&
            prim == 1) ? 1 : 0,
            "FP_PRIM_FILE");

    x16_fp_primary((const char *)0);
    x16_fp_filter((const char *)0);
}

/* 'n' prompts for a name (typed through the same buffer) and makes the
** folder. The independent proof is rmdir: it only succeeds on a
** directory that exists and is empty.
*/
static void test_edit_newdir(void)
{
    static const unsigned char script[] = { K_N, K_X, K_RET, K_ESC };
    unsigned char ret, gone;

    x16_fp_filter("*.bin");
    x16_fp_start_dir("/");

    inject(script, 4);
    ret = x16_fp_open();
    x16_fp_close();

    gone = x16_dos_rmdir("X", 1);

    t_check((ret == X16_FPK_NONE && gone < X16_DOS_OK_BELOW) ? 1 : 0,
            "FP_EDIT_NEWDIR");
}

/* 'd' plus the y confirm deletes the selected file, and the panel
** re-reads the directory before ESC hands control back. x16_fs_load()
** proves the file is gone from the drive, not merely from the panel.
*/
static void test_edit_delete(void)
{
    static const unsigned char script[] = {
        K_DOWN, K_DOWN, K_DOWN, K_D, K_Y, K_ESC
    };
    static unsigned char scratch[8];
    unsigned char ret, loaded;

    x16_fp_filter("*.bin");
    x16_fp_start_dir("/SUBDIR");

    inject(script, 6);
    ret = x16_fp_open();
    x16_fp_close();

    /* The drive was left standing in /SUBDIR. */
    loaded = x16_fs_load("NESTED.BIN", 10, X16_DEVICE_SD,
                         X16_SA_ADDR, scratch, (unsigned int *)0);

    t_check((ret == X16_FPK_NONE && loaded != 0) ? 1 : 0, "FP_EDIT_DELETE");
}

/* SUBDIR is empty now the picker deleted its file, so rmdir succeeds --
** the second, independent proof of the delete, and it leaves fsroot
** clean for the next run.
*/
static void test_cleanup(void)
{
    unsigned char code;

    x16_dos_chdir("/", 1);
    code = x16_dos_rmdir("SUBDIR", 6);

    t_check((code < X16_DOS_OK_BELOW) ? 1 : 0, "FP_CLEANUP");
}

/* ------------------------------------------------------------------ */

int main(void)
{
    t_init();

    t_check(fs_setup(), "FP_FSSETUP");

    test_match();
    test_nav_pick();
    test_geometry();            /* after the first open sized the panel */
    test_nav_descend();
    test_here();
    test_cancel();
    test_saveunder();
    test_alt_resume();
    test_primary();
    test_edit_newdir();
    test_edit_delete();
    test_cleanup();

    t_done();
    return 0;
}
