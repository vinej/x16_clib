; =====================================================================
; x16clib :: gfx/shapes_curves.s -- v0.8.0 curve shapes (separate object
; so a program using only the basic shapes does not link this code/state;
; vbcc's vlink has no dead-stripping). Uses shp_bind*/shp_do_*/shp_mcol
; from gfx/shapes.s and sin8/cos8 from util/math.s.
; =====================================================================

        include        "macros.inc"
        include        "x16zp.inc"

        zpage   r0
        zpage   r1
        zpage   r2
        zpage   r3
        zpage   r4
        zpage   r5
        zpage   r6
        zpage   r7

        global  _x16_gfx_polygon
        global  _x16_gfx_fpolygon
        global  _x16_gfx2_polygon
        global  _x16_gfx2_fpolygon
        global  _x16_gfx_rrect
        global  _x16_gfx_frrect
        global  _x16_gfx2_rrect
        global  _x16_gfx2_frrect
        global  _x16_gfx_arc
        global  _x16_gfx2_arc
        global  _x16_gfx_pie
        global  _x16_gfx2_pie
        global  _x16_gfx_bezier
        global  _x16_gfx2_bezier

        section text
; --- v0.8.0 curve-shape marshals (vbcc: args 0-3 in r0-r7, 4+ on (sp)) ---
shp_pmarshal8:                  ; polygon/arc/pie 8bpp
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
        sta     X16_P5
        ldy     #0
        lda     (sp),y
        sta     X16_P6                  ; rot/a1 (stacked arg4)
        iny
        lda     (sp),y
        sta     shp_mcol                ; col (stacked arg5)
        rts
shp_pmarshal2:                  ; polygon/arc/pie 2bpp
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
        sta     X16_P5
        ldy     #0
        lda     (sp),y
        sta     X16_P6
        iny
        lda     (sp),y
        sta     shp_mcol
        rts
shp_rmarshal:                   ; rrect (x,y,w,h 16-bit, r, col)
        lda     r0
        sta     rr_x
        lda     r1
        sta     rr_x+1
        lda     r2
        sta     rr_y
        lda     r3
        sta     rr_y+1
        lda     r4
        sta     rr_w
        lda     r5
        sta     rr_w+1
        lda     r6
        sta     rr_h
        lda     r7
        sta     rr_h+1
        ldy     #0
        lda     (sp),y
        sta     rr_r                    ; stacked arg4
        iny
        lda     (sp),y
        sta     shp_mcol                ; stacked arg5
        rts
shp_bmarshal:                   ; bezier: pts in r0/r1 (zp), col in r2
        lda     r2
        sta     shp_mcol
        ldy     #0
shp_bmcopy:
        lda     (r0),y
        sta     bez_x0,y
        iny
        cpy     #16
        bne     shp_bmcopy
        rts

_x16_gfx_polygon:
        jsr     shp_pmarshal8
        jsr     shp_bind8
        lda     shp_mcol
        jmp     shape_polygon
_x16_gfx_fpolygon:
        jsr     shp_pmarshal8
        jsr     shp_bind8
        lda     shp_mcol
        jmp     shape_fpolygon
_x16_gfx2_polygon:
        jsr     shp_pmarshal2
        jsr     shp_bind2
        lda     shp_mcol
        jmp     shape_polygon
_x16_gfx2_fpolygon:
        jsr     shp_pmarshal2
        jsr     shp_bind2
        lda     shp_mcol
        jmp     shape_fpolygon
_x16_gfx_rrect:
        jsr     shp_rmarshal
        jsr     shp_bind8
        lda     shp_mcol
        jmp     shape_rrect
_x16_gfx_frrect:
        jsr     shp_rmarshal
        jsr     shp_bind8
        lda     shp_mcol
        jmp     shape_frrect
_x16_gfx2_rrect:
        jsr     shp_rmarshal
        jsr     shp_bind2
        lda     shp_mcol
        jmp     shape_rrect
_x16_gfx2_frrect:
        jsr     shp_rmarshal
        jsr     shp_bind2
        lda     shp_mcol
        jmp     shape_frrect
_x16_gfx_arc:
        jsr     shp_pmarshal8
        jsr     shp_bind8
        lda     shp_mcol
        jmp     shape_arc
_x16_gfx2_arc:
        jsr     shp_pmarshal2
        jsr     shp_bind2
        lda     shp_mcol
        jmp     shape_arc
_x16_gfx_pie:
        jsr     shp_pmarshal8
        jsr     shp_bind8
        lda     shp_mcol
        jmp     shape_pie
_x16_gfx2_pie:
        jsr     shp_pmarshal2
        jsr     shp_bind2
        lda     shp_mcol
        jmp     shape_pie
_x16_gfx_bezier:
        jsr     shp_bmarshal
        jsr     shp_bind8
        lda     shp_mcol
        jmp     shape_bezier
_x16_gfx2_bezier:
        jsr     shp_bmarshal
        jsr     shp_bind2
        lda     shp_mcol
        jmp     shape_bezier

        section text
POLY_MAX = 24
; ==== v0.8.0 curve-shape cores ====
shape_polygon:
	sta poly_col
	stz poly_efl                ; outline
	jmp shp_poly_begin
shape_fpolygon:
	sta poly_col
	lda #1                      ; filled
	sta poly_efl
	; fall through
shp_poly_begin:
	lda X16_P5                  ; clamp the side count to 3..POLY_MAX
	cmp #3
	bcc shp_pg_bret                ; fewer than 3: not a polygon
	cmp #(POLY_MAX + 1)
	bcc shp_pg_bnok
	lda #POLY_MAX
shp_pg_bnok:
	sta poly_n
	lda X16_P0
	sta poly_cx
	lda X16_P1
	sta poly_cx+1
	lda X16_P2
	sta poly_cy
	lda X16_P3
	sta poly_cy+1
	lda X16_P4
	sta poly_r
	stz poly_acc                ; angle accumulator = rotation << 8
	lda X16_P6
	sta poly_acc+1
	jsr shp_poly_verts
	lda poly_efl
	bne shp_pg_bfill
	jmp shp_poly_outline
shp_pg_bfill:
	jmp shp_poly_fill
shp_pg_bret:
	rts

; compute the N vertices into poly_vx[]/poly_vy[]
shp_poly_verts:
	jsr shp_poly_step              ; poly_step = 65536 / n
	stz poly_i
shp_pg_vloop:
	lda poly_i
	cmp poly_n
	beq shp_pg_vend
	lda poly_acc+1              ; this vertex's byte angle
	pha
	jsr cos8                    ; A = cos * 127 (signed)
	jsr shp_poly_scale             ; poly_off = round(r * A / 128), signed
	lda poly_i
	asl
	tax                         ; 2*i
	clc
	lda poly_cx
	adc poly_off
	sta poly_vx,x
	lda poly_cx+1
	adc poly_off+1
	sta poly_vx+1,x
	pla                         ; the angle again
	jsr sin8                    ; A = sin * 127 (signed)
	jsr shp_poly_scale
	lda poly_i
	asl
	tax
	clc
	lda poly_cy
	adc poly_off
	sta poly_vy,x
	lda poly_cy+1
	adc poly_off+1
	sta poly_vy+1,x
	clc                         ; acc += step
	lda poly_acc
	adc poly_step
	sta poly_acc
	lda poly_acc+1
	adc poly_step+1
	sta poly_acc+1
	inc poly_i
	bra shp_pg_vloop
shp_pg_vend:
	rts

; poly_off = round(poly_r * |A| / 128) with A's sign, A a signed byte
shp_poly_scale:
	stz poly_sgn
	pha
	and #$80
	beq shp_pg_spos
	inc poly_sgn
	pla
	eor #$FF
	clc
	adc #1
	bra shp_pg_smul
shp_pg_spos:
	pla
shp_pg_smul:
	jsr shp_poly_mul8              ; poly_p16 = poly_r * |A|
	clc
	lda poly_p16                ; + 0.5 LSB, so >>7 rounds
	adc #64
	sta poly_p16
	lda poly_p16+1
	adc #0
	sta poly_p16+1
	lda poly_p16                ; >>7 (product < 32768, so one byte out)
	asl
	lda poly_p16+1
	rol
	sta poly_off
	stz poly_off+1
	lda poly_sgn
	beq shp_pg_sdone
	sec                         ; negate
	lda #0
	sbc poly_off
	sta poly_off
	lda #0
	sbc poly_off+1
	sta poly_off+1
shp_pg_sdone:
	rts

; poly_p16 = poly_r * A  (8x8 -> 16, unsigned)
shp_poly_mul8:
	sta poly_t
	lda #0
	ldx #8
shp_pg_mloop:
	lsr poly_t
	bcc shp_pg_mskip
	clc
	adc poly_r
shp_pg_mskip:
	ror
	ror poly_p16
	dex
	bne shp_pg_mloop
	sta poly_p16+1
	rts

; poly_step = floor(65536 / poly_n), by restoring division of $010000
shp_poly_step:
	stz poly_dvd
	stz poly_dvd+1
	lda #1
	sta poly_dvd+2
	stz poly_rem
	stz poly_step
	stz poly_step+1
	ldx #24
shp_pg_dloop:
	asl poly_dvd
	rol poly_dvd+1
	rol poly_dvd+2
	rol poly_rem                ; carry = the remainder's 9th bit
	bcs shp_pg_dsub                ; overflowed 8 bits: certainly >= n
	lda poly_rem
	cmp poly_n
	bcc shp_pg_dnoq
shp_pg_dsub:
	lda poly_rem                ; carry is set on both paths here
	sbc poly_n
	sta poly_rem
	sec                         ; quotient bit = 1
	bra shp_pg_dbit
shp_pg_dnoq:
	clc                         ; quotient bit = 0
shp_pg_dbit:
	rol poly_step
	rol poly_step+1
	dex
	bne shp_pg_dloop
	rts

; --- outline ---------------------------------------------------------
shp_poly_outline:
	stz poly_i
shp_pg_oloop:
	lda poly_i                  ; endpoint 0 = vertex i
	asl
	tax
	lda poly_vx,x
	sta poly_lx0
	lda poly_vx+1,x
	sta poly_lx0+1
	lda poly_vy,x
	sta poly_ly0
	lda poly_vy+1,x
	sta poly_ly0+1
	lda poly_i                  ; endpoint 1 = vertex (i+1) mod n
	clc
	adc #1
	cmp poly_n
	bne shp_pg_ojok
	lda #0
shp_pg_ojok:
	asl
	tax
	lda poly_vx,x
	sta poly_lx1
	lda poly_vx+1,x
	sta poly_lx1+1
	lda poly_vy,x
	sta poly_ly1
	lda poly_vy+1,x
	sta poly_ly1+1
	jsr shp_poly_line
	inc poly_i
	lda poly_i
	cmp poly_n
	bne shp_pg_oloop
	rts

; 16-bit Bresenham from (lx0,ly0) to (lx1,ly1), plotting through shp_do_pset
; (the gfx2_line algorithm, engine-agnostic and clipping via the binding)
shp_poly_line:
	sec                         ; dx = |x1 - x0|, sx = direction
	lda poly_lx1
	sbc poly_lx0
	sta poly_ldx
	lda poly_lx1+1
	sbc poly_lx0+1
	sta poly_ldx+1
	bpl shp_pg_ldxp
	sec
	lda #0
	sbc poly_ldx
	sta poly_ldx
	lda #0
	sbc poly_ldx+1
	sta poly_ldx+1
	lda #$FF
	sta poly_lsx
	sta poly_lsx+1
	bra shp_pg_ldxd
shp_pg_ldxp:
	lda #1
	sta poly_lsx
	stz poly_lsx+1
shp_pg_ldxd:
	sec                         ; dy = -|y1 - y0|, sy = direction
	lda poly_ly1
	sbc poly_ly0
	sta poly_lt
	lda poly_ly1+1
	sbc poly_ly0+1
	sta poly_lt+1
	bpl shp_pg_ldyp
	sec
	lda #0
	sbc poly_lt
	sta poly_lt
	lda #0
	sbc poly_lt+1
	sta poly_lt+1
	lda #$FF
	sta poly_lsy
	sta poly_lsy+1
	bra shp_pg_ldyd
shp_pg_ldyp:
	lda #1
	sta poly_lsy
	stz poly_lsy+1
shp_pg_ldyd:
	sec                         ; ldy = -|dy|
	lda #0
	sbc poly_lt
	sta poly_ldy
	lda #0
	sbc poly_lt+1
	sta poly_ldy+1
	clc                         ; err = dx + dy
	lda poly_ldx
	adc poly_ldy
	sta poly_lerr
	lda poly_ldx+1
	adc poly_ldy+1
	sta poly_lerr+1
shp_pg_lloop:
	lda poly_lx0
	sta X16_P0
	lda poly_lx0+1
	sta X16_P1
	lda poly_ly0
	sta X16_P2
	lda poly_ly0+1
	sta X16_P3
	lda poly_col
	jsr shp_do_pset
	lda poly_lx0                ; reached the endpoint?
	cmp poly_lx1
	bne shp_pg_lstep
	lda poly_lx0+1
	cmp poly_lx1+1
	bne shp_pg_lstep
	lda poly_ly0
	cmp poly_ly1
	bne shp_pg_lstep
	lda poly_ly0+1
	cmp poly_ly1+1
	bne shp_pg_lstep
	rts
shp_pg_lstep:
	lda poly_lerr               ; e2 = 2 * err
	asl
	sta poly_le2
	lda poly_lerr+1
	rol
	sta poly_le2+1
	sec                         ; e2 >= dy ?  err += dy, x0 += sx
	lda poly_le2
	sbc poly_ldy
	lda poly_le2+1
	sbc poly_ldy+1
	bvc shp_pg_lnv1
	eor #$80
shp_pg_lnv1:
	bmi shp_pg_lskx
	clc
	lda poly_lerr
	adc poly_ldy
	sta poly_lerr
	lda poly_lerr+1
	adc poly_ldy+1
	sta poly_lerr+1
	clc
	lda poly_lx0
	adc poly_lsx
	sta poly_lx0
	lda poly_lx0+1
	adc poly_lsx+1
	sta poly_lx0+1
shp_pg_lskx:
	sec                         ; e2 <= dx ?  err += dx, y0 += sy
	lda poly_ldx
	sbc poly_le2
	lda poly_ldx+1
	sbc poly_le2+1
	bvc shp_pg_lnv2
	eor #$80
shp_pg_lnv2:
	bmi shp_pg_lsky
	clc
	lda poly_lerr
	adc poly_ldx
	sta poly_lerr
	lda poly_lerr+1
	adc poly_ldx+1
	sta poly_lerr+1
	clc
	lda poly_ly0
	adc poly_lsy
	sta poly_ly0
	lda poly_ly0+1
	adc poly_lsy+1
	sta poly_ly0+1
shp_pg_lsky:
	jmp shp_pg_lloop

; --- fill ------------------------------------------------------------
; one scanline at a time; shp_poly_scanline gathers the row's span and draws
; it, shp_poly_edge does the per-edge crossing. Kept apart so every branch
; stays in range and each routine owns its own zone-local labels.
shp_poly_fill:
	jsr shp_poly_ybounds           ; poly_ymin / poly_ymax over all vertices
	lda poly_ymin
	sta poly_y
	lda poly_ymin+1
	sta poly_y+1
shp_pg_floop:
	lda poly_ymax               ; y > ymax ? done
	cmp poly_y
	lda poly_ymax+1
	sbc poly_y+1
	bvc shp_pg_fl1
	eor #$80
shp_pg_fl1:
	bmi shp_pg_fret                ; ymax < y
	jsr shp_poly_scanline
	inc poly_y
	bne shp_pg_floop
	inc poly_y+1
	bra shp_pg_floop
shp_pg_fret:
	rts

; fill row poly_y: find the span (xl..xr) across the edges, draw it
shp_poly_scanline:
	stz poly_found
	lda #$FF                    ; xl = +32767, xr = -32768
	sta poly_xl
	lda #$7F
	sta poly_xl+1
	stz poly_xr
	lda #$80
	sta poly_xr+1
	stz poly_i
shp_pg_slloop:
	lda poly_i
	cmp poly_n
	beq shp_pg_sldraw
	jsr shp_poly_edge
	inc poly_i
	bra shp_pg_slloop
shp_pg_sldraw:
	lda poly_found
	beq shp_pg_slret
	lda poly_xl                 ; span (xl .. xr) on row y
	sta X16_P0
	lda poly_xl+1
	sta X16_P1
	lda poly_y
	sta X16_P2
	lda poly_y+1
	sta X16_P3
	sec                         ; len = xr - xl + 1
	lda poly_xr
	sbc poly_xl
	sta X16_P4
	lda poly_xr+1
	sbc poly_xl+1
	sta X16_P5
	inc X16_P4
	bne shp_pg_sllen
	inc X16_P5
shp_pg_sllen:
	lda poly_col
	jmp shp_do_hline
shp_pg_slret:
	rts

; edge poly_i crossing row poly_y: if it spans the row, fold its x into
; poly_xl (min) / poly_xr (max) and set poly_found
shp_poly_edge:
	lda poly_i                  ; vertex a = i
	asl
	tax
	lda poly_i                  ; vertex b = (i+1) mod n
	clc
	adc #1
	cmp poly_n
	bne shp_pg_ejok
	lda #0
shp_pg_ejok:
	asl
	tay
	lda poly_vx,x
	sta poly_xa
	lda poly_vx+1,x
	sta poly_xa+1
	lda poly_vy,x
	sta poly_ya
	lda poly_vy+1,x
	sta poly_ya+1
	lda poly_vx,y
	sta poly_xb
	lda poly_vx+1,y
	sta poly_xb+1
	lda poly_vy,y
	sta poly_yb
	lda poly_vy+1,y
	sta poly_yb+1
	lda poly_ya                 ; top = the smaller-y endpoint
	cmp poly_yb
	lda poly_ya+1
	sbc poly_yb+1
	bvc shp_pg_escab
	eor #$80
shp_pg_escab:
	bmi shp_pg_eatop               ; ya < yb
	lda poly_xb                 ; b on top
	sta poly_xtop
	lda poly_xb+1
	sta poly_xtop+1
	lda poly_yb
	sta poly_ytop
	lda poly_yb+1
	sta poly_ytop+1
	lda poly_xa
	sta poly_xbot
	lda poly_xa+1
	sta poly_xbot+1
	lda poly_ya
	sta poly_ybot
	lda poly_ya+1
	sta poly_ybot+1
	bra shp_pg_eedge
shp_pg_eatop:
	lda poly_xa                 ; a on top
	sta poly_xtop
	lda poly_xa+1
	sta poly_xtop+1
	lda poly_ya
	sta poly_ytop
	lda poly_ya+1
	sta poly_ytop+1
	lda poly_xb
	sta poly_xbot
	lda poly_xb+1
	sta poly_xbot+1
	lda poly_yb
	sta poly_ybot
	lda poly_yb+1
	sta poly_ybot+1
shp_pg_eedge:
	lda poly_y                  ; y < ytop ? out (also skips horizontals)
	cmp poly_ytop
	lda poly_y+1
	sbc poly_ytop+1
	bvc shp_pg_esct
	eor #$80
shp_pg_esct:
	bmi shp_pg_eout
	lda poly_y                  ; y >= ybot ? out (half-open bottom)
	cmp poly_ybot
	lda poly_y+1
	sbc poly_ybot+1
	bvc shp_pg_escb
	eor #$80
shp_pg_escb:
	bpl shp_pg_eout
	bra shp_pg_ein
shp_pg_eout:
	rts
shp_pg_ein:
	sec                         ; md3 = dy = ybot - ytop  (> 0)
	lda poly_ybot
	sbc poly_ytop
	sta poly_md3
	lda poly_ybot+1
	sbc poly_ytop+1
	sta poly_md3+1
	sec                         ; md2 = t = y - ytop
	lda poly_y
	sbc poly_ytop
	sta poly_md2
	lda poly_y+1
	sbc poly_ytop+1
	sta poly_md2+1
	sec                         ; md1 = dx = xbot - xtop (signed)
	lda poly_xbot
	sbc poly_xtop
	sta poly_md1
	lda poly_xbot+1
	sbc poly_xtop+1
	sta poly_md1+1
	stz poly_dxs
	lda poly_md1+1
	bpl shp_pg_edxpos
	inc poly_dxs                ; dx < 0: take |dx|, remember the sign
	sec
	lda #0
	sbc poly_md1
	sta poly_md1
	lda #0
	sbc poly_md1+1
	sta poly_md1+1
shp_pg_edxpos:
	jsr shp_poly_umuldiv           ; poly_mdq = |dx| * t / dy
	lda poly_dxs
	bne shp_pg_exneg
	clc                         ; x = xtop + mdq
	lda poly_xtop
	adc poly_mdq
	sta poly_x
	lda poly_xtop+1
	adc poly_mdq+1
	sta poly_x+1
	bra shp_pg_egotx
shp_pg_exneg:
	sec                         ; x = xtop - mdq
	lda poly_xtop
	sbc poly_mdq
	sta poly_x
	lda poly_xtop+1
	sbc poly_mdq+1
	sta poly_x+1
shp_pg_egotx:
	lda #1
	sta poly_found
	lda poly_x                  ; xl = min(xl, x)
	cmp poly_xl
	lda poly_x+1
	sbc poly_xl+1
	bvc shp_pg_escl
	eor #$80
shp_pg_escl:
	bpl shp_pg_enoxl               ; x >= xl
	lda poly_x
	sta poly_xl
	lda poly_x+1
	sta poly_xl+1
shp_pg_enoxl:
	lda poly_xr                 ; xr = max(xr, x)
	cmp poly_x
	lda poly_xr+1
	sbc poly_x+1
	bvc shp_pg_escr
	eor #$80
shp_pg_escr:
	bpl shp_pg_enoxr               ; xr >= x
	lda poly_x
	sta poly_xr
	lda poly_x+1
	sta poly_xr+1
shp_pg_enoxr:
	rts

; poly_ymin / poly_ymax = the y extent of the vertices
shp_poly_ybounds:
	lda poly_vy
	sta poly_ymin
	sta poly_ymax
	lda poly_vy+1
	sta poly_ymin+1
	sta poly_ymax+1
	lda #1
	sta poly_i
shp_pg_ybloop:
	lda poly_i
	cmp poly_n
	beq shp_pg_ybend
	asl
	tax
	lda poly_vy,x               ; vy[i] < ymin ?
	cmp poly_ymin
	lda poly_vy+1,x
	sbc poly_ymin+1
	bvc shp_pg_ybc1
	eor #$80
shp_pg_ybc1:
	bpl shp_pg_ybnmin
	lda poly_vy,x
	sta poly_ymin
	lda poly_vy+1,x
	sta poly_ymin+1
shp_pg_ybnmin:
	lda poly_ymax               ; vy[i] > ymax ?
	cmp poly_vy,x
	lda poly_ymax+1
	sbc poly_vy+1,x
	bvc shp_pg_ybc2
	eor #$80
shp_pg_ybc2:
	bpl shp_pg_ybnmax
	lda poly_vy,x
	sta poly_ymax
	lda poly_vy+1,x
	sta poly_ymax+1
shp_pg_ybnmax:
	inc poly_i
	bra shp_pg_ybloop
shp_pg_ybend:
	rts

; poly_mdq = poly_md1 * poly_md2 / poly_md3, all unsigned (16x16->32, /16)
shp_poly_umuldiv:
	stz poly_prod+2
	stz poly_prod+3
	ldx #16
shp_pg_uml:
	lsr poly_md2+1
	ror poly_md2
	bcc shp_pg_unoadd
	lda poly_prod+2
	clc
	adc poly_md1
	sta poly_prod+2
	lda poly_prod+3
	adc poly_md1+1
	bra shp_pg_urot
shp_pg_unoadd:
	lda poly_prod+3
shp_pg_urot:
	ror
	sta poly_prod+3
	ror poly_prod+2
	ror poly_prod+1
	ror poly_prod
	dex
	bne shp_pg_uml
	stz poly_rem
	stz poly_rem+1
	ldx #32
shp_pg_udv:
	asl poly_prod
	rol poly_prod+1
	rol poly_prod+2
	rol poly_prod+3
	rol poly_rem
	rol poly_rem+1
	sec
	lda poly_rem
	sbc poly_md3
	tay
	lda poly_rem+1
	sbc poly_md3+1
	bcc shp_pg_udvno
	sta poly_rem+1
	sty poly_rem
	inc poly_prod
shp_pg_udvno:
	dex
	bne shp_pg_udv
	lda poly_prod
	sta poly_mdq
	lda poly_prod+1
	sta poly_mdq+1
	rts

; --- polygon state ---------------------------------------------------




; ---------------------------------------------------------------------
; shp_line -- shared 16-bit Bresenham (X16_USE_SHP_LINE)
; ---------------------------------------------------------------------
; The curve shapes (arc, bezier) sample a handful of points and join
; them; this is the join. It is the same engine-agnostic gfx2_line walk
; the polygon carries privately (shp_poly_line), lifted out so arc and
; bezier share ONE copy behind their own gate. A program that wants only
; the polygon still pays nothing for this; one that wants an arc pays for
; it once, not once per curve.
;
;   in: shl_x0/shl_y0 -> shl_x1/shl_y1 (signed words), shl_col = colour
;       draws through shp_do_pset, so it clips wherever pset clips.
; ---------------------------------------------------------------------

shp_line:
	sec                         ; dx = |x1 - x0|, sx = direction
	lda shl_x1
	sbc shl_x0
	sta shl_dx
	lda shl_x1+1
	sbc shl_x0+1
	sta shl_dx+1
	bpl shp_sl_dxp
	sec
	lda #0
	sbc shl_dx
	sta shl_dx
	lda #0
	sbc shl_dx+1
	sta shl_dx+1
	lda #$FF
	sta shl_sx
	sta shl_sx+1
	bra shp_sl_dxd
shp_sl_dxp:
	lda #1
	sta shl_sx
	stz shl_sx+1
shp_sl_dxd:
	sec                         ; dy = -|y1 - y0|, sy = direction
	lda shl_y1
	sbc shl_y0
	sta shl_t
	lda shl_y1+1
	sbc shl_y0+1
	sta shl_t+1
	bpl shp_sl_dyp
	sec
	lda #0
	sbc shl_t
	sta shl_t
	lda #0
	sbc shl_t+1
	sta shl_t+1
	lda #$FF
	sta shl_sy
	sta shl_sy+1
	bra shp_sl_dyd
shp_sl_dyp:
	lda #1
	sta shl_sy
	stz shl_sy+1
shp_sl_dyd:
	sec                         ; dy stored negative
	lda #0
	sbc shl_t
	sta shl_dy
	lda #0
	sbc shl_t+1
	sta shl_dy+1
	clc                         ; err = dx + dy
	lda shl_dx
	adc shl_dy
	sta shl_err
	lda shl_dx+1
	adc shl_dy+1
	sta shl_err+1
shp_sl_loop:
	lda shl_x0
	sta X16_P0
	lda shl_x0+1
	sta X16_P1
	lda shl_y0
	sta X16_P2
	lda shl_y0+1
	sta X16_P3
	lda shl_col
	jsr shp_do_pset
	lda shl_x0                  ; reached the endpoint?
	cmp shl_x1
	bne shp_sl_step
	lda shl_x0+1
	cmp shl_x1+1
	bne shp_sl_step
	lda shl_y0
	cmp shl_y1
	bne shp_sl_step
	lda shl_y0+1
	cmp shl_y1+1
	bne shp_sl_step
	rts
shp_sl_step:
	lda shl_err                 ; e2 = 2 * err
	asl
	sta shl_e2
	lda shl_err+1
	rol
	sta shl_e2+1
	sec                         ; e2 >= dy ?  err += dy, x0 += sx
	lda shl_e2
	sbc shl_dy
	lda shl_e2+1
	sbc shl_dy+1
	bvc shp_sl_nv1
	eor #$80
shp_sl_nv1:
	bmi shp_sl_skx
	clc
	lda shl_err
	adc shl_dy
	sta shl_err
	lda shl_err+1
	adc shl_dy+1
	sta shl_err+1
	clc
	lda shl_x0
	adc shl_sx
	sta shl_x0
	lda shl_x0+1
	adc shl_sx+1
	sta shl_x0+1
shp_sl_skx:
	sec                         ; e2 <= dx ?  err += dx, y0 += sy
	lda shl_dx
	sbc shl_e2
	lda shl_dx+1
	sbc shl_e2+1
	bvc shp_sl_nv2
	eor #$80
shp_sl_nv2:
	bmi shp_sl_sky
	clc
	lda shl_err
	adc shl_dx
	sta shl_err
	lda shl_err+1
	adc shl_dx+1
	sta shl_err+1
	clc
	lda shl_y0
	adc shl_sy
	sta shl_y0
	lda shl_y0+1
	adc shl_sy+1
	sta shl_y0+1
shp_sl_sky:
	jmp shp_sl_loop



; ---------------------------------------------------------------------
; shape_rrect / shape_frrect -- rounded rectangle (X16_USE_SHAPES_RRECT)
; ---------------------------------------------------------------------
; A rectangle with quarter-circle corners. Self-contained: the corners
; come from a midpoint circle walk (no trig, no MATH dependency), the
; straight runs from shp_do_hline / shp_do_pset.
;
;   in: rr_x/rr_y = top-left corner (signed words),
;       rr_w/rr_h = width / height (words, >= 1),
;       rr_r      = corner radius (0-255, clamped to min(w,h)/2),
;       A         = colour
;
; shape_rrect draws the outline through shp_do_pset (so it clips like
; shape_circle); shape_frrect fills it with shp_do_hline spans (so it does
; NOT clip -- keep it on screen, like shape_disc). r = 0 degenerates to
; a plain rectangle.
;
; The fill precomputes rr_ext[d] = the corner's horizontal half-extent at
; vertical offset d (0..r) once, then draws one span per row: full width
; in the straight middle band, inset by rr_ext[d] through the rounded
; top and bottom bands.
; ---------------------------------------------------------------------

shape_rrect:
	sta rr_col
	stz rr_fl
	jmp shp_rr_begin
shape_frrect:
	sta rr_col
	lda #1
	sta rr_fl
shp_rr_begin:
	lda rr_x                    ; corner reference points:
	sta rr_x0                   ;   x0 = x, x1 = x + w - 1
	lda rr_x+1
	sta rr_x0+1
	clc
	lda rr_x
	adc rr_w
	sta rr_x1
	lda rr_x+1
	adc rr_w+1
	sta rr_x1+1
	lda rr_x1                   ; x1 -= 1
	bne shpv_1
	dec rr_x1+1
shpv_1:	dec rr_x1
	lda rr_y                    ;   y0 = y, y1 = y + h - 1
	sta rr_y0
	lda rr_y+1
	sta rr_y0+1
	clc
	lda rr_y
	adc rr_h
	sta rr_y1
	lda rr_y+1
	adc rr_h+1
	sta rr_y1+1
	lda rr_y1                   ; y1 -= 1
	bne shpv_2
	dec rr_y1+1
shpv_2:	dec rr_y1

	jsr shp_rr_clampr              ; rr_r = min(rr_r, min(w,h)/2)
	lda rr_x0                   ; corner centres:
	clc                         ;   cxl = x0 + r, cxr = x1 - r
	adc rr_r
	sta rr_cxl
	lda rr_x0+1
	adc #0
	sta rr_cxl+1
	sec
	lda rr_x1
	sbc rr_r
	sta rr_cxr
	lda rr_x1+1
	sbc #0
	sta rr_cxr+1
	lda rr_y0                   ;   cyt = y0 + r, cyb = y1 - r
	clc
	adc rr_r
	sta rr_cyt
	lda rr_y0+1
	adc #0
	sta rr_cyt+1
	sec
	lda rr_y1
	sbc rr_r
	sta rr_cyb
	lda rr_y1+1
	sbc #0
	sta rr_cyb+1

	lda rr_fl
	beq shp_rr_out
	jmp shp_rr_fill
shp_rr_out:
	jmp shp_rr_outline

; rr_r = min(rr_r, min(rr_w, rr_h) / 2)
shp_rr_clampr:
	lda rr_w                    ; m = min(w, h)  (16-bit unsigned)
	sta rr_m
	lda rr_w+1
	sta rr_m+1
	lda rr_h+1
	cmp rr_m+1
	bcc shp_rr_cmh
	bne shp_rr_cmok
	lda rr_h
	cmp rr_m
	bcs shp_rr_cmok
shp_rr_cmh:
	lda rr_h
	sta rr_m
	lda rr_h+1
	sta rr_m+1
shp_rr_cmok:
	lsr rr_m+1                  ; m /= 2
	ror rr_m
	lda rr_m+1                  ; m >= 256 ? radius already fits any byte
	bne shp_rr_crok
	lda rr_r                    ; r > m ? clamp to m
	cmp rr_m
	bcc shp_rr_crok
	lda rr_m
	sta rr_r
shp_rr_crok:
	rts

; --- outline ---------------------------------------------------------
shp_rr_outline:
	jsr shp_rr_corners             ; the four quarter-circle corners
	; top edge: (cxl, y0) .. (cxr, y0)
	lda rr_cxl
	sta X16_P0
	lda rr_cxl+1
	sta X16_P1
	lda rr_y0
	sta X16_P2
	lda rr_y0+1
	sta X16_P3
	jsr shp_rr_hspan               ; pset run from P0 to cxr on row P2/P3
	; bottom edge: (cxl, y1) .. (cxr, y1)
	lda rr_cxl
	sta X16_P0
	lda rr_cxl+1
	sta X16_P1
	lda rr_y1
	sta X16_P2
	lda rr_y1+1
	sta X16_P3
	jsr shp_rr_hspan
	; left edge: column x0, rows cyt..cyb
	lda rr_x0
	sta X16_P0
	lda rr_x0+1
	sta X16_P1
	jsr shp_rr_vspan
	; right edge: column x1, rows cyt..cyb
	lda rr_x1
	sta X16_P0
	lda rr_x1+1
	sta X16_P1
	jsr shp_rr_vspan
	rts

; pset a horizontal run from (P0/P1) to x=rr_cxr on the row in P2/P3
shp_rr_hspan:
	sec                         ; empty run when cxr < cxl (r reaches w/2):
	lda rr_cxr                  ; the rounded ends meet, no straight top/bottom
	sbc rr_cxl
	lda rr_cxr+1
	sbc rr_cxl+1
	bvc shpv_3
	eor #$80
shpv_3:	bmi shp_rr_hsd
	lda X16_P2                  ; hold the row (pset reloads P0..P3)
	sta rr_ry
	lda X16_P3
	sta rr_ry+1
shp_rr_hsl:
	lda rr_ry
	sta X16_P2
	lda rr_ry+1
	sta X16_P3
	lda rr_col
	jsr shp_do_pset
	lda X16_P0                  ; at cxr ?
	cmp rr_cxr
	bne shp_rr_hsn
	lda X16_P1
	cmp rr_cxr+1
	beq shp_rr_hsd
shp_rr_hsn:
	inc X16_P0
	bne shp_rr_hsl
	inc X16_P1
	bra shp_rr_hsl
shp_rr_hsd:
	rts

; pset a vertical run on column (P0/P1) from y=rr_cyt to y=rr_cyb
shp_rr_vspan:
	sec                         ; empty run when cyb < cyt (r reaches h/2):
	lda rr_cyb                  ; the rounded ends meet, no straight sides
	sbc rr_cyt
	lda rr_cyb+1
	sbc rr_cyt+1
	bvc shpv_4
	eor #$80
shpv_4:	bmi shp_rr_vsd
	lda X16_P0
	sta rr_rx
	lda X16_P1
	sta rr_rx+1
	lda rr_cyt
	sta X16_P2
	lda rr_cyt+1
	sta X16_P3
shp_rr_vsl:
	lda rr_rx
	sta X16_P0
	lda rr_rx+1
	sta X16_P1
	lda rr_col
	jsr shp_do_pset
	lda X16_P2                  ; at cyb ?
	cmp rr_cyb
	bne shp_rr_vsn
	lda X16_P3
	cmp rr_cyb+1
	beq shp_rr_vsd
shp_rr_vsn:
	inc X16_P2
	bne shp_rr_vsl
	inc X16_P3
	bra shp_rr_vsl
shp_rr_vsd:
	rts

; walk the quarter circle once; each octant point plots at all 4 corners
shp_rr_corners:
	lda rr_r                    ; x = r, y = 0, err = 1 - r
	sta rr_wx
	stz rr_wy
	sec
	lda #1
	sbc rr_r
	sta rr_werr
	lda #0
	sbc #0
	sta rr_werr+1
shp_rr_cwl:
	lda rr_wy                   ; while y <= x
	cmp rr_wx
	beq shp_rr_cwp
	bcs shp_rr_cwd
shp_rr_cwp:
	lda rr_wx                   ; plot (a,b) = (x,y) and (y,x) at 4 corners
	sta rr_ca
	lda rr_wy
	sta rr_cb
	jsr shp_rr_c4
	lda rr_wy
	sta rr_ca
	lda rr_wx
	sta rr_cb
	jsr shp_rr_c4
	jsr shp_rr_wstep
	bra shp_rr_cwl
shp_rr_cwd:
	rts

; plot (a,b) offsets at the four corner centres
shp_rr_c4:
	sec                         ; TL: (cxl - a, cyt - b)
	lda rr_cxl
	sbc rr_ca
	sta X16_P0
	lda rr_cxl+1
	sbc #0
	sta X16_P1
	sec
	lda rr_cyt
	sbc rr_cb
	sta X16_P2
	lda rr_cyt+1
	sbc #0
	sta X16_P3
	lda rr_col
	jsr shp_do_pset
	clc                         ; TR: (cxr + a, cyt - b)
	lda rr_cxr
	adc rr_ca
	sta X16_P0
	lda rr_cxr+1
	adc #0
	sta X16_P1
	sec
	lda rr_cyt
	sbc rr_cb
	sta X16_P2
	lda rr_cyt+1
	sbc #0
	sta X16_P3
	lda rr_col
	jsr shp_do_pset
	sec                         ; BL: (cxl - a, cyb + b)
	lda rr_cxl
	sbc rr_ca
	sta X16_P0
	lda rr_cxl+1
	sbc #0
	sta X16_P1
	clc
	lda rr_cyb
	adc rr_cb
	sta X16_P2
	lda rr_cyb+1
	adc #0
	sta X16_P3
	lda rr_col
	jsr shp_do_pset
	clc                         ; BR: (cxr + a, cyb + b)
	lda rr_cxr
	adc rr_ca
	sta X16_P0
	lda rr_cxr+1
	adc #0
	sta X16_P1
	clc
	lda rr_cyb
	adc rr_cb
	sta X16_P2
	lda rr_cyb+1
	adc #0
	sta X16_P3
	lda rr_col
	jmp shp_do_pset

; midpoint error walk shared by shp_rr_corners and the fill's table build
shp_rr_wstep:
	inc rr_wy
	bit rr_werr+1
	bmi shp_rr_wgrow
	dec rr_wx
	sec                         ; t = y - x
	lda rr_wy
	sbc rr_wx
	sta rr_wt
	lda #0
	sbc #0
	sta rr_wt+1
	bra shp_rr_wap
shp_rr_wgrow:
	lda rr_wy                   ; t = y
	sta rr_wt
	lda #0
	sta rr_wt+1
shp_rr_wap:
	asl rr_wt                   ; err += 2t + 1
	rol rr_wt+1
	inc rr_wt
	bne shpv_5
	inc rr_wt+1
shpv_5:	clc
	lda rr_werr
	adc rr_wt
	sta rr_werr
	lda rr_werr+1
	adc rr_wt+1
	sta rr_werr+1
	rts

; --- fill ------------------------------------------------------------
shp_rr_fill:
	jsr shp_rr_build               ; rr_ext[0..r] = corner half-extents
	lda rr_y0                   ; row = y0
	sta rr_ry
	lda rr_y0+1
	sta rr_ry+1
shp_rr_fl:
	lda rr_y1                   ; row > y1 ? done
	cmp rr_ry
	lda rr_y1+1
	sbc rr_ry+1
	bvc shpv_6
	eor #$80
shpv_6:	bmi shp_rr_fld
	jsr shp_rr_row
	inc rr_ry
	bne shp_rr_fl
	inc rr_ry+1
	bra shp_rr_fl
shp_rr_fld:
	rts

; draw the one span for row rr_ry: full width in the middle band, inset
; by rr_ext[d] in the rounded top/bottom bands
shp_rr_row:
	lda rr_ry                   ; row < cyt ?  top rounded band, d = cyt-row
	cmp rr_cyt
	lda rr_ry+1
	sbc rr_cyt+1
	bvc shpv_7
	eor #$80
shpv_7:	bmi shp_rr_rtop
	lda rr_cyb                  ; row > cyb ?  bottom band, d = row-cyb
	cmp rr_ry
	lda rr_cyb+1
	sbc rr_ry+1
	bvc shpv_8
	eor #$80
shpv_8:	bmi shp_rr_rbot
	ldx #0                      ; middle band: d = 0, ext[0] = r -> full width
	beq shp_rr_inset               ; (always: ldx #0 set Z)
shp_rr_rtop:
	sec                         ; d = cyt - row (1..r)
	lda rr_cyt
	sbc rr_ry
	tax
	bra shp_rr_inset
shp_rr_rbot:
	sec                         ; d = row - cyb (1..r)
	lda rr_ry
	sbc rr_cyb
	tax
shp_rr_inset:
	lda rr_ext,x                ; ins = rr_ext[d]
	sta rr_ins
	stz rr_ins+1
	sec                         ; P0 = left = cxl - ins
	lda rr_cxl
	sbc rr_ins
	sta X16_P0
	lda rr_cxl+1
	sbc #0
	sta X16_P1
	lda rr_ry                   ; row
	sta X16_P2
	lda rr_ry+1
	sta X16_P3
	clc                         ; right = cxr + ins  -> T0
	lda rr_cxr
	adc rr_ins
	sta X16_T0
	lda rr_cxr+1
	adc rr_ins+1
	sta X16_T0+1
	sec                         ; len = right - left + 1
	lda X16_T0
	sbc X16_P0
	sta X16_P4
	lda X16_T0+1
	sbc X16_P1
	sta X16_P5
	inc X16_P4
	bne shpv_9
	inc X16_P5
shpv_9:	lda rr_col
	jmp shp_do_hline

; rr_ext[d] = corner half-extent at vertical offset d, for d = 0..r
shp_rr_build:
	ldx #0                      ; zero rr_ext[0..255]
	lda #0
shp_rr_bz:
	sta rr_ext,x
	inx
	bne shp_rr_bz
	lda rr_r                    ; ext[0] = r
	sta rr_ext
	lda rr_r                    ; walk the quarter circle
	sta rr_wx
	stz rr_wy
	sec
	lda #1
	sbc rr_r
	sta rr_werr
	lda #0
	sbc #0
	sta rr_werr+1
shp_rr_bwl:
	lda rr_wy                   ; while y <= x
	cmp rr_wx
	beq shp_rr_bwp
	bcs shp_rr_bwd
shp_rr_bwp:
	ldx rr_wy                   ; ext[y] = max(ext[y], x)
	lda rr_wx
	cmp rr_ext,x
	bcc shpv_10
	sta rr_ext,x
shpv_10:	ldx rr_wx                   ; ext[x] = max(ext[x], y)
	lda rr_wy
	cmp rr_ext,x
	bcc shpv_11
	sta rr_ext,x
shpv_11:	jsr shp_rr_wstep
	bra shp_rr_bwl
shp_rr_bwd:
	rts

; --- rounded-rect state ----------------------------------------------


; ---------------------------------------------------------------------
; shape_arc -- a portion of a circle outline (X16_USE_SHAPES_ARC)
; ---------------------------------------------------------------------
; The arc runs from byte-angle `start` to `end`, increasing (0 = east,
; 64 = south, 128 = west, 192 = north -- the sin8/cos8, screen-y-down
; convention shared with the polygon). It is sampled every ~4 byte-angle
; units and the samples are joined with shp_line, so the chord error is
; under a third of a pixel even at r = 255 and the arc clips wherever
; shp_do_pset clips. When start == end the whole circle is drawn.
;
;   in: P0/P1 = cx, P2/P3 = cy, P4 = r (0-255),
;       P5 = start angle, P6 = end angle, A = colour
;
; shp_arc_point / shp_arc_scale place a sample the same way the polygon places
; a vertex (r * cos8/sin8 / 128, rounded); shape_pie reuses them, which
; is why they live in this gate and PIE depends on ARC.
; ---------------------------------------------------------------------

ARC_STEP = 4                    ; byte-angle units between samples

shape_arc:
	sta shl_col                 ; shp_line draws in this colour
	lda X16_P0
	sta arc_cx
	lda X16_P1
	sta arc_cx+1
	lda X16_P2
	sta arc_cy
	lda X16_P3
	sta arc_cy+1
	lda X16_P4
	sta arc_r
	lda X16_P5
	sta arc_a0
	sec                         ; span = (end - start) & 255; 0 -> 256
	lda X16_P6
	sbc arc_a0
	sta arc_span
	stz arc_span+1
	lda arc_span
	bne shp_ar_have
	inc arc_span+1
shp_ar_have:
	lda arc_a0                  ; first sample -> shl_x0/y0 (prev point)
	jsr shp_arc_point
	lda arc_px
	sta shl_x0
	lda arc_px+1
	sta shl_x0+1
	lda arc_py
	sta shl_y0
	lda arc_py+1
	sta shl_y0+1
	lda arc_a0
	sta arc_ang
shp_ar_loop:
	lda arc_span+1              ; step = min(ARC_STEP, span)
	bne shp_ar_full
	lda arc_span
	cmp #ARC_STEP
	bcc shp_ar_last
shp_ar_full:
	lda #ARC_STEP
	sta arc_step
	bra shp_ar_adv
shp_ar_last:
	lda arc_span
	sta arc_step
shp_ar_adv:
	clc                         ; ang = (ang + step) mod 256
	lda arc_ang
	adc arc_step
	sta arc_ang
	sec                         ; span -= step
	lda arc_span
	sbc arc_step
	sta arc_span
	lda arc_span+1
	sbc #0
	sta arc_span+1
	lda arc_ang                 ; this sample -> shl_x1/y1
	jsr shp_arc_point
	lda arc_px
	sta shl_x1
	lda arc_px+1
	sta shl_x1+1
	lda arc_py
	sta shl_y1
	lda arc_py+1
	sta shl_y1+1
	jsr shp_line
	lda shl_x1                  ; cur -> prev for the next segment
	sta shl_x0
	lda shl_x1+1
	sta shl_x0+1
	lda shl_y1
	sta shl_y0
	lda shl_y1+1
	sta shl_y0+1
	lda arc_span                ; span exhausted ? done
	ora arc_span+1
	bne shp_ar_loop
	rts

; sample at byte-angle A -> (arc_px, arc_py)
shp_arc_point:
	pha
	jsr cos8                    ; A = cos * 127 (signed)
	jsr shp_arc_scale              ; arc_off = round(r * A / 128)
	clc
	lda arc_cx
	adc arc_off
	sta arc_px
	lda arc_cx+1
	adc arc_off+1
	sta arc_px+1
	pla
	jsr sin8                    ; A = sin * 127 (signed)
	jsr shp_arc_scale
	clc
	lda arc_cy
	adc arc_off
	sta arc_py
	lda arc_cy+1
	adc arc_off+1
	sta arc_py+1
	rts

; arc_off = round(arc_r * |A| / 128) with A's sign (A a signed byte)
shp_arc_scale:
	stz arc_sgn
	pha
	and #$80
	beq shp_as_pos
	inc arc_sgn
	pla
	eor #$FF
	clc
	adc #1
	bra shp_as_mul
shp_as_pos:
	pla
shp_as_mul:
	jsr shp_arc_mul8               ; arc_p16 = arc_r * |A|
	clc
	lda arc_p16                 ; + 0.5 LSB so >>7 rounds
	adc #64
	sta arc_p16
	lda arc_p16+1
	adc #0
	sta arc_p16+1
	lda arc_p16                 ; >>7 (product < 32768, one byte out)
	asl
	lda arc_p16+1
	rol
	sta arc_off
	stz arc_off+1
	lda arc_sgn
	beq shp_as_done
	sec                         ; negate
	lda #0
	sbc arc_off
	sta arc_off
	lda #0
	sbc arc_off+1
	sta arc_off+1
shp_as_done:
	rts

; arc_p16 = arc_r * A  (8x8 -> 16, unsigned)
shp_arc_mul8:
	sta arc_t
	lda #0
	ldx #8
shp_am_loop:
	lsr arc_t
	bcc shp_am_skip
	clc
	adc arc_r
shp_am_skip:
	ror
	ror arc_p16
	dex
	bne shp_am_loop
	sta arc_p16+1
	rts

; --- arc state (shared with shape_pie) -------------------------------


; ---------------------------------------------------------------------
; shape_pie -- a filled wedge from the centre to the arc (X16_USE_SHAPES_PIE)
; ---------------------------------------------------------------------
; Same arguments and angle convention as shape_arc; the region swept
; between the two radii and the arc is filled. It is built as a fan of
; thin triangles (centre, sample_i, sample_i+1) so ANY span works,
; including the reflex (> 180-degree) case a single convex scan cannot
; do; start == end fills the whole disc. The triangles share their radial
; edges, so like shape_disc it draws with shp_do_hline (no clipping) and its
; overdraw on the shared edges is harmless. It reuses ARC's shp_arc_point.
;
;   in: P0/P1 = cx, P2/P3 = cy, P4 = r (0-255),
;       P5 = start angle, P6 = end angle, A = colour
; ---------------------------------------------------------------------

shape_pie:
	sta pie_col
	lda X16_P0
	sta arc_cx
	lda X16_P1
	sta arc_cx+1
	lda X16_P2
	sta arc_cy
	lda X16_P3
	sta arc_cy+1
	lda X16_P4
	sta arc_r
	lda X16_P5
	sta arc_a0
	sec                         ; span = (end - start) & 255; 0 -> 256
	lda X16_P6
	sbc arc_a0
	sta arc_span
	stz arc_span+1
	lda arc_span
	bne shp_pie_have
	inc arc_span+1
shp_pie_have:
	lda arc_a0                  ; prev = sample(start)
	jsr shp_arc_point
	lda arc_px
	sta pie_prevx
	lda arc_px+1
	sta pie_prevx+1
	lda arc_py
	sta pie_prevy
	lda arc_py+1
	sta pie_prevy+1
	lda arc_a0
	sta arc_ang
shp_pie_loop:
	lda arc_span+1              ; step = min(ARC_STEP, span)
	bne shp_pie_full
	lda arc_span
	cmp #ARC_STEP
	bcc shp_pie_last
shp_pie_full:
	lda #ARC_STEP
	sta arc_step
	bra shp_pie_adv
shp_pie_last:
	lda arc_span
	sta arc_step
shp_pie_adv:
	clc
	lda arc_ang
	adc arc_step
	sta arc_ang
	sec
	lda arc_span
	sbc arc_step
	sta arc_span
	lda arc_span+1
	sbc #0
	sta arc_span+1
	lda arc_ang                 ; cur = sample(ang)
	jsr shp_arc_point
	lda arc_cx                  ; triangle A = centre
	sta tf_ax
	lda arc_cx+1
	sta tf_ax+1
	lda arc_cy
	sta tf_ay
	lda arc_cy+1
	sta tf_ay+1
	lda pie_prevx               ; B = prev sample
	sta tf_bx
	lda pie_prevx+1
	sta tf_bx+1
	lda pie_prevy
	sta tf_by
	lda pie_prevy+1
	sta tf_by+1
	lda arc_px                  ; C = cur sample
	sta tf_cx
	lda arc_px+1
	sta tf_cx+1
	lda arc_py
	sta tf_cy
	lda arc_py+1
	sta tf_cy+1
	jsr shp_tf_fill
	lda arc_px                  ; prev = cur
	sta pie_prevx
	lda arc_px+1
	sta pie_prevx+1
	lda arc_py
	sta pie_prevy
	lda arc_py+1
	sta pie_prevy+1
	lda arc_span                ; span exhausted ? done
	ora arc_span+1
	beq shp_pie_done
	jmp shp_pie_loop
shp_pie_done:
	rts

; --- triangle scanline fill (fan primitive) --------------------------
; Fills triangle (tf_ax/ay, tf_bx/by, tf_cx/cy) in pie_col with shp_do_hline
; spans. Sorts the vertices by y, then walks the long edge and the two
; short edges by scanline with a division-free DDA (err += |dx|; while
; err >= dy: x += sign, err -= dy). A zero-height triangle has no area
; and is skipped. Edge state is two-wide: index 0 = long, 2 = short.
shp_tf_fill:
	jsr shp_tf_sort                ; ay <= by <= cy
	lda tf_ay                   ; ay == cy ? zero height, nothing to fill
	cmp tf_cy
	bne shp_tf_go
	lda tf_ay+1
	cmp tf_cy+1
	bne shp_tf_go
	rts
shp_tf_go:
	lda tf_ax                   ; long edge a -> c  (index 0)
	sta tf_isx
	lda tf_ax+1
	sta tf_isx+1
	lda tf_ay
	sta tf_isy
	lda tf_ay+1
	sta tf_isy+1
	lda tf_cx
	sta tf_iex
	lda tf_cx+1
	sta tf_iex+1
	lda tf_cy
	sta tf_iey
	lda tf_cy+1
	sta tf_iey+1
	ldx #0
	jsr shp_tf_init
	lda tf_ay                   ; y = ay
	sta tf_y
	lda tf_ay+1
	sta tf_y+1
	sec                         ; phase 1 only if ay < by
	lda tf_ay
	sbc tf_by
	lda tf_ay+1
	sbc tf_by+1
	bvc shpv_12
	eor #$80
shpv_12:	bpl shp_tf_p2init              ; ay >= by (flat top): skip to phase 2
	lda tf_ax                   ; short edge a -> b  (index 2)
	sta tf_isx
	lda tf_ax+1
	sta tf_isx+1
	lda tf_ay
	sta tf_isy
	lda tf_ay+1
	sta tf_isy+1
	lda tf_bx
	sta tf_iex
	lda tf_bx+1
	sta tf_iex+1
	lda tf_by
	sta tf_iey
	lda tf_by+1
	sta tf_iey+1
	ldx #2
	jsr shp_tf_init
shp_tf_p1loop:
	sec                         ; y >= by ? phase 1 done
	lda tf_y
	sbc tf_by
	lda tf_y+1
	sbc tf_by+1
	bvc shpv_13
	eor #$80
shpv_13:	bmi shp_tf_p1do
	jmp shp_tf_p2init
shp_tf_p1do:
	jsr shp_tf_emitrow
	ldx #0
	jsr shp_tf_adv
	ldx #2
	jsr shp_tf_adv
	inc tf_y
	bne shpv_14
	inc tf_y+1
shpv_14:	jmp shp_tf_p1loop
shp_tf_p2init:
	lda tf_bx                   ; short edge b -> c  (index 2)
	sta tf_isx
	lda tf_bx+1
	sta tf_isx+1
	lda tf_by
	sta tf_isy
	lda tf_by+1
	sta tf_isy+1
	lda tf_cx
	sta tf_iex
	lda tf_cx+1
	sta tf_iex+1
	lda tf_cy
	sta tf_iey
	lda tf_cy+1
	sta tf_iey+1
	ldx #2
	jsr shp_tf_init
shp_tf_p2loop:
	jsr shp_tf_emitrow
	lda tf_y                    ; y == cy ? done (last row)
	cmp tf_cy
	bne shp_tf_p2do
	lda tf_y+1
	cmp tf_cy+1
	bne shp_tf_p2do
	rts
shp_tf_p2do:
	ldx #0
	jsr shp_tf_adv
	ldx #2
	jsr shp_tf_adv
	inc tf_y
	bne shpv_15
	inc tf_y+1
shpv_15:	jmp shp_tf_p2loop

; sort tf_a/tf_b/tf_c by y ascending (each slot is x.w then y.w)
shp_tf_sort:
	jsr shp_tf_cmp_ab
	bpl shpv_16
	jsr shp_tf_swap_ab
shpv_16:	jsr shp_tf_cmp_bc
	bpl shpv_17
	jsr shp_tf_swap_bc
shpv_17:	jsr shp_tf_cmp_ab
	bpl shpv_18
	jsr shp_tf_swap_ab
shpv_18:	rts
shp_tf_cmp_ab:
	sec
	lda tf_by
	sbc tf_ay
	lda tf_by+1
	sbc tf_ay+1
	bvc shpv_19
	eor #$80
shpv_19:	rts
shp_tf_cmp_bc:
	sec
	lda tf_cy
	sbc tf_by
	lda tf_cy+1
	sbc tf_by+1
	bvc shpv_20
	eor #$80
shpv_20:	rts
shp_tf_swap_ab:
	ldx #3
shp_tsab:
	lda tf_ax,x
	ldy tf_bx,x
	sta tf_bx,x
	tya
	sta tf_ax,x
	dex
	bpl shp_tsab
	rts
shp_tf_swap_bc:
	ldx #3
shp_tsbc:
	lda tf_bx,x
	ldy tf_cx,x
	sta tf_cx,x
	tya
	sta tf_bx,x
	dex
	bpl shp_tsbc
	rts

; init edge X (0 long / 2 short) from (tf_isx,tf_isy) to (tf_iex,tf_iey)
shp_tf_init:
	lda tf_isx
	sta e_curx,x
	lda tf_isx+1
	sta e_curx+1,x
	sec                         ; dy = iey - isy  (>= 0)
	lda tf_iey
	sbc tf_isy
	sta e_dy,x
	lda tf_iey+1
	sbc tf_isy+1
	sta e_dy+1,x
	sec                         ; dx = iex - isx  (signed)
	lda tf_iex
	sbc tf_isx
	sta tf_edx
	lda tf_iex+1
	sbc tf_isx+1
	sta tf_edx+1
	bpl shp_ti_pos
	sec                         ; adx = -dx, sx = -1
	lda #0
	sbc tf_edx
	sta e_adx,x
	lda #0
	sbc tf_edx+1
	sta e_adx+1,x
	lda #$FF
	sta e_sx,x
	sta e_sx+1,x
	bra shp_ti_err
shp_ti_pos:
	lda tf_edx                  ; adx = dx, sx = +1
	sta e_adx,x
	lda tf_edx+1
	sta e_adx+1,x
	lda #1
	sta e_sx,x
	stz e_sx+1,x
shp_ti_err:
	stz e_err,x
	stz e_err+1,x
	rts

; advance edge X by one scanline (dy for this edge must be > 0)
shp_tf_adv:
	clc                         ; err += adx
	lda e_err,x
	adc e_adx,x
	sta e_err,x
	lda e_err+1,x
	adc e_adx+1,x
	sta e_err+1,x
shp_ta_w:
	sec                         ; err >= dy ?
	lda e_err,x
	sbc e_dy,x
	tay
	lda e_err+1,x
	sbc e_dy+1,x
	bcc shp_ta_done                ; err < dy
	sta e_err+1,x               ; err -= dy
	tya
	sta e_err,x
	clc                         ; x += sx
	lda e_curx,x
	adc e_sx,x
	sta e_curx,x
	lda e_curx+1,x
	adc e_sx+1,x
	sta e_curx+1,x
	bra shp_ta_w
shp_ta_done:
	rts

; HLINE on row tf_y between the long (index 0) and short (index 2) x's
shp_tf_emitrow:
	sec                         ; diff = short_x - long_x
	lda e_curx+2
	sbc e_curx
	sta tf_tmp
	lda e_curx+3
	sbc e_curx+1
	sta tf_tmp+1
	bpl shp_te_pos                 ; short >= long: left = long, len = diff+1
	lda e_curx+2                ; short < long: left = short, len = -diff+1
	sta X16_P0
	lda e_curx+3
	sta X16_P1
	sec
	lda #0
	sbc tf_tmp
	sta X16_P4
	lda #0
	sbc tf_tmp+1
	sta X16_P5
	bra shp_te_len
shp_te_pos:
	lda e_curx
	sta X16_P0
	lda e_curx+1
	sta X16_P1
	lda tf_tmp
	sta X16_P4
	lda tf_tmp+1
	sta X16_P5
shp_te_len:
	inc X16_P4                  ; len = |diff| + 1
	bne shpv_21
	inc X16_P5
shpv_21:	lda tf_y
	sta X16_P2
	lda tf_y+1
	sta X16_P3
	lda pie_col
	jsr shp_do_hline
	rts

; --- pie / triangle-fill state ---------------------------------------


; ---------------------------------------------------------------------
; shape_bezier -- cubic Bezier curve (X16_USE_SHAPES_BEZIER)
; ---------------------------------------------------------------------
; The curve through four control points P0 (on the curve), P1, P2
; (handles), P3 (on the curve), by de Casteljau at a handful of t and
; shp_line between the samples. The sample count adapts to the control
; polygon's size (its Manhattan perimeter / 8, clamped to 4..64), so a
; small curve is cheap and a large one stays smooth. Clips wherever
; shp_do_pset clips.
;
;   in: bez_x0/bez_y0 .. bez_x3/bez_y3 = the four control points
;       (signed words, set by the caller), A = colour
;
; t is an 8-bit fraction (0..255); the endpoints P0 and P3 are emitted
; exactly rather than evaluated, so the curve meets its anchors.
; ---------------------------------------------------------------------

shape_bezier:
	sta shl_col
	jsr shp_bz_nseg                ; bez_n = clamp(perimeter/8, 4, 64)
	lda bez_x0                  ; prev = P0 (emitted exactly)
	sta shl_x0
	lda bez_x0+1
	sta shl_x0+1
	lda bez_y0
	sta shl_y0
	lda bez_y0+1
	sta shl_y0+1
	lda #1
	sta bez_i
	stz bez_tb
	stz bez_rem
	stz bez_rem+1
shp_bz_loop:
	lda bez_i                   ; i == n ? last segment goes to P3
	cmp bez_n
	beq shp_bz_last
	inc bez_rem+1               ; rem += 256; while rem >= n: tb++, rem -= n
shp_bz_tw:
	lda bez_rem+1
	bne shp_bz_tsub
	lda bez_rem
	cmp bez_n
	bcc shp_bz_tdone
shp_bz_tsub:
	sec
	lda bez_rem
	sbc bez_n
	sta bez_rem
	lda bez_rem+1
	sbc #0
	sta bez_rem+1
	inc bez_tb
	bra shp_bz_tw
shp_bz_tdone:
	jsr shp_bz_eval                ; (bez_rx, bez_ry) = B(tb)
	lda bez_rx
	sta shl_x1
	lda bez_rx+1
	sta shl_x1+1
	lda bez_ry
	sta shl_y1
	lda bez_ry+1
	sta shl_y1+1
	jsr shp_line
	lda shl_x1                  ; cur -> prev
	sta shl_x0
	lda shl_x1+1
	sta shl_x0+1
	lda shl_y1
	sta shl_y0
	lda shl_y1+1
	sta shl_y0+1
	inc bez_i
	jmp shp_bz_loop
shp_bz_last:
	lda bez_x3                  ; final sample = P3, exact
	sta shl_x1
	lda bez_x3+1
	sta shl_x1+1
	lda bez_y3
	sta shl_y1
	lda bez_y3+1
	sta shl_y1+1
	jmp shp_line

; bez_n = clamp(Manhattan perimeter of the control polygon / 8, 4, 64)
shp_bz_nseg:
	stz bez_per
	stz bez_per+1
	ldx #0                      ; X = 4*k over the three control segments
shp_bn_loop:
	sec                         ; dx = pts[k+1]shp_x - pts[k]shp_x
	lda bez_x0+4,x
	sbc bez_x0,x
	sta bez_tmp
	lda bez_x0+5,x
	sbc bez_x0+1,x
	sta bez_tmp+1
	jsr shp_bz_absacc
	sec                         ; dy = pts[k+1]shp_y - pts[k]shp_y
	lda bez_x0+6,x
	sbc bez_x0+2,x
	sta bez_tmp
	lda bez_x0+7,x
	sbc bez_x0+3,x
	sta bez_tmp+1
	jsr shp_bz_absacc
	inx
	inx
	inx
	inx
	cpx #12
	bne shp_bn_loop
	ldx #3                      ; per >>= 3
shp_bn_sh:
	lsr bez_per+1
	ror bez_per
	dex
	bne shp_bn_sh
	lda bez_per+1               ; clamp high -> 64
	bne shp_bn_hi
	lda bez_per
	cmp #64
	bcs shp_bn_hi
	cmp #4
	bcs shp_bn_ok                  ; 4..63
	lda #4
shp_bn_ok:
	sta bez_n
	rts
shp_bn_hi:
	lda #64
	sta bez_n
	rts

; bez_per += |bez_tmp|  (signed word magnitude)
shp_bz_absacc:
	lda bez_tmp+1
	bpl shp_ba_pos
	sec
	lda #0
	sbc bez_tmp
	sta bez_tmp
	lda #0
	sbc bez_tmp+1
	sta bez_tmp+1
shp_ba_pos:
	clc
	lda bez_per
	adc bez_tmp
	sta bez_per
	lda bez_per+1
	adc bez_tmp+1
	sta bez_per+1
	rts

; (bez_rx, bez_ry) = cubic B(bez_tb) by de Casteljau
shp_bz_eval:
	ldx #0                      ; copy control points into the work arrays
	ldy #0
shp_be_cp:
	lda bez_x0,y
	sta bez_wx,x
	lda bez_x0+1,y
	sta bez_wx+1,x
	lda bez_x0+2,y
	sta bez_wy,x
	lda bez_x0+3,y
	sta bez_wy+1,x
	inx
	inx
	tya
	clc
	adc #4
	tay
	cpx #8
	bne shp_be_cp
	lda #3
	sta bez_cnt
shp_be_lvl:
	lda bez_cnt                 ; inner loop j = 0 .. cnt-1  (index j*2)
	asl
	sta bez_lim
	stz bez_jx
shp_be_jx:
	ldx bez_jx                  ; wx[j] = lerp(wx[j], wx[j+1], t)
	lda bez_wx,x
	sta bez_p
	lda bez_wx+1,x
	sta bez_p+1
	lda bez_wx+2,x
	sta bez_q
	lda bez_wx+3,x
	sta bez_q+1
	jsr shp_bz_lerp
	ldx bez_jx
	lda bez_r
	sta bez_wx,x
	lda bez_r+1
	sta bez_wx+1,x
	lda bez_wy,x                ; wy[j] = lerp(wy[j], wy[j+1], t)
	sta bez_p
	lda bez_wy+1,x
	sta bez_p+1
	lda bez_wy+2,x
	sta bez_q
	lda bez_wy+3,x
	sta bez_q+1
	jsr shp_bz_lerp
	ldx bez_jx
	lda bez_r
	sta bez_wy,x
	lda bez_r+1
	sta bez_wy+1,x
	lda bez_jx
	clc
	adc #2
	sta bez_jx
	cmp bez_lim
	bne shp_be_jx
	dec bez_cnt
	bne shp_be_lvl
	lda bez_wx                  ; result = work[0]
	sta bez_rx
	lda bez_wx+1
	sta bez_rx+1
	lda bez_wy
	sta bez_ry
	lda bez_wy+1
	sta bez_ry+1
	rts

; bez_r = bez_p + round((bez_q - bez_p) * bez_tb / 256)   (signed)
shp_bz_lerp:
	sec                         ; d = q - p
	lda bez_q
	sbc bez_p
	sta bez_d
	lda bez_q+1
	sbc bez_p+1
	sta bez_d+1
	stz bez_dsgn
	lda bez_d+1                 ; take |d|, remember the sign
	bpl shp_bl_pos
	inc bez_dsgn
	sec
	lda #0
	sbc bez_d
	sta bez_d
	lda #0
	sbc bez_d+1
	sta bez_d+1
shp_bl_pos:
	jsr shp_bz_mul                 ; bez_prod = |d| * t (24-bit)
	clc                         ; + 128 (round), then take bytes 1..2 (>>8)
	lda bez_prod
	adc #128
	lda bez_prod+1
	adc #0
	sta bez_m
	lda bez_prod+2
	adc #0
	sta bez_m+1
	lda bez_dsgn
	beq shp_bl_add
	sec                         ; re-apply the sign
	lda #0
	sbc bez_m
	sta bez_m
	lda #0
	sbc bez_m+1
	sta bez_m+1
shp_bl_add:
	clc                         ; r = p + m
	lda bez_p
	adc bez_m
	sta bez_r
	lda bez_p+1
	adc bez_m+1
	sta bez_r+1
	rts

; bez_prod (24-bit) = bez_d (16-bit) * bez_tb (8-bit), unsigned
shp_bz_mul:
	stz bez_prod
	stz bez_prod+1
	stz bez_prod+2
	lda bez_tb
	sta bez_mt
	ldx #8
shp_bm_loop:
	asl bez_prod
	rol bez_prod+1
	rol bez_prod+2
	asl bez_mt
	bcc shp_bm_skip
	clc
	lda bez_prod
	adc bez_d
	sta bez_prod
	lda bez_prod+1
	adc bez_d+1
	sta bez_prod+1
	lda bez_prod+2
	adc #0
	sta bez_prod+2
shp_bm_skip:
	dex
	bne shp_bm_loop
	rts

; --- bezier state ----------------------------------------------------



        section bss
poly_col: reserve 1
poly_efl: reserve 1
poly_cx: reserve 2
poly_cy: reserve 2
poly_r: reserve 1
poly_n: reserve 1
poly_i: reserve 1
poly_acc: reserve 2
poly_step: reserve 2
poly_off: reserve 2
poly_sgn: reserve 1
poly_p16: reserve 2
poly_t: reserve 1
poly_dvd: reserve 3
poly_rem: reserve 2
poly_vx: reserve POLY_MAX * 2
poly_vy: reserve POLY_MAX * 2
poly_lx0: reserve 2
poly_ly0: reserve 2
poly_lx1: reserve 2
poly_ly1: reserve 2
poly_ldx: reserve 2
poly_ldy: reserve 2
poly_lerr: reserve 2
poly_le2: reserve 2
poly_lsx: reserve 2
poly_lsy: reserve 2
poly_lt: reserve 2
poly_ymin: reserve 2
poly_ymax: reserve 2
poly_y: reserve 2
poly_found: reserve 1
poly_xa: reserve 2
poly_ya: reserve 2
poly_xb: reserve 2
poly_yb: reserve 2
poly_xtop: reserve 2
poly_ytop: reserve 2
poly_xbot: reserve 2
poly_ybot: reserve 2
poly_x: reserve 2
poly_xl: reserve 2
poly_xr: reserve 2
poly_dxs: reserve 1
poly_md1: reserve 2
poly_md2: reserve 2
poly_md3: reserve 2
poly_mdq: reserve 2
poly_prod: reserve 4
shl_x0: reserve 2
shl_y0: reserve 2
shl_x1: reserve 2
shl_y1: reserve 2
shl_col: reserve 1
shl_dx: reserve 2
shl_dy: reserve 2
shl_sx: reserve 2
shl_sy: reserve 2
shl_err: reserve 2
shl_e2: reserve 2
shl_t: reserve 2
rr_x: reserve 2
rr_y: reserve 2
rr_w: reserve 2
rr_h: reserve 2
rr_r: reserve 1
rr_col: reserve 1
rr_fl: reserve 1
rr_x0: reserve 2
rr_y0: reserve 2
rr_x1: reserve 2
rr_y1: reserve 2
rr_cxl: reserve 2
rr_cxr: reserve 2
rr_cyt: reserve 2
rr_cyb: reserve 2
rr_m: reserve 2
rr_ry: reserve 2
rr_rx: reserve 2
rr_ins: reserve 2
rr_ca: reserve 1
rr_cb: reserve 1
rr_wx: reserve 1
rr_wy: reserve 1
rr_werr: reserve 2
rr_wt: reserve 2
rr_ext: reserve 256
arc_cx: reserve 2
arc_cy: reserve 2
arc_r: reserve 1
arc_a0: reserve 1
arc_ang: reserve 1
arc_step: reserve 1
arc_span: reserve 2
arc_px: reserve 2
arc_py: reserve 2
arc_off: reserve 2
arc_sgn: reserve 1
arc_p16: reserve 2
arc_t: reserve 1
pie_col: reserve 1
pie_prevx: reserve 2
pie_prevy: reserve 2
tf_ax: reserve 2
tf_ay: reserve 2
tf_bx: reserve 2
tf_by: reserve 2
tf_cx: reserve 2
tf_cy: reserve 2
tf_y: reserve 2
tf_isx: reserve 2
tf_isy: reserve 2
tf_iex: reserve 2
tf_iey: reserve 2
tf_edx: reserve 2
tf_tmp: reserve 2
e_curx: reserve 4
e_err: reserve 4
e_adx: reserve 4
e_dy: reserve 4
e_sx: reserve 4
bez_x0: reserve 2
bez_y0: reserve 2
bez_x1: reserve 2
bez_y1: reserve 2
bez_x2: reserve 2
bez_y2: reserve 2
bez_x3: reserve 2
bez_y3: reserve 2
bez_n: reserve 1
bez_i: reserve 1
bez_tb: reserve 1
bez_rem: reserve 2
bez_per: reserve 2
bez_tmp: reserve 2
bez_rx: reserve 2
bez_ry: reserve 2
bez_wx: reserve 8
bez_wy: reserve 8
bez_cnt: reserve 1
bez_lim: reserve 1
bez_jx: reserve 1
bez_p: reserve 2
bez_q: reserve 2
bez_d: reserve 2
bez_dsgn: reserve 1
bez_prod: reserve 3
bez_mt: reserve 1
bez_m: reserve 2
bez_r: reserve 2
