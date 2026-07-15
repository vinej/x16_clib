; =====================================================================
; x16clib :: system/irq.s -- VSYNC counter, raster line and sprite
;                            collision interrupts
; =====================================================================
; Chains onto the KERNAL's IRQ vector (CINV, $0314) rather than taking
; the interrupt over. Our handler services its own sources and then jumps
; to whatever was there before, so the KERNAL still scans the keyboard,
; moves the mouse, blinks the cursor, and acknowledges the VERA VSYNC
; interrupt.
;
; The KERNAL only ever acknowledges VSYNC. LINE and SPRCOL must be
; acknowledged here (writing their ISR bit), or the moment the handler
; returns the same interrupt fires again and the machine livelocks.
;
; Under cc65 this composes correctly: cc65's own CONDES interruptor chain
; also hangs off CINV, so installing here inserts us above it and the
; KERNAL still runs underneath.
;
; ---------------------------------------------------------------------
; TWO THINGS THE ASSEMBLY LIBRARY DID NOT HAVE TO SOLVE.
;
; 1. Zero page. A user callback runs inside the interrupt, and in C it is
;    compiled code that uses cc65's zero-page runtime (sp, sreg, ptr1-4,
;    tmp1-4, regbank) and may call library routines that use X16_P0..T7
;    or the KERNAL's virtual registers r0-r15. The interrupted code owns
;    all three blocks, mid-expression. mem_copy in particular loads r0-r2
;    and then runs MEMORY_COPY with interrupts enabled: a callback that
;    calls another mem_* or mouse_get would corrupt the interrupted
;    copy's pointers on resume.
;
;    So all three are saved before a callback and restored after -- 74
;    bytes, about 1,600 cycles, and only when a callback is actually
;    installed. That is roughly 1.2% of a frame, and it is what makes a
;    raster handler written in C correct rather than usually-correct: it
;    may call anything.
;
;    The AFLOW handler is exempt: it is this library's own assembly and
;    touches none of them.
;
; 2. Module coupling. The ACME original called pcm_stream_isr behind an
;    !ifdef, which cannot survive separate compilation -- irq.o would
;    drag the audio module into every program. Instead the AFLOW service
;    is a runtime vector, irq_aflow_vec, which audio/pcm.s fills in. When
;    nothing has claimed it the handler skips the source entirely.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

; vbcc's soft-stack pointer and 32-bit accumulators, saved around a C
; callback (see save_zp). They are volatile, so a callback may use them.
        zpage	sp
        zpage	btmp0

        global	_x16_irq_install
        global	_x16_irq_remove
        global	_x16_irq_frames
        global	_x16_vsync_wait
        global	_x16_irq_line_install
        global	_x16_irq_line_remove
        global	_x16_irq_sprcol_install
        global	_x16_irq_sprcol_remove
        global	_x16_sprite_collisions

; audio/pcmstream.s installs its AFLOW service here, and needs the CINV
; hook in place before it does. Nothing else may touch either.
        global	irq_aflow_vec
        global	irq_install

; vbcc argument registers: irq_line_install takes the scanline in r0/r1
; and the handler pointer in r2/r3.
        zpage	r0
        zpage	r1
        zpage	r2
        zpage	r3

; NOTE: cc65's build auto-unhooks the CINV vector at program exit via a
; .destructor. vbcc's +x16 runtime has no ctor/dtor mechanism, so that
; safety net is gone: a program that installs an IRQ handler MUST call
; x16_irq_remove() itself before returning, or the next interrupt after
; exit jumps through a stale vector into whatever BASIC has reused.

; What a C callback might clobber, and therefore what save_zp parks:
;   the library's 16-byte P/T block (X16_P0..X16_T7)
;   the KERNAL's virtual registers r0..r15 at $02..$21 -- which are ALSO
;     vbcc's own pseudo-registers r0..r15, so this one range covers both
;   the rest of vbcc's runtime: r16..r31 (compiler temporaries), sp (the
;     C soft-stack pointer) and btmp0..btmp3 (the 32-bit accumulators)
X16_ZP_SAVE_SIZE = 16                   ; X16_P0..X16_T7
VREG_SAVE_SIZE   = 32                   ; vbcc/KERNAL r0..r31 at $02..$21
SP_SAVE_SIZE     = 2                     ; vbcc's soft-stack pointer
BTMP_SAVE_SIZE   = 16                   ; btmp0..btmp3, 4 bytes each

        section text

; ---------------------------------------------------------------------
; irq_handler -- services VSYNC / LINE / SPRCOL / AFLOW, then chains
; ---------------------------------------------------------------------
irq_handler:
        lda     VERA_ISR
        sta     irq_isr

        and     #VERA_IRQ_VSYNC
        beq     .no_vsync
        inc     irq_frame_count         ; the KERNAL acks VSYNC for us
.no_vsync:

        lda     irq_isr
        and     #VERA_IRQ_LINE
        beq     .no_line
        sta     VERA_ISR                ; ack FIRST: nobody else will
        lda     irq_line_armed
        beq     .no_line
        jsr     save_zp
        jsr     call_line
        jsr     restore_zp
.no_line:

        lda     irq_isr
        and     #VERA_IRQ_SPRCOL
        beq     .no_sprcol
        sta     VERA_ISR                ; ack FIRST: nobody else will
        lda     irq_isr
        and     #VERA_ISR_COLLISION     ; which collision groups fired (7:4)
        ora     irq_sprcol_mask         ; accumulate until sprite_collisions
        sta     irq_sprcol_mask         ; reads and clears it
        lda     irq_sprcol_armed
        beq     .no_sprcol
        jsr     save_zp
        lda     irq_isr
        and     #VERA_ISR_COLLISION
        jsr     call_sprcol             ; A = the collision groups
        jsr     restore_zp
.no_sprcol:

        ; AFLOW: this library's own assembly service, or nobody. No zero
        ; page to save, and no C callback to protect.
        lda     irq_isr
        and     #VERA_IRQ_AFLOW
        beq     .no_aflow
        lda     irq_aflow_vec
        ora     irq_aflow_vec+1
        beq     .no_aflow               ; nothing has claimed the source
        jsr     call_aflow
.no_aflow:

        jmp     (irq_old_vector)

call_line:
        jmp     (irq_line_vec)
call_sprcol:
        jmp     (irq_sprcol_vec)
call_aflow:
        jmp     (irq_aflow_vec)

; ---------------------------------------------------------------------
; Park every zero-page block a callback might touch, and put them back.
;
;   cc65's runtime      zpspace (26) bytes from `sp`
;   the library's       16 bytes from X16_P0
;   the KERNAL's        32 bytes of r0-r15, at $02-$21
;
; Neither loop can be interrupted: the KERNAL's stub reaches CINV with
; the I flag already set.
; ---------------------------------------------------------------------
; The four save regions sit consecutively in zp_save, in this order:
;   [0 .. VREG_SAVE_SIZE)                     r0..r31        ($02..$21)
;   [VREG .. +X16_ZP_SAVE_SIZE)               X16_P0..X16_T7
;   [.. +SP_SAVE_SIZE)                        sp
;   [.. +BTMP_SAVE_SIZE)                      btmp0..btmp3
VREG_OFF = 0
LIB_OFF  = VREG_OFF + VREG_SAVE_SIZE
SP_OFF   = LIB_OFF + X16_ZP_SAVE_SIZE
BTMP_OFF = SP_OFF + SP_SAVE_SIZE

save_zp:
        ldx     #VREG_SAVE_SIZE - 1
.vreg:
        lda     r0L,x                   ; vbcc r0..r31 = KERNAL r0..r15, $02..$21
        sta     zp_save + VREG_OFF,x
        dex
        bpl     .vreg
        ldx     #X16_ZP_SAVE_SIZE - 1
.lib:
        lda     X16_P0,x
        sta     zp_save + LIB_OFF,x
        dex
        bpl     .lib
        ldx     #SP_SAVE_SIZE - 1
.sp:
        lda     sp,x
        sta     zp_save + SP_OFF,x
        dex
        bpl     .sp
        ldx     #BTMP_SAVE_SIZE - 1
.btmp:
        lda     btmp0,x
        sta     zp_save + BTMP_OFF,x
        dex
        bpl     .btmp
        rts

restore_zp:
        ldx     #VREG_SAVE_SIZE - 1
.vreg:
        lda     zp_save + VREG_OFF,x
        sta     r0L,x
        dex
        bpl     .vreg
        ldx     #X16_ZP_SAVE_SIZE - 1
.lib:
        lda     zp_save + LIB_OFF,x
        sta     X16_P0,x
        dex
        bpl     .lib
        ldx     #SP_SAVE_SIZE - 1
.sp:
        lda     zp_save + SP_OFF,x
        sta     sp,x
        dex
        bpl     .sp
        ldx     #BTMP_SAVE_SIZE - 1
.btmp:
        lda     zp_save + BTMP_OFF,x
        sta     btmp0,x
        dex
        bpl     .btmp
        rts

; ---------------------------------------------------------------------
; void x16_irq_install(void) -- hook CINV and count frames. Idempotent.
; ---------------------------------------------------------------------
_x16_irq_install:
irq_install:
        lda     irq_armed
        bne     .done

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
.done:
        rts

; ---------------------------------------------------------------------
; void x16_irq_remove(void)
;   Restore the previous handler and disable every source we own.
;
; AFLOW goes too, unlike in the assembly library. Leaving it enabled with
; our handler unhooked would assert the IRQ line forever: the KERNAL does
; not acknowledge AFLOW, and AFLOW only clears by refilling the FIFO.
; A stream in progress therefore stops here.
; ---------------------------------------------------------------------
_x16_irq_remove:
irq_remove:
        lda     irq_armed
        beq     .done
        php
        sei
        lda     #(VERA_IRQ_LINE | VERA_IRQ_SPRCOL | VERA_IRQ_AFLOW)
        trb     VERA_IEN                ; ours alone; VSYNC stays for the KERNAL
        stz     irq_line_armed
        stz     irq_sprcol_armed
        stz     irq_aflow_vec
        stz     irq_aflow_vec+1
        lda     irq_old_vector
        sta     CINV
        lda     irq_old_vector+1
        sta     CINV+1
        stz     irq_armed
        plp
.done:
        rts

; ---------------------------------------------------------------------
; unsigned char x16_irq_frames(void)
;   The frame counter, which wraps at 256. Byte subtraction wraps
;   correctly, so deltas stay valid across the wrap.
; ---------------------------------------------------------------------
_x16_irq_frames:
        lda     irq_frame_count
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; void x16_vsync_wait(void) -- block until the next frame boundary.
;
; Waits for the counter to change rather than polling VERA, so it can
; neither miss a frame nor spin twice within one. Requires
; x16_irq_install() and enabled interrupts; it hangs otherwise, which is
; why x16emu's headless -testbench (no video, hence no VSYNC) must never
; reach it.
; ---------------------------------------------------------------------
_x16_vsync_wait:
vsync_wait:
        lda     irq_frame_count
.wait:
        cmp     irq_frame_count
        beq     .wait
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_irq_line_install(unsigned int line,
;                                        x16_irq_handler handler)
;
; The handler arrives in A/X and irq_install clobbers both, so it rides
; the hardware stack across the pop AND across the install. Getting that
; wrong is invisible to any headless test: the vector is only
; dereferenced when the interrupt actually fires.
; ---------------------------------------------------------------------
; void x16_irq_line_install(__reg("r0/r1") unsigned int line,
;                           __reg("r2/r3") x16_irq_handler handler)
;   irq_line_install wants A/X = handler, X16_P0/P1 = scanline.
_x16_irq_line_install:
        lda     r0
        sta     X16_P0                  ; scanline
        lda     r1
        sta     X16_P1
        lda     r2                      ; A = handler low
        ldx     r3                      ; X = handler high
        ; fall through

; irq_line_install -- in: A/X = handler, X16_P0/P1 = scanline (0-511)
irq_line_install:
        pha                             ; irq_install clobbers A -- and A/X are
        phx                             ; the handler this routine exists to keep
        jsr     irq_install             ; make sure the CINV hook is in place
        plx
        pla
        php
        sei
        sta     irq_line_vec
        stx     irq_line_vec+1
        lda     X16_P0
        sta     VERA_IRQ_LINE_L
        lda     X16_P1
        lsr     a                       ; scanline bit 8 -> carry
        lda     #$80                    ; ...lives in IEN bit 7
        bcs     .bit8_set
        trb     VERA_IEN
        bra     .bit8_done
.bit8_set:
        tsb     VERA_IEN
.bit8_done:
        lda     #VERA_IRQ_LINE
        sta     VERA_ISR                ; drop any stale pending LINE interrupt
        lda     #1
        sta     irq_line_armed
        lda     #VERA_IRQ_LINE
        tsb     VERA_IEN
        plp
        rts

; ---------------------------------------------------------------------
; void x16_irq_line_remove(void)
; ---------------------------------------------------------------------
_x16_irq_line_remove:
        php
        sei
        lda     #VERA_IRQ_LINE
        trb     VERA_IEN
        sta     VERA_ISR                ; ack anything still pending
        stz     irq_line_armed
        plp
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_irq_sprcol_install(x16_sprcol_handler handler)
;   handler may be NULL: the groups still accumulate for
;   x16_sprite_collisions(), but nothing is called.
;
; The pointer already arrives in A/X, which is what irq_sprcol_install
; wants: no shim.
; ---------------------------------------------------------------------
_x16_irq_sprcol_install:
irq_sprcol_install:
        pha                             ; irq_install clobbers A
        phx
        jsr     irq_install
        plx
        pla
        php
        sei
        sta     irq_sprcol_vec
        stx     irq_sprcol_vec+1
        ora     irq_sprcol_vec+1        ; A|X == 0 -> poll-only, no callback
        beq     .polling
        lda     #1
.polling:
        sta     irq_sprcol_armed
        stz     irq_sprcol_mask
        lda     #VERA_IRQ_SPRCOL
        sta     VERA_ISR                ; drop any stale pending collision
        tsb     VERA_IEN
        plp
        rts

; ---------------------------------------------------------------------
; void x16_irq_sprcol_remove(void)
; ---------------------------------------------------------------------
_x16_irq_sprcol_remove:
        php
        sei
        lda     #VERA_IRQ_SPRCOL
        trb     VERA_IEN
        sta     VERA_ISR
        stz     irq_sprcol_armed
        plp
        rts

; ---------------------------------------------------------------------
; unsigned char x16_sprite_collisions(void)
;   The collision group bits (ISR 7:4) seen since the last call, then
;   cleared. Requires x16_irq_sprcol_install(); a NULL handler is fine.
; ---------------------------------------------------------------------
_x16_sprite_collisions:
        php
        sei                             ; read-and-clear must be atomic against
        lda     irq_sprcol_mask         ; the accumulating interrupt handler
        stz     irq_sprcol_mask
        plp
        ldx     #0
        rts

; ---------------------------------------------------------------------
; Zeroed by cc65's crt0 (it imports zerobss) before main runs, so
; irq_armed starts clear, and irq_aflow_vec starts unclaimed.
; ---------------------------------------------------------------------
        section bss

irq_old_vector:   reserve 2
irq_frame_count:  reserve 1
irq_armed:        reserve 1
irq_isr:          reserve 1                ; ISR snapshot for the current interrupt

irq_line_vec:     reserve 2
irq_line_armed:   reserve 1
irq_sprcol_vec:   reserve 2
irq_sprcol_armed: reserve 1
irq_sprcol_mask:  reserve 1                ; groups seen since the last read

irq_aflow_vec:    reserve 2                ; audio/pcm.s claims this, or nobody

zp_save:          reserve VREG_SAVE_SIZE + X16_ZP_SAVE_SIZE + SP_SAVE_SIZE + BTMP_SAVE_SIZE
