// =====================================================================
// x16clib :: x16/irq.c -- VSYNC counter, raster line and sprite
//                         collision interrupts
// =====================================================================
// Chains onto the KERNAL's IRQ vector (CINV, $0314). The handler
// services its own sources and then jumps to whatever was there before,
// so the KERNAL still scans the keyboard and acknowledges VSYNC. LINE
// and SPRCOL must be acknowledged HERE (writing their ISR bit), or the
// moment the handler returns the same interrupt fires again and the
// machine livelocks.
//
// ---------------------------------------------------------------------
// HOW THE C CALLBACKS ARE REACHED, AND WHY THERE IS A DISPATCHER.
//
// Inline asm can only jsr labels in its own block and jmp through
// pointer VARIABLES -- it cannot name another C function. So every
// C-function address the handler needs sits in a statically initialised
// function-pointer global, and the handler jumps through those:
//
//   x16__irq_handler_ptr   -> the handler itself, for the CINV swap
//   x16__irq_dispatch_ptr  -> x16__irq_dispatch(), which calls the
//                             user's callbacks in plain C
//   x16__irq_aflow_vec     -> the PCM streamer's asm service, or 0
//
// The asm half only sets `fired` flags; the C dispatcher runs strictly
// between save_zp and restore_zp, because compiled Oscar64 code -- the
// dispatcher included -- uses zero-page locations the interrupted
// mainline owns mid-expression: the register file growing up from $23,
// and the __zeropage region at $F7-$FF (where this library keeps three
// slots). A callback that calls the KERNAL also scratches r0-r15 at
// $02-$21. So the save is $02-$8F plus $F7-$FF, 151 bytes each way --
// $02-$8F rather than the observed ceiling ($54) because the register
// file's upper bound under pressure is the compiler's business, not a
// contract. ~2.5% of a frame, paid only when a callback is actually
// installed; it is what lets a callback call ANYTHING.
//
// The cc65 build unhooked itself at exit through a linker destructor.
// This port keeps that manual: a program that returns to BASIC must
// call x16_irq_remove() itself.
// =====================================================================

#include <x16/irq.h>

void x16__irq_handler(void);
void x16__irq_dispatch(void);

// All state explicitly zeroed: this must not depend on leftover RAM.
volatile char x16__irq_armed = 0;
volatile char x16__irq_isrsnap = 0;     // ISR snapshot, current interrupt
volatile char x16__irq_nframes = 0;

volatile char x16__irq_line_armed = 0;
volatile char x16__irq_line_fired = 0;
volatile x16_irq_handler x16__irq_line_fn = 0;

volatile char x16__irq_sprcol_armed = 0;
volatile char x16__irq_sprcol_fired = 0;
volatile char x16__irq_groups = 0;      // groups for the pending callback
volatile char x16__irq_sprcol_mask = 0; // accumulated since the last read
volatile x16_sprcol_handler x16__irq_sprcol_fn = 0;

// audio/pcm claims this, or nobody. An asm-level service: no zp save.
volatile x16_irq_handler x16__irq_aflow_vec = 0;

volatile x16_irq_handler x16__irq_old_fn = 0;

// The statically initialised trampoline targets (see the header
// comment). volatile, so the compiler cannot fold them away.
volatile x16_irq_handler x16__irq_handler_ptr = &x16__irq_handler;
volatile x16_irq_handler x16__irq_dispatch_ptr = &x16__irq_dispatch;

char x16__irq_zpsave[151];              // $02-$8F and $F7-$FF park here

// ---------------------------------------------------------------------
// The interrupt handler. Entered via CINV with I set and A/X/Y already
// saved by the KERNAL's stub; leaves by jumping to the previous vector,
// whose code ends with the KERNAL's restore-and-rti.
// ---------------------------------------------------------------------
void x16__irq_handler(void) {
    __asm {
        lda 0x9f27                      /* VERA_ISR */
        sta x16__irq_isrsnap

        and #0x01                       /* VERA_IRQ_VSYNC */
        beq ih_no_vsync
        inc x16__irq_nframes            /* the KERNAL acks VSYNC for us */
    ih_no_vsync:

        lda x16__irq_isrsnap
        and #0x02                       /* VERA_IRQ_LINE */
        beq ih_no_line
        sta 0x9f27          /* ack FIRST: nobody else will (VERA_ISR) */
        lda x16__irq_line_armed
        beq ih_no_line
        lda #1
        sta x16__irq_line_fired
    ih_no_line:

        lda x16__irq_isrsnap
        and #0x04                       /* VERA_IRQ_SPRCOL */
        beq ih_no_sprcol
        sta 0x9f27          /* ack FIRST: nobody else will (VERA_ISR) */
        lda x16__irq_isrsnap
        and #0xf0 /* which groups fired (7:4) (VERA_ISR_COLLISION) */
        ora x16__irq_sprcol_mask        /* accumulate until */
        sta x16__irq_sprcol_mask        /* x16_sprite_collisions reads it */
        lda x16__irq_sprcol_armed
        beq ih_no_sprcol
        lda x16__irq_isrsnap
        and #0xf0
        sta x16__irq_groups
        lda #1
        sta x16__irq_sprcol_fired
    ih_no_sprcol:

        /* AFLOW: this library's own assembly service, or nobody. No */
        /* zero page to save, and no C callback to protect. */
        lda x16__irq_isrsnap
        and #0x08                       /* VERA_IRQ_AFLOW */
        beq ih_no_aflow
        lda x16__irq_aflow_vec
        ora x16__irq_aflow_vec+1
        beq ih_no_aflow                 /* nothing has claimed the source */
        jsr ih_aflow
    ih_no_aflow:

        /* C callbacks pending? Park the zero page, dispatch, restore. */
        lda x16__irq_line_fired
        ora x16__irq_sprcol_fired
        beq ih_chain
        jsr ih_save
        jsr ih_dispatch
        jsr ih_restore
    ih_chain:
        jmp (x16__irq_old_fn)

    ih_aflow:
        jmp (x16__irq_aflow_vec)
    ih_dispatch:
        jmp (x16__irq_dispatch_ptr)

        /* Park 0x02-0x8F and 0xF7-0xFF and put them back. Neither */
        /* loop can be interrupted: the KERNAL's stub reaches CINV with */
        /* I set. */
    ih_save:
        ldx #142
    ih_sv:
        lda 0x01,x
        sta x16__irq_zpsave-1,x
        dex
        bne ih_sv
        ldx #9
    ih_sv2:
        lda 0xf6,x
        sta x16__irq_zpsave+141,x
        dex
        bne ih_sv2
        rts
    ih_restore:
        ldx #142
    ih_rs:
        lda x16__irq_zpsave-1,x
        sta 0x01,x
        dex
        bne ih_rs
        ldx #9
    ih_rs2:
        lda x16__irq_zpsave+141,x
        sta 0xf6,x
        dex
        bne ih_rs2
        rts
    }
}

// ---------------------------------------------------------------------
// Plain C, but running inside the interrupt -- only ever called between
// ih_save and ih_restore, so it (and the user's callbacks) may touch
// any zero page and call anything.
// ---------------------------------------------------------------------
void x16__irq_dispatch(void) {
    unsigned char g;

    if (x16__irq_line_fired) {
        x16__irq_line_fired = 0;
        (*x16__irq_line_fn)();
    }
    if (x16__irq_sprcol_fired) {
        x16__irq_sprcol_fired = 0;
        g = x16__irq_groups;
        (*x16__irq_sprcol_fn)(g);
    }
}

// ---------------------------------------------------------------------
// Hook CINV and count frames. Idempotent.
// ---------------------------------------------------------------------
void x16_irq_install(void) {
    if (x16__irq_armed) {
        return;
    }
    __asm {
        php                             /* restore the caller's I flag */
        sei                             /* after, rather than a blind cli */
        lda 0x0314                      /* CINV */
        sta x16__irq_old_fn
        lda 0x0315
        sta x16__irq_old_fn+1
        lda x16__irq_handler_ptr
        sta 0x0314
        lda x16__irq_handler_ptr+1
        sta 0x0315
        lda #0
        sta x16__irq_nframes
        lda 0x9f26  /*VERA_IEN*/          /* the KERNAL enables it anyway */
        ora #0x01
        sta 0x9f26
        lda #1
        sta x16__irq_armed
        plp
    }
}

// ---------------------------------------------------------------------
// Restore the previous handler and disable every source we own. AFLOW
// goes too: leaving it enabled with our handler unhooked would assert
// the IRQ line forever -- the KERNAL does not acknowledge AFLOW, and
// AFLOW only clears by refilling the FIFO. A stream in progress
// therefore stops here.
// ---------------------------------------------------------------------
void x16_irq_remove(void) {
    if (!x16__irq_armed) {
        return;
    }
    __asm {
        php
        sei
        lda 0x9f26  /*VERA_IEN*/          /* ours alone; VSYNC stays KERNAL's */
        and #0xf1
        sta 0x9f26
        lda #0
        sta x16__irq_line_armed
        sta x16__irq_line_fired
        sta x16__irq_sprcol_armed
        sta x16__irq_sprcol_fired
        sta x16__irq_aflow_vec
        sta x16__irq_aflow_vec+1
        lda x16__irq_old_fn
        sta 0x0314                      /* CINV */
        lda x16__irq_old_fn+1
        sta 0x0315
        lda #0
        sta x16__irq_armed
        plp
    }
}

unsigned char x16_irq_frames(void) {
    return x16__irq_nframes;
}

void x16_vsync_wait(void) {
    __asm {
        lda x16__irq_nframes
    vw_wait:
        cmp x16__irq_nframes
        beq vw_wait
    }
}

// ---------------------------------------------------------------------
// The vector store is plain C, safe without sei: the LINE source is not
// enabled until the asm block below arms it, and x16_irq_remove
// disarmed it if it was.
// ---------------------------------------------------------------------
void x16_irq_line_install(unsigned int line, x16_irq_handler handler) {
    x16_irq_install();                  // make sure the CINV hook is in
    x16__irq_line_fn = handler;
    __asm {
        php
        sei
        lda line
        sta 0x9f28                      /* VERA_IRQ_LINE_L */
        lda line+1
        lsr                             /* scanline bit 8 -> carry */
        bcs il_bit8_set
        lda 0x9f26                      /* ...lives in IEN bit 7 */
        and #0x7f
        sta 0x9f26                      /* VERA_IEN */
        jmp il_bit8_done
    il_bit8_set:
        lda 0x9f26
        ora #0x80
        sta 0x9f26                      /* VERA_IEN */
    il_bit8_done:
        lda #0x02                       /* VERA_IRQ_LINE */
        sta 0x9f27          /* drop any stale pending LINE (VERA_ISR) */
        lda #1
        sta x16__irq_line_armed
        lda 0x9f26  /*VERA_IEN*/
        ora #0x02
        sta 0x9f26
        plp
    }
}

void x16_irq_line_remove(void) {
    __asm {
        php
        sei
        lda 0x9f26  /*VERA_IEN*/
        and #0xfd
        sta 0x9f26
        sta 0x9f27          /* ack anything still pending (VERA_ISR) */
        lda #0
        sta x16__irq_line_armed
        sta x16__irq_line_fired
        plp
    }
}

// ---------------------------------------------------------------------
// handler may be NULL: the groups still accumulate for
// x16_sprite_collisions(), but nothing is called.
// ---------------------------------------------------------------------
void x16_irq_sprcol_install(x16_sprcol_handler handler) {
    x16_irq_install();
    x16__irq_sprcol_fn = handler;
    x16__irq_sprcol_armed = (handler != 0) ? 1 : 0;
    __asm {
        php
        sei
        lda #0
        sta x16__irq_sprcol_mask
        sta x16__irq_sprcol_fired
        lda #0x04                       /* VERA_IRQ_SPRCOL */
        sta 0x9f27          /* drop any stale pending collision (VERA_ISR) */
        lda 0x9f26
        ora #0x04
        sta 0x9f26                      /* VERA_IEN */
        plp
    }
}

void x16_irq_sprcol_remove(void) {
    __asm {
        php
        sei
        lda 0x9f26  /*VERA_IEN*/
        and #0xfb
        sta 0x9f26
        sta 0x9f27                      /* VERA_ISR */
        lda #0
        sta x16__irq_sprcol_armed
        sta x16__irq_sprcol_fired
        plp
    }
}

// ---------------------------------------------------------------------
// The collision group bits (ISR 7:4) seen since the last call, then
// cleared. The read-and-clear is atomic against the accumulating
// interrupt handler.
// ---------------------------------------------------------------------
unsigned char x16_sprite_collisions(void) {
    return __asm {
        php
        sei
        lda x16__irq_sprcol_mask
        sta accu
        lda #0
        sta x16__irq_sprcol_mask
        plp
    };
}
