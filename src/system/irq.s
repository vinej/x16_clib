; =====================================================================
; x16clib :: system/irq.s -- VSYNC frame counter and IRQ hook
; =====================================================================
; Chains onto the KERNAL's IRQ vector (CINV, $0314) rather than taking
; the interrupt over. Our handler bumps a frame counter and then jumps to
; whatever was there before, so the KERNAL still scans the keyboard,
; moves the mouse, blinks the cursor, and acknowledges the VERA VSYNC
; interrupt. We deliberately do NOT ack it ourselves.
;
; The KERNAL's stub has already pushed A/X/Y by the time it reaches CINV,
; and restores them on the way out, so a chained handler is free to
; clobber the registers.
;
; Under cc65 this composes correctly. cc65's own IRQ path -- the CONDES
; interruptor chain reached through __CALLIRQ__ -- also hangs off CINV,
; so installing here simply inserts us above it, and the KERNAL still
; runs underneath. Registering as a cc65 interruptor instead would be
; more integrated but needs linker CONDES setup and buys nothing.
;
; If you add work of your own to the per-frame path, keep it short and
; save/restore any VERA address port you touch -- an interrupt landing
; between an x16_vera_addr0() and its data access will otherwise move the
; port out from under you.
;
; The handler must NOT call any x16_* routine that uses the X16_P/T
; scratch block: it is shared and not reentrant. See include/x16/irq.h.
; =====================================================================

        .include        "macros.inc"

        .export         _x16_irq_install
        .export         _x16_irq_remove
        .export         _x16_irq_frames
        .export         _x16_vsync_wait

; Unhook automatically at exit, before cc65 returns to BASIC. Without
; this, a program that forgets x16_irq_remove() leaves CINV pointing into
; memory that BASIC is about to reuse -- the next interrupt then jumps
; into whatever landed there. irq_remove is idempotent, so a program that
; does unhook pays nothing.
        .destructor     irq_remove, 10

        .segment        "CODE"

; ---------------------------------------------------------------------
; void x16_irq_install(void) -- start counting frames. Idempotent.
; ---------------------------------------------------------------------
_x16_irq_install:
irq_install:
        lda     irq_armed
        bne     @done

        php                             ; restore the caller's I flag afterwards,
        sei                             ; rather than a blind cli
        lda     CINV
        sta     irq_old_vector
        lda     CINV+1
        sta     irq_old_vector+1
        lda     #<irq_handler
        sta     CINV
        lda     #>irq_handler
        sta     CINV+1
        stz     irq_frame_count
        lda     #VERA_IRQ_VSYNC
        tsb     VERA_IEN                ; the KERNAL already enables it; harmless
        lda     #1
        sta     irq_armed
        plp
@done:
        rts

; ---------------------------------------------------------------------
; void x16_irq_remove(void) -- restore the previous handler. Idempotent.
; ---------------------------------------------------------------------
_x16_irq_remove:
irq_remove:
        lda     irq_armed
        beq     @done
        php
        sei
        lda     irq_old_vector
        sta     CINV
        lda     irq_old_vector+1
        sta     CINV+1
        stz     irq_armed
        plp
@done:
        rts

; ---------------------------------------------------------------------
; unsigned char x16_irq_frames(void)
;   The frame counter, which wraps at 256.
;
; Byte subtraction wraps correctly, so deltas stay valid across the wrap:
;       start = x16_irq_frames();
;       ...work...
;       elapsed = (unsigned char)(x16_irq_frames() - start);
;
; Reading a single byte is atomic against the interrupt that writes it.
; ---------------------------------------------------------------------
_x16_irq_frames:
        lda     irq_frame_count
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; void x16_vsync_wait(void) -- block until the next frame boundary.
;
; Frame-locked: it waits for the counter to change rather than polling
; VERA, so it cannot miss a frame or spin twice within one. Requires
; x16_irq_install(), and interrupts enabled -- it hangs otherwise, which
; is why x16emu's headless -testbench mode (no video, hence no VSYNC)
; must never reach it.
; ---------------------------------------------------------------------
_x16_vsync_wait:
vsync_wait:
        lda     irq_frame_count
@wait:
        cmp     irq_frame_count
        beq     @wait
        rts

; ---------------------------------------------------------------------
; irq_handler -- runs once per frame, then chains
; ---------------------------------------------------------------------
irq_handler:
        lda     VERA_ISR
        and     #VERA_IRQ_VSYNC
        beq     @chain                  ; not a VSYNC: someone else's interrupt
        inc     irq_frame_count
@chain:
        jmp     (irq_old_vector)

; ---------------------------------------------------------------------
; Zeroed by cc65's crt0 (it imports zerobss) before main runs, so
; irq_armed starts clear and the first install is not mistaken for a
; second one.
; ---------------------------------------------------------------------
        .segment        "BSS"

irq_old_vector:  .res 2
irq_frame_count: .res 1
irq_armed:       .res 1
