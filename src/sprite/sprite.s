; =====================================================================
; x16clib :: sprite/sprite.s -- VERA hardware sprites
; =====================================================================
; 128 sprites, an 8-byte attribute record each, at $1FC00:
;   0  image address bits 12:5
;   1  mode(7) | image address bits 16:13
;   2  X bits 7:0
;   3  X bits 9:8
;   4  Y bits 7:0
;   5  Y bits 9:8
;   6  collision mask(7:4) | Z-depth(3:2) | vflip(1) | hflip(0)
;   7  height(7:6) | width(5:4) | palette offset(3:0)
;
; That region is write-only: reads return the last value the host wrote.
; Read-modify-write therefore only works on records this program has
; already initialised. x16_sprite_init_all() does that.
;
; Internal scratch: sprite_setptr owns X16_T1/T2, and the routines that
; call it own T3/T4. The C shims therefore stage into T5/T6.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

        .import         popa, popax
        .import         vera_fill
        .importzp       ptr1, ptr2, sreg

        .export         _x16_sprite_setptr
        .export         _x16_sprites_on
        .export         _x16_sprites_off
        .export         _x16_sprite_pos
        .export         _x16_sprite_get_pos
        .export         _x16_sprite_image
        .export         _x16_sprite_flags
        .export         _x16_sprite_z
        .export         _x16_sprite_size
        .export         _x16_sprite_init_all

        .segment        "CODE"

; ---------------------------------------------------------------------
; void __fastcall__ x16_sprite_setptr(unsigned char sprite,
;                                     unsigned char offset)
; ---------------------------------------------------------------------
_x16_sprite_setptr:
        sta     X16_T5                  ; offset (rightmost arg, in A)
        jsr     popa                    ; sprite
        tax
        lda     X16_T5
        jmp     sprite_setptr

; ---------------------------------------------------------------------
; void x16_sprites_on(void) / void x16_sprites_off(void)
;
; cc65's vera_sprites_enable() does the same job; these are here so the
; sprite module is usable without reaching into <cx16.h>.
; ---------------------------------------------------------------------
_x16_sprites_on:
sprites_on:
        vera_dcsel 0
        lda     #VERA_VIDEO_SPRITES_EN
        tsb     VERA_DC_VIDEO
        rts

_x16_sprites_off:
sprites_off:
        vera_dcsel 0
        lda     #VERA_VIDEO_SPRITES_EN
        trb     VERA_DC_VIDEO
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_sprite_pos(unsigned char sprite,
;                                  unsigned int x, unsigned int y)
;
; Coordinates are 10-bit, in the 640x480 display space -- not the 320x240
; of the bitmap modes.
; ---------------------------------------------------------------------
_x16_sprite_pos:
        sta     X16_P2                  ; y lo (rightmost arg: A/X)
        stx     X16_P3                  ; y hi
        jsr     popax                   ; x
        sta     X16_P0
        stx     X16_P1
        jsr     popa                    ; sprite
        tax
        jmp     sprite_pos

; ---------------------------------------------------------------------
; void __fastcall__ x16_sprite_get_pos(unsigned char sprite,
;                                      unsigned int *x, unsigned int *y)
;
; Out-params rather than a struct return: cc65 passes a struct back
; through a hidden caller pointer anyway, so this is no slower and the
; aliasing stays visible.
; ---------------------------------------------------------------------
_x16_sprite_get_pos:
        sta     ptr2                    ; y* (rightmost arg: A/X)
        stx     ptr2+1
        jsr     popax                   ; x*
        sta     ptr1
        stx     ptr1+1
        jsr     popa                    ; sprite
        tax

        jsr     sprite_get_pos          ; X16_P0..P3

        ldy     #0
        lda     X16_P0
        sta     (ptr1),y
        lda     X16_P2
        sta     (ptr2),y
        iny
        lda     X16_P1
        sta     (ptr1),y
        lda     X16_P3
        sta     (ptr2),y
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_sprite_image(unsigned char sprite,
;                                    unsigned char mode, unsigned long addr)
;
; addr goes last so cc65 hands all four bytes over in registers, the same
; reason x16_vera_addr0() takes it last. mode is X16_SPRITE_4BPP or 8BPP.
; ---------------------------------------------------------------------
_x16_sprite_image:
        sta     X16_P0                  ; addr bits 0-7
        stx     X16_P1                  ; addr bits 8-15
        lda     sreg
        sta     X16_P2                  ; addr bit 16
        jsr     popa                    ; mode
        sta     X16_T5
        jsr     popa                    ; sprite
        tax
        lda     X16_T5
        jmp     sprite_image

; ---------------------------------------------------------------------
; void __fastcall__ x16_sprite_flags(unsigned char sprite,
;                                    unsigned char flags)
; ---------------------------------------------------------------------
_x16_sprite_flags:
        sta     X16_T5                  ; flags
        jsr     popa                    ; sprite
        tax
        lda     X16_T5
        jmp     sprite_flags

; ---------------------------------------------------------------------
; void __fastcall__ x16_sprite_z(unsigned char sprite, unsigned char z)
; ---------------------------------------------------------------------
_x16_sprite_z:
        sta     X16_T5                  ; z
        jsr     popa                    ; sprite
        tax
        lda     X16_T5
        jmp     sprite_z

; ---------------------------------------------------------------------
; void __fastcall__ x16_sprite_size(unsigned char sprite,
;                                   unsigned char width, unsigned char height,
;                                   unsigned char pal_offset)
; ---------------------------------------------------------------------
_x16_sprite_size:
        sta     X16_P0                  ; palette offset (rightmost arg)
        jsr     popa
        sta     X16_T5                  ; height code
        jsr     popa
        sta     X16_T6                  ; width code
        jsr     popa                    ; sprite
        tax
        lda     X16_T6                  ; A = width
        ldy     X16_T5                  ; Y = height
        jmp     sprite_size

; ---------------------------------------------------------------------
; void x16_sprite_init_all(void)
; ---------------------------------------------------------------------
_x16_sprite_init_all:
sprite_init_all:
        ; Zero all 128 attribute records. Disables every sprite and, more
        ; importantly, gives the write-only attribute RAM a known
        ; host-side shadow, so sprite_z's read-modify-write is meaningful.
        vera_addr 0, VRAM_SPRITE_ATTR, VERA_INC_1
        lda     #0
        ldx     #<(128 * 8)
        ldy     #>(128 * 8)
        jmp     vera_fill

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; sprite_setptr -- point data port 0 at one byte of a sprite record.
;   in:  X = sprite number (0-127), A = byte offset within the record
;   Leaves the port on auto-increment, so consecutive fields stream.
; ---------------------------------------------------------------------
sprite_setptr:
        sta     X16_T2                  ; byte offset
        lda     #VERA_CTRL_ADDRSEL
        trb     VERA_CTRL               ; ADDRSEL = 0, DCSEL untouched

        stz     X16_T1
        txa
        asl     a
        rol     X16_T1
        asl     a
        rol     X16_T1
        asl     a
        rol     X16_T1                  ; T1:A = sprite * 8

        clc
        adc     X16_T2
        sta     VERA_ADDR_L
        lda     X16_T1
        adc     #>VRAM_SPRITE_ATTR      ; $FC, plus any carry from the offset
        sta     VERA_ADDR_M
        lda     #(VERA_ADDR_H_BANK | (VERA_INC_1 << 4))
        sta     VERA_ADDR_H
        rts

; ---------------------------------------------------------------------
; sprite_pos -- set a sprite's 10-bit position
;   in:  X = sprite, X16_P0/P1 = x, X16_P2/P3 = y
; ---------------------------------------------------------------------
sprite_pos:
        lda     #SPRITE_ATTR_X_L
        jsr     sprite_setptr
        lda     X16_P0
        sta     VERA_DATA0
        lda     X16_P1
        and     #$03
        sta     VERA_DATA0
        lda     X16_P2
        sta     VERA_DATA0
        lda     X16_P3
        and     #$03
        sta     VERA_DATA0
        rts

; ---------------------------------------------------------------------
; sprite_get_pos -- read it back
;   in:  X = sprite
;   out: X16_P0/P1 = x, X16_P2/P3 = y
; ---------------------------------------------------------------------
sprite_get_pos:
        lda     #SPRITE_ATTR_X_L
        jsr     sprite_setptr
        lda     VERA_DATA0
        sta     X16_P0
        lda     VERA_DATA0
        and     #$03
        sta     X16_P1
        lda     VERA_DATA0
        sta     X16_P2
        lda     VERA_DATA0
        and     #$03
        sta     X16_P3
        rts

; ---------------------------------------------------------------------
; sprite_image -- point a sprite at its pixel data in VRAM
;   in:  X = sprite
;        X16_P0 = addr low, X16_P1 = addr mid, X16_P2 = addr bit 16
;        A = SPRITE_MODE_4BPP or SPRITE_MODE_8BPP
;
; The record stores address bits 16:5, so the data must be 32-byte
; aligned; the low five bits are simply dropped.
; ---------------------------------------------------------------------
sprite_image:
        sta     X16_T3                  ; mode flag
        lda     #SPRITE_ATTR_ADDR_L
        jsr     sprite_setptr

        lda     X16_P0
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a                       ; addr bits 7:5 -> 2:0
        sta     X16_T4
        lda     X16_P1
        asl     a
        asl     a
        asl     a                       ; addr bits 12:8 -> 7:3
        ora     X16_T4
        sta     VERA_DATA0              ; byte 0 = addr 12:5

        lda     X16_P1
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a                       ; addr bits 15:13 -> 2:0
        sta     X16_T4
        lda     X16_P2
        and     #$01
        asl     a
        asl     a
        asl     a                       ; addr bit 16 -> bit 3
        ora     X16_T4
        ora     X16_T3                  ; mode in bit 7
        sta     VERA_DATA0              ; byte 1
        rts

; ---------------------------------------------------------------------
; sprite_flags -- byte 6: collision mask, Z-depth, flips
;   in:  X = sprite, A = collision<<4 | Z | vflip | hflip
; ---------------------------------------------------------------------
sprite_flags:
        sta     X16_T3
        lda     #SPRITE_ATTR_FLAGS
        jsr     sprite_setptr
        lda     X16_T3
        sta     VERA_DATA0
        rts

; ---------------------------------------------------------------------
; sprite_z -- change only the Z-depth, preserving the other bits
;   in:  X = sprite, A = SPRITE_Z_DISABLED/BEHIND/MIDDLE/FRONT
;
; Read-modify-write. Only valid once the record has been written at
; least once (see the note at the top of this file).
; ---------------------------------------------------------------------
sprite_z:
        sta     X16_T3
        lda     #SPRITE_ATTR_FLAGS
        jsr     sprite_setptr
        lda     VERA_DATA0              ; read advances the port past byte 6
        and     #%11110011
        ora     X16_T3
        sta     X16_T4
        lda     #SPRITE_ATTR_FLAGS
        jsr     sprite_setptr           ; point at byte 6 again to write it
        lda     X16_T4
        sta     VERA_DATA0
        rts

; ---------------------------------------------------------------------
; sprite_size -- byte 7: size codes and palette offset
;   in:  X = sprite
;        A = width code (SPRITE_SIZE_8/16/32/64)
;        Y = height code
;        X16_P0 = palette offset (0-15)
; ---------------------------------------------------------------------
sprite_size:
        and     #$03
        asl     a
        asl     a
        asl     a
        asl     a                       ; width into bits 5:4
        sta     X16_T3
        tya
        and     #$03
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a                       ; height into bits 7:6
        ora     X16_T3
        sta     X16_T3
        lda     X16_P0
        and     #$0F                    ; an offset >15 must not corrupt the size
        ora     X16_T3
        sta     X16_T3

        lda     #SPRITE_ATTR_SIZE_PAL
        jsr     sprite_setptr
        lda     X16_T3
        sta     VERA_DATA0
        rts
