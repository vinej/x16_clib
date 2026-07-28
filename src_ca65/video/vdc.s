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

        .importzp       ptr1
        .import         popa, popax

        .export         _x16_vdc_get_video
        .export         _x16_vdc_set_video
        .export         _x16_vdc_set_output
        .export         _x16_vdc_set_layers
        .export         _x16_vdc_layer_on
        .export         _x16_vdc_layer_off
        .export         _x16_vdc_get_scale
        .export         _x16_vdc_set_scale
        .export         _x16_vdc_get_border
        .export         _x16_vdc_set_border
        .export         _x16_vdc_get_active_raw
        .export         _x16_vdc_set_active_raw
        .export         _x16_vdc_set_active
        .export         _x16_vdc_fullscreen
        .export         _x16_vdc_get_version

VDC_LAYER_MASK = VERA_VIDEO_LAYER0_EN | VERA_VIDEO_LAYER1_EN | VERA_VIDEO_SPRITES_EN

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_vdc_get_video(void)
; void __fastcall__ x16_vdc_set_video(unsigned char video)
;   The raw DC_VIDEO byte. Set ignores bit 7 (FIELD is read-only).
; ---------------------------------------------------------------------
_x16_vdc_get_video:
        vera_dcsel 0
        lda     VERA_DC_VIDEO
        ldx     #0
        rts

_x16_vdc_set_video:
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
_x16_vdc_set_output:
        and     #%00000011
        sta     X16_T0
        vera_dcsel 0
        lda     VERA_DC_VIDEO
        and     #%01111100
        ora     X16_T0
        sta     VERA_DC_VIDEO
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_vdc_set_layers(unsigned char mask)
;   Replace the three enable bits with `mask`; the mode and chroma
;   bits survive.
; ---------------------------------------------------------------------
_x16_vdc_set_layers:
        and     #VDC_LAYER_MASK
        sta     X16_T0
        vera_dcsel 0
        lda     VERA_DC_VIDEO
        and     #%00001111
        ora     X16_T0
        sta     VERA_DC_VIDEO
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_vdc_layer_on(unsigned char mask)
; void __fastcall__ x16_vdc_layer_off(unsigned char mask)
; ---------------------------------------------------------------------
_x16_vdc_layer_on:
        and     #VDC_LAYER_MASK
        pha
        vera_dcsel 0
        pla
        tsb     VERA_DC_VIDEO
        rts

_x16_vdc_layer_off:
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
_x16_vdc_get_scale:
        vera_dcsel 0
        lda     VERA_DC_HSCALE
        ldx     VERA_DC_VSCALE
        rts

_x16_vdc_set_scale:
        sta     X16_T1                  ; v (rightmost arg, in A)
        jsr     popa
        sta     X16_T0                  ; h
        vera_dcsel 0
        lda     X16_T0
        sta     VERA_DC_HSCALE
        lda     X16_T1
        sta     VERA_DC_VSCALE
        rts

; ---------------------------------------------------------------------
; unsigned char x16_vdc_get_border(void)
; void __fastcall__ x16_vdc_set_border(unsigned char index)
;   The border palette index.
; ---------------------------------------------------------------------
_x16_vdc_get_border:
        vera_dcsel 0
        lda     VERA_DC_BORDER
        ldx     #0
        rts

_x16_vdc_set_border:
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
_x16_vdc_get_active_raw:
        sta     ptr1
        stx     ptr1+1
        vera_dcsel 1
        lda     VERA_DC_HSTART
        ldy     #0
        sta     (ptr1),y
        lda     VERA_DC_HSTOP
        iny
        sta     (ptr1),y
        lda     VERA_DC_VSTART
        iny
        sta     (ptr1),y
        lda     VERA_DC_VSTOP
        iny
        sta     (ptr1),y
        vera_dcsel 0
        rts

_x16_vdc_set_active_raw:
        sta     ptr1
        stx     ptr1+1
        ldy     #0
        lda     (ptr1),y
        sta     X16_T0
        iny
        lda     (ptr1),y
        sta     X16_T1
        iny
        lda     (ptr1),y
        sta     X16_T2
        iny
        lda     (ptr1),y
        sta     X16_T3
        jmp     vdc_store_active_t

; ---------------------------------------------------------------------
; void __fastcall__ x16_vdc_set_active(unsigned int hstart,
;                                      unsigned int hstop,
;                                      unsigned int vstart,
;                                      unsigned int vstop)
;   In pixels; converted to composer units (horizontal / 4,
;   vertical / 2).
; ---------------------------------------------------------------------
_x16_vdc_set_active:
        sta     X16_P6                  ; vstop (rightmost arg, in A/X)
        stx     X16_P7
        jsr     popax
        sta     X16_P4                  ; vstart
        stx     X16_P5
        jsr     popax
        sta     X16_P2                  ; hstop
        stx     X16_P3
        jsr     popax
        sta     X16_P0                  ; hstart
        stx     X16_P1
        ; fall through

; vdc_set_active -- in: X16_P0/P1 = HSTART px, P2/P3 = HSTOP px,
;                       P4/P5 = VSTART px, P6/P7 = VSTOP px
vdc_set_active:
        lda     X16_P0
        lsr
        lsr
        sta     X16_T0
        lda     X16_P1
        and     #%00000011
        asl
        asl
        asl
        asl
        asl
        asl
        ora     X16_T0
        sta     X16_T0

        lda     X16_P2
        lsr
        lsr
        sta     X16_T1
        lda     X16_P3
        and     #%00000011
        asl
        asl
        asl
        asl
        asl
        asl
        ora     X16_T1
        sta     X16_T1

        lda     X16_P4
        lsr
        sta     X16_T2
        lda     X16_P5
        and     #%00000001
        asl
        asl
        asl
        asl
        asl
        asl
        asl
        ora     X16_T2
        sta     X16_T2

        lda     X16_P6
        lsr
        sta     X16_T3
        lda     X16_P7
        and     #%00000001
        asl
        asl
        asl
        asl
        asl
        asl
        asl
        ora     X16_T3
        sta     X16_T3
        jmp     vdc_store_active_t

; ---------------------------------------------------------------------
; void x16_vdc_fullscreen(void) -- active area = 0,0 to 640,480
; ---------------------------------------------------------------------
_x16_vdc_fullscreen:
        stz     X16_T0
        lda     #160
        sta     X16_T1
        stz     X16_T2
        lda     #240
        sta     X16_T3
        ; fall through

; vdc_store_active_t -- in: X16_T0..T3 = HSTART/HSTOP/VSTART/VSTOP,
;                       already in composer units
vdc_store_active_t:
        vera_dcsel 1
        lda     X16_T0
        sta     VERA_DC_HSTART
        lda     X16_T1
        sta     VERA_DC_HSTOP
        lda     X16_T2
        sta     VERA_DC_VSTART
        lda     X16_T3
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
_x16_vdc_get_version:
        sta     ptr1
        stx     ptr1+1
        jsr     vdc_get_version
        lda     #0
        rol     a                       ; carry -> 0/1
        pha
        lda     X16_T0
        ldy     #0
        sta     (ptr1),y
        lda     X16_T1
        iny
        sta     (ptr1),y
        lda     X16_T2
        iny
        sta     (ptr1),y
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
        bne     @no
        lda     VERA_DC_VER1
        sta     X16_T0
        lda     VERA_DC_VER2
        sta     X16_T1
        lda     VERA_DC_VER3
        sta     X16_T2
        vera_dcsel 0
        lda     X16_T0
        ldx     X16_T1
        ldy     X16_T2
        sec
        rts
@no:
        vera_dcsel 0
        stz     X16_T0
        stz     X16_T1
        stz     X16_T2
        lda     #0
        tax
        tay
        clc
        rts
