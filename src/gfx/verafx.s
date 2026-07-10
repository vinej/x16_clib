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

        .import         popa, popax
        .importzp       sreg, ptr1

        .export         _x16_fx_off
        .export         _x16_fx_mult
        .export         _x16_fx_fill
        .export         _x16_fx_clear
        .export         _x16_fx_line
        .export         _x16_fx_triangle
        .export         _x16_fx_copy
        .export         _x16_fx_transp_on
        .export         _x16_fx_transp_off

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void x16_fx_off(void)
; ---------------------------------------------------------------------
_x16_fx_off:
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
_x16_fx_mult:
        sta     X16_P2                  ; b lo (rightmost arg: A/X)
        stx     X16_P3                  ; b hi
        jsr     popax                   ; a
        sta     X16_P0
        stx     X16_P1

        jsr     fx_mult

        lda     X16_P6
        sta     sreg
        lda     X16_P7
        sta     sreg+1
        lda     X16_P4
        ldx     X16_P5
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fx_fill(unsigned char value, unsigned int count,
;                               unsigned long addr)
;
; addr goes last so cc65 passes all four bytes in registers, as with
; x16_vera_addr0(). Only bit 16 of the high half reaches the hardware.
; ---------------------------------------------------------------------
_x16_fx_fill:
        jsr     fill_marshal
        jsr     popa                    ; value
        jmp     fx_fill

; ---------------------------------------------------------------------
; void __fastcall__ x16_fx_clear(unsigned int count, unsigned long addr)
; ---------------------------------------------------------------------
_x16_fx_clear:
        jsr     fill_marshal
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
_x16_fx_copy:
        sta     X16_P6                  ; count lo (rightmost arg: A/X)
        stx     X16_P7                  ; count hi

        jsr     popax                   ; dst bits 0-15
        sta     X16_P3
        stx     X16_P4
        jsr     popax                   ; dst bits 16-31; only bit 16 exists
        sta     X16_P5

        jsr     popax                   ; src bits 0-15
        sta     X16_P0
        stx     X16_P1
        jsr     popax                   ; src bits 16-31
        sta     X16_P2

        jmp     fx_copy

; ---------------------------------------------------------------------
; void x16_fx_transp_on(void) / void x16_fx_transp_off(void)
; ---------------------------------------------------------------------
_x16_fx_transp_on:
        vera_dcsel 2
        lda     VERA_FX_CTRL
        ora     #VERA_FX_TRANSPARENT
        sta     VERA_FX_CTRL
        vera_dcsel 0
        rts

_x16_fx_transp_off:
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
_x16_fx_line:
        sta     X16_P6                  ; color
        jsr     popa
        sta     X16_P5                  ; y1
        jsr     popax
        sta     X16_P3                  ; x1
        stx     X16_P4
        jsr     popa
        sta     X16_P2                  ; y0
        jsr     popax
        sta     X16_P0                  ; x0
        stx     X16_P1
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
_x16_fx_triangle:
        sta     tri_color               ; colour (rightmost arg, in A)

        jsr     popax                   ; vertex c
        sta     ptr1
        stx     ptr1+1
        ldy     #2
@copy_c:
        lda     (ptr1),y
        sta     tri_x2,y
        dey
        bpl     @copy_c

        jsr     popax                   ; vertex b
        sta     ptr1
        stx     ptr1+1
        ldy     #2
@copy_b:
        lda     (ptr1),y
        sta     tri_x1,y
        dey
        bpl     @copy_b

        jsr     popax                   ; vertex a
        sta     ptr1
        stx     ptr1+1
        ldy     #2
@copy_a:
        lda     (ptr1),y
        sta     tri_x0,y
        dey
        bpl     @copy_a

        jmp     fx_triangle

; in:  A/X/sreg = addr, count (and possibly value) still on the C stack
; out: X16_P0/P1/P2 = addr, X16_P3/P4 = count
fill_marshal:
        sta     X16_P0                  ; addr bits 0-7
        stx     X16_P1                  ; addr bits 8-15
        lda     sreg
        sta     X16_P2                  ; addr bit 16
        jsr     popax                   ; count
        sta     X16_P3
        stx     X16_P4
        rts

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
        beq     @tail                   ; fewer than four bytes

        ldx     X16_T1
        ldy     X16_T2
        txa
        beq     @full
        iny
@full:
@loop:
        stz     VERA_DATA0              ; writes the four cache bytes
        dex
        bne     @loop
        dey
        bne     @loop

@tail:
        ; FX off first: the leftover bytes must be written singly.
        vera_dcsel 2
        stz     VERA_FX_CTRL
        vera_dcsel 0

        lda     X16_T3
        beq     @done

        ; Port 0 already sits just past the quads. Keep its bank and DECR
        ; bits, switch the increment back to 1.
        lda     VERA_ADDR_H
        and     #$0F
        ora     #(VERA_INC_1 << 4)
        sta     VERA_ADDR_H

        ldx     X16_T3
        lda     X16_T0
@rest:
        sta     VERA_DATA0
        dex
        bne     @rest
@done:
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
        beq     @tail

        ldx     X16_T1
        ldy     X16_T2
        txa
        beq     @full
        iny
@full:
@quad:
        lda     VERA_DATA1              ; four reads fill the cache...
        lda     VERA_DATA1
        lda     VERA_DATA1
        lda     VERA_DATA1
        stz     VERA_DATA0              ; ...one write flushes it (mask 0)
        dex
        bne     @quad
        dey
        bne     @quad

@tail:
        vera_dcsel 2
        stz     VERA_FX_CTRL            ; leftovers are plain byte copies
        vera_dcsel 0

        lda     X16_T3
        beq     @cdone
        lda     VERA_ADDR_H             ; port 0 sits just past the quads:
        and     #$0F                    ; step it by 1 for the tail
        ora     #(VERA_INC_1 << 4)
        sta     VERA_ADDR_H
        ldx     X16_T3
@crest:
        lda     VERA_DATA1
        sta     VERA_DATA0
        dex
        bne     @crest
@cdone:
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
        bpl     @dx_done
        inc     fxl_sx                  ; x runs right to left
        sec
        lda     #0
        sbc     fxl_dx
        sta     fxl_dx
        lda     #0
        sbc     fxl_dx+1
        sta     fxl_dx+1
@dx_done:

        ; |dy| and the y direction, in 16 bits (239 - 0 overflows a byte)
        stz     fxl_sy
        sec
        lda     X16_P5
        sbc     X16_P2
        sta     fxl_dy
        lda     #0
        sbc     #0
        sta     fxl_dy+1
        bpl     @dy_done
        inc     fxl_sy
        sec
        lda     #0
        sbc     fxl_dy
        sta     fxl_dy
        lda     #0
        sbc     fxl_dy+1
        sta     fxl_dy+1
@dy_done:

        ; pick the octant: ADDR1 steps the major axis every pixel, ADDR0's
        ; increment is borrowed for the sometimes-step along the minor axis
        lda     fxl_dy+1
        cmp     fxl_dx+1
        bne     @which
        lda     fxl_dy
        cmp     fxl_dx
@which:
        bcc     @x_major

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
        beq     @ym1
        ora     #VERA_ADDR_H_DECR
@ym1:
        sta     fxl_h1                  ; ADDR1: a row per step
        lda     #(VERA_INC_1 << 4)
        ldx     fxl_sx
        beq     @ym0
        ora     #VERA_ADDR_H_DECR
@ym0:
        sta     fxl_h0                  ; ADDR0: a pixel, sometimes
        bra     @slope

@x_major:
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
        beq     @xm1
        ora     #VERA_ADDR_H_DECR
@xm1:
        sta     fxl_h1
        lda     #(VERA_INC_320 << 4)
        ldx     fxl_sy
        beq     @xm0
        ora     #VERA_ADDR_H_DECR
@xm0:
        sta     fxl_h0

@slope:
        ; slope = minor/major in 1/512ths (0..512); a point has no slope
        stz     fxl_v
        stz     fxl_v+1
        lda     fxl_major
        ora     fxl_major+1
        beq     @program
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

@program:
        jsr     pix_addr                ; fxa = address of (P0/P1, P2)

        ; An axis-aligned line (minor delta 0) is just a run along port
        ; 1's increment -- no FX needed.
        lda     fxl_minor
        ora     fxl_minor+1
        beq     @plain

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
        bra     @count                  ; would eat the line's first minor-step

@plain:
        jsr     set_addr1

@count:
        ; draw major+1 pixels
        clc
        lda     fxl_major
        adc     #1
        tax
        lda     fxl_major+1
        adc     #0
        tay
        txa
        beq     @full
        iny
@full:
        lda     X16_P6
@draw:
        sta     VERA_DATA1
        dex
        bne     @draw
        dey
        bne     @draw
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
        bcs     @s1
        jsr     swap01
@s1:
        lda     tri_y2
        cmp     tri_y1
        bcs     @s2
        jsr     swap12
@s2:
        lda     tri_y1
        cmp     tri_y0
        bcs     @s3
        jsr     swap01
@s3:
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
        bne     @go
        rts                             ; a single row: nothing (half-open)
@go:
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
        bne     @two_parts
        jmp     @flat_top               ; out of branch range from here
@two_parts:

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
        bcs     @b_left
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
        bra     @pos
@b_left:
        lda     fxs_el                  ; B left, A (long) right
        sta     fxt_xl
        lda     fxs_eh
        sta     fxt_xh
        lda     fxt_a_l
        sta     fxt_yl
        lda     fxt_a_h
        sta     fxt_yh
        stz     fxt_swap                ; part 2 replaces the X slot
@pos:
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
        bne     @have_part2
        jmp     fx_off                  ; flat bottom: one part was the whole
@have_part2:

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
        beq     @repl_x
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
        bra     @part2
@repl_x:
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
@part2:
        vera_dcsel 5                    ; back to the fill-length window
        lda     fxt_n2
        jsr     poly_rows
        jmp     fx_off

@flat_top:
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
        bne     @ft_pick
        lda     tri_x0
        cmp     tri_x1
@ft_pick:
        bcc     @ft_v0_left
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
        bra     @ft_run
@ft_v0_left:
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
@ft_run:
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
@prow:
        lda     fxt_rows
        beq     @pdone
        lda     VERA_DATA1              ; latch: half-step edges, point ADDR1
        lda     VERA_FX_POLY_FILL_L
        sta     fxt_fl
        bmi     @plong
        lsr     a                       ; short row: length is bits 4:1
        and     #$0F
        sta     fxt_len
        stz     fxt_len+1
        bra     @pdraw
@plong:
        lda     VERA_FX_POLY_FILL_H
        sta     fxt_fh
        and     #$C0
        cmp     #$C0
        beq     @pskip                  ; bits 9+8 set: negative width, no row
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
@pdraw:
        ldx     fxt_len
        ldy     fxt_len+1
        txa
        ora     fxt_len+1
        beq     @pskip                  ; zero-width row
        txa
        beq     @pfull
        iny
@pfull:
        lda     tri_color
@ploop:
        sta     VERA_DATA1
        dex
        bne     @ploop
        dey
        bne     @ploop
@pskip:
        lda     VERA_DATA0              ; second half-step, ADDR0 to next row
        dec     fxt_rows
        bra     @prow
@pdone:
        rts

; signed (fxs_dxl/h * 256) / fxs_dy -> the 15-bit (+32x) register format
; in fxs_el/eh, with sign and 24-bit magnitude kept for the left/right
; comparison. *256, not *512: the poly filler wants HALF the per-row
; increment because it steps each edge twice per row.
slope:
        stz     fxs_sgn
        lda     fxs_dxh
        bpl     @sl_abs
        inc     fxs_sgn
        sec
        lda     #0
        sbc     fxs_dxl
        sta     fxs_dxl
        lda     #0
        sbc     fxs_dxh
        sta     fxs_dxh
@sl_abs:
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
        bne     @sl_big
        lda     fxd_num+1
        cmp     #$40
        bcc     @sl_small
@sl_big:
        ldx     #5
@sl_shift:
        lsr     fxd_num+2
        ror     fxd_num+1
        ror     fxd_num
        dex
        bne     @sl_shift
        inc     fxs_32
@sl_small:
        lda     fxd_num
        sta     fxs_el
        lda     fxd_num+1
        sta     fxs_eh
        lda     fxs_sgn
        beq     @sl_pos
        sec                             ; two's complement within the 15 bits
        lda     #0
        sbc     fxs_el
        sta     fxs_el
        lda     #0
        sbc     fxs_eh
        and     #$7F
        sta     fxs_eh
@sl_pos:
        lda     fxs_32
        beq     @sl_done
        lda     fxs_eh
        ora     #$80                    ; the 32x flag rides on bit 15
        sta     fxs_eh
@sl_done:
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
        beq     @cmp_same
        lda     fxs_sgn                 ; different signs: the negative is less
        bne     @cmp_yes
        clc
        rts
@cmp_same:
        lda     fxt_a_sgn
        bne     @cmp_neg
        lda     fxs_mag+2               ; both positive: B < A iff |B| < |A|
        cmp     fxt_a_mag+2
        bne     @cmp_p
        lda     fxs_mag+1
        cmp     fxt_a_mag+1
        bne     @cmp_p
        lda     fxs_mag
        cmp     fxt_a_mag
@cmp_p:
        bcc     @cmp_yes
        clc
        rts
@cmp_neg:
        lda     fxt_a_mag+2             ; both negative: B < A iff |A| < |B|
        cmp     fxs_mag+2
        bne     @cmp_n
        lda     fxt_a_mag+1
        cmp     fxs_mag+1
        bne     @cmp_n
        lda     fxt_a_mag
        cmp     fxs_mag
@cmp_n:
        bcc     @cmp_yes
        clc
        rts
@cmp_yes:
        sec
        rts

; fxd_num(24) / fxd_den(16) -> quotient in fxd_num, remainder fxd_rem
udiv24:
        stz     fxd_rem
        stz     fxd_rem+1
        ldx     #24
@dv:
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
        bcc     @dv_no
        sta     fxd_rem+1
        sty     fxd_rem
        inc     fxd_num
@dv_no:
        dex
        bne     @dv
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
        .segment        "BSS"

tri_x0:    .res 2
tri_y0:    .res 1
tri_x1:    .res 2
tri_y1:    .res 1
tri_x2:    .res 2
tri_y2:    .res 1
tri_color: .res 1

fxl_dx:    .res 2
fxl_dy:    .res 2
fxl_major: .res 2
fxl_minor: .res 2
fxl_v:     .res 2
fxl_sx:    .res 1
fxl_sy:    .res 1
fxl_h1:    .res 1
fxl_h0:    .res 1

fxt_n1:    .res 1
fxt_n2:    .res 1
fxt_swap:  .res 1
fxt_xl:    .res 1                       ; encoded increments for the two slots
fxt_xh:    .res 1
fxt_yl:    .res 1
fxt_yh:    .res 1
fxt_px:    .res 2                       ; starting x of each edge
fxt_py:    .res 2
fxt_a_l:   .res 1                       ; the long edge, parked
fxt_a_h:   .res 1
fxt_a_sgn: .res 1
fxt_a_mag: .res 3
fxt_rows:  .res 1
fxt_fl:    .res 1
fxt_fh:    .res 1
fxt_len:   .res 2

fxs_dxl:   .res 1
fxs_dxh:   .res 1
fxs_dy:    .res 1
fxs_sgn:   .res 1
fxs_32:    .res 1
fxs_mag:   .res 3
fxs_el:    .res 1
fxs_eh:    .res 1

fxd_num:   .res 3
fxd_den:   .res 2
fxd_rem:   .res 2

fxa:       .res 3
