; =====================================================================
; x16clib :: gfx/shapes.s -- circle / disc / flood for BOTH bitmap modes
; =====================================================================
; One engine-agnostic shape algorithm (ported from x16_library's
; gfx/shapes.asm), bound at call time to the 8bpp module (x16_gfx_*) or
; the 2bpp module (x16_gfx2_*) by shp_bind8 / shp_bind2.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

        zpage   r0
        zpage   r1
        zpage   r2
        zpage   r3
        zpage   r4
        zpage   r6

        global  _x16_gfx_circle
        global  _x16_gfx_disc
        global  _x16_gfx_flood
        global  _x16_gfx2_circle
        global  _x16_gfx2_disc
        global  _x16_gfx2_flood

        section text

shp_do_pset:
        jmp     (shp_psetv)
shp_do_hline:
        jmp     (shp_hlinev)
shp_do_read:
        jmp     (shp_readv)

shp_pset8:
        sta     X16_P3
        jmp     gfx_pset
shp_hline8:
        sta     X16_P3
        jmp     gfx_hline

shp_bind8:
        lda     #<shp_pset8
        sta     shp_psetv
        lda     #>shp_pset8
        sta     shp_psetv+1
        lda     #<shp_hline8
        sta     shp_hlinev
        lda     #>shp_hline8
        sta     shp_hlinev+1
        lda     #<gfx_read
        sta     shp_readv
        lda     #>gfx_read
        sta     shp_readv+1
        lda     #<320
        sta     shp_w
        lda     #>320
        sta     shp_w+1
        lda     #<240
        sta     shp_h
        lda     #>240
        sta     shp_h+1
        rts

shp_bind2:
        lda     #<gfx2_pset
        sta     shp_psetv
        lda     #>gfx2_pset
        sta     shp_psetv+1
        lda     #<gfx2_hline
        sta     shp_hlinev
        lda     #>gfx2_hline
        sta     shp_hlinev+1
        lda     #<gfx2_read
        sta     shp_readv
        lda     #>gfx2_read
        sta     shp_readv+1
        lda     #<640
        sta     shp_w
        lda     #>640
        sta     shp_w+1
        lda     #<480
        sta     shp_h
        lda     #>480
        sta     shp_h+1
        rts

; --- C entry points (vbcc: cx/x int in r0/r1; chars in r2/r4/r6) --------
; gfx_circle/disc(cx:int, cy:char, r:char, color:char)
_x16_gfx_circle:
        jsr     shp_marshal8
        jsr     shp_bind8
        lda     shp_mcol
        jmp     shape_circle
_x16_gfx_disc:
        jsr     shp_marshal8
        jsr     shp_bind8
        lda     shp_mcol
        jmp     shape_disc
shp_marshal8:                           ; cx->r0/r1, cy->r2, r->r4, col->r6
        lda     r0
        sta     X16_P0
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2
        stz     X16_P3
        lda     r4
        sta     X16_P4
        lda     r6
        sta     shp_mcol
        rts

; gfx2_circle/disc(cx:int, cy:int, r:char, color:char)
_x16_gfx2_circle:
        jsr     shp_marshal2
        jsr     shp_bind2
        lda     shp_mcol
        jmp     shape_circle
_x16_gfx2_disc:
        jsr     shp_marshal2
        jsr     shp_bind2
        lda     shp_mcol
        jmp     shape_disc
shp_marshal2:                           ; cx->r0/r1, cy->r2/r3, r->r4, col->r6
        lda     r0
        sta     X16_P0
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2
        lda     r3
        sta     X16_P3
        lda     r4
        sta     X16_P4
        lda     r6
        sta     shp_mcol
        rts

; gfx_flood(x:int, y:char, color:char)
_x16_gfx_flood:
        lda     r0
        sta     X16_P0
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2
        stz     X16_P3
        lda     r4
        sta     shp_mcol
        jsr     shp_bind8
        lda     shp_mcol
        jsr     shape_flood             ; carry set = the fill is incomplete
        lda     #0
        rol     a
        eor     #1                      ; report completeness, not overflow
        rts
; gfx2_flood(x:int, y:int, color:char)
_x16_gfx2_flood:
        lda     r0
        sta     X16_P0
        lda     r1
        sta     X16_P1
        lda     r2
        sta     X16_P2
        lda     r3
        sta     X16_P3
        lda     r4
        sta     shp_mcol
        jsr     shp_bind2
        lda     shp_mcol
        jsr     shape_flood             ; carry set = the fill is incomplete
        lda     #0
        rol     a
        eor     #1                      ; report completeness, not overflow
        rts

shape_circle:
	sta shp_col
	jsr shp_take_cxy               ; cx/cy out of the P block, x=r, y=0
shp_cloop:
	lda shp_y                      ; while y <= x
	cmp shp_x
	beq shp_cplot                  ; the diagonal point still plots
	bcs shp_cdone
shp_cplot:
	lda shp_x                      ; the (x,y) octant pair...
	sta shp_a
	lda shp_y
	sta shp_b
	jsr shp_pair4
	lda shp_y                      ; ..shp_and the (y,x) pair
	sta shp_a
	lda shp_x
	sta shp_b
	jsr shp_pair4
	jsr shp_step                   ; the midpoint error walk
	bra shp_cloop
shp_cdone:
	rts

; ---------------------------------------------------------------------
; shape_disc
; ---------------------------------------------------------------------
shape_disc:
	sta shp_col
	jsr shp_take_cxy
shp_dloop:
	lda shp_y
	cmp shp_x
	beq shp_dspan
	bcs shp_ddone
shp_dspan:
	lda shp_x                      ; spans at cy +/- y, half-width x...
	sta shp_a
	lda shp_y
	sta shp_b
	jsr shp_span2
	lda shp_y                      ; ..shp_and at cy +/- x, half-width y
	sta shp_a
	lda shp_x
	sta shp_b
	jsr shp_span2
	jsr shp_step
	bra shp_dloop
shp_ddone:
	rts

; --- shared circle/disc machinery -------------------------------------
shp_take_cxy:                       ; P -> locals; x = r, y = 0, err = 1 - r
	lda X16_P0
	sta shp_cx
	lda X16_P1
	sta shp_cx+1
	lda X16_P2
	sta shp_cy
	lda X16_P3
	sta shp_cy+1
	lda X16_P4
	sta shp_x
	lda #0
	sta shp_y
	sec                         ; err = 1 - r, signed 16-bit
	lda #1
	sbc shp_x
	sta shp_err
	lda #0
	sbc #0
	sta shp_err+1
	rts

shp_step:                           ; y++; err < 0 ? err += 2y+1
	inc shp_y                      ;      else x--, err += 2(y-x)+1
	bit shp_err+1
	bmi shp_grow
	dec shp_x
	sec                         ; t = y - x, sign-extended
	lda shp_y
	sbc shp_x
	sta shp_t
	lda #0
	sbc #0
	sta shp_t+1
	bra shp_apply
shp_grow:
	lda shp_y                      ; t = y (positive)
	sta shp_t
	lda #0
	sta shp_t+1
shp_apply:
	asl shp_t                      ; err += 2t + 1
	rol shp_t+1
	inc shp_t
	bne shp_g1
	inc shp_t+1
shp_g1:	clc
	lda shp_err
	adc shp_t
	sta shp_err
	lda shp_err+1
	adc shp_t+1
	sta shp_err+1
	rts

shp_pair4:                          ; pset the 4 sign combos of (cx±a, cy±b)
	lda #0
	sta shp_sx
	sta shp_sy
shp_p4go:
	jsr shp_emit1
	lda shp_sx                     ; walk ++, -+, +-, -- via two flags
	eor #1
	sta shp_sx
	bne shp_p4go
	lda shp_sy
	eor #1
	sta shp_sy
	bne shp_p4go
	rts

shp_emit1:                          ; one pset at (cx sx? -a : +a, cy sy? -b : +b)
	lda shp_sx
	bne shp_e1xm
	clc                         ; x = cx + a
	lda shp_cx
	adc shp_a
	sta X16_P0
	lda shp_cx+1
	adc #0
	sta X16_P1
	bra shp_e1y
shp_e1xm:
	sec                         ; x = cx - a
	lda shp_cx
	sbc shp_a
	sta X16_P0
	lda shp_cx+1
	sbc #0
	sta X16_P1
shp_e1y:
	lda shp_sy
	bne shp_e1ym
	clc
	lda shp_cy
	adc shp_b
	sta X16_P2
	lda shp_cy+1
	adc #0
	sta X16_P3
	bra shp_e1go
shp_e1ym:
	sec
	lda shp_cy
	sbc shp_b
	sta X16_P2
	lda shp_cy+1
	sbc #0
	sta X16_P3
shp_e1go:
	lda shp_col
	jmp shp_do_pset

shp_span2:                          ; hline half-width a at cy+b and cy-b
	lda #0
	sta shp_sy
	jsr shp_espan
	lda #1
	sta shp_sy
	; fall through
shp_espan:
	sec                         ; x = cx - a
	lda shp_cx
	sbc shp_a
	sta X16_P0
	lda shp_cx+1
	sbc #0
	sta X16_P1
	lda shp_sy
	bne shp_esym
	clc
	lda shp_cy
	adc shp_b
	sta X16_P2
	lda shp_cy+1
	adc #0
	sta X16_P3
	bra shp_esgo
shp_esym:
	sec
	lda shp_cy
	sbc shp_b
	sta X16_P2
	lda shp_cy+1
	sbc #0
	sta X16_P3
shp_esgo:
	lda shp_a                      ; len = 2a + 1
	sta X16_P4
	lda #0
	sta X16_P5
	asl X16_P4
	rol X16_P5
	inc X16_P4
	bne shp_g2
	inc X16_P5
shp_g2:	lda shp_col
	jmp shp_do_hline

; ---------------------------------------------------------------------
; shape_flood
; ---------------------------------------------------------------------
; Pop a seed, widen it into a span of the target colour, fill the span,
; then scan the rows above and below for runs of target and push one
; seed per run. The stack holds seeds as xshp_w yshp_w; when it is full a
; seed is dropped and the overflow is remembered in the carry.
; ---------------------------------------------------------------------
FLOOD_MAX = 96                  ; seeds; 4 bytes each

shape_flood:
	sta shp_col
	lda #0
	sta shp_ovf
	sta shp_sp
	jsr shp_rd_p                   ; the target = the seed's own colour
	sta shp_tgt
	cmp shp_col                    ; filling with itself never ends: done
	bne shp_fseed
	clc                         ; (no overflow could have happened yet)
	rts
shp_fseed:
	lda X16_P0                  ; push the seed
	sta shp_qx
	lda X16_P1
	sta shp_qx+1
	lda X16_P2
	sta shp_qy
	lda X16_P3
	sta shp_qy+1
	jsr shp_push
shp_floop:
	lda shp_sp                     ; stack empty: finished
	bne shp_fbody
	jmp shp_fexit
shp_fbody:
	jsr shp_pop                    ; seed -> shp_qx/shp_qy
	jsr shp_rd_q                   ; still target? (may have been filled)
	cmp shp_tgt
	bne shp_floop

	lda shp_qx                     ; widen left: xl = leftmost target
	sta shp_xl
	lda shp_qx+1
	sta shp_xl+1
shp_wleft:
	lda shp_xl
	ora shp_xl+1
	beq shp_wldone                 ; at column 0
	sec                         ; probe xl-1
	lda shp_xl
	sbc #1
	sta shp_qx
	lda shp_xl+1
	sbc #0
	sta shp_qx+1
	jsr shp_rd_q
	cmp shp_tgt
	bne shp_wldone
	lda shp_qx
	sta shp_xl
	lda shp_qx+1
	sta shp_xl+1
	bra shp_wleft
shp_wldone:
	lda shp_qy                     ; widen right: xr = rightmost target
	sta shp_qy                     ; (qy already holds the row)
	lda shp_xl
	sta shp_xr
	lda shp_xl+1
	sta shp_xr+1
shp_wright:
	clc                         ; probe xr+1, stop at shp_w-1
	lda shp_xr
	adc #1
	sta shp_qx
	lda shp_xr+1
	adc #0
	sta shp_qx+1
	lda shp_qx                     ; qx == W? off the right edge
	cmp shp_w
	bne shp_wrprobe
	lda shp_qx+1
	cmp shp_w+1
	beq shp_wrdone
shp_wrprobe:
	jsr shp_rd_q
	cmp shp_tgt
	bne shp_wrdone
	lda shp_qx
	sta shp_xr
	lda shp_qx+1
	sta shp_xr+1
	bra shp_wright
shp_wrdone:
	lda shp_xl                     ; fill the span: hline(xl, y, xr-xl+1)
	sta X16_P0
	lda shp_xl+1
	sta X16_P1
	lda shp_qy
	sta X16_P2
	lda shp_qy+1
	sta X16_P3
	sec
	lda shp_xr
	sbc shp_xl
	sta X16_P4
	lda shp_xr+1
	sbc shp_xl+1
	sta X16_P5
	inc X16_P4
	bne shp_g3
	inc X16_P5
shp_g3:	lda shp_col
	jsr shp_do_hline

	lda shp_qy                     ; shp_scanrow clobbers shp_qy, so keep the
	sta shp_row                    ; filled row here for BOTH neighbour scans
	lda shp_qy+1
	sta shp_row+1

	lda shp_row                    ; the row above...
	sta shp_ry
	lda shp_row+1
	sta shp_ry+1
	lda shp_ry
	ora shp_ry+1
	beq shp_below                  ; row 0 has nothing above
	sec
	lda shp_ry
	sbc #1
	sta shp_ry
	lda shp_ry+1
	sbc #0
	sta shp_ry+1
	jsr shp_scanrow
shp_below:
	clc                         ; ..and the row below
	lda shp_row
	adc #1
	sta shp_ry
	lda shp_row+1
	adc #0
	sta shp_ry+1
	lda shp_ry                     ; ry == H? off the bottom
	cmp shp_h
	bne shp_bscan
	lda shp_ry+1
	cmp shp_h+1
	beq shp_fnext
shp_bscan:
	jsr shp_scanrow
shp_fnext:
	jmp shp_floop
shp_fexit:
	lsr shp_ovf                    ; overflow -> carry
	rts

; scan shp_xl..shp_xr on row shp_ry for runs of target; push one seed per run
shp_scanrow:
	lda #0
	sta shp_run
	lda shp_xl
	sta shp_tx
	lda shp_xl+1
	sta shp_tx+1
shp_srloop:
	lda shp_tx                     ; read (tx, ry)
	sta shp_qx
	lda shp_tx+1
	sta shp_qx+1
	lda shp_ry
	sta shp_qy
	lda shp_ry+1
	sta shp_qy+1
	jsr shp_rd_q
	cmp shp_tgt
	bne shp_srmiss
	lda shp_run                    ; entering a run: one seed
	bne shp_srnext
	lda #1
	sta shp_run
	jsr shp_push
	bra shp_srnext
shp_srmiss:
	lda #0
	sta shp_run
shp_srnext:
	lda shp_tx                     ; tx == xr? done
	cmp shp_xr
	bne shp_srinc
	lda shp_tx+1
	cmp shp_xr+1
	beq shp_srdone
shp_srinc:
	inc shp_tx
	bne shp_srloop
	inc shp_tx+1
	bra shp_srloop
shp_srdone:
	rts

shp_rd_p:                           ; read at the CALLER's P block (entry)
	jmp shp_do_read
shp_rd_q:                           ; read at (shp_qx, shp_qy)
	lda shp_qx
	sta X16_P0
	lda shp_qx+1
	sta X16_P1
	lda shp_qy
	sta X16_P2
	lda shp_qy+1
	sta X16_P3
	jmp shp_do_read

shp_push:                           ; (shp_qx,shp_qy) onto the stack, or drop + ovf
	lda shp_sp
	cmp #FLOOD_MAX
	bcc shp_g4
	lda #2                      ; remembered; lsr at exit -> carry
	sta shp_ovf
	rts
shp_g4:	asl                         ; sp * 4
	asl
	tax
	lda shp_qx
	sta shp_stk,x
	lda shp_qx+1
	sta shp_stk+1,x
	lda shp_qy
	sta shp_stk+2,x
	lda shp_qy+1
	sta shp_stk+3,x
	inc shp_sp
	rts

shp_pop:                            ; the top seed -> (shp_qx,shp_qy)
	dec shp_sp
	lda shp_sp
	asl
	asl
	tax
	lda shp_stk,x
	sta shp_qx
	lda shp_stk+1,x
	sta shp_qx+1
	lda shp_stk+2,x
	sta shp_qy
	lda shp_stk+3,x
	sta shp_qy+1
	rts


        section bss

shp_psetv:  reserve 2
shp_hlinev: reserve 2
shp_readv:  reserve 2
shp_w:      reserve 2
shp_h:      reserve 2
shp_mcol:   reserve 1

shp_col: reserve 1
shp_cx: reserve 2
shp_cy: reserve 2
shp_x: reserve 1
shp_y: reserve 1
shp_a: reserve 1
shp_b: reserve 1
shp_sx: reserve 1
shp_sy: reserve 1
shp_err: reserve 2
shp_t: reserve 2

shp_tgt: reserve 1
shp_ovf: reserve 1
shp_sp: reserve 1
shp_qx: reserve 2
shp_qy: reserve 2
shp_xl: reserve 2
shp_xr: reserve 2
shp_ry: reserve 2
shp_row: reserve 2
shp_tx: reserve 2
shp_run: reserve 1
shp_stk: reserve FLOOD_MAX * 4

