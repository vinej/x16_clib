; =====================================================================
; x16clib :: gfx/verafx.s -- VERA FX: hardware multiply, fast fills,
;                            hardware lines and filled triangles
; =====================================================================
; Requires VERA firmware v0.3.1+ (emulator R44+). Probe with
; x16_vera_has_fx() before calling anything here; on older VERA these
; routines write to registers that do not exist and quietly do the wrong
; thing.
;
; The FX registers are $9F29-$9F2C banked behind DCSEL 2..6. Always
; select the bank with the vera_dcsel macro, which preserves ADDRSEL --
; writing VERA_CTRL directly (as the reference manual's examples do)
; would deselect whatever data port the caller had chosen.
;
; Every routine here leaves FX disabled (FX_CTRL = 0, Addr1 Mode 0) and
; DCSEL back at 0. Leaving Addr1 Mode set would silently change how
; ordinary VRAM addressing behaves for everyone downstream.
;
; ---------------------------------------------------------------------
; TWO HARDWARE FACTS THE FX REFERENCE DOES NOT TELL YOU. Both were found
; by reading the emulator's video.c, and both are load-bearing:
;
;   * Every write to an ADDRx register makes VERA prefetch. With line
;     mode already enabled, that prefetch steps the line helper using
;     whatever slope is lingering in the increment registers -- bending
;     the first pixels. So: all addresses while the mode is OFF, then the
;     mode, then the slope, in that order.
;
;   * Writing the increment seeds the subpixel accumulator to half a
;     pixel, but does NOT clear the position's carry bit, whatever the
;     reference implies. A carry left by an earlier FX operation eats the
;     line's first minor-axis step. Zero X_POS explicitly.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; llvm-mos argument placement, measured on the machine:
;   POINTERS take aligned __rc PAIRS -- __rc2/__rc3, __rc4/__rc5, ... in
;   order, skipping any pair already partly used.
;   INTEGER bytes take A, then X, then single __rc bytes from __rc2 up,
;   skipping any byte a pointer already claimed.
;   The two draw from the same __rc space, left to right by argument.
; Returns: char in A; int in A/X; long in A/X/__rc2/__rc3; POINTER in
; __rc2/__rc3.

        .globl  x16_fx_off
        .globl  x16_fx_mult
        .globl  x16_fx_fill
        .globl  x16_fx_clear
        .globl  x16_fx_line
        .globl  x16_fx_triangle
        .globl  x16_fx_copy
        .globl  x16_fx_transp_on
        .globl  x16_fx_transp_off

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void x16_fx_off(void)
; ---------------------------------------------------------------------
x16_fx_off:
fx_off:
        ; Disable FX and return DCSEL to 0. Safe whether or not FX was
        ; ever enabled.
        vera_dcsel 2
        stz     VERA_FX_CTRL            ; cache off, transparency off, Addr1 mode 0
        stz     VERA_FX_MULT            ; multiplier off
        vera_dcsel 0
        rts

; ---------------------------------------------------------------------
; long __fastcall__ x16_fx_mult(int a, int b)
;   Signed 16 x 16 -> 32, in hardware.
; ---------------------------------------------------------------------
; a -> A/X, b -> __rc2/__rc3. A 32-bit result goes back in A, X, __rc2,
; __rc3 -- the same places it would have arrived.
x16_fx_mult:
        sta     X16_P0                  ; a lo
        stx     X16_P1                  ; a hi
        lda     __rc2
        sta     X16_P2                  ; b lo
        lda     __rc3
        sta     X16_P3                  ; b hi

        jsr     fx_mult

        lda     X16_P6
        sta     __rc2                   ; product bits 16-23
        lda     X16_P7
        sta     __rc3                   ; product bits 24-31
        lda     X16_P4                  ; bits 0-7
        ldx     X16_P5                  ; bits 8-15
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fx_fill(unsigned char value, unsigned int count,
;                               unsigned long addr)
;
; addr goes last so cc65 passes all four bytes in registers, as with
; x16_vera_addr0(). Only bit 16 of the high half reaches the hardware.
; ---------------------------------------------------------------------
; value -> A, count -> X/__rc2, addr -> __rc3..__rc6. No pointers here, so
; the bytes simply run A, X, __rc2, __rc3, ...
x16_fx_fill:
        pha                             ; value
        stx     X16_P3                  ; count lo
        lda     __rc2
        sta     X16_P4                  ; count hi
        lda     __rc3
        sta     X16_P0                  ; addr bits 0-7
        lda     __rc4
        sta     X16_P1                  ; addr bits 8-15
        lda     __rc5
        sta     X16_P2                  ; addr bit 16
        pla                             ; A = value
        jmp     fx_fill

; ---------------------------------------------------------------------
; void __fastcall__ x16_fx_clear(unsigned int count, unsigned long addr)
; ---------------------------------------------------------------------
; count -> A/X, addr -> __rc2..__rc5.
x16_fx_clear:
        sta     X16_P3                  ; count lo
        stx     X16_P4                  ; count hi
        lda     __rc2
        sta     X16_P0                  ; addr bits 0-7
        lda     __rc3
        sta     X16_P1                  ; addr bits 8-15
        lda     __rc4
        sta     X16_P2                  ; addr bit 16
        lda     #0
        jmp     fx_fill

; ---------------------------------------------------------------------
; void __fastcall__ x16_fx_copy(unsigned long src, unsigned long dst,
;                               unsigned int count)
;
; cc65 has no 32-bit stack pop, so each `unsigned long` argument comes
; off as two popax calls: low word, then high word. Arguments were pushed
; left to right, so `dst` -- the last one on the stack -- is popped first.
; ---------------------------------------------------------------------
; Three integer arguments, ten bytes, so they run straight through:
; src -> A, X, __rc2, __rc3;  dst -> __rc4..__rc7;  count -> __rc8/__rc9.
; cc65 needed four popax calls because it has no 32-bit stack pop.
x16_fx_copy:
        sta     X16_P0                  ; src bits 0-7
        stx     X16_P1                  ; src bits 8-15
        lda     __rc2
        sta     X16_P2                  ; src bit 16
        lda     __rc4
        sta     X16_P3                  ; dst bits 0-7
        lda     __rc5
        sta     X16_P4                  ; dst bits 8-15
        lda     __rc6
        sta     X16_P5                  ; dst bit 16
        lda     __rc8
        sta     X16_P6                  ; count lo
        lda     __rc9
        sta     X16_P7                  ; count hi
        jmp     fx_copy

; ---------------------------------------------------------------------
; void x16_fx_transp_on(void) / void x16_fx_transp_off(void)
; ---------------------------------------------------------------------
x16_fx_transp_on:
        vera_dcsel 2
        lda     VERA_FX_CTRL
        ora     #VERA_FX_TRANSPARENT
        sta     VERA_FX_CTRL
        vera_dcsel 0
        rts

x16_fx_transp_off:
        vera_dcsel 2
        lda     VERA_FX_CTRL
        and     #($FF - VERA_FX_TRANSPARENT)
        sta     VERA_FX_CTRL
        vera_dcsel 0
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fx_line(unsigned int x0, unsigned char y0,
;                               unsigned int x1, unsigned char y1,
;                               unsigned char color)
;
; Same arguments and endpoints as x16_gfx_line(), drawn by the hardware
; helper: one `sta VERA_DATA1` per pixel instead of a software Bresenham.
; ---------------------------------------------------------------------
; x0 -> A/X, y0 -> __rc2, x1 -> __rc3/__rc4, y1 -> __rc5, color -> __rc6.
x16_fx_line:
        sta     X16_P0                  ; x0 lo
        stx     X16_P1                  ; x0 hi
        lda     __rc2
        sta     X16_P2                  ; y0
        lda     __rc3
        sta     X16_P3                  ; x1 lo
        lda     __rc4
        sta     X16_P4                  ; x1 hi
        lda     __rc5
        sta     X16_P5                  ; y1
        lda     __rc6
        sta     X16_P6                  ; color
        jmp     fx_line

; ---------------------------------------------------------------------
; void __fastcall__ x16_fx_triangle(const x16_point *a, const x16_point *b,
;                                   const x16_point *c, unsigned char color)
;
; x16_point is { unsigned int x; unsigned char y; } -- three bytes, in the
; same order as tri_x0/tri_y0. So each vertex is a straight three-byte
; copy, not a field-by-field unpack. Keep the struct and those .res
; directives in step.
; ---------------------------------------------------------------------
; Three pointers -> __rc2/__rc3, __rc4/__rc5, __rc6/__rc7; colour -> A.
; Each vertex is a straight three-byte copy, so one indirect pair is
; enough if it is reloaded between vertices. T0/T1 serves: fx_triangle
; reads tri_* and the P block, never T.
x16_fx_triangle:
        sta     tri_color               ; colour

        lda     __rc2
        sta     X16_T0
        lda     __rc3
        sta     X16_T1
        ldy     #2
.Lx16_fx_triangle_copy_a:
        lda     (X16_T0),y
        sta     tri_x0,y
        dey
        bpl     .Lx16_fx_triangle_copy_a

        lda     __rc4
        sta     X16_T0
        lda     __rc5
        sta     X16_T1
        ldy     #2
.Lx16_fx_triangle_copy_b:
        lda     (X16_T0),y
        sta     tri_x1,y
        dey
        bpl     .Lx16_fx_triangle_copy_b

        lda     __rc6
        sta     X16_T0
        lda     __rc7
        sta     X16_T1
        ldy     #2
.Lx16_fx_triangle_copy_c:
        lda     (X16_T0),y
        sta     tri_x2,y
        dey
        bpl     .Lx16_fx_triangle_copy_c

        jmp     fx_triangle

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; fx_mult -- signed 16 x 16 -> 32 in hardware
;   in:  X16_P0/P1 = a, X16_P2/P3 = b
;   out: X16_P4..P7 = product, low byte first
;
; The two operands go into the halves of the 32-bit cache. The result is
; not readable from a register: triggering the multiply writes four bytes
; to VRAM, so we park them at VRAM_FX_SCRATCH and read them back.
;
; Only ADDR0/DATA0 is used. VERA pre-fetches whenever an address pointer
; changes or increments -- even with increment 0 -- so touching the same
; VRAM through the other port here would risk reading a stale latch.
; ---------------------------------------------------------------------
fx_mult:
        vera_dcsel 2
        stz     VERA_FX_CTRL            ; Addr1 Mode 0
        lda     #VERA_FX_MULT_ENABLE
        sta     VERA_FX_MULT

        vera_dcsel 6
        lda     VERA_FX_ACCUM_RESET     ; a *read* clears the accumulator
        lda     X16_P0
        sta     VERA_FX_CACHE_L
        lda     X16_P1
        sta     VERA_FX_CACHE_M         ; cache 15:0  = a
        lda     X16_P2
        sta     VERA_FX_CACHE_H
        lda     X16_P3
        sta     VERA_FX_CACHE_U         ; cache 31:16 = b

        vera_dcsel 2
        lda     #VERA_FX_CACHE_WRITE
        sta     VERA_FX_CTRL            ; with multiplier on, writes the product

        ; Trigger: any store to DATA0 emits the 32-bit result. The stored
        ; value itself is ignored.
        vera_addr 0, VRAM_FX_SCRATCH, VERA_INC_0
        stz     VERA_DATA0

        ; Read it back, now advancing one byte at a time.
        vera_addr 0, VRAM_FX_SCRATCH, VERA_INC_1
        lda     VERA_DATA0
        sta     X16_P4
        lda     VERA_DATA0
        sta     X16_P5
        lda     VERA_DATA0
        sta     X16_P6
        lda     VERA_DATA0
        sta     X16_P7

        jmp     fx_off

; ---------------------------------------------------------------------
; fx_fill -- fill VRAM through the 32-bit write cache (~4x a byte loop)
;   in:  A = byte value
;        X16_P0/P1/P2 = destination VRAM address (17-bit)
;        X16_P3/P4    = byte count
;
; With Cache Write Enable set, one store to DATA0 writes all four cache
; bytes. Stepping the port by 4 covers the region a quad at a time; any
; remaining 1-3 bytes are written normally with FX switched back off.
; ---------------------------------------------------------------------
fx_fill:
        sta     X16_T0                  ; fill value

        vera_dcsel 2
        stz     VERA_FX_MULT            ; multiplier off: write the cache itself
        lda     #VERA_FX_CACHE_WRITE
        sta     VERA_FX_CTRL

        vera_dcsel 6
        lda     X16_T0
        sta     VERA_FX_CACHE_L
        sta     VERA_FX_CACHE_M
        sta     VERA_FX_CACHE_H
        sta     VERA_FX_CACHE_U
        vera_dcsel 0

        ; Point port 0 at the destination, stepping 4 bytes per write.
        lda     #VERA_CTRL_ADDRSEL
        trb     VERA_CTRL
        lda     X16_P0
        sta     VERA_ADDR_L
        lda     X16_P1
        sta     VERA_ADDR_M
        lda     X16_P2
        and     #VERA_ADDR_H_BANK
        ora     #(VERA_INC_4 << 4)
        sta     VERA_ADDR_H

        ; quads = count >> 2, remainder = count & 3
        lda     X16_P3
        and     #$03
        sta     X16_T3
        lda     X16_P4
        sta     X16_T2
        lda     X16_P3
        sta     X16_T1
        lsr     X16_T2
        ror     X16_T1
        lsr     X16_T2
        ror     X16_T1

        lda     X16_T1
        ora     X16_T2
        beq     .Lfx_fill_tail                   ; fewer than four bytes

        ldx     X16_T1
        ldy     X16_T2
        txa
        beq     .Lfx_fill_full
        iny
.Lfx_fill_full:
.Lfx_fill_loop:
        stz     VERA_DATA0              ; writes the four cache bytes
        dex
        bne     .Lfx_fill_loop
        dey
        bne     .Lfx_fill_loop

.Lfx_fill_tail:
        ; FX off first: the leftover bytes must be written singly.
        vera_dcsel 2
        stz     VERA_FX_CTRL
        vera_dcsel 0

        lda     X16_T3
        beq     .Lfx_fill_done

        ; Port 0 already sits just past the quads. Keep its bank and DECR
        ; bits, switch the increment back to 1.
        lda     VERA_ADDR_H
        and     #$0F
        ora     #(VERA_INC_1 << 4)
        sta     VERA_ADDR_H

        ldx     X16_T3
        lda     X16_T0
.Lfx_fill_rest:
        sta     VERA_DATA0
        dex
        bne     .Lfx_fill_rest
.Lfx_fill_done:
        rts

; ---------------------------------------------------------------------
; fx_copy -- VRAM to VRAM through the 32-bit cache (~4x a byte loop)
;   in:  X16_P0/P1/P2 = source address (17-bit)
;        X16_P3/P4/P5 = destination address, 4-BYTE ALIGNED
;        X16_P6/P7    = byte count
;
; With Cache Fill enabled, each DATA1 read latches a byte into the cache;
; after four, one DATA0 write (mask 0) flushes all four to the aligned
; destination. The 0-3 leftover bytes are copied singly with FX off. The
; source needs no alignment.
; ---------------------------------------------------------------------
fx_copy:
        vera_dcsel 2
        stz     VERA_FX_CTRL            ; mode 0 while the ports are aimed
        stz     VERA_FX_MULT            ; multiplier off, cache index to 0

        vera_addrsel 1                  ; port 1 reads the source
        lda     X16_P0
        sta     VERA_ADDR_L
        lda     X16_P1
        sta     VERA_ADDR_M
        lda     X16_P2
        and     #VERA_ADDR_H_BANK
        ora     #(VERA_INC_1 << 4)
        sta     VERA_ADDR_H
        vera_addrsel 0                  ; port 0 writes quads
        lda     X16_P3
        sta     VERA_ADDR_L
        lda     X16_P4
        sta     VERA_ADDR_M
        lda     X16_P5
        and     #VERA_ADDR_H_BANK
        ora     #(VERA_INC_4 << 4)
        sta     VERA_ADDR_H

        vera_dcsel 2
        lda     #(VERA_FX_CACHE_FILL | VERA_FX_CACHE_WRITE)
        sta     VERA_FX_CTRL

        ; quads = count >> 2, remainder = count & 3
        lda     X16_P6
        and     #$03
        sta     X16_T3
        lda     X16_P7
        sta     X16_T2
        lda     X16_P6
        sta     X16_T1
        lsr     X16_T2
        ror     X16_T1
        lsr     X16_T2
        ror     X16_T1

        lda     X16_T1
        ora     X16_T2
        beq     .Lfx_copy_tail

        ldx     X16_T1
        ldy     X16_T2
        txa
        beq     .Lfx_copy_full
        iny
.Lfx_copy_full:
.Lfx_copy_quad:
        lda     VERA_DATA1              ; four reads fill the cache...
        lda     VERA_DATA1
        lda     VERA_DATA1
        lda     VERA_DATA1
        stz     VERA_DATA0              ; ...one write flushes it (mask 0)
        dex
        bne     .Lfx_copy_quad
        dey
        bne     .Lfx_copy_quad

.Lfx_copy_tail:
        vera_dcsel 2
        stz     VERA_FX_CTRL            ; leftovers are plain byte copies
        vera_dcsel 0

        lda     X16_T3
        beq     .Lfx_copy_cdone
        lda     VERA_ADDR_H             ; port 0 sits just past the quads:
        and     #$0F                    ; step it by 1 for the tail
        ora     #(VERA_INC_1 << 4)
        sta     VERA_ADDR_H
        ldx     X16_T3
.Lfx_copy_crest:
        lda     VERA_DATA1
        sta     VERA_DATA0
        dex
        bne     .Lfx_copy_crest
.Lfx_copy_cdone:
        vera_addrsel 0
        rts

; =====================================================================
; FX line draw helper (Addr1 Mode 1)
;
; VERA tracks the Bresenham error internally: ADDR1 steps one pixel along
; the major axis on every DATA1 write, and a 9.9 fixed-point accumulator
; (seeded to half a pixel) carries it one step along the minor axis
; whenever the slope fraction overflows. The CPU's whole job is one
; `sta VERA_DATA1` per pixel.
;
; Increment registers hold 15-bit signed 6.9 fixed point: write the value
; in 1/512ths, low byte to INCR_L, high 7 bits to INCR_H (bit 7 of INCR_H
; multiplies by 32 -- not needed for a line's 0.0..1.0).
; =====================================================================

; ---------------------------------------------------------------------
; fx_line -- in: X16_P0/P1 = x0, X16_P2 = y0
;                X16_P3/P4 = x1, X16_P5 = y1
;                X16_P6    = colour
;
; Assumes the 320x240@8bpp framebuffer at VRAM $00000 (gfx_init's mode).
; Does NOT clip; keep both endpoints on screen.
; ---------------------------------------------------------------------
fx_line:
        ; |dx| and the x direction
        stz     fxl_sx
        sec
        lda     X16_P3
        sbc     X16_P0
        sta     fxl_dx
        lda     X16_P4
        sbc     X16_P1
        sta     fxl_dx+1
        bpl     .Lfx_line_dx_done
        inc     fxl_sx                  ; x runs right to left
        sec
        lda     #0
        sbc     fxl_dx
        sta     fxl_dx
        lda     #0
        sbc     fxl_dx+1
        sta     fxl_dx+1
.Lfx_line_dx_done:

        ; |dy| and the y direction, in 16 bits (239 - 0 overflows a byte)
        stz     fxl_sy
        sec
        lda     X16_P5
        sbc     X16_P2
        sta     fxl_dy
        lda     #0
        sbc     #0
        sta     fxl_dy+1
        bpl     .Lfx_line_dy_done
        inc     fxl_sy
        sec
        lda     #0
        sbc     fxl_dy
        sta     fxl_dy
        lda     #0
        sbc     fxl_dy+1
        sta     fxl_dy+1
.Lfx_line_dy_done:

        ; pick the octant: ADDR1 steps the major axis every pixel, ADDR0's
        ; increment is borrowed for the sometimes-step along the minor axis
        lda     fxl_dy+1
        cmp     fxl_dx+1
        bne     .Lfx_line_which
        lda     fxl_dy
        cmp     fxl_dx
.Lfx_line_which:
        bcc     .Lfx_line_x_major

        ; Y-major
        lda     fxl_dy
        sta     fxl_major
        lda     fxl_dy+1
        sta     fxl_major+1
        lda     fxl_dx
        sta     fxl_minor
        lda     fxl_dx+1
        sta     fxl_minor+1
        lda     #(VERA_INC_320 << 4)
        ldx     fxl_sy
        beq     .Lfx_line_ym1
        ora     #VERA_ADDR_H_DECR
.Lfx_line_ym1:
        sta     fxl_h1                  ; ADDR1: a row per step
        lda     #(VERA_INC_1 << 4)
        ldx     fxl_sx
        beq     .Lfx_line_ym0
        ora     #VERA_ADDR_H_DECR
.Lfx_line_ym0:
        sta     fxl_h0                  ; ADDR0: a pixel, sometimes
        bra     .Lfx_line_slope

.Lfx_line_x_major:
        lda     fxl_dx
        sta     fxl_major
        lda     fxl_dx+1
        sta     fxl_major+1
        lda     fxl_dy
        sta     fxl_minor
        lda     fxl_dy+1
        sta     fxl_minor+1
        lda     #(VERA_INC_1 << 4)
        ldx     fxl_sx
        beq     .Lfx_line_xm1
        ora     #VERA_ADDR_H_DECR
.Lfx_line_xm1:
        sta     fxl_h1
        lda     #(VERA_INC_320 << 4)
        ldx     fxl_sy
        beq     .Lfx_line_xm0
        ora     #VERA_ADDR_H_DECR
.Lfx_line_xm0:
        sta     fxl_h0

.Lfx_line_slope:
        ; slope = minor/major in 1/512ths (0..512); a point has no slope
        stz     fxl_v
        stz     fxl_v+1
        lda     fxl_major
        ora     fxl_major+1
        beq     .Lfx_line_program
        stz     fxd_num                 ; dividend = minor * 512
        lda     fxl_minor
        asl     a
        sta     fxd_num+1
        lda     fxl_minor+1
        rol     a
        sta     fxd_num+2
        lda     fxl_major
        sta     fxd_den
        lda     fxl_major+1
        sta     fxd_den+1
        jsr     udiv24
        lda     fxd_num
        sta     fxl_v
        lda     fxd_num+1
        sta     fxl_v+1

.Lfx_line_program:
        jsr     pix_addr                ; fxa = address of (P0/P1, P2)

        ; An axis-aligned line (minor delta 0) is just a run along port
        ; 1's increment -- no FX needed.
        lda     fxl_minor
        ora     fxl_minor+1
        beq     .Lfx_line_plain

        ; ORDER IS LOAD-BEARING -- see the header. All addresses while the
        ; mode is still off, then the mode, and the slope very last.
        vera_dcsel 2
        stz     VERA_FX_CTRL            ; addr1 mode 0 while addressing
        jsr     set_addr1
        vera_addrsel 0                  ; only ADDR0's increment matters here
        lda     fxl_h0
        sta     VERA_ADDR_H
        vera_dcsel 2
        lda     #VERA_FX_ADDR1_LINE
        sta     VERA_FX_CTRL
        vera_dcsel 3
        lda     fxl_v
        sta     VERA_FX_X_INCR_L
        lda     fxl_v+1
        sta     VERA_FX_X_INCR_H        ; seeds the fraction to 0.5...
        vera_dcsel 4
        stz     VERA_FX_X_POS_L         ; ...but NOT the integer/carry bits: a
        stz     VERA_FX_X_POS_H         ; leftover carry from an earlier FX op
        bra     .Lfx_line_count                  ; would eat the line's first minor-step

.Lfx_line_plain:
        jsr     set_addr1

.Lfx_line_count:
        ; draw major+1 pixels
        clc
        lda     fxl_major
        adc     #1
        tax
        lda     fxl_major+1
        adc     #0
        tay
        txa
        beq     .Lfx_line_full
        iny
.Lfx_line_full:
        lda     X16_P6
.Lfx_line_draw:
        sta     VERA_DATA1
        dex
        bne     .Lfx_line_draw
        dey
        bne     .Lfx_line_draw
        jmp     fx_off

; point port 1 at the start pixel with the major-axis increment
set_addr1:
        vera_addrsel 1
        lda     fxa
        sta     VERA_ADDR_L
        lda     fxa+1
        sta     VERA_ADDR_M
        lda     fxa+2
        and     #VERA_ADDR_H_BANK
        ora     fxl_h1
        sta     VERA_ADDR_H
        rts

; =====================================================================
; FX polygon filler (Addr1 Mode 2)
;
; VERA walks two edges at once: the X and Y/X2 position registers carry
; the left and right x, each advanced by its own signed slope twice per
; row (hence: program HALF the per-row increment). Reading DATA1 latches
; the row -- VERA points ADDR1 at the left edge and computes the span
; width, read back from POLY_FILL_L/H. The CPU fills that many pixels and
; a DATA0 read advances to the next row.
; =====================================================================

; ---------------------------------------------------------------------
; fx_triangle -- filled triangle via the polygon helper
;   in:  tri_x0/tri_y0, tri_x1/tri_y1, tri_x2/tri_y2 = vertices
;        (x 0-319, y 0-239), tri_color = fill colour
;
; Vertices may come in any order. The rasterisation is half-open: the
; bottom row (max y) is not drawn, so triangles sharing an edge do not
; double-paint it. Assumes the 320x240@8bpp framebuffer. Does NOT clip.
; ---------------------------------------------------------------------
fx_triangle:
        ; sort the vertices by y (three compare-swaps)
        lda     tri_y1
        cmp     tri_y0
        bcs     .Lfx_triangle_s1
        jsr     swap01
.Lfx_triangle_s1:
        lda     tri_y2
        cmp     tri_y1
        bcs     .Lfx_triangle_s2
        jsr     swap12
.Lfx_triangle_s2:
        lda     tri_y1
        cmp     tri_y0
        bcs     .Lfx_triangle_s3
        jsr     swap01
.Lfx_triangle_s3:
        sec                             ; row counts of the two parts
        lda     tri_y1
        sbc     tri_y0
        sta     fxt_n1
        sec
        lda     tri_y2
        sbc     tri_y1
        sta     fxt_n2
        lda     fxt_n1
        ora     fxt_n2
        bne     .Lfx_triangle_go
        rts                             ; a single row: nothing (half-open)
.Lfx_triangle_go:
        ; slope of the long edge v0 -> v2 (always needed)
        lda     fxt_n1
        clc
        adc     fxt_n2
        sta     fxs_dy
        sec
        lda     tri_x2
        sbc     tri_x0
        sta     fxs_dxl
        lda     tri_x2+1
        sbc     tri_x0+1
        sta     fxs_dxh
        jsr     slope
        jsr     save_a                  ; edge A = the long edge

        lda     fxt_n1
        bne     .Lfx_triangle_two_parts
        jmp     .Lfx_triangle_flat_top               ; out of branch range from here
.Lfx_triangle_two_parts:

        ; slope of the top short edge v0 -> v1
        lda     fxt_n1
        sta     fxs_dy
        sec
        lda     tri_x1
        sbc     tri_x0
        sta     fxs_dxl
        lda     tri_x1+1
        sbc     tri_x0+1
        sta     fxs_dxh
        jsr     slope                   ; edge B, still in fxs_*

        jsr     cmp_b_lt_a              ; carry set: B is the left edge
        bcs     .Lfx_triangle_b_left
        lda     fxt_a_l                 ; A (long) left in the X slot,
        sta     fxt_xl                  ; B right in the Y/X2 slot
        lda     fxt_a_h
        sta     fxt_xh
        lda     fxs_el
        sta     fxt_yl
        lda     fxs_eh
        sta     fxt_yh
        lda     #1
        sta     fxt_swap                ; part 2 replaces the Y/X2 slot
        bra     .Lfx_triangle_pos
.Lfx_triangle_b_left:
        lda     fxs_el                  ; B left, A (long) right
        sta     fxt_xl
        lda     fxs_eh
        sta     fxt_xh
        lda     fxt_a_l
        sta     fxt_yl
        lda     fxt_a_h
        sta     fxt_yh
        stz     fxt_swap                ; part 2 replaces the X slot
.Lfx_triangle_pos:
        lda     tri_x0                  ; both edges start at the apex
        sta     fxt_px
        sta     fxt_py
        lda     tri_x0+1
        sta     fxt_px+1
        sta     fxt_py+1
        jsr     poly_setup
        lda     fxt_n1
        jsr     poly_rows

        lda     fxt_n2
        bne     .Lfx_triangle_have_part2
        jmp     fx_off                  ; flat bottom: one part was the whole
.Lfx_triangle_have_part2:

        ; part 2: the finished short edge becomes v1 -> v2
        lda     fxt_n2
        sta     fxs_dy
        sec
        lda     tri_x2
        sbc     tri_x1
        sta     fxs_dxl
        lda     tri_x2+1
        sbc     tri_x1+1
        sta     fxs_dxh
        jsr     slope
        vera_dcsel 3
        lda     fxt_swap
        beq     .Lfx_triangle_repl_x
        lda     fxs_el
        sta     VERA_FX_Y_INCR_L
        lda     fxs_eh
        sta     VERA_FX_Y_INCR_H        ; resets that edge's subpixel to 0.5
        vera_dcsel 4
        lda     tri_x1
        sta     VERA_FX_Y_POS_L
        lda     tri_x1+1
        and     #$07
        sta     VERA_FX_Y_POS_H
        bra     .Lfx_triangle_part2
.Lfx_triangle_repl_x:
        lda     fxs_el
        sta     VERA_FX_X_INCR_L
        lda     fxs_eh
        sta     VERA_FX_X_INCR_H
        vera_dcsel 4
        lda     tri_x1
        sta     VERA_FX_X_POS_L
        lda     tri_x1+1
        and     #$07
        sta     VERA_FX_X_POS_H
.Lfx_triangle_part2:
        vera_dcsel 5                    ; back to the fill-length window
        lda     fxt_n2
        jsr     poly_rows
        jmp     fx_off

.Lfx_triangle_flat_top:
        ; v0 and v1 share the top row; the second edge is v1 -> v2
        lda     fxt_n2
        sta     fxs_dy
        sec
        lda     tri_x2
        sbc     tri_x1
        sta     fxs_dxl
        lda     tri_x2+1
        sbc     tri_x1+1
        sta     fxs_dxh
        jsr     slope                   ; edge B = v1 -> v2

        lda     tri_x0+1                ; the leftmost vertex owns the X slot
        cmp     tri_x1+1
        bne     .Lfx_triangle_ft_pick
        lda     tri_x0
        cmp     tri_x1
.Lfx_triangle_ft_pick:
        bcc     .Lfx_triangle_ft_v0_left
        lda     fxs_el                  ; v1 left: B in X at x1, A in Y at x0
        sta     fxt_xl
        lda     fxs_eh
        sta     fxt_xh
        lda     fxt_a_l
        sta     fxt_yl
        lda     fxt_a_h
        sta     fxt_yh
        lda     tri_x1
        sta     fxt_px
        lda     tri_x1+1
        sta     fxt_px+1
        lda     tri_x0
        sta     fxt_py
        lda     tri_x0+1
        sta     fxt_py+1
        bra     .Lfx_triangle_ft_run
.Lfx_triangle_ft_v0_left:
        lda     fxt_a_l                 ; v0 left: A in X at x0, B in Y at x1
        sta     fxt_xl
        lda     fxt_a_h
        sta     fxt_xh
        lda     fxs_el
        sta     fxt_yl
        lda     fxs_eh
        sta     fxt_yh
        lda     tri_x0
        sta     fxt_px
        lda     tri_x0+1
        sta     fxt_px+1
        lda     tri_x1
        sta     fxt_py
        lda     tri_x1+1
        sta     fxt_py+1
.Lfx_triangle_ft_run:
        jsr     poly_setup
        lda     fxt_n2
        jsr     poly_rows
        jmp     fx_off

; program the polygon helper: mode, both slopes, both positions, ADDR0 at
; the top row (+320/row), ADDR1 stepping +1, DCSEL left at 5.
poly_setup:
        vera_dcsel 2
        lda     #VERA_FX_ADDR1_POLY
        sta     VERA_FX_CTRL
        vera_dcsel 3
        lda     fxt_xl
        sta     VERA_FX_X_INCR_L
        lda     fxt_xh
        sta     VERA_FX_X_INCR_H        ; seeds the subpixel to 0.5
        lda     fxt_yl
        sta     VERA_FX_Y_INCR_L
        lda     fxt_yh
        sta     VERA_FX_Y_INCR_H
        vera_dcsel 4
        lda     fxt_px
        sta     VERA_FX_X_POS_L
        lda     fxt_px+1
        and     #$07
        sta     VERA_FX_X_POS_H
        lda     fxt_py
        sta     VERA_FX_Y_POS_L
        lda     fxt_py+1
        and     #$07
        sta     VERA_FX_Y_POS_H

        stz     X16_P0                  ; ADDR0 = row base of the top row
        stz     X16_P1
        lda     tri_y0
        sta     X16_P2
        jsr     pix_addr
        vera_addrsel 0
        lda     fxa
        sta     VERA_ADDR_L
        lda     fxa+1
        sta     VERA_ADDR_M
        lda     fxa+2
        and     #VERA_ADDR_H_BANK
        ora     #(VERA_INC_320 << 4)
        sta     VERA_ADDR_H
        vera_addrsel 1                  ; ADDR1: VERA sets the address, we +1
        lda     #(VERA_INC_1 << 4)
        sta     VERA_ADDR_H
        vera_dcsel 5
        rts

; draw A rows. DCSEL must be 5 (poly_setup leaves it there).
poly_rows:
        sta     fxt_rows
.Lpoly_rows_prow:
        lda     fxt_rows
        beq     .Lpoly_rows_pdone
        lda     VERA_DATA1              ; latch: half-step edges, point ADDR1
        lda     VERA_FX_POLY_FILL_L
        sta     fxt_fl
        bmi     .Lpoly_rows_plong
        lsr     a                       ; short row: length is bits 4:1
        and     #$0F
        sta     fxt_len
        stz     fxt_len+1
        bra     .Lpoly_rows_pdraw
.Lpoly_rows_plong:
        lda     VERA_FX_POLY_FILL_H
        sta     fxt_fh
        and     #$C0
        cmp     #$C0
        beq     .Lpoly_rows_pskip                  ; bits 9+8 set: negative width, no row
        lda     fxt_fl
        lsr     a
        and     #$0F
        sta     fxt_len
        stz     fxt_len+1
        lda     fxt_fh
        lsr     a                       ; H bits 7:1 are length bits 9:3
        asl     a                       ; ...so shift them up by 3 in 16 bits
        rol     fxt_len+1
        asl     a
        rol     fxt_len+1
        asl     a
        rol     fxt_len+1
        ora     fxt_len
        sta     fxt_len
.Lpoly_rows_pdraw:
        ldx     fxt_len
        ldy     fxt_len+1
        txa
        ora     fxt_len+1
        beq     .Lpoly_rows_pskip                  ; zero-width row
        txa
        beq     .Lpoly_rows_pfull
        iny
.Lpoly_rows_pfull:
        lda     tri_color
.Lpoly_rows_ploop:
        sta     VERA_DATA1
        dex
        bne     .Lpoly_rows_ploop
        dey
        bne     .Lpoly_rows_ploop
.Lpoly_rows_pskip:
        lda     VERA_DATA0              ; second half-step, ADDR0 to next row
        dec     fxt_rows
        bra     .Lpoly_rows_prow
.Lpoly_rows_pdone:
        rts

; signed (fxs_dxl/h * 256) / fxs_dy -> the 15-bit (+32x) register format
; in fxs_el/eh, with sign and 24-bit magnitude kept for the left/right
; comparison. *256, not *512: the poly filler wants HALF the per-row
; increment because it steps each edge twice per row.
slope:
        stz     fxs_sgn
        lda     fxs_dxh
        bpl     .Lslope_sl_abs
        inc     fxs_sgn
        sec
        lda     #0
        sbc     fxs_dxl
        sta     fxs_dxl
        lda     #0
        sbc     fxs_dxh
        sta     fxs_dxh
.Lslope_sl_abs:
        stz     fxd_num                 ; dividend = |dx| * 256
        lda     fxs_dxl
        sta     fxd_num+1
        lda     fxs_dxh
        sta     fxd_num+2
        lda     fxs_dy
        sta     fxd_den
        stz     fxd_den+1
        jsr     udiv24

        lda     fxd_num                 ; keep the magnitude for cmp_b_lt_a
        sta     fxs_mag
        lda     fxd_num+1
        sta     fxs_mag+1
        lda     fxd_num+2
        sta     fxs_mag+2

        stz     fxs_32                  ; encode: 14 bits direct, else /32
        lda     fxd_num+2
        bne     .Lslope_sl_big
        lda     fxd_num+1
        cmp     #$40
        bcc     .Lslope_sl_small
.Lslope_sl_big:
        ldx     #5
.Lslope_sl_shift:
        lsr     fxd_num+2
        ror     fxd_num+1
        ror     fxd_num
        dex
        bne     .Lslope_sl_shift
        inc     fxs_32
.Lslope_sl_small:
        lda     fxd_num
        sta     fxs_el
        lda     fxd_num+1
        sta     fxs_eh
        lda     fxs_sgn
        beq     .Lslope_sl_pos
        sec                             ; two's complement within the 15 bits
        lda     #0
        sbc     fxs_el
        sta     fxs_el
        lda     #0
        sbc     fxs_eh
        and     #$7F
        sta     fxs_eh
.Lslope_sl_pos:
        lda     fxs_32
        beq     .Lslope_sl_done
        lda     fxs_eh
        ora     #$80                    ; the 32x flag rides on bit 15
        sta     fxs_eh
.Lslope_sl_done:
        rts

; park the fxs_* result as edge A
save_a:
        lda     fxs_el
        sta     fxt_a_l
        lda     fxs_eh
        sta     fxt_a_h
        lda     fxs_sgn
        sta     fxt_a_sgn
        lda     fxs_mag
        sta     fxt_a_mag
        lda     fxs_mag+1
        sta     fxt_a_mag+1
        lda     fxs_mag+2
        sta     fxt_a_mag+2
        rts

; carry set if edge B (fxs_*) is a smaller signed slope than edge A.
; Ties go to A-left, which for coincident edges makes no difference.
cmp_b_lt_a:
        lda     fxs_sgn
        cmp     fxt_a_sgn
        beq     .Lcmp_b_lt_a_cmp_same
        lda     fxs_sgn                 ; different signs: the negative is less
        bne     .Lcmp_b_lt_a_cmp_yes
        clc
        rts
.Lcmp_b_lt_a_cmp_same:
        lda     fxt_a_sgn
        bne     .Lcmp_b_lt_a_cmp_neg
        lda     fxs_mag+2               ; both positive: B < A iff |B| < |A|
        cmp     fxt_a_mag+2
        bne     .Lcmp_b_lt_a_cmp_p
        lda     fxs_mag+1
        cmp     fxt_a_mag+1
        bne     .Lcmp_b_lt_a_cmp_p
        lda     fxs_mag
        cmp     fxt_a_mag
.Lcmp_b_lt_a_cmp_p:
        bcc     .Lcmp_b_lt_a_cmp_yes
        clc
        rts
.Lcmp_b_lt_a_cmp_neg:
        lda     fxt_a_mag+2             ; both negative: B < A iff |A| < |B|
        cmp     fxs_mag+2
        bne     .Lcmp_b_lt_a_cmp_n
        lda     fxt_a_mag+1
        cmp     fxs_mag+1
        bne     .Lcmp_b_lt_a_cmp_n
        lda     fxt_a_mag
        cmp     fxs_mag
.Lcmp_b_lt_a_cmp_n:
        bcc     .Lcmp_b_lt_a_cmp_yes
        clc
        rts
.Lcmp_b_lt_a_cmp_yes:
        sec
        rts

; fxd_num(24) / fxd_den(16) -> quotient in fxd_num, remainder fxd_rem
udiv24:
        stz     fxd_rem
        stz     fxd_rem+1
        ldx     #24
.Ludiv24_dv:
        asl     fxd_num
        rol     fxd_num+1
        rol     fxd_num+2
        rol     fxd_rem
        rol     fxd_rem+1
        sec
        lda     fxd_rem
        sbc     fxd_den
        tay
        lda     fxd_rem+1
        sbc     fxd_den+1
        bcc     .Ludiv24_dv_no
        sta     fxd_rem+1
        sty     fxd_rem
        inc     fxd_num
.Ludiv24_dv_no:
        dex
        bne     .Ludiv24_dv
        rts

; fxa = X16_P0/P1 + X16_P2 * 320  (the 17-bit bitmap pixel address)
pix_addr:
        lda     X16_P2                  ; y << 6
        stz     X16_T3
        asl     a
        rol     X16_T3
        asl     a
        rol     X16_T3
        asl     a
        rol     X16_T3
        asl     a
        rol     X16_T3
        asl     a
        rol     X16_T3
        asl     a
        rol     X16_T3
        sta     fxa                     ; low byte of y*64 (and of y*320)
        clc                             ; + y << 8
        lda     X16_P2
        adc     X16_T3
        sta     fxa+1
        lda     #0
        adc     #0
        sta     fxa+2
        clc                             ; + x
        lda     fxa
        adc     X16_P0
        sta     fxa
        lda     fxa+1
        adc     X16_P1
        sta     fxa+1
        lda     fxa+2
        adc     #0
        sta     fxa+2
        rts

swap01:
        lda     tri_x0
        ldx     tri_x1
        stx     tri_x0
        sta     tri_x1
        lda     tri_x0+1
        ldx     tri_x1+1
        stx     tri_x0+1
        sta     tri_x1+1
        lda     tri_y0
        ldx     tri_y1
        stx     tri_y0
        sta     tri_y1
        rts

swap12:
        lda     tri_x1
        ldx     tri_x2
        stx     tri_x1
        sta     tri_x2
        lda     tri_x1+1
        ldx     tri_x2+1
        stx     tri_x1+1
        sta     tri_x2+1
        lda     tri_y1
        ldx     tri_y2
        stx     tri_y1
        sta     tri_y2
        rts

; ---------------------------------------------------------------------
; Module variables.
;
; tri_x0 .. tri_y2 must stay adjacent in threes: _x16_fx_triangle
; block-copies each x16_point straight onto them. Do not reorder.
; ---------------------------------------------------------------------
        .section .bss,"aw",@nobits

tri_x0:    .zero  2
tri_y0:    .zero  1
tri_x1:    .zero  2
tri_y1:    .zero  1
tri_x2:    .zero  2
tri_y2:    .zero  1
tri_color: .zero  1

fxl_dx:    .zero  2
fxl_dy:    .zero  2
fxl_major: .zero  2
fxl_minor: .zero  2
fxl_v:     .zero  2
fxl_sx:    .zero  1
fxl_sy:    .zero  1
fxl_h1:    .zero  1
fxl_h0:    .zero  1

fxt_n1:    .zero  1
fxt_n2:    .zero  1
fxt_swap:  .zero  1
fxt_xl:    .zero  1                       ; encoded increments for the two slots
fxt_xh:    .zero  1
fxt_yl:    .zero  1
fxt_yh:    .zero  1
fxt_px:    .zero  2                       ; starting x of each edge
fxt_py:    .zero  2
fxt_a_l:   .zero  1                       ; the long edge, parked
fxt_a_h:   .zero  1
fxt_a_sgn: .zero  1
fxt_a_mag: .zero  3
fxt_rows:  .zero  1
fxt_fl:    .zero  1
fxt_fh:    .zero  1
fxt_len:   .zero  2

fxs_dxl:   .zero  1
fxs_dxh:   .zero  1
fxs_dy:    .zero  1
fxs_sgn:   .zero  1
fxs_32:    .zero  1
fxs_mag:   .zero  3
fxs_el:    .zero  1
fxs_eh:    .zero  1

fxd_num:   .zero  3
fxd_den:   .zero  2
fxd_rem:   .zero  2

fxa:       .zero  3
