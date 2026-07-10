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

; llvm-mos argument placement, measured on the machine, not read from a
; wiki: INTEGER bytes fill A, then X, then __rc2, __rc3, ... left to
; right. A POINTER instead takes a whole __rc pair (__rc2/__rc3, then
; __rc4/__rc5) and consumes no A or X, because only zero page can be
; indirected through. A one-byte return comes back in A alone.
; (import dropped: vera_fill)
; (import dropped: ptr1, ptr2, sreg)

        .globl  x16_sprite_setptr
        .globl  x16_sprites_on
        .globl  x16_sprites_off
        .globl  x16_sprite_pos
        .globl  x16_sprite_get_pos
        .globl  x16_sprite_image
        .globl  x16_sprite_flags
        .globl  x16_sprite_z
        .globl  x16_sprite_size
        .globl  x16_sprite_init_all

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void __fastcall__ x16_sprite_setptr(unsigned char sprite,
;                                     unsigned char offset)
; ---------------------------------------------------------------------
x16_sprite_setptr:
        pha                             ; A = sprite, X = offset;
        txa                             ; sprite_setptr wants them the
        plx                             ; other way round
        jmp     sprite_setptr

; ---------------------------------------------------------------------
; void x16_sprites_on(void) / void x16_sprites_off(void)
;
; cc65's vera_sprites_enable() does the same job; these are here so the
; sprite module is usable without reaching into <cx16.h>.
; ---------------------------------------------------------------------
x16_sprites_on:
sprites_on:
        vera_dcsel 0
        lda     #VERA_VIDEO_SPRITES_EN
        tsb     VERA_DC_VIDEO
        rts

x16_sprites_off:
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
; A = sprite, X = x lo, __rc2 = x hi, __rc3 = y lo, __rc4 = y hi.
x16_sprite_pos:
        stx     X16_P0                  ; x lo
        pha                             ; sprite -- A is about to go
        lda     __rc2
        sta     X16_P1                  ; x hi
        lda     __rc3
        sta     X16_P2                  ; y lo
        lda     __rc4
        sta     X16_P3                  ; y hi
        plx                             ; X = sprite
        jmp     sprite_pos

; ---------------------------------------------------------------------
; void __fastcall__ x16_sprite_get_pos(unsigned char sprite,
;                                      unsigned int *x, unsigned int *y)
;
; Out-params rather than a struct return: cc65 passes a struct back
; through a hidden caller pointer anyway, so this is no slower and the
; aliasing stays visible.
; ---------------------------------------------------------------------
; A = sprite; x* in __rc2/__rc3; y* in __rc4/__rc5. The two destinations go
; into T4/T5 and T6/T7 -- adjacent, because core/x16zp.s lays the block out
; in one object -- and sprite_get_pos answers through P0..P3, which the
; store loop must not disturb.
x16_sprite_get_pos:
        tax                             ; X = sprite
        lda     __rc2
        sta     X16_T4                  ; x* lo
        lda     __rc3
        sta     X16_T5                  ; x* hi
        lda     __rc4
        sta     X16_T6                  ; y* lo
        lda     __rc5
        sta     X16_T7                  ; y* hi

        jsr     sprite_get_pos          ; X16_P0..P3

        ldy     #0
        lda     X16_P0
        sta     (X16_T4),y
        lda     X16_P2
        sta     (X16_T6),y
        iny
        lda     X16_P1
        sta     (X16_T4),y
        lda     X16_P3
        sta     (X16_T6),y
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_sprite_image(unsigned char sprite,
;                                    unsigned char mode, unsigned long addr)
;
; addr goes last so cc65 hands all four bytes over in registers, the same
; reason x16_vera_addr0() takes it last. mode is X16_SPRITE_4BPP or 8BPP.
; ---------------------------------------------------------------------
; A = sprite, X = mode, __rc2..__rc5 = addr bytes 0-3. Only bit 16 of the
; address exists, so __rc5 is ignored.
x16_sprite_image:
        pha                             ; sprite
        phx                             ; mode
        lda     __rc2
        sta     X16_P0                  ; addr bits 0-7
        lda     __rc3
        sta     X16_P1                  ; addr bits 8-15
        lda     __rc4
        sta     X16_P2                  ; addr bit 16
        pla                             ; A = mode
        plx                             ; X = sprite
        jmp     sprite_image

; ---------------------------------------------------------------------
; void __fastcall__ x16_sprite_flags(unsigned char sprite,
;                                    unsigned char flags)
; ---------------------------------------------------------------------
x16_sprite_flags:
        pha                             ; A = sprite, X = flags
        txa
        plx
        jmp     sprite_flags

; ---------------------------------------------------------------------
; void __fastcall__ x16_sprite_z(unsigned char sprite, unsigned char z)
; ---------------------------------------------------------------------
x16_sprite_z:
        pha                             ; A = sprite, X = z
        txa
        plx
        jmp     sprite_z

; ---------------------------------------------------------------------
; void __fastcall__ x16_sprite_size(unsigned char sprite,
;                                   unsigned char width, unsigned char height,
;                                   unsigned char pal_offset)
; ---------------------------------------------------------------------
; A = sprite, X = width, __rc2 = height, __rc3 = palette offset.
; sprite_size wants X = sprite, A = width, Y = height, X16_P0 = offset.
x16_sprite_size:
        pha                             ; sprite
        lda     __rc3
        sta     X16_P0                  ; palette offset
        ldy     __rc2                   ; Y = height
        txa                             ; A = width
        plx                             ; X = sprite
        jmp     sprite_size

; ---------------------------------------------------------------------
; void x16_sprite_init_all(void)
; ---------------------------------------------------------------------
x16_sprite_init_all:
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
