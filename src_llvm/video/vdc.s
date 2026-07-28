; =====================================================================
; x16clib :: video/vdc.s -- VERA display composer helpers
; =====================================================================
; The display composer is the DCSEL=0/1 view of $9F29-$9F2C: output
; mode, layer enables, scaling, border colour, active display window,
; and (at DCSEL=63) the bitstream version registers. This drives VERA
; itself -- not the C128's 8563 chip the VDC name usually means.
;
; Routines leave DCSEL = 0, which is what the rest of the library (and
; cc65's runtime) assumes between calls.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; (import dropped: X16_TPTR0)
; (import dropped: popa, popax)

        .globl  x16_vdc_get_video
        .globl  x16_vdc_set_video
        .globl  x16_vdc_set_output
        .globl  x16_vdc_set_layers
        .globl  x16_vdc_layer_on
        .globl  x16_vdc_layer_off
        .globl  x16_vdc_get_scale
        .globl  x16_vdc_set_scale
        .globl  x16_vdc_get_border
        .globl  x16_vdc_set_border
        .globl  x16_vdc_get_active_raw
        .globl  x16_vdc_set_active_raw
        .globl  x16_vdc_set_active
        .globl  x16_vdc_fullscreen
        .globl  x16_vdc_get_version

VDC_LAYER_MASK = VERA_VIDEO_LAYER0_EN | VERA_VIDEO_LAYER1_EN | VERA_VIDEO_SPRITES_EN

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_vdc_get_video(void)
; void __fastcall__ x16_vdc_set_video(unsigned char video)
;   The raw DC_VIDEO byte. Set ignores bit 7 (FIELD is read-only).
; ---------------------------------------------------------------------
x16_vdc_get_video:
        vera_dcsel 0
        lda     VERA_DC_VIDEO
        ldx     #0
        rts

x16_vdc_set_video:
        pha
        vera_dcsel 0
        pla
        and     #%01111111
        sta     VERA_DC_VIDEO
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_vdc_set_output(unsigned char mode)
;   X16_VDC_OUTPUT_*, preserving the other DC_VIDEO bits.
; ---------------------------------------------------------------------
x16_vdc_set_output:
        and     #%00000011
        sta     mos8(X16_T0)
        vera_dcsel 0
        lda     VERA_DC_VIDEO
        and     #%01111100
        ora     mos8(X16_T0)
        sta     VERA_DC_VIDEO
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_vdc_set_layers(unsigned char mask)
;   Replace the three enable bits with `mask`; the mode and chroma
;   bits survive.
; ---------------------------------------------------------------------
x16_vdc_set_layers:
        and     #VDC_LAYER_MASK
        sta     mos8(X16_T0)
        vera_dcsel 0
        lda     VERA_DC_VIDEO
        and     #%00001111
        ora     mos8(X16_T0)
        sta     VERA_DC_VIDEO
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_vdc_layer_on(unsigned char mask)
; void __fastcall__ x16_vdc_layer_off(unsigned char mask)
; ---------------------------------------------------------------------
x16_vdc_layer_on:
        and     #VDC_LAYER_MASK
        pha
        vera_dcsel 0
        pla
        tsb     VERA_DC_VIDEO
        rts

x16_vdc_layer_off:
        and     #VDC_LAYER_MASK
        pha
        vera_dcsel 0
        pla
        trb     VERA_DC_VIDEO
        rts

; ---------------------------------------------------------------------
; unsigned int x16_vdc_get_scale(void)   -- HSCALE | VSCALE << 8
; void __fastcall__ x16_vdc_set_scale(unsigned char h, unsigned char v)
;   $80 means one output pixel per input pixel.
; ---------------------------------------------------------------------
x16_vdc_get_scale:
        vera_dcsel 0
        lda     VERA_DC_HSCALE
        ldx     VERA_DC_VSCALE
        rts

x16_vdc_set_scale:
        sta     mos8(X16_T0)            ; hscale
        stx     mos8(X16_T1)            ; vscale
        vera_dcsel 0
        lda     mos8(X16_T0)
        sta     VERA_DC_HSCALE
        lda     mos8(X16_T1)
        sta     VERA_DC_VSCALE
        rts

; ---------------------------------------------------------------------
; unsigned char x16_vdc_get_border(void)
; void __fastcall__ x16_vdc_set_border(unsigned char index)
;   The border palette index.
; ---------------------------------------------------------------------
x16_vdc_get_border:
        vera_dcsel 0
        lda     VERA_DC_BORDER
        ldx     #0
        rts

x16_vdc_set_border:
        pha
        vera_dcsel 0
        pla
        sta     VERA_DC_BORDER
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_vdc_get_active_raw(x16_vdc_active *out)
; void __fastcall__ x16_vdc_set_active_raw(const x16_vdc_active *in)
;
; Raw registers are native display coordinates with low bits omitted:
; HSTART/HSTOP = pixel / 4, VSTART/VSTOP = pixel / 2.
; ---------------------------------------------------------------------
x16_vdc_get_active_raw:
        lda     mos8(__rc2)             ; a lone pointer never lands in
        sta     mos8(X16_TPTR0)         ; A/X -- it takes an __rc pair
        lda     mos8(__rc3)
        sta     mos8(X16_TPTR0+1)
        vera_dcsel 1
        lda     VERA_DC_HSTART
        ldy     #0
        sta     (X16_TPTR0),y
        lda     VERA_DC_HSTOP
        iny
        sta     (X16_TPTR0),y
        lda     VERA_DC_VSTART
        iny
        sta     (X16_TPTR0),y
        lda     VERA_DC_VSTOP
        iny
        sta     (X16_TPTR0),y
        vera_dcsel 0
        rts

x16_vdc_set_active_raw:
        lda     mos8(__rc2)             ; a lone pointer never lands in
        sta     mos8(X16_TPTR0)         ; A/X -- it takes an __rc pair
        lda     mos8(__rc3)
        sta     mos8(X16_TPTR0+1)
        ldy     #0
        lda     (X16_TPTR0),y
        sta     mos8(X16_T0)
        iny
        lda     (X16_TPTR0),y
        sta     mos8(X16_T1)
        iny
        lda     (X16_TPTR0),y
        sta     mos8(X16_T2)
        iny
        lda     (X16_TPTR0),y
        sta     mos8(X16_T3)
        jmp     vdc_store_active_t

; ---------------------------------------------------------------------
; void __fastcall__ x16_vdc_set_active(unsigned int hstart,
;                                      unsigned int hstop,
;                                      unsigned int vstart,
;                                      unsigned int vstop)
;   In pixels; converted to composer units (horizontal / 4,
;   vertical / 2).
; ---------------------------------------------------------------------
x16_vdc_set_active:
        pha                             ; A and X carry arguments that
        phx                             ; the loads below clobber
        lda     mos8(__rc2)
        sta     mos8(X16_P2)          ; hstop
        lda     mos8(__rc3)
        sta     mos8(X16_P3)
        lda     mos8(__rc4)
        sta     mos8(X16_P4)          ; vstart
        lda     mos8(__rc5)
        sta     mos8(X16_P5)
        lda     mos8(__rc6)
        sta     mos8(X16_P6)          ; vstop (rightmost arg, in A/X)
        lda     mos8(__rc7)
        sta     mos8(X16_P7)
        plx
        pla
        sta     mos8(X16_P0)          ; hstart
        stx     mos8(X16_P1)
vdc_set_active:
        lda     mos8(X16_P0)
        lsr
        lsr
        sta     mos8(X16_T0)
        lda     mos8(X16_P1)
        and     #%00000011
        asl
        asl
        asl
        asl
        asl
        asl
        ora     mos8(X16_T0)
        sta     mos8(X16_T0)

        lda     mos8(X16_P2)
        lsr
        lsr
        sta     mos8(X16_T1)
        lda     mos8(X16_P3)
        and     #%00000011
        asl
        asl
        asl
        asl
        asl
        asl
        ora     mos8(X16_T1)
        sta     mos8(X16_T1)

        lda     mos8(X16_P4)
        lsr
        sta     mos8(X16_T2)
        lda     mos8(X16_P5)
        and     #%00000001
        asl
        asl
        asl
        asl
        asl
        asl
        asl
        ora     mos8(X16_T2)
        sta     mos8(X16_T2)

        lda     mos8(X16_P6)
        lsr
        sta     mos8(X16_T3)
        lda     mos8(X16_P7)
        and     #%00000001
        asl
        asl
        asl
        asl
        asl
        asl
        asl
        ora     mos8(X16_T3)
        sta     mos8(X16_T3)
        jmp     vdc_store_active_t

; ---------------------------------------------------------------------
; void x16_vdc_fullscreen(void) -- active area = 0,0 to 640,480
; ---------------------------------------------------------------------
x16_vdc_fullscreen:
        stz     mos8(X16_T0)
        lda     #160
        sta     mos8(X16_T1)
        stz     mos8(X16_T2)
        lda     #240
        sta     mos8(X16_T3)
        ; fall through

; vdc_store_active_t -- in: X16_T0..T3 = HSTART/HSTOP/VSTART/VSTOP,
;                       already in composer units
vdc_store_active_t:
        vera_dcsel 1
        lda     mos8(X16_T0)
        sta     VERA_DC_HSTART
        lda     mos8(X16_T1)
        sta     VERA_DC_HSTOP
        lda     mos8(X16_T2)
        sta     VERA_DC_VSTART
        lda     mos8(X16_T3)
        sta     VERA_DC_VSTOP
        vera_dcsel 0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_vdc_get_version(x16_vdc_version *out)
;   1 with major/minor/build filled in when DC_VER0 answers 'V';
;   0 (and zeros) on the FPGA bitstreams from before the version
;   registers existed. Agrees with x16_vera_has_fx() by construction:
;   both read the same magic.
; ---------------------------------------------------------------------
x16_vdc_get_version:
        lda     mos8(__rc2)             ; a lone pointer never lands in
        sta     mos8(X16_TPTR0)         ; A/X -- it takes an __rc pair
        lda     mos8(__rc3)
        sta     mos8(X16_TPTR0+1)
        jsr     vdc_get_version
        lda     #0
        rol     a                       ; carry -> 0/1
        pha
        lda     mos8(X16_T0)
        ldy     #0
        sta     (X16_TPTR0),y
        lda     mos8(X16_T1)
        iny
        sta     (X16_TPTR0),y
        lda     mos8(X16_T2)
        iny
        sta     (X16_TPTR0),y
        pla
        ldx     #0
        rts

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; vdc_get_version
;   out: carry set if the version is valid, with A/T0 = major,
;        X/T1 = minor, Y/T2 = build; carry clear and zeros if DC_VER0
;        is not 'V'.
; ---------------------------------------------------------------------
vdc_get_version:
        vera_dcsel VERA_DCSEL_FX_VERSION
        lda     VERA_DC_VER0
        cmp     #VERA_VERSION_MAGIC
        bne     .Lvdc_get_version_no
        lda     VERA_DC_VER1
        sta     mos8(X16_T0)
        lda     VERA_DC_VER2
        sta     mos8(X16_T1)
        lda     VERA_DC_VER3
        sta     mos8(X16_T2)
        vera_dcsel 0
        lda     mos8(X16_T0)
        ldx     mos8(X16_T1)
        ldy     mos8(X16_T2)
        sec
        rts
.Lvdc_get_version_no:
        vera_dcsel 0
        stz     mos8(X16_T0)
        stz     mos8(X16_T1)
        stz     mos8(X16_T2)
        lda     #0
        tax
        tay
        clc
        rts
