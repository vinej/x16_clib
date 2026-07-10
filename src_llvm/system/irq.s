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

        .include        "macros.inc"
        .include        "x16zp.inc"

        .globl  x16_irq_install
        .globl  x16_irq_remove
        .globl  x16_irq_frames
        .globl  x16_vsync_wait
        .globl  x16_irq_line_install
        .globl  x16_irq_line_remove
        .globl  x16_irq_sprcol_install
        .globl  x16_irq_sprcol_remove
        .globl  x16_sprite_collisions

; audio/pcmstream.s installs its AFLOW service here, and needs the CINV
; hook in place before it does. Nothing else may touch either.
        .globl  irq_aflow_vec
        .globl  irq_install


; Unhook automatically at exit, before the program returns to BASIC.
; Without this, a program that forgets x16_irq_remove() leaves CINV
; pointing into memory that BASIC is about to reuse -- the next interrupt
; then jumps into whatever landed there. irq_remove is idempotent, so a
; program that does unhook pays nothing.
;
; cc65 spells this `.destructor irq_remove, 10`. llvm-mos has no such
; directive; it uses the ELF convention, a table of function pointers that
; the runtime's __do_fini_array walks on the way out. commodore.ld keeps
; the section, so a plain .word is all it takes.
        .section .fini_array,"aw",@fini_array
        .word   irq_remove

; ---------------------------------------------------------------------
; The zero page a C callback can destroy.
;
; cc65 needed three separate blocks. llvm-mos needs two, because on this
; target the compiler's imaginary registers and the KERNAL's virtual
; registers are THE SAME BYTES: cx16/lib/imag-regs.ld aliases __rc2..__rc29
; onto r0..r15, and puts __rc0/__rc1 and __rc30/__rc31 just above them.
; So $02-$25 covers all thirty-two imaginary registers, all sixteen KERNAL
; registers, and the r4/r5 hole between them -- one contiguous run.
;
; The library's own P/T block is separate, wherever the linker placed it.
; ---------------------------------------------------------------------
IMAG_SAVE_BASE   = $02                  ; __rc0..__rc31 == KERNAL r0..r15
IMAG_SAVE_SIZE   = $26 - $02            ; 36 bytes, $02..$25 inclusive
X16_ZP_SAVE_SIZE = 16                   ; X16_P0..X16_T7

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; irq_handler -- services VSYNC / LINE / SPRCOL / AFLOW, then chains
; ---------------------------------------------------------------------
irq_handler:
        lda     VERA_ISR
        sta     irq_isr

        and     #VERA_IRQ_VSYNC
        beq     .Lirq_handler_no_vsync
        inc     irq_frame_count         ; the KERNAL acks VSYNC for us
.Lirq_handler_no_vsync:

        lda     irq_isr
        and     #VERA_IRQ_LINE
        beq     .Lirq_handler_no_line
        sta     VERA_ISR                ; ack FIRST: nobody else will
        lda     irq_line_armed
        beq     .Lirq_handler_no_line
        jsr     save_zp
        jsr     call_line
        jsr     restore_zp
.Lirq_handler_no_line:

        lda     irq_isr
        and     #VERA_IRQ_SPRCOL
        beq     .Lirq_handler_no_sprcol
        sta     VERA_ISR                ; ack FIRST: nobody else will
        lda     irq_isr
        and     #VERA_ISR_COLLISION     ; which collision groups fired (7:4)
        ora     irq_sprcol_mask         ; accumulate until sprite_collisions
        sta     irq_sprcol_mask         ; reads and clears it
        lda     irq_sprcol_armed
        beq     .Lirq_handler_no_sprcol
        jsr     save_zp
        lda     irq_isr
        and     #VERA_ISR_COLLISION
        jsr     call_sprcol             ; A = the collision groups
        jsr     restore_zp
.Lirq_handler_no_sprcol:

        ; AFLOW: this library's own assembly service, or nobody. No zero
        ; page to save, and no C callback to protect.
        lda     irq_isr
        and     #VERA_IRQ_AFLOW
        beq     .Lirq_handler_no_aflow
        lda     irq_aflow_vec
        ora     irq_aflow_vec+1
        beq     .Lirq_handler_no_aflow               ; nothing has claimed the source
        jsr     call_aflow
.Lirq_handler_no_aflow:

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
save_zp:
        ldx     #IMAG_SAVE_SIZE - 1
.Lsave_zp_imag:
        lda     IMAG_SAVE_BASE,x        ; __rc0..__rc31, and r0..r15 with them
        sta     zp_save,x
        dex
        bpl     .Lsave_zp_imag
        ldx     #X16_ZP_SAVE_SIZE - 1
.Lsave_zp_lib:
        lda     X16_P0,x
        sta     zp_save + IMAG_SAVE_SIZE,x
        dex
        bpl     .Lsave_zp_lib
        rts

restore_zp:
        ldx     #IMAG_SAVE_SIZE - 1
.Lrestore_zp_imag:
        lda     zp_save,x
        sta     IMAG_SAVE_BASE,x
        dex
        bpl     .Lrestore_zp_imag
        ldx     #X16_ZP_SAVE_SIZE - 1
.Lrestore_zp_lib:
        lda     zp_save + IMAG_SAVE_SIZE,x
        sta     X16_P0,x
        dex
        bpl     .Lrestore_zp_lib
        rts

; ---------------------------------------------------------------------
; void x16_irq_install(void) -- hook CINV and count frames. Idempotent.
; ---------------------------------------------------------------------
x16_irq_install:
irq_install:
        lda     irq_armed
        bne     .Lirq_install_done

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
.Lirq_install_done:
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
x16_irq_remove:
irq_remove:
        lda     irq_armed
        beq     .Lirq_remove_done
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
.Lirq_remove_done:
        rts

; ---------------------------------------------------------------------
; unsigned char x16_irq_frames(void)
;   The frame counter, which wraps at 256. Byte subtraction wraps
;   correctly, so deltas stay valid across the wrap.
; ---------------------------------------------------------------------
x16_irq_frames:
        lda     irq_frame_count         ; a char return is A alone
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
x16_vsync_wait:
vsync_wait:
        lda     irq_frame_count
.Lvsync_wait_wait:
        cmp     irq_frame_count
        beq     .Lvsync_wait_wait
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
; `line` is an int -> A/X. `handler` is a FUNCTION POINTER, so it takes the
; first __rc pair, __rc2/__rc3, and not A/X as under cc65.
; irq_line_install wants A/X = handler, X16_P0/P1 = line.
x16_irq_line_install:
        sta     X16_P0                  ; line lo
        stx     X16_P1                  ; line hi
        lda     __rc2                   ; A/X = handler
        ldx     __rc3
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
        bcs     .Lirq_line_install_bit8_set
        trb     VERA_IEN
        bra     .Lirq_line_install_bit8_done
.Lirq_line_install_bit8_set:
        tsb     VERA_IEN
.Lirq_line_install_bit8_done:
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
x16_irq_line_remove:
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
; The handler is a pointer, so it arrives in __rc2/__rc3, not A/X --
; irq_sprcol_install wants A/X. cc65 needed no shim here; llvm-mos does.
; ---------------------------------------------------------------------
x16_irq_sprcol_install:
        lda     __rc2
        ldx     __rc3
        ; fall through
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
        beq     .Lirq_sprcol_install_polling
        lda     #1
.Lirq_sprcol_install_polling:
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
x16_irq_sprcol_remove:
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
x16_sprite_collisions:
        php
        sei                             ; read-and-clear must be atomic against
        lda     irq_sprcol_mask         ; the accumulating interrupt handler
        stz     irq_sprcol_mask
        plp
        rts

; ---------------------------------------------------------------------
; Zeroed by the runtime before main runs, so irq_armed starts clear and
; irq_aflow_vec starts unclaimed.
; ---------------------------------------------------------------------
        .section .bss,"aw",@nobits

irq_old_vector:   .zero  2
irq_frame_count:  .zero  1
irq_armed:        .zero  1
irq_isr:          .zero  1                ; ISR snapshot for the current interrupt

irq_line_vec:     .zero  2
irq_line_armed:   .zero  1
irq_sprcol_vec:   .zero  2
irq_sprcol_armed: .zero  1
irq_sprcol_mask:  .zero  1                ; groups seen since the last read

irq_aflow_vec:    .zero  2                ; audio/pcm.s claims this, or nobody

zp_save:          .zero  IMAG_SAVE_SIZE + X16_ZP_SAVE_SIZE
