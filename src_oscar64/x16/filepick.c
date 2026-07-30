// =====================================================================
// x16clib :: x16/filepick.c -- a file browser on a panel
// =====================================================================
// UNFINISHED. This module is NOT wired into <x16/x16.h>, NOT listed in
// the README's supported set, and test_oscar64/runner14.c is NOT in
// build_oscar64.ps1's suite list -- on purpose. It compiles and 14 of
// the 16 ported checks pass; two do not, and until they do this must
// not be advertised as parity. Run it with
//
//      .\build_oscar64.ps1 -Test -Source test_oscar64\runner14.c
//
// FP_DRAW -- row_has_ink(3) reads all spaces across the panel's span of
//   the header row. Drawing DOES reach the text map: FP_SAVEUNDER's
//   "covered" check only passes because a body row was painted with
//   spaces over the test's sentinel, which is proof the blits land at
//   $1B000. So the panel is painting, and the header row specifically is
//   coming out blank -- suspect fp_puts_at's screen-code conversion or
//   the header's column arithmetic, not the addressing.
//
// FP_PRIM_FILE -- with filter "*.*" and primary "*.bin", five Downs then
//   one Up should select PICKA.BIN. FP_PRIM_DATA passes, so the LAST
//   entry really is PICKB.TXT and the clamp works; the entry above it is
//   not what the test expects. Either the listing carries an entry the
//   scripts do not account for (a host-filesystem name clear_strays
//   skips, since it only deletes PRG and SEQ), or the three-pass group
//   ordering puts something between the primary and the data file. DUMP
//   THE LISTING before theorising -- name and kind for every index --
//   rather than reasoning about what readdir ought to return.
//
// =====================================================================
// Written in C over this tree's own modules -- dir.c for the listing,
// dos.c for the editing, screen.c for the text map, fileio.c for the
// keyboard -- rather than transliterated from the 2,800-line ca65
// original. The behaviour is the same, and test_oscar64/runner14.c is
// the ca65 suite's checks, so the two are held to one contract.
//
// THE LISTING LIVES IN VRAM, not in a C array, because the header
// promises x16_fp_cache() means something: 2,560 bytes, 32 to an entry,
// so 80 entries. Upstream needs VRAM because the module can be banked
// and a banked module cannot page a bank into the window it executes
// from; here it is simply the documented contract, and 2.5 KB is real
// money on a machine with 38 KB of low RAM.
//
//      entry[0]      1 = directory, 0 = file
//      entry[1]      1 = matches the primary pattern
//      entry[2..31]  the name, NUL-terminated (29 characters)
//
// ORDER IS LOAD-BEARING: directories first, then files that match the
// primary pattern, then the rest. A launcher listing "*.*" with a
// primary of "*.prg" therefore shows programs above data, and
// x16_fp_is_primary() tells the caller which kind came back. The three
// groups are three passes over the directory, which costs three reads
// and saves holding the whole listing twice.
//
// The event loop reads the KERNAL keyboard buffer through GETIN and
// never touches the hardware, which is what lets a test script a whole
// session by pushing keys into that buffer before calling open(). It is
// UNBOUNDED: it returns when a key says so and not before.
// =====================================================================

#include <x16/filepick.h>
#include <x16/dir.h>
#include <x16/dos.h>
#include <x16/fileio.h>
#include <x16/screen.h>
#include <x16/vera.h>
#include <x16/input.h>
#include <x16/load.h>

#define FP_ENT_SIZE     32
#define FP_MAX_ENT      80              // 80 * 32 = the 2,560-byte cache
#define FP_NAME_MAX     29              // what is left of an entry

#define TMAP_BASE       0x1B000UL
#define TMAP_STRIDE     256U            // 128 tiles wide, two bytes each

// The keys, as the PETSCII bytes GETIN hands back.
#define K_STOP  0x03
#define K_RET   0x0D
#define K_DOWN  0x11
#define K_HOME  0x13
#define K_DEL   0x14
#define K_ESC   0x1B
#define K_UP    0x91

// --- configuration, all with the documented defaults ------------------
static const char *fp_filt;             // 0 = "*.*"
static const char *fp_prim;             // 0 = the filter
static const char *fp_head;             // 0 = "files in "
static const char *fp_foot;
static const char *fp_startat;          // 0 = "/"

static unsigned char fp_apanel = 0xF6;  // blue on light grey
static unsigned char fp_abar   = 0xF6;
static unsigned char fp_asel   = 0x6F;  // inverted
static unsigned char fp_chset  = 3;     // PET upper/lower
static unsigned long fp_cachev = 0x12000UL;
static unsigned long fp_underv = 0x14000UL;
static unsigned char fp_undon  = 0;

// --- geometry, sized to the screen open() finds -----------------------
static unsigned char fp_scrw = 80, fp_scrh = 60;
static unsigned char fp_top = 3, fp_left = 6, fp_wide = 68, fp_rows = 40;

// --- session state ----------------------------------------------------
static char fp_curdir[64] = { '/', 0 };
static char fp_nm[FP_NAME_MAX + 1];     // the chosen entry's name
static char fp_full[96];                // ...and its absolute path
static char fp_clip[96];                // what 'c' remembered
static unsigned char fp_clipok;
static unsigned char fp_nent;           // entries in the cache
static unsigned char fp_sel;            // selected index
static unsigned char fp_scroll;         // index of the top visible row
static unsigned char fp_prime;          // is_primary of the chosen entry
static unsigned char fp_saved;          // the save-under holds something

// =====================================================================
// Small string helpers. The module carries its own rather than pulling
// <string.h> into every program that opens a panel.
// =====================================================================
static unsigned char fp_len(const char *s) {
    unsigned char n = 0;

    while (s[n] != 0) {
        ++n;
    }
    return n;
}

static void fp_cpy(char *d, const char *s, unsigned char max) {
    unsigned char i = 0;

    while (s[i] != 0 && i < max) {
        d[i] = s[i];
        ++i;
    }
    d[i] = 0;
}

// Upper-case one ASCII/PETSCII letter, so matching folds case.
static char fp_up(char c) {
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 32);
    }
    return c;
}

// =====================================================================
// The matcher. One pattern is "*.ext"; a ';' joins alternatives; NULL
// matches everything, and so does "*.*".
// =====================================================================
static unsigned char fp_match_one(const char *name, const char *pat,
                                  unsigned char plen) {
    unsigned char nlen, i;
    const char *ext;

    if (plen == 0) {
        return 0;
    }
    // "*.*" matches anything, extension or not.
    if (plen == 3 && pat[0] == '*' && pat[1] == '.' && pat[2] == '*') {
        return 1;
    }
    // Anything that is not "*.something" is not a pattern we know.
    if (plen < 3 || pat[0] != '*' || pat[1] != '.') {
        return 0;
    }

    nlen = fp_len(name);
    if (nlen < plen - 1) {              // needs at least ".ext"
        return 0;
    }
    // Compare the tail, dot included, folding case.
    ext = name + (nlen - (plen - 1));
    for (i = 1; i < plen; ++i) {
        if (fp_up(ext[i - 1]) != fp_up(pat[i])) {
            return 0;
        }
    }
    return 1;
}

unsigned char x16_fp_match(const char *name, const char *patterns) {
    const char *p;
    unsigned char n;

    if (patterns == 0) {
        return 1;                       // no filter means everything
    }
    p = patterns;
    for (;;) {
        n = 0;
        while (p[n] != 0 && p[n] != ';') {
            ++n;
        }
        if (fp_match_one(name, p, n)) {
            return 1;
        }
        if (p[n] == 0) {
            return 0;
        }
        p += n + 1;                     // step past the ';'
    }
}

// The pattern a file must match to count as primary: the primary list,
// falling back to the filter, then to everything.
static const char *fp_primary_pat(void) {
    if (fp_prim != 0) {
        return fp_prim;
    }
    return fp_filt;                     // may itself be 0 = everything
}

// =====================================================================
// The VRAM listing cache.
// =====================================================================
static void fp_ent_seek(unsigned char idx) {
    x16_vera_addr0(X16_INC_1, fp_cachev + (unsigned long)idx * FP_ENT_SIZE);
}

static void fp_ent_put(unsigned char idx, unsigned char kind,
                       unsigned char primary, const char *name) {
    unsigned char i;

    fp_ent_seek(idx);
    __asm {
        lda kind
        sta 0x9f23                      /* VERA_DATA0, auto-incrementing */
        lda primary
        sta 0x9f23
    }
    for (i = 0; i < FP_NAME_MAX; ++i) {
        unsigned char c = (unsigned char)name[i];

        __asm {
            lda c
            sta 0x9f23
        }
        if (c == 0) {
            break;                      // the rest of the entry is stale
        }
    }
}

static unsigned char fp_ent_kind(unsigned char idx) {
    fp_ent_seek(idx);
    return __asm {
        lda 0x9f23
        sta accu
    };
}

static unsigned char fp_ent_primary(unsigned char idx) {
    x16_vera_addr0(X16_INC_1, fp_cachev + (unsigned long)idx * FP_ENT_SIZE + 1);
    return __asm {
        lda 0x9f23
        sta accu
    };
}

static void fp_ent_name(unsigned char idx, char *dest) {
    unsigned char i, c;

    x16_vera_addr0(X16_INC_1, fp_cachev + (unsigned long)idx * FP_ENT_SIZE + 2);
    for (i = 0; i < FP_NAME_MAX; ++i) {
        c = __asm {
            lda 0x9f23
            sta accu
        };
        dest[i] = (char)c;
        if (c == 0) {
            return;
        }
    }
    dest[FP_NAME_MAX] = 0;
}

// =====================================================================
// Reading the directory into the cache, in three passes so the groups
// come out in order.
// =====================================================================
static void fp_scan(void) {
    char name[FP_NAME_MAX + 1];
    unsigned char pass, t, isdir, prim;
    const char *primpat = fp_primary_pat();

    fp_nent = 0;

    for (pass = 0; pass < 3; ++pass) {
        if (!x16_dir_open("$", 1, X16_DEVICE_SD)) {
            return;
        }
        while (x16_dir_next(name, sizeof(name))) {
            if (fp_nent >= FP_MAX_ENT) {
                break;
            }
            t = x16_dir_type();
            isdir = (t == X16_DIR_TYPE_DIR) ? 1 : 0;

            if (pass == 0) {
                // Directories, always, whatever the filter says -- there
                // would otherwise be no way to reach the file you wanted.
                if (!isdir) {
                    continue;
                }
                fp_ent_put(fp_nent, 1, 0, name);
                ++fp_nent;
                continue;
            }

            if (isdir || t == X16_DIR_TYPE_NONE) {
                continue;
            }
            if (!x16_fp_match(name, fp_filt)) {
                continue;
            }
            prim = x16_fp_match(name, primpat);
            if ((pass == 1 && !prim) || (pass == 2 && prim)) {
                continue;
            }
            fp_ent_put(fp_nent, 0, prim, name);
            ++fp_nent;
        }
        x16_dir_close();
    }

    if (fp_sel >= fp_nent) {
        fp_sel = (fp_nent != 0) ? (unsigned char)(fp_nent - 1) : 0;
    }
}

// =====================================================================
// Drawing. Everything goes through screen.c's text-map blitter, which
// writes character/colour pairs at the address x16_screen_addr() set.
// =====================================================================
static void fp_puts_at(unsigned char row, unsigned char col,
                       const char *s, unsigned char max,
                       unsigned char color) {
    unsigned char i;
    char sc[96];
    unsigned char n = 0;

    for (i = 0; i < max && s[i] != 0; ++i) {
        sc[n] = (char)x16_screen_scode((unsigned char)s[i]);
        ++n;
    }
    if (n == 0) {
        return;
    }
    x16_screen_addr(row, col);
    x16_screen_blit(sc, n, color);
}

static void fp_fill_row(unsigned char row, unsigned char color) {
    x16_screen_addr(row, fp_left);
    x16_screen_blitfill(fp_wide, color, 0x20);
}

// One listing row: the name, a [dir] or [dat] tag, and the selection
// colour when it is the current entry.
static void fp_draw_row(unsigned char slot) {
    unsigned char idx = fp_scroll + slot;
    unsigned char row = fp_top + 1 + slot;
    unsigned char color;
    char name[FP_NAME_MAX + 1];

    color = (idx == fp_sel) ? fp_asel : fp_apanel;
    fp_fill_row(row, color);
    if (idx >= fp_nent) {
        return;
    }

    fp_ent_name(idx, name);
    fp_puts_at(row, fp_left + 1, name, FP_NAME_MAX, color);

    if (fp_ent_kind(idx)) {
        fp_puts_at(row, fp_left + fp_wide - 6, "[dir]", 5, color);
    } else if (!fp_ent_primary(idx)) {
        fp_puts_at(row, fp_left + fp_wide - 6, "[dat]", 5, color);
    }
}

void x16_fp_redraw(void) {
    unsigned char i;

    // Header: the heading text then the directory being browsed.
    fp_fill_row(fp_top, fp_abar);
    fp_puts_at(fp_top, fp_left + 1,
               (fp_head != 0) ? fp_head : "files in ", 32, fp_abar);
    fp_puts_at(fp_top, fp_left + 1
                       + fp_len((fp_head != 0) ? fp_head : "files in "),
               fp_curdir, 40, fp_abar);
    // The x box, at the right end of the bar.
    fp_puts_at(fp_top, fp_left + fp_wide - 2, "x", 1, fp_abar);

    for (i = 0; i < fp_rows; ++i) {
        fp_draw_row(i);
    }

    fp_fill_row(fp_top + fp_rows + 1, fp_abar);
    if (fp_foot != 0) {
        fp_puts_at(fp_top + fp_rows + 1, fp_left + 1, fp_foot,
                   fp_wide - 2, fp_abar);
    }
}

// =====================================================================
// The save-under. The text map IS VRAM, so keeping what the panel covers
// is a VRAM-to-VRAM copy of the panel's rows.
// =====================================================================
static void fp_under(unsigned char save) {
    unsigned long src, dst;
    unsigned char r, nrows = fp_rows + 2;

    dst = fp_underv;
    for (r = 0; r < nrows; ++r) {
        src = TMAP_BASE + (unsigned long)(fp_top + r) * TMAP_STRIDE
              + (unsigned long)fp_left * 2;
        if (save) {
            x16_vera_addr0(X16_INC_1, src);
            x16_vera_addr1(X16_INC_1, dst);
        } else {
            x16_vera_addr0(X16_INC_1, dst);
            x16_vera_addr1(X16_INC_1, src);
        }
        x16_vera_copy((unsigned int)fp_wide * 2);
        dst += (unsigned long)fp_wide * 2;
    }
}

// =====================================================================
// Paths. fp_full is the absolute path of the selected entry; joining
// never doubles the separator at the root.
// =====================================================================
static void fp_join(char *dest, const char *dir, const char *name) {
    unsigned char n = 0;

    while (dir[n] != 0) {
        dest[n] = dir[n];
        ++n;
    }
    if (n == 0 || dest[n - 1] != '/') {
        dest[n] = '/';
        ++n;
    }
    while (*name != 0) {
        dest[n] = *name;
        ++n;
        ++name;
    }
    dest[n] = 0;
}

static void fp_take_selection(void) {
    if (fp_nent == 0) {
        fp_nm[0] = 0;
        fp_full[0] = 0;
        fp_prime = 0;
        return;
    }
    fp_ent_name(fp_sel, fp_nm);
    fp_prime = fp_ent_primary(fp_sel);
    fp_join(fp_full, fp_curdir, fp_nm);
}

// Descend into the selected directory, or climb when it is "..".
static void fp_descend(void) {
    char name[FP_NAME_MAX + 1];
    unsigned char n;

    fp_ent_name(fp_sel, name);
    if (x16_dos_chdir(name, fp_len(name)) >= X16_DOS_OK_BELOW) {
        return;                         // the drive refused: stay put
    }

    if (name[0] == '.' && name[1] == '.' && name[2] == 0) {
        n = fp_len(fp_curdir);          // climb: drop the last component
        while (n > 1 && fp_curdir[n - 1] != '/') {
            --n;
        }
        if (n > 1) {
            --n;                        // drop the separator too
        }
        fp_curdir[n] = 0;
        if (fp_curdir[0] == 0) {
            fp_curdir[0] = '/';
            fp_curdir[1] = 0;
        }
    } else {
        char joined[64];

        fp_join(joined, fp_curdir, name);
        fp_cpy(fp_curdir, joined, 62);
    }

    fp_sel = 0;
    fp_scroll = 0;
    fp_scan();
}

// =====================================================================
// A one-line text field, for the new-folder and rename prompts. Drawn
// blue on yellow whatever the style: a field that blends in is a field
// nobody sees. Returns 1 when Enter accepted a non-empty name.
// =====================================================================
static unsigned char fp_prompt(const char *label, char *buf,
                               unsigned char max) {
    unsigned char n = 0;
    unsigned char row = fp_top + fp_rows + 1;
    unsigned char c;

    buf[0] = 0;
    for (;;) {
        fp_fill_row(row, 0x7E);         // blue on yellow
        fp_puts_at(row, fp_left + 1, label, 20, 0x7E);
        fp_puts_at(row, fp_left + 1 + fp_len(label), buf, max, 0x7E);

        c = x16_fio_getin();
        if (c == 0) {
            continue;
        }
        if (c == K_RET) {
            return (n != 0) ? 1 : 0;
        }
        if (c == K_ESC || c == K_STOP) {
            return 0;
        }
        if (c == K_DEL) {
            if (n != 0) {
                --n;
                buf[n] = 0;
            }
            continue;
        }
        if (c >= 0x20 && c < 0x60 && n < max) {
            buf[n] = (char)c;
            ++n;
            buf[n] = 0;
        }
    }
}

// =====================================================================
// The editing gestures.
// =====================================================================
static void fp_new_dir(void) {
    char name[32];

    if (fp_prompt("new folder: ", name, 24)) {
        x16_dos_mkdir(name, fp_len(name));
        fp_scan();
    }
    x16_fp_redraw();
}

static void fp_rename(void) {
    char name[32];
    char old[FP_NAME_MAX + 1];

    if (fp_nent == 0) {
        return;
    }
    fp_ent_name(fp_sel, old);
    if (fp_prompt("rename to: ", name, 24)) {
        x16_dos_rename(old, fp_len(old), name, fp_len(name));
        fp_scan();
    }
    x16_fp_redraw();
}

// y/n on the footer row. Anything but 'y' is no.
static unsigned char fp_confirm(const char *label) {
    unsigned char row = fp_top + fp_rows + 1;
    unsigned char c;

    fp_fill_row(row, 0x7E);
    fp_puts_at(row, fp_left + 1, label, 40, 0x7E);
    for (;;) {
        c = x16_fio_getin();
        if (c == 0) {
            continue;
        }
        return (c == 'y' || c == 'Y') ? 1 : 0;
    }
}

static void fp_delete(void) {
    char name[FP_NAME_MAX + 1];

    if (fp_nent == 0) {
        return;
    }
    fp_ent_name(fp_sel, name);
    if (fp_confirm("delete? y/n ")) {
        if (fp_ent_kind(fp_sel)) {
            x16_dos_rmdir(name, fp_len(name));
        } else {
            x16_dos_delete(name, fp_len(name));
        }
        fp_scan();
    }
    x16_fp_redraw();
}

// 'c' remembers the selected file; 'v' copies it into the folder on
// show, 256 bytes at a time through the KERNAL's own channels.
static void fp_paste(void) {
    char name[FP_NAME_MAX + 1];
    unsigned char n, i, st;
    unsigned char buf[256];

    if (!fp_clipok) {
        return;
    }
    n = fp_len(fp_clip);                // the name is after the last '/'
    while (n != 0 && fp_clip[n - 1] != '/') {
        --n;
    }
    fp_cpy(name, fp_clip + n, FP_NAME_MAX);

    if (!x16_fio_open_read(fp_clip, fp_len(fp_clip), 2, X16_DEVICE_SD, 2)) {
        return;
    }
    if (!x16_fio_open_write(name, fp_len(name), 3, X16_DEVICE_SD, 3)) {
        x16_fio_close(2);
        return;
    }
    for (;;) {
        x16_fio_chkin(2);
        for (i = 0; i < 255; ++i) {
            buf[i] = x16_fio_chrin();
            if (x16_fio_readst() != 0) {
                break;
            }
        }
        st = x16_fio_readst();
        x16_fio_clrchn();

        x16_fio_chkout(3);
        for (n = 0; n < i; ++n) {
            x16_fio_chrout(buf[n]);
        }
        x16_fio_clrchn();

        if (st != 0) {
            break;
        }
    }
    x16_fio_close(2);
    x16_fio_close(3);
    fp_scan();
    x16_fp_redraw();
}

// =====================================================================
// Selection movement, with the clamping the panel is navigated by.
// =====================================================================
static void fp_move(signed char delta) {
    if (fp_nent == 0) {
        return;
    }
    if (delta < 0) {
        if (fp_sel != 0) {
            --fp_sel;
        }
    } else {
        if (fp_sel + 1 < fp_nent) {
            ++fp_sel;
        }
    }
    if (fp_sel < fp_scroll) {
        fp_scroll = fp_sel;
    } else if (fp_sel >= fp_scroll + fp_rows) {
        fp_scroll = (unsigned char)(fp_sel - fp_rows + 1);
    }
}

// =====================================================================
// The event loop. Unbounded by construction: it reads GETIN until a key
// answers the question.
// =====================================================================
static unsigned char fp_loop(void) {
    unsigned char c;

    x16_fp_redraw();
    x16_mouse_show(1);

    for (;;) {
        c = x16_fio_getin();
        if (c == 0) {
            continue;
        }

        if (c == K_ESC || c == K_STOP) {
            return X16_FPK_NONE;
        }
        if (c == K_DOWN) {
            fp_move(1);
            x16_fp_redraw();
            continue;
        }
        if (c == K_UP) {
            fp_move(-1);
            x16_fp_redraw();
            continue;
        }
        if (c == K_HOME) {
            fp_sel = 0;
            fp_scroll = 0;
            x16_fp_redraw();
            continue;
        }
        if (c == K_RET || c == 'r' || c == 'R') {
            if (fp_nent == 0) {
                continue;
            }
            if (fp_ent_kind(fp_sel)) {
                fp_descend();
                x16_fp_redraw();
                continue;
            }
            fp_take_selection();
            return X16_FPK_PICK;
        }
        if (c == 'a' || c == 'A') {
            if (fp_nent == 0) {
                continue;
            }
            fp_take_selection();
            return X16_FPK_ALT;
        }
        if (c == 'h' || c == 'H') {
            return X16_FPK_HERE;
        }
        if (c == 'n' || c == 'N') {
            fp_new_dir();
            continue;
        }
        if (c == 'e' || c == 'E') {
            fp_rename();
            continue;
        }
        if (c == 'd' || c == 'D') {
            fp_delete();
            continue;
        }
        if (c == 'c' || c == 'C') {
            if (fp_nent != 0 && !fp_ent_kind(fp_sel)) {
                fp_take_selection();
                fp_cpy(fp_clip, fp_full, 94);
                fp_clipok = 1;
            }
            continue;
        }
        if (c == 'v' || c == 'V') {
            fp_paste();
            continue;
        }
    }
}

// =====================================================================
// Configuration.
// =====================================================================
void x16_fp_filter(const char *patterns)   { fp_filt = patterns; }
void x16_fp_primary(const char *patterns)  { fp_prim = patterns; }
void x16_fp_start_dir(const char *path)    { fp_startat = path; }
void x16_fp_heading(const char *text)      { fp_head = text; }
void x16_fp_footing(const char *text)      { fp_foot = text; }
void x16_fp_charset(unsigned char charset) { fp_chset = charset; }
void x16_fp_cache(unsigned long vaddr)     { fp_cachev = vaddr; }

void x16_fp_style(unsigned char panel, unsigned char bar,
                  unsigned char sel) {
    fp_apanel = panel;
    fp_abar = bar;
    fp_asel = sel;
}

void x16_fp_saveunder(unsigned char on, unsigned long vaddr) {
    fp_undon = on;
    fp_underv = vaddr;
}

// =====================================================================
// The session.
// =====================================================================
unsigned char x16_fp_open(void) {
    const char *start = (fp_startat != 0) ? fp_startat : "/";

    // Size the panel to the screen it finds: a fixed top row, a margin
    // either side, and whatever height is left after the two bars.
    x16_screen_get_size(&fp_scrw, &fp_scrh);
    if (fp_scrw < 24 || fp_scrh < 12) {
        fp_scrw = 80;
        fp_scrh = 60;
    }
    fp_top = 3;
    fp_left = 6;
    fp_wide = (unsigned char)(fp_scrw - 12);
    fp_rows = (unsigned char)(fp_scrh - 20);

    if (fp_chset != 255) {
        x16_screen_charset(fp_chset);
    }

    x16_dos_chdir(start, fp_len(start));
    fp_cpy(fp_curdir, start, 62);
    fp_sel = 0;
    fp_scroll = 0;
    fp_scan();

    if (fp_undon) {
        fp_under(1);
        fp_saved = 1;
    }
    return fp_loop();
}

unsigned char x16_fp_resume(void) {
    fp_scan();
    return fp_loop();
}

void x16_fp_close(void) {
    x16_mouse_hide();
    if (fp_saved) {
        fp_under(0);
        fp_saved = 0;
    }
}

// =====================================================================
// What the caller reads back. Each COPIES -- see the header.
// =====================================================================
static unsigned char fp_copy_out(char *dest, unsigned char size,
                                 const char *src) {
    unsigned char n = 0;

    if (size == 0) {
        return 0;
    }
    while (src[n] != 0 && n + 1 < size) {
        dest[n] = src[n];
        ++n;
    }
    dest[n] = 0;
    return n;
}

unsigned char x16_fp_path(char *dest, unsigned char size) {
    return fp_copy_out(dest, size, fp_full);
}

unsigned char x16_fp_name(char *dest, unsigned char size) {
    return fp_copy_out(dest, size, fp_nm);
}

unsigned char x16_fp_dir(char *dest, unsigned char size) {
    return fp_copy_out(dest, size, fp_curdir);
}

unsigned char x16_fp_is_primary(void) {
    return fp_prime;
}

// =====================================================================
// Geometry, valid once open() has sized the panel.
// =====================================================================
unsigned char x16_fp_panel_top(void)   { return fp_top; }
unsigned char x16_fp_panel_left(void)  { return fp_left; }
unsigned char x16_fp_panel_width(void) { return fp_wide; }
unsigned char x16_fp_panel_rows(void)  { return fp_rows; }
