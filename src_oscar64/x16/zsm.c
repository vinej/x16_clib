// =====================================================================
// x16clib :: x16/zsm.c -- compact ZSM stream player
// =====================================================================
// A ZSM stream is a byte-coded command list. The first byte of each
// command says what it is:
//
//   $00-$3F   write the next stream byte to PSG register <cmd>
//   $40       EXTCMD: one ccnnnnnn byte, then nnnnnn payload bytes
//   $41-$7F   (cmd & $3F) YM register/value pairs follow
//   $80       end of stream: loop back, or stop
//   $81-$FF   delay (cmd & $7F) ticks
//
// The player only advances when x16_zsm_tick() is called, so the caller
// owns the timing. Where ca65 keeps the stream cursor as a low/high byte
// pair in zero page, this holds a real C pointer -- the walking is the
// compiler's problem, and the two behave identically.
//
// The ZSM and PCM-table magic are compared as EXPLICIT BYTES: what is in
// the file is ASCII, and a character literal would be at the mercy of the
// including program's encoding pragma.
// =====================================================================

#include <x16/zsm.h>
#include <x16/pcm.h>

// pcm.c's loop flag: a looping PCM instrument sets it before starting the
// stream, which is the same reach-in the ca65 port makes.
extern volatile char x16__pcm_loop;

#define ZSM_FLAG_ACTIVE 0x01
#define ZSM_FLAG_LOOP   0x02
#define ZSM_FLAG_EOF    0x04
#define ZSM_FLAG_PCM    0x08

#define ZSM_MAX_VERSION 1
#define ZSM_YM_TIMEOUT  128

#define ZSM_PCM_FIFO_RESET 0x80
#define ZSM_PCM_16BIT      0x20
#define ZSM_PCM_STEREO     0x10

#define VRAM_PSG_L      0xC0            // VRAM $1F9C0: 16 voices x 4 bytes
#define VRAM_PSG_M      0xF9
#define VERA_ADDR_H_BANK 0x01           // VRAM address bit 16

static unsigned char zsm_code = X16_ZSM_ERR_NONE;
static const unsigned char *zsm_base;
static const unsigned char *zsm_start;
static const unsigned char *zsm_ptr;
static const unsigned char *zsm_loop_at;
static unsigned int  zsm_tickrate = 60;
static unsigned char zsm_delay;
static unsigned char zsm_flags;
static unsigned char zsm_extlen;        // EXTCMD payload countdown

static const unsigned char *zsm_pcm_hdr;
static const unsigned char *zsm_pcm_data;
static unsigned char zsm_pcm_last;
static unsigned char zsm_pcm_rate;
static unsigned char zsm_pcm_flags;

volatile unsigned char x16__zsm_v;      // asm -> C hand-off
volatile unsigned char x16__zsm_r;      // register number, for the YM write

// ---------------------------------------------------------------------
// One stream byte, cursor advanced.
// ---------------------------------------------------------------------
static unsigned char zsm_next(void) {
    unsigned char b = *zsm_ptr;

    ++zsm_ptr;
    return b;
}

// ---------------------------------------------------------------------
// PSG registers live in VRAM at $1F9C0, so a write is an addressed VERA
// poke through data port 0. ADDRSEL must be 0 for that port.
// ---------------------------------------------------------------------
static void zsm_psg_write(unsigned char reg, unsigned char value) {
    x16__zsm_r = reg;
    x16__zsm_v = value;
    __asm {
        lda 0x9f25                      /* VERA_CTRL: ADDRSEL = 0 */
        and #0xfe
        sta 0x9f25
        lda x16__zsm_r
        clc
        adc #0xc0                       /* low byte of VRAM_PSG */
        sta 0x9f20                      /* VERA_ADDR_L */
        lda #0xf9
        adc #0
        sta 0x9f21                      /* VERA_ADDR_M */
        lda #0x01                       /* VERA_ADDR_H_BANK: bit 16 */
        sta 0x9f22                      /* VERA_ADDR_H */
        lda x16__zsm_v
        sta 0x9f23                      /* VERA_DATA0 */
    }
}

// ---------------------------------------------------------------------
// The YM2151 wants its register byte, a settling gap, then the value --
// and it must not be interrupted in between, hence the sei. The busy
// wait is bounded by a countdown so a missing or wedged chip cannot hang
// the player.
// ---------------------------------------------------------------------
static void zsm_ym_write(unsigned char reg, unsigned char value) {
    x16__zsm_r = reg;
    x16__zsm_v = value;
    __asm {
        php
        sei
        ldy #128                        /* ZSM_YM_TIMEOUT */
    zy_wait:
        dey
        bmi zy_done
        bit 0x9f41                      /* YM_DATA: bit 7 = busy */
        bmi zy_wait
        lda x16__zsm_r
        sta 0x9f40                      /* YM_REG */
        nop
        nop
        nop
        lda x16__zsm_v
        sta 0x9f41                      /* YM_DATA */
    zy_done:
        plp
    }
}

// ---------------------------------------------------------------------
// Parse the optional PCM header/table out of the ZSM header.
// Returns 1 if it is present but unsupported or invalid.
// ---------------------------------------------------------------------
static unsigned char zsm_pcm_init(const unsigned char *hdr) {
    unsigned int off;
    unsigned int table;
    unsigned long sum;                  // 17 bits, to catch the wrap

    zsm_pcm_flags = 0;
    zsm_pcm_rate = 0;

    if (hdr[8] != 0) {
        return 1;                       // PCM offset needs more than 16 bits
    }
    off = (unsigned int)hdr[6] | ((unsigned int)hdr[7] << 8);
    if (off == 0) {
        return 0;                       // no PCM header at all
    }

    // ca65 catches these two as the carry out of a 16-bit add; C pointer
    // arithmetic would wrap silently, so the sums are done wide.
    sum = (unsigned long)(unsigned int)zsm_base + off;
    if (sum > 0xFFFFUL) {
        return 1;
    }
    zsm_pcm_hdr = zsm_base + off;

    // "PCM" then the highest instrument index
    if (zsm_pcm_hdr[0] != 0x50 || zsm_pcm_hdr[1] != 0x43
            || zsm_pcm_hdr[2] != 0x4D) {
        return 1;
    }
    zsm_pcm_last = zsm_pcm_hdr[3];

    // data base = header + 4 + 16 * (last index + 1); 256 entries is
    // 4096 bytes exactly, which is why the count is widened first.
    table = ((unsigned int)zsm_pcm_last + 1) << 4;
    sum = (unsigned long)(unsigned int)zsm_pcm_hdr + 4 + table;
    if (sum > 0xFFFFUL) {
        return 1;
    }
    zsm_pcm_data = zsm_pcm_hdr + 4 + table;

    zsm_pcm_flags |= ZSM_FLAG_PCM;
    return 0;
}

// ---------------------------------------------------------------------
// Initialize from a ZSM file image.
// ---------------------------------------------------------------------
unsigned char x16_zsm_init(const void *header) {
    const unsigned char *h = (const unsigned char *)header;
    unsigned int loop_off;

    zsm_base = h;

    if (h[0] != 0x7A || h[1] != 0x6D) {         // "zm"
        zsm_code = X16_ZSM_ERR_MAGIC;
        return zsm_code;
    }
    if (h[2] > ZSM_MAX_VERSION) {
        zsm_code = X16_ZSM_ERR_VERSION;
        return zsm_code;
    }

    zsm_tickrate = (unsigned int)h[0x0C] | ((unsigned int)h[0x0D] << 8);

    if (zsm_pcm_init(h)) {
        zsm_code = X16_ZSM_ERR_PCM;
        return zsm_code;
    }

    zsm_ptr = h + 16;                   // the command stream follows
    zsm_start = zsm_ptr;

    if (h[5] != 0) {                    // loop offset bit 16 unsupported
        zsm_code = X16_ZSM_ERR_RANGE;
        return zsm_code;
    }
    loop_off = (unsigned int)h[3] | ((unsigned int)h[4] << 8);
    if (loop_off != 0) {
        zsm_loop_at = zsm_base + loop_off;
        zsm_flags = ZSM_FLAG_ACTIVE | ZSM_FLAG_LOOP;
    } else {
        zsm_loop_at = (const unsigned char *)0;
        zsm_flags = ZSM_FLAG_ACTIVE;
    }
    zsm_delay = 0;
    zsm_code = X16_ZSM_ERR_NONE;
    return zsm_code;
}

unsigned char x16_zsm_lasterr(void) {
    return zsm_code;
}

// ---------------------------------------------------------------------
// Initialize from a raw headerless stream.
// ---------------------------------------------------------------------
void x16_zsm_init_stream(const void *stream, const void *loop) {
    zsm_base  = (const unsigned char *)stream;
    zsm_ptr   = zsm_base;
    zsm_start = zsm_base;
    zsm_loop_at = (const unsigned char *)loop;

    zsm_pcm_flags = 0;
    zsm_pcm_rate = 0;

    if (zsm_loop_at != 0) {
        zsm_flags = ZSM_FLAG_ACTIVE | ZSM_FLAG_LOOP;
    } else {
        zsm_flags = ZSM_FLAG_ACTIVE;
    }
    zsm_delay = 0;
    zsm_tickrate = 60;
}

// ---------------------------------------------------------------------
// Pause / resume / restart.
// ---------------------------------------------------------------------
void x16_zsm_play(void) {
    zsm_flags |= ZSM_FLAG_ACTIVE;
}

void x16_zsm_stop(void) {
    zsm_flags &= (unsigned char)~ZSM_FLAG_ACTIVE;
    x16_pcm_stream_stop();
    __asm {
        lda #0
        sta 0x9f3c                      /* VERA_AUDIO_RATE: silence the DAC */
    }
}

void x16_zsm_rewind(void) {
    zsm_ptr = zsm_start;
    zsm_delay = 0;
    zsm_flags &= (unsigned char)~ZSM_FLAG_EOF;
}

unsigned int x16_zsm_get_tickrate(void) {
    return zsm_tickrate;
}

unsigned char x16_zsm_status(void) {
    return zsm_flags;
}

unsigned char x16_zsm_pcm_present(void) {
    if ((zsm_pcm_flags & ZSM_FLAG_PCM) != 0) {
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------
// Fire a PCM instrument from the file's table.
//
// Silently ignores a missing table, an out-of-range index, a sample with
// a >64K offset or length, and a zero-length sample -- a music file is
// not worth crashing over.
// ---------------------------------------------------------------------
void x16_zsm_pcm_trigger(unsigned char instrument) {
    const unsigned char *ins;
    unsigned char fmt;
    unsigned int sample_off;
    unsigned int sample_len;

    if (!x16_zsm_pcm_present()) {
        return;
    }
    if (instrument > zsm_pcm_last) {
        return;
    }

    // instrument record = header + 4 + index * 16
    ins = zsm_pcm_hdr + 4 + ((unsigned int)instrument << 4);

    fmt = (unsigned char)(ins[1] & (ZSM_PCM_16BIT | ZSM_PCM_STEREO));

    if (ins[4] != 0 || ins[7] != 0) {
        return;                         // offset or length beyond 16 bits
    }
    sample_off = (unsigned int)ins[2] | ((unsigned int)ins[3] << 8);
    sample_len = (unsigned int)ins[5] | ((unsigned int)ins[6] << 8);
    if (sample_len == 0) {
        return;
    }

    x16__pcm_loop = (char)(ins[8] & 0x80);

    x16__zsm_v = fmt;
    __asm {
        lda #0
        sta 0x9f3c                      /* VERA_AUDIO_RATE off while we set up */
        lda 0x9f3b                      /* VERA_AUDIO_CTRL */
        and #0x0f                       /* keep the volume */
        ora x16__zsm_v                  /* the instrument's format bits */
        ora #0x80                       /* ZSM_PCM_FIFO_RESET */
        sta 0x9f3b
    }

    x16_pcm_stream_start(zsm_pcm_data + sample_off, sample_len, zsm_pcm_rate);
}

// ---------------------------------------------------------------------
// EXTCMD channel 0: command/argument pairs. Command 0 sets AUDIO_CTRL,
// 1 sets AUDIO_RATE, 2 triggers an instrument. Unknown commands and a
// truncated pair are consumed, not obeyed.
// ---------------------------------------------------------------------
static void zsm_ext_pcm(void) {
    unsigned char cmd, arg;

    while (zsm_extlen != 0) {
        cmd = zsm_next();
        --zsm_extlen;
        if (zsm_extlen == 0) {
            return;                     // truncated command: consumed
        }
        arg = zsm_next();
        --zsm_extlen;

        if (cmd == 0) {
            x16__zsm_v = arg;
            __asm {
                lda x16__zsm_v
                sta 0x9f3b              /* VERA_AUDIO_CTRL */
            }
        } else if (cmd == 1) {
            zsm_pcm_rate = arg;
            x16__zsm_v = arg;
            __asm {
                lda x16__zsm_v
                sta 0x9f3c              /* VERA_AUDIO_RATE */
            }
        } else if (cmd == 2) {
            x16_zsm_pcm_trigger(arg);
        }
        // anything else: ignored, its argument already eaten
    }
}

static void zsm_skip_ext(void) {
    while (zsm_extlen != 0) {
        zsm_next();
        --zsm_extlen;
    }
}

// ---------------------------------------------------------------------
// Advance playback by one tick, then report the status.
// ---------------------------------------------------------------------
unsigned char x16_zsm_tick(void) {
    unsigned char cmd, n, reg;

    if ((zsm_flags & ZSM_FLAG_ACTIVE) == 0) {
        return zsm_flags;
    }
    if (zsm_delay != 0) {
        --zsm_delay;
        return zsm_flags;
    }

    for (;;) {
        cmd = zsm_next();

        if (cmd < 0x40) {               // PSG write
            reg = cmd;
            zsm_psg_write(reg, zsm_next());
        } else if (cmd == 0x40) {       // EXTCMD
            n = zsm_next();             // ccnnnnnn
            zsm_extlen = (unsigned char)(n & 0x3F);
            if ((n & 0xC0) == 0) {      // channel 0 is PCM; others skipped
                zsm_ext_pcm();
            } else {
                zsm_skip_ext();
            }
        } else if (cmd < 0x80) {        // YM register/value pairs
            n = (unsigned char)(cmd & 0x3F);
            while (n != 0) {
                reg = zsm_next();
                zsm_ym_write(reg, zsm_next());
                --n;
            }
        } else if (cmd == 0x80) {       // end of stream
            if ((zsm_flags & ZSM_FLAG_LOOP) != 0) {
                zsm_ptr = zsm_loop_at;
                continue;
            }
            zsm_flags &= (unsigned char)~ZSM_FLAG_ACTIVE;
            zsm_flags |= ZSM_FLAG_EOF;
            return zsm_flags;
        } else {                        // delay 1..127 ticks
            zsm_delay = (unsigned char)(cmd & 0x7F);
            return zsm_flags;
        }
    }
}
