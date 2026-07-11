/* =====================================================================
 * x16clib :: test_kickc/runner3.c -- the storage and audio half
 * =====================================================================
 * The third PRG of the suite: banked RAM, the KERNAL block ops, load/
 * save, the DOS command channel, BMX, PCM and ADPCM. Split from the
 * others for the same zero-page reason runner2.c documents.
 *
 * The load/save tests write into the -fsroot scratch directory the
 * build script points device 8 at; each test deletes what it creates.
 * =====================================================================
 */

#include "testlib.h"
#include <x16/x16.h>

#define TESTVRAM        0x4000          /* bank 0: clear of the text map */

/* The same independent VRAM path as runner.c. */
void t_vsetaddr(unsigned char bank, unsigned int addr) {
    asm {
        lda #$01
        trb $9f25
        lda addr
        sta $9f20
        lda addr+1
        sta $9f21
        lda bank
        and #$01
        sta $9f22
    }
}

unsigned char t_vpeek(unsigned char bank, unsigned int addr) {
    char r;
    t_vsetaddr(bank, addr);
    asm { lda $9f23 sta r }
    return r;
}

void t_vpoke(unsigned char bank, unsigned int addr, unsigned char v) {
    t_vsetaddr(bank, addr);
    asm { lda v sta $9f23 }
}

void vram_poison(unsigned char bank, unsigned int base, unsigned int n,
                 unsigned char v) {
    unsigned int i;
    for (i = 0; i < n; i++) {
        t_vpoke(bank, base + i, v);
    }
}

unsigned char t_ien(void) {
    char r;
    asm { lda $9f26 sta r }
    return r;
}

/* ------------------------------------------------------------------ */
/* banked RAM                                                          */
/* ------------------------------------------------------------------ */

/* A poke must land in the named bank, and must leave the caller's bank
** mapped exactly as it found it.
*/
void test_bank_roundtrip(void) {
    unsigned char before;

    x16_bank_set(7);
    before = x16_bank_get();

    x16_bank_poke(2, 100, 0x5A);
    x16_bank_poke(3, 100, 0xA5);

    t_check((x16_bank_peek(2, 100) == 0x5A &&
            x16_bank_peek(3, 100) == 0xA5 &&
            x16_bank_get() == before &&
            before == 7) ? 1 : 0,
            "BANK_ROUNDTRIP");

    x16_bank_set(1);
}

/* The window pointer must snap from $BFFF back to $A000 and step the
** bank: write four bytes starting two from the end of bank 1.
*/
const unsigned char bb_src[4] = { 0x11, 0x22, 0x33, 0x44 };

void test_bank_boundary(void) {
    x16_bank_poke(2, 0, 0x00);
    x16_bank_poke(2, 1, 0x00);

    x16_mem_to_bank(bb_src, 1, X16_BANK_SIZE - 2, 4);

    t_check((x16_bank_peek(1, X16_BANK_SIZE - 2) == 0x11 &&
            x16_bank_peek(1, X16_BANK_SIZE - 1) == 0x22 &&
            x16_bank_peek(2, 0) == 0x33 &&
            x16_bank_peek(2, 1) == 0x44) ? 1 : 0,
            "BANK_BOUNDARY");
}

const unsigned char btm_src[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
unsigned char btm_dst[4];

void test_bank_to_mem(void) {
    x16_mem_to_bank(btm_src, 4, 512, 4);
    btm_dst[0] = 0;
    btm_dst[1] = 0;
    btm_dst[2] = 0;
    btm_dst[3] = 0;
    x16_bank_to_mem(4, 512, btm_dst, 4);

    t_check((btm_dst[0] == 0xDE && btm_dst[1] == 0xAD &&
            btm_dst[2] == 0xBE && btm_dst[3] == 0xEF) ? 1 : 0,
            "BANK_TO_MEM");
}

void test_bank_copy_far(void) {
    unsigned char before;

    x16_bank_set(11);
    before = x16_bank_get();

    x16_bank_poke(5, 8190, 0x11);
    x16_bank_poke(5, 8191, 0x22);
    x16_bank_poke(6, 0, 0x33);
    x16_bank_poke(6, 1, 0x44);

    x16_bank_poke(8, 8190, 0x00);
    x16_bank_poke(8, 8191, 0x00);
    x16_bank_poke(9, 0, 0x00);
    x16_bank_poke(9, 1, 0x00);

    x16_bank_copy_far(5, 8190, 8, 8190, 4);

    t_check((x16_bank_peek(8, 8190) == 0x11 &&
            x16_bank_peek(8, 8191) == 0x22 &&
            x16_bank_peek(9, 0) == 0x33 &&
            x16_bank_peek(9, 1) == 0x44 &&
            x16_bank_get() == before) ? 1 : 0,  /* caller's bank restored */
            "BANK_COPY_FAR");

    x16_bank_set(1);
}

/* Longer than the 128-byte bounce buffer, so the chunk loop runs. */
void test_bank_copy_far_long(void) {
    unsigned int i;
    unsigned char e;
    unsigned char ok = 1;

    for (i = 0; i < 200; i++) {
        e = (unsigned char)i ^ 0x5A;
        x16_bank_poke(5, 100 + i, e);
        x16_bank_poke(8, 300 + i, 0x00);
    }
    x16_bank_poke(8, 500, 0x00);        /* the one-past-the-end guard */

    x16_bank_copy_far(5, 100, 8, 300, 200);

    for (i = 0; i < 200; i++) {
        e = (unsigned char)i ^ 0x5A;
        if (x16_bank_peek(8, 300 + i) != e) ok = 0;
    }
    t_check((ok && x16_bank_peek(8, 500) == 0x00) ? 1 : 0,
            "BANK_COPY_FAR_LONG");
}

/* ------------------------------------------------------------------ */
/* the bank allocator                                                  */
/* ------------------------------------------------------------------ */

void test_bank_alloc(void) {
    unsigned char a;
    unsigned char b;
    unsigned char c;
    unsigned char d;

    x16_bank_alloc_init(1, 3);
    a = x16_bank_alloc();               /* lowest first */
    b = x16_bank_alloc();
    c = x16_bank_alloc();
    d = x16_bank_alloc();               /* exhausted */

    t_check((a == 1 && b == 2 && c == 3 && d == 0) ? 1 : 0, "BANK_ALLOC");
}

void test_bank_free(void) {
    x16_bank_alloc_init(1, 3);
    x16_bank_alloc();                   /* 1 */
    x16_bank_alloc();                   /* 2 */
    x16_bank_alloc();                   /* 3 */
    x16_bank_free(2);

    t_check((x16_bank_alloc() == 2 && x16_bank_alloc() == 0) ? 1 : 0,
            "BANK_FREE");
}

void test_bank_reserve(void) {
    unsigned char first;
    unsigned char again;
    unsigned char after_free;
    unsigned char outside;

    x16_bank_alloc_init(4, 6);
    first = x16_bank_reserve(5);        /* free -> claimed */
    again = x16_bank_reserve(5);        /* already taken */
    x16_bank_free(5);
    after_free = x16_bank_reserve(5);
    outside = x16_bank_reserve(9);      /* never in the pool */

    t_check((first == 1 && again == 0 && after_free == 1 && outside == 0) ? 1 : 0,
            "BANK_RESERVE");
}

void test_bank_alloc_uninit(void) {
    x16_bank_alloc_init(1, 1);
    x16_bank_alloc();                   /* drain it */

    t_check((x16_bank_alloc() == 0) ? 1 : 0, "BANK_ALLOC_UNINIT");
}

/* ------------------------------------------------------------------ */
/* KERNAL block operations                                             */
/* ------------------------------------------------------------------ */

unsigned char mf_buf[8];

void test_mem_fill(void) {
    unsigned char i;
    unsigned char ok = 1;

    for (i = 0; i < 8; i++) {
        mf_buf[i] = 0x11;
    }
    x16_mem_fill(mf_buf, 5, 0xC3);
    x16_mem_fill(mf_buf + 6, 0, 0x99);  /* a zero count fills nothing */

    for (i = 0; i < 5; i++) {
        if (mf_buf[i] != 0xC3) ok = 0;
    }
    t_check((ok && mf_buf[5] == 0x11 && mf_buf[6] == 0x11) ? 1 : 0,
            "MEM_FILL");
}

void test_mem_fill_vram(void) {
    vram_poison(0, TESTVRAM, 6, 0x00);

    x16_vera_addr0(X16_INC_1, 0x04000);
    x16_mem_fill(X16_VERA_DATA0, 4, 0x7E);

    t_check((t_vpeek(0, TESTVRAM) == 0x7E &&
            t_vpeek(0, TESTVRAM + 3) == 0x7E &&
            t_vpeek(0, TESTVRAM + 4) == 0x00) ? 1 : 0,
            "MEM_FILL_VRAM");
}

const unsigned char mc_src[6] = { 1, 2, 3, 4, 5, 6 };
unsigned char mc_dst[8];

void test_mem_copy(void) {
    unsigned char i;
    unsigned char ok = 1;

    for (i = 0; i < 8; i++) {
        mc_dst[i] = 0xEE;
    }
    x16_mem_copy(mc_src, mc_dst, 6);
    x16_mem_copy(mc_src, mc_dst + 7, 0);        /* zero copies nothing */

    for (i = 0; i < 6; i++) {
        if (mc_dst[i] != i + 1) ok = 0;
    }
    t_check((ok && mc_dst[6] == 0xEE && mc_dst[7] == 0xEE) ? 1 : 0,
            "MEM_COPY");
}

const unsigned char crc_check[9] = { 0x31, 0x32, 0x33, 0x34, 0x35,
                                     0x36, 0x37, 0x38, 0x39 };   /* "123456789" */

void test_mem_crc(void) {
    t_check((x16_mem_crc(crc_check, 9) == 0x29B1 &&
            x16_mem_crc(crc_check, 0) == 0xFFFF) ? 1 : 0, /* the init value */
            "MEM_CRC");
}

/* The same phrase as the ZX0 test, packed by lzsa -r -f2. */
const unsigned char lzsa_packed[31] = {
    0x3f, 0xf4, 0x06, 0x58, 0x31, 0x36, 0x4c, 0x49, 0x42, 0x2d, 0x44, 0x45,
    0x43, 0x4f, 0x4d, 0x50, 0x52, 0x45, 0x53, 0x53, 0x2d, 0x54, 0x45, 0x53,
    0x54, 0x21, 0x21, 0xff, 0x30, 0xe7, 0xe8
};
const unsigned char lzsa_phrase[24] = {
    'X', '1', '6', 'L', 'I', 'B', '-', 'D', 'E', 'C', 'O', 'M', 'P', 'R',
    'E', 'S', 'S', '-', 'T', 'E', 'S', 'T', '!', '!'
};
unsigned char lz_out[97];

void test_mem_decompress(void) {
    unsigned char *end;
    unsigned char i;
    unsigned char r;
    unsigned char ok = 1;

    lz_out[96] = 0x77;                  /* guard, one past the output */
    end = (unsigned char *)x16_mem_decompress(lzsa_packed, lz_out);

    if (end != lz_out + 96 || lz_out[96] != 0x77) {
        t_check(0, "MEM_DECOMPRESS");
        return;
    }
    for (r = 0; r < 4; r++) {
        for (i = 0; i < 24; i++) {
            if (lz_out[r * 24 + i] != lzsa_phrase[i]) ok = 0;
        }
    }
    t_check(ok, "MEM_DECOMPRESS");
}

/* The property that makes mem_decompress special: a $9Fxx target is not
** incremented, so it unpacks straight through a VERA data port.
*/
void test_mem_decompress_vram(void) {
    unsigned char i;
    unsigned char r;
    unsigned char ok = 1;

    vram_poison(0, TESTVRAM, 4, 0x00);

    x16_vera_addr0(X16_INC_1, 0x04000);
    x16_mem_decompress(lzsa_packed, X16_VERA_DATA0);

    for (r = 0; r < 4; r++) {
        for (i = 0; i < 24; i++) {
            if (t_vpeek(0, TESTVRAM + r * 24 + i) != lzsa_phrase[i]) ok = 0;
        }
    }
    t_check(ok, "MEM_DECOMPRESS_VRAM");
}

/* ------------------------------------------------------------------ */
/* load and save                                                       */
/* ------------------------------------------------------------------ */

const char fs_name[] = "TESTDATA.BIN";
unsigned char fs_out[16];
unsigned char fs_in[16];

void test_fs_roundtrip(void) {
    unsigned int end = 0;
    unsigned int want;
    unsigned char i;
    unsigned char err;
    unsigned char ok;

    for (i = 0; i < 16; i++) {
        fs_out[i] = 0xF0 ^ i;
        fs_in[i] = 0x00;
    }

    err = x16_fs_save(fs_name, 12, X16_DEVICE_SD, fs_out, fs_out + 16);
    if (err) {
        t_check(0, "FS_ROUNDTRIP");
        return;
    }

    err = x16_fs_load(fs_name, 12, X16_DEVICE_SD, X16_SA_ADDR, fs_in, &end);

    ok = (err == 0) ? 1 : 0;
    for (i = 0; i < 16; i++) {
        if (fs_in[i] != (unsigned char)(0xF0 ^ i)) {
            ok = 0;
        }
    }
    /* The load reports one past the last byte written. */
    want = (unsigned int)fs_in;
    want = want + 16;
    if (end != want) ok = 0;

    t_check(ok, "FS_ROUNDTRIP");
}

const char fs_missing[] = "NOSUCH.BIN";
unsigned char fs_junk[4];

void test_fs_load_missing(void) {
    t_check((x16_fs_load(fs_missing, 10, X16_DEVICE_SD, X16_SA_ADDR,
                         fs_junk, (unsigned int *)0) != 0) ? 1 : 0,
            "FS_LOAD_MISSING");
}

/* Reuses TESTDATA.BIN from the roundtrip, straight into VRAM. */
void test_fs_vload(void) {
    unsigned char err;

    vram_poison(0, TESTVRAM, 16, 0x00);
    err = x16_fs_vload(fs_name, 12, X16_DEVICE_SD, 0x04000);

    t_check((err == 0 &&
            t_vpeek(0, TESTVRAM) == 0xF0 &&
            t_vpeek(0, TESTVRAM + 15) == (0xF0 ^ 15)) ? 1 : 0,
            "FS_VLOAD");
}

/* ------------------------------------------------------------------ */
/* the DOS command channel                                             */
/* ------------------------------------------------------------------ */

void test_dos_status(void) {
    unsigned char code = x16_dos_status();
    const char *msg = x16_dos_msg();

    t_check((code != X16_DOS_NO_CHANNEL &&
            code <= 99 &&
            msg[0] >= '0' && msg[0] <= '9' &&
            msg[1] >= '0' && msg[1] <= '9' &&
            msg[2] == ',') ? 1 : 0,
            "DOS_STATUS");
}

void test_dos_delete_missing(void) {
    t_check((x16_dos_delete(fs_missing, 10) == 62) ? 1 : 0,
            "DOS_DELETE_MISSING");
}

const char dos_name[] = "DOSTEST.BIN";
unsigned char dos_buf[4] = { 1, 2, 3, 4 };

void test_dos_delete(void) {
    unsigned char saved;
    unsigned char deleted;
    unsigned char loaded;

    saved = x16_fs_save(dos_name, 11, X16_DEVICE_SD, dos_buf, dos_buf + 4);
    deleted = x16_dos_delete(dos_name, 11);
    loaded = x16_fs_load(dos_name, 11, X16_DEVICE_SD, X16_SA_ADDR,
                         dos_buf, (unsigned int *)0);

    t_check((saved == 0 &&
            deleted < X16_DOS_OK_BELOW &&       /* the drive said yes */
            loaded != 0) ? 1 : 0,               /* ...it really is gone */
            "DOS_DELETE");
}

const char dos_old[] = "DOSOLD.BIN";
const char dos_new[] = "DOSNEW.BIN";
unsigned char dr_out[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
unsigned char dr_in[4];

void test_dos_rename(void) {
    unsigned char code;
    unsigned char loaded;

    x16_fs_save(dos_old, 10, X16_DEVICE_SD, dr_out, dr_out + 4);

    code = x16_dos_rename(dos_old, 10, dos_new, 10);

    dr_in[0] = 0;
    loaded = x16_fs_load(dos_new, 10, X16_DEVICE_SD, X16_SA_ADDR,
                         dr_in, (unsigned int *)0);

    x16_dos_delete(dos_new, 10);

    t_check((code < X16_DOS_OK_BELOW && loaded == 0 && dr_in[0] == 0xDE) ? 1 : 0,
            "DOS_RENAME");
}

/* ------------------------------------------------------------------ */
/* BMX                                                                 */
/* ------------------------------------------------------------------ */

const char bmx_name[] = "PIC.BMX";
x16_bmx_info bmx_out;
x16_bmx_info bmx_back;

void test_bmx_roundtrip(void) {
    unsigned char i;
    unsigned char ok;

    /* An 8x4 image of a colour ramp at TESTVRAM. */
    for (i = 0; i < 32; i++) {
        t_vpoke(0, TESTVRAM + i, 0x40 + i);
    }
    x16_pal_set(200, 0x0F0F);           /* two palette entries to carry */
    x16_pal_set(201, 0x00F0);

    bmx_out.width = 8;
    bmx_out.height = 4;
    bmx_out.bpp = 8;
    bmx_out.palstart = 200;
    bmx_out.palcount = 2;
    bmx_out.border = 5;
    bmx_out.stride = 8;                 /* contiguous, not a screen row */
    x16_bmx_set_info(&bmx_out);

    if (x16_bmx_save(bmx_name, 7, X16_DEVICE_SD, 0x04000) != 0) {
        t_check(0, "BMX_ROUNDTRIP");
        return;
    }

    vram_poison(1, 0x0000, 32, 0x00);   /* TESTVRAM_HI = $10000: bank 1 */
    x16_pal_set(200, 0x0000);           /* wipe the palette we saved */
    x16_pal_set(201, 0x0000);

    if (x16_bmx_load(bmx_name, 7, X16_DEVICE_SD, 0x10000) != 0) {
        t_check(0, "BMX_ROUNDTRIP");
        return;
    }

    x16_bmx_get_info(&bmx_back);
    ok = (bmx_back.width == 8 && bmx_back.height == 4 && bmx_back.bpp == 8 &&
          bmx_back.palstart == 200 && bmx_back.palcount == 2 &&
          bmx_back.border == 5) ? 1 : 0;

    for (i = 0; i < 32; i++) {          /* pixels, across the VRAM bank */
        if (t_vpeek(1, 0x0000 + i) != 0x40 + i) ok = 0;
    }
    /* ...and the palette came back: entry 200 = 0x0F0F, 201 = 0x00F0 */
    if (t_vpeek(1, 0xFA00 + 400) != 0x0F) ok = 0;
    if (t_vpeek(1, 0xFA00 + 401) != 0x0F) ok = 0;
    if (t_vpeek(1, 0xFA00 + 402) != 0xF0) ok = 0;
    if (t_vpeek(1, 0xFA00 + 403) != 0x00) ok = 0;

    x16_dos_delete(bmx_name, 7);
    t_check(ok, "BMX_ROUNDTRIP");
}

const char nb_name[] = "NOTABMX.BIN";
unsigned char nb_junk[20];

void test_bmx_bad_format(void) {
    unsigned char i;
    unsigned char code;

    for (i = 0; i < 20; i++) {
        nb_junk[i] = i;
    }
    x16_fs_save(nb_name, 11, X16_DEVICE_SD, nb_junk, nb_junk + 20);

    code = x16_bmx_load(nb_name, 11, X16_DEVICE_SD, 0x04000);
    x16_dos_delete(nb_name, 11);

    t_check((code == X16_BMX_ERR_FORMAT) ? 1 : 0, "BMX_BAD_FORMAT");
}

const char bm_name[] = "NOSUCH.BMX";

void test_bmx_missing(void) {
    t_check((x16_bmx_load(bm_name, 10, X16_DEVICE_SD, 0x04000) ==
            X16_BMX_ERR_IO) ? 1 : 0,
            "BMX_MISSING");
}

/* ------------------------------------------------------------------ */
/* PCM                                                                 */
/* ------------------------------------------------------------------ */

/* AUDIO_RATE above 128 is invalid, so the rate is clamped -- checked on
** the returned value, because the register is not readable.
*/
void test_pcm_rate_clamp(void) {
    unsigned char over = x16_pcm_rate(200);
    unsigned char under = x16_pcm_rate(64);

    x16_pcm_rate(0);                    /* stop playback again */

    t_check((over == 128 && under == 64 && x16_pcm_empty() == 1) ? 1 : 0,
            "PCM_RATE_CLAMP");
}

/* AFLOW streaming, primed at rate 0 so nothing actually plays -- which
** is what makes this runnable headless. A buffer bigger than the 4 KB
** FIFO must leave the stream active with the refill interrupt armed;
** one that fits outright must leave it inactive with AFLOW disabled.
*/
void test_pcm_stream(void) {
    unsigned char big_full;
    unsigned char big_active;
    unsigned char big_ien;
    unsigned char stopped_ien;
    unsigned char stopped_active;
    unsigned char sml_active;
    unsigned char sml_ien;
    unsigned char sml_queued;

    x16_pcm_rate(0);
    x16_pcm_ctrl(X16_PCM_VOLUME(15));   /* 8-bit mono, full volume */
    x16_pcm_reset();

    /* Any readable RAM does as sample data. */
    x16_pcm_stream_start((void *)0x2000, 5120, 0);   /* > 4 KB FIFO */
    big_full = x16_pcm_full();
    big_active = x16_pcm_stream_active();
    big_ien = t_ien() & 0x08;           /* AFLOW enable */

    x16_pcm_stream_stop();
    stopped_ien = t_ien() & 0x08;
    stopped_active = x16_pcm_stream_active();

    x16_pcm_reset();
    x16_pcm_stream_start((void *)0x2000, 64, 0);     /* fits outright */
    sml_active = x16_pcm_stream_active();
    sml_ien = t_ien() & 0x08;
    sml_queued = (x16_pcm_empty() == 0) ? 1 : 0;

    x16_pcm_reset();
    x16_irq_remove();

    t_check((big_full == 1 && big_active == 1 && big_ien != 0 &&
            stopped_ien == 0 && stopped_active == 0 &&
            sml_active == 0 && sml_ien == 0 && sml_queued) ? 1 : 0,
            "PCM_STREAM");
}

void test_pcm_stream_empty(void) {
    x16_pcm_rate(0);
    x16_pcm_reset();
    x16_pcm_stream_start((void *)0x2000, 0, 0);

    t_check((x16_pcm_stream_active() == 0 && (t_ien() & 0x08) == 0) ? 1 : 0,
            "PCM_STREAM_EMPTY");

    x16_pcm_reset();
    x16_irq_remove();
}

/* ------------------------------------------------------------------ */
/* ADPCM                                                               */
/* ------------------------------------------------------------------ */

/* Eight ADPCM bytes decode to sixteen signed 16-bit samples. The
** expected values came from Python's audioop, so this pins the
** algorithm -- the saturation, the index clamp, the low-nibble-first
** order -- not just our plumbing.
*/
const unsigned char ad_packed[8] = {
    0x17, 0x28, 0x93, 0x4C, 0xE5, 0x0A, 0x71, 0xBF
};
const int ad_expect[16] = {
    0x000b, 0x0011, 0x0010, 0x0017, 0x0021, 0x001e, 0x0013, 0x0020,
    0x0032, 0x0011, -5,     -1,     0x0009, 0x003d, -51,    -164
};
int ad_out[16];
int ad_whole[16];
int ad_sliced[16];

void test_adpcm(void) {
    unsigned char i;
    unsigned char ok = 1;
    int got;
    int want;

    x16_adpcm_init();
    x16_adpcm_block(ad_packed, ad_out, 8);

    for (i = 0; i < 16; i++) {
        got = ad_out[i];                /* via locals: KickC has no code */
        want = ad_expect[i];            /* fragment for int[] == int[] */
        if (got != want) ok = 0;
    }
    /* The decoder state must end where the reference says it does. */
    ok = (ok && x16_adpcm_predictor() == -164 &&
          x16_adpcm_index() == 29) ? 1 : 0;

    t_check(ok, "ADPCM");
}

/* State carries across calls, so decoding a block in slices gives the
** same answer as decoding it in one go. That is what makes it stream.
*/
void test_adpcm_sliced(void) {
    unsigned char i;
    unsigned char ok = 1;
    int got;
    int want;

    x16_adpcm_init();
    x16_adpcm_block(ad_packed, ad_whole, 8);

    x16_adpcm_init();
    x16_adpcm_block(ad_packed, ad_sliced, 3);
    x16_adpcm_block(ad_packed + 3, ad_sliced + 6, 5);

    for (i = 0; i < 16; i++) {
        got = ad_whole[i];
        want = ad_sliced[i];
        if (got != want) ok = 0;
    }
    t_check(ok, "ADPCM_SLICED");
}

/* An IMA WAV block header carries the initial predictor and step index. */
void test_adpcm_state(void) {
    x16_adpcm_set_state(-1000, 42);

    t_check((x16_adpcm_predictor() == -1000 &&
            x16_adpcm_index() == 42) ? 1 : 0,
            "ADPCM_STATE");
}

/* ------------------------------------------------------------------ */

void main(void) {
    t_init();

    test_bank_roundtrip();
    test_bank_boundary();
    test_bank_to_mem();
    test_bank_copy_far();
    test_bank_copy_far_long();

    test_bank_alloc();
    test_bank_free();
    test_bank_reserve();
    test_bank_alloc_uninit();

    test_mem_fill();
    test_mem_fill_vram();
    test_mem_copy();
    test_mem_crc();
    test_mem_decompress();
    test_mem_decompress_vram();

    test_fs_roundtrip();
    test_fs_load_missing();
    test_fs_vload();

    test_dos_status();
    test_dos_delete_missing();
    test_dos_delete();
    test_dos_rename();

    test_bmx_roundtrip();
    test_bmx_bad_format();
    test_bmx_missing();

    test_pcm_rate_clamp();
    test_pcm_stream();
    test_pcm_stream_empty();

    test_adpcm();
    test_adpcm_sliced();
    test_adpcm_state();

    t_done();
}
