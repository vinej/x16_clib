// =====================================================================
// x16clib :: x16/psg.c -- VERA PSG (16 voices)
// =====================================================================
// Same code as src_ca65/audio/psg.s. The shared env_write internal
// became the C function x16__psg_env_write; the per-voice envelope
// step in x16_psg_env_tick keeps its asm body but is driven by a C
// loop, with a dirty flag standing in for the ca65 version's
// `jsr env_write` inside the loop -- inline asm cannot call across
// functions.
// =====================================================================

#include <x16/psg.h>
#include <x16/vera.h>

volatile char x16__ps_t;                // register byte under construction
volatile char x16__ps_dirty;            // this voice's volume changed

// Envelope state, one byte per voice. Only the stage array needs a
// known start value (0 = idle: the tick skips the voice); the others
// are written by env_start before the stage arms.
volatile char x16__env_stage[16] = { 0, 0, 0, 0, 0, 0, 0, 0,
                                     0, 0, 0, 0, 0, 0, 0, 0 };
volatile char x16__env_vol[16];
volatile char x16__env_peak[16];
volatile char x16__env_astep[16];
volatile char x16__env_sus[16];
volatile char x16__env_rstep[16];

// ---------------------------------------------------------------------
// Point data port 0 at a voice register, on auto-increment.
// ---------------------------------------------------------------------
void x16_psg_voice_ptr(unsigned char voice, unsigned char offset) {
    __asm {
        lda 0x9f25  /*VERA_CTRL*/
        and #0xfe
        sta 0x9f25

        lda voice
        asl
        asl                             /* voice * 4, never carries (max 60) */
        clc
        adc offset
        clc
        adc #0xc0                        /* <VRAM_PSG, may carry */
        sta 0x9f20                      /* VERA_ADDR_L */
        lda #0xf9                        /* >VRAM_PSG */
        adc #0
        sta 0x9f21                      /* VERA_ADDR_M */
        lda #0x11                        /* ADDR_H_BANK | (VERA_INC_1 << 4) */
        sta 0x9f22                      /* VERA_ADDR_H */
    }
}

// Silence all 16 voices.
void x16_psg_init(void) {
    x16_vera_addr0(X16_INC_1, 0x1F9C0); // X16_VRAM_PSG
    x16_vera_fill(0, 16 * 4);
}

// ---------------------------------------------------------------------
// The HIGH byte is written first, stepping the port DOWNWARD from
// offset 1. Low-byte-first leaves the voice running on new-low/old-high
// for a few cycles -- an audible click on every pitch change.
// ---------------------------------------------------------------------
void x16_psg_set_freq(unsigned char voice, unsigned int freq) {
    x16_psg_voice_ptr(voice, 1);        // point at freq bits 15:8
    __asm {
        lda 0x9f22                      /* VERA_ADDR_H */
        ora #0x08   /* ...and walk backwards (VERA_ADDR_H_DECR) */
        sta 0x9f22                      /* VERA_ADDR_H */
        lda freq+1
        sta 0x9f23        /* high byte first (VERA_DATA0) */
        lda freq
        sta 0x9f23        /* then low, at offset 0 (VERA_DATA0) */
    }
}

void x16_psg_set_vol(unsigned char voice, unsigned char vol,
                     unsigned char pan) {
    __asm {
        lda vol
        and #0x3f
        sta x16__ps_t
        lda pan
        and #0xc0                       /* PSG_PAN_BOTH */
        ora x16__ps_t
        sta x16__ps_t
    }
    x16_psg_voice_ptr(voice, 2);
    __asm {
        lda x16__ps_t
        sta 0x9f23                      /* VERA_DATA0 */
    }
}

void x16_psg_set_wave(unsigned char voice, unsigned char wave,
                      unsigned char width) {
    __asm {
        lda wave
        and #0xc0                        /* keep bits 7:6 */
        sta x16__ps_t
        lda width
        and #0x3f
        ora x16__ps_t
        sta x16__ps_t
    }
    x16_psg_voice_ptr(voice, 3);
    __asm {
        lda x16__ps_t
        sta 0x9f23                      /* VERA_DATA0 */
    }
}

// Volume to zero, everything else kept -- via the host-written shadow.
void x16_psg_note_off(unsigned char voice) {
    x16_psg_voice_ptr(voice, 2);
    __asm {
        lda 0x9f23        /* the host-written shadow (VERA_DATA0) */
        and #0xc0       /* keep the panning, drop the volume (PSG_PAN_BOTH) */
        sta x16__ps_t
    }
    x16_psg_voice_ptr(voice, 2);
    __asm {
        lda x16__ps_t
        sta 0x9f23                      /* VERA_DATA0 */
    }
}

// ---------------------------------------------------------------------
// Internal: write voice's env_vol to its volume bits, preserving the
// pan bits via the shadow, exactly like x16_psg_note_off.
// ---------------------------------------------------------------------
void x16__psg_env_write(unsigned char voice) {
    x16_psg_voice_ptr(voice, 2);
    __asm {
        ldx voice
        lda 0x9f23        /* the shadow's pan bits (VERA_DATA0) */
        and #0xc0                       /* PSG_PAN_BOTH */
        ora x16__env_vol,x
        sta x16__ps_t
    }
    x16_psg_voice_ptr(voice, 2);
    __asm {
        lda x16__ps_t
        sta 0x9f23                      /* VERA_DATA0 */
    }
}

// ---------------------------------------------------------------------
// (Re)trigger a voice's envelope.
// ---------------------------------------------------------------------
void x16_psg_env_start(unsigned char voice, unsigned char peak,
                       unsigned char attack, unsigned char sustain,
                       unsigned char release) {
    __asm {
        lda voice
        and #0x0f
        tax
        lda peak
        and #0x3f
        sta x16__env_peak,x
        lda attack
        sta x16__env_astep,x
        lda sustain
        sta x16__env_sus,x
        lda release
        sta x16__env_rstep,x
        lda attack
        beq es_instant
        lda #0
        sta x16__env_vol,x
        lda #1                          /* stage 1: attack */
        sta x16__env_stage,x
        lda #0
        sta x16__ps_dirty
        jmp es_done
    es_instant:
        lda x16__env_peak,x
        sta x16__env_vol,x
        lda #2                          /* straight to sustain */
        sta x16__env_stage,x
        lda #1                          /* make the jump audible immediately */
        sta x16__ps_dirty
    es_done:
    }
    if (x16__ps_dirty) {
        x16__psg_env_write(voice & 0x0F);
    }
}

// Enter the release phase now.
void x16_psg_env_release(unsigned char voice) {
    __asm {
        lda voice
        and #0x0f
        tax
        lda x16__env_stage,x
        beq er_done                     /* not playing */
        lda #3
        sta x16__env_stage,x
    er_done:
    }
}

// Silence and disarm immediately.
void x16_psg_env_stop(unsigned char voice) {
    __asm {
        lda voice
        and #0x0f
        tax
        lda #0
        sta x16__env_stage,x
        sta x16__env_vol,x
    }
    x16__psg_env_write(voice & 0x0F);
}

// ---------------------------------------------------------------------
// Advance every armed envelope one step and write the changed volumes
// to the PSG. Call once per frame. The voice loop is C; the step logic
// per voice is the ca65 build's, verbatim, with a dirty flag where it
// jsr'd env_write.
// ---------------------------------------------------------------------
void x16_psg_env_tick(void) {
    unsigned char v;

    for (v = 0; v < 16; v++) {
        x16__ps_dirty = 0;
        __asm {
            ldx v
            lda x16__env_stage,x
            beq et_next                 /* 0: idle */
            cmp #2
            beq et_sustain
            bcc et_attack               /* 1 */

            /* --- release --- */
            lda x16__env_rstep,x
            beq et_next                 /* rstep 0: hold until env_stop */
            sta x16__ps_t
            lda x16__env_vol,x
            sec
            sbc x16__ps_t
            bcs et_rel_ok
            lda #0
        et_rel_ok:
            sta x16__env_vol,x
            bne et_write
            sta x16__env_stage,x        /* faded out: disarm (A is 0 -- */
            jmp et_write                /* the bne fell through on Z) */

        et_attack:
            lda x16__env_vol,x
            clc
            adc x16__env_astep,x
            cmp x16__env_peak,x
            bcc et_att_ok
            lda x16__env_peak,x         /* reached (or overshot) the peak */
            pha
            lda #2
            sta x16__env_stage,x
            pla
        et_att_ok:
            sta x16__env_vol,x
            jmp et_write

        et_sustain:
            lda x16__env_sus,x
            cmp #255
            beq et_next                 /* 255: hold until env_release */
            dec x16__env_sus,x
            bne et_next
            lda #3                      /* sustain over: release */
            sta x16__env_stage,x
            jmp et_next                 /* volume unchanged this tick */

        et_write:
            lda #1
            sta x16__ps_dirty
        et_next:
        }
        if (x16__ps_dirty) {
            x16__psg_env_write(v);
        }
    }
}
