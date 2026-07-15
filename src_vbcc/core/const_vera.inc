; =====================================================================
; x16clib :: core/const_vera.inc -- VERA registers, VRAM map, bitfields
; =====================================================================
; Pure symbol file. Safe to .include any number of times.
;
; Sources of truth:
;   doc/X16 Reference - 09 - VERA Programmer's Reference.md
;   doc/X16 Reference - 10 - VERA FX Reference.md
;   x16-rom-r49/inc/io.inc
;
; NOTE ON VERA_INC_*: these are raw increment INDICES (0-15), shifted
; into ADDR_H bits 7:4 at the use site. cc65's cx16.h defines constants
; of the same name that are ALREADY shifted (its VERA_INC_1 == $10).
; Same final bits, sixteen times apart as constants. These must never
; reach a C header -- see include/x16/vera.h.
; =====================================================================

	ifndef X16_CONST_VERA_INC
X16_CONST_VERA_INC = 1

VERA_BASE = $9F20

VERA_ADDR_L       = VERA_BASE + $00
VERA_ADDR_M       = VERA_BASE + $01
VERA_ADDR_H       = VERA_BASE + $02   ; bank + increment index + DECR
VERA_DATA0        = VERA_BASE + $03
VERA_DATA1        = VERA_BASE + $04
VERA_CTRL         = VERA_BASE + $05   ; RESET | DCSEL(6) | ADDRSEL
VERA_IEN          = VERA_BASE + $06
VERA_ISR          = VERA_BASE + $07   ; bits 7:4 = sprite collisions
VERA_IRQ_LINE_L   = VERA_BASE + $08

; --- $9F29-$9F2C are banked by DCSEL -------------------------------
; DCSEL = 0
VERA_DC_VIDEO     = VERA_BASE + $09
VERA_DC_HSCALE    = VERA_BASE + $0A
VERA_DC_VSCALE    = VERA_BASE + $0B
VERA_DC_BORDER    = VERA_BASE + $0C
; DCSEL = 1
VERA_DC_HSTART    = VERA_BASE + $09
VERA_DC_HSTOP     = VERA_BASE + $0A
VERA_DC_VSTART    = VERA_BASE + $0B
VERA_DC_VSTOP     = VERA_BASE + $0C
; DCSEL = 2  (FX core)
VERA_FX_CTRL      = VERA_BASE + $09   ; R/W
VERA_FX_TILEBASE  = VERA_BASE + $0A   ; W
VERA_FX_MAPBASE   = VERA_BASE + $0B   ; W
VERA_FX_MULT      = VERA_BASE + $0C   ; W
; DCSEL = 3  (line/poly increments)
VERA_FX_X_INCR_L  = VERA_BASE + $09   ; W
VERA_FX_X_INCR_H  = VERA_BASE + $0A   ; W
VERA_FX_Y_INCR_L  = VERA_BASE + $0B   ; W
VERA_FX_Y_INCR_H  = VERA_BASE + $0C   ; W
; DCSEL = 4  (line/poly positions)
VERA_FX_X_POS_L   = VERA_BASE + $09   ; W
VERA_FX_X_POS_H   = VERA_BASE + $0A   ; W
VERA_FX_Y_POS_L   = VERA_BASE + $0B   ; W
VERA_FX_Y_POS_H   = VERA_BASE + $0C   ; W
; DCSEL = 5
VERA_FX_X_POS_S   = VERA_BASE + $09   ; W
VERA_FX_Y_POS_S   = VERA_BASE + $0A   ; W
VERA_FX_POLY_FILL_L = VERA_BASE + $0B ; R
VERA_FX_POLY_FILL_H = VERA_BASE + $0C ; R
; DCSEL = 6  (32-bit cache / accumulator)
VERA_FX_CACHE_L     = VERA_BASE + $09 ; W
VERA_FX_ACCUM_RESET = VERA_BASE + $09 ; R
VERA_FX_CACHE_M     = VERA_BASE + $0A ; W
VERA_FX_ACCUM       = VERA_BASE + $0A ; R
VERA_FX_CACHE_H     = VERA_BASE + $0B ; W
VERA_FX_CACHE_U     = VERA_BASE + $0C ; W
; DCSEL = 63 (version probe; DC_VER0 reads ASCII 'V')
VERA_DC_VER0      = VERA_BASE + $09   ; R
VERA_DC_VER1      = VERA_BASE + $0A   ; R  major
VERA_DC_VER2      = VERA_BASE + $0B   ; R  minor
VERA_DC_VER3      = VERA_BASE + $0C   ; R  build
; -------------------------------------------------------------------

VERA_L0_CONFIG    = VERA_BASE + $0D
VERA_L0_MAPBASE   = VERA_BASE + $0E
VERA_L0_TILEBASE  = VERA_BASE + $0F
VERA_L0_HSCROLL_L = VERA_BASE + $10
VERA_L0_HSCROLL_H = VERA_BASE + $11
VERA_L0_VSCROLL_L = VERA_BASE + $12
VERA_L0_VSCROLL_H = VERA_BASE + $13

VERA_L1_CONFIG    = VERA_BASE + $14
VERA_L1_MAPBASE   = VERA_BASE + $15
VERA_L1_TILEBASE  = VERA_BASE + $16
VERA_L1_HSCROLL_L = VERA_BASE + $17
VERA_L1_HSCROLL_H = VERA_BASE + $18
VERA_L1_VSCROLL_L = VERA_BASE + $19
VERA_L1_VSCROLL_H = VERA_BASE + $1A

VERA_AUDIO_CTRL   = VERA_BASE + $1B
VERA_AUDIO_RATE   = VERA_BASE + $1C
VERA_AUDIO_DATA   = VERA_BASE + $1D

VERA_SPI_DATA     = VERA_BASE + $1E
VERA_SPI_CTRL     = VERA_BASE + $1F

; YM2151 FM chip. NOT at $9FE0 -- see x16-rom-r49/inc/io.inc.
YM_REG            = $9F40
YM_DATA           = $9F41

; ---------------------------------------------------------------------
; CTRL bitfields.  DCSEL is SIX bits at 6:1, ADDRSEL is bit 0.
; Writing DCSEL naively clobbers ADDRSEL -- always use vera_dcsel.
; Never set bit 7: it resets the whole chip.
; ---------------------------------------------------------------------
VERA_CTRL_ADDRSEL = %00000001
VERA_CTRL_DCSEL   = %01111110
VERA_CTRL_RESET   = %10000000

; ---------------------------------------------------------------------
; ADDR_H bitfields.  The increment field is an INDEX, not an amount.
; ---------------------------------------------------------------------
VERA_ADDR_H_BANK  = %00000001   ; VRAM address bit 16
VERA_ADDR_H_DECR  = %00001000   ; decrement instead of increment
VERA_ADDR_H_INCR  = %11110000   ; increment index, bits 7:4

VERA_INC_0   = 0
VERA_INC_1   = 1
VERA_INC_2   = 2
VERA_INC_4   = 3
VERA_INC_8   = 4
VERA_INC_16  = 5
VERA_INC_32  = 6
VERA_INC_64  = 7
VERA_INC_128 = 8
VERA_INC_256 = 9
VERA_INC_512 = 10
VERA_INC_40  = 11   ; one 40-column text row
VERA_INC_80  = 12   ; one 80-column text row
VERA_INC_160 = 13
VERA_INC_320 = 14   ; one 320-pixel bitmap row
VERA_INC_640 = 15

; The C API packs "which increment" and "step backwards" into one byte:
; bits 3:0 are the index above, bit 4 asks for DECR. Not a hardware
; layout -- the shims unpack it into ADDR_H. Mirrored as X16_DECR in
; include/x16/vera.h; the two must agree.
X16_DECR = %00010000

; ---------------------------------------------------------------------
; DC_VIDEO (DCSEL=0) bitfields.
; ---------------------------------------------------------------------
VERA_VIDEO_MODE_OFF   = 0
VERA_VIDEO_MODE_VGA   = 1
VERA_VIDEO_MODE_NTSC  = 2
VERA_VIDEO_MODE_RGB   = 3
VERA_VIDEO_CHROMA_DIS = %00000100
VERA_VIDEO_240P       = %00001000
VERA_VIDEO_LAYER0_EN  = %00010000
VERA_VIDEO_LAYER1_EN  = %00100000
VERA_VIDEO_SPRITES_EN = %01000000
VERA_VIDEO_FIELD      = %10000000   ; read-only

; ---------------------------------------------------------------------
; ISR / IEN bitfields.  ISR bits 7:4 report sprite collision groups.
; ---------------------------------------------------------------------
VERA_IRQ_VSYNC    = %00000001
VERA_IRQ_LINE     = %00000010
VERA_IRQ_SPRCOL   = %00000100
VERA_IRQ_AFLOW    = %00001000
VERA_ISR_COLLISION = %11110000

; ---------------------------------------------------------------------
; FX_CTRL (DCSEL=2) bitfields.
; ---------------------------------------------------------------------
VERA_FX_ADDR1_NORMAL  = 0
VERA_FX_ADDR1_LINE    = 1
VERA_FX_ADDR1_POLY    = 2
VERA_FX_ADDR1_AFFINE  = 3
VERA_FX_4BIT_MODE     = %00000100
VERA_FX_16BIT_HOP     = %00001000
VERA_FX_CACHE_CYCLE   = %00010000
VERA_FX_CACHE_FILL    = %00100000
VERA_FX_CACHE_WRITE   = %01000000
VERA_FX_TRANSPARENT   = %10000000

; FX_MULT (DCSEL=2) bitfields.
VERA_FX_MULT_2BYTE_INCR = %00000001
VERA_FX_MULT_NIB_INDEX  = %00000010
VERA_FX_MULT_BYTE_INDEX = %00001100
VERA_FX_MULT_ENABLE     = %00010000
VERA_FX_MULT_SUBTRACT   = %00100000
VERA_FX_MULT_ACCUMULATE = %01000000
VERA_FX_MULT_RESET_ACC  = %10000000

VERA_DCSEL_FX_VERSION = 63
VERA_VERSION_MAGIC    = $56          ; 'V' in DC_VER0

; ---------------------------------------------------------------------
; Layer CONFIG bitfields.
; ---------------------------------------------------------------------
VERA_LAYER_BPP_1      = 0
VERA_LAYER_BPP_2      = 1
VERA_LAYER_BPP_4      = 2
VERA_LAYER_BPP_8      = 3
VERA_LAYER_T256C      = %00001000    ; 256-colour text
VERA_LAYER_BITMAP     = %00000100    ; bitmap instead of tile mode
; Map size, bits 7:6 = height, 5:4 = width (0=32,1=64,2=128,3=256 tiles)
VERA_LAYER_MAPW_32    = %00000000
VERA_LAYER_MAPW_64    = %00010000
VERA_LAYER_MAPW_128   = %00100000
VERA_LAYER_MAPW_256   = %00110000
VERA_LAYER_MAPH_32    = %00000000
VERA_LAYER_MAPH_64    = %01000000
VERA_LAYER_MAPH_128   = %10000000
VERA_LAYER_MAPH_256   = %11000000

; ---------------------------------------------------------------------
; VRAM map.  17-bit addresses: bit 16 is the "bank" in ADDR_H.
;
; NOTE: $1F9C0-$1FFFF (PSG, palette, sprite attributes) is WRITE-ONLY.
; Reads return the last value the host wrote, not the register's real
; state.  Reading back your own writes is fine; inferring hardware state
; after a reset is not.
; ---------------------------------------------------------------------
VRAM_BITMAP       = $00000       ; default 320x240x256 framebuffer
VRAM_SPRITE_DATA  = $13000       ; KERNAL's sprite image area
VRAM_TEXT         = $1B000       ; default text-mode tilemap
VRAM_CHARSET      = $1F000
VRAM_PSG          = $1F9C0       ; 16 voices x 4 bytes
VRAM_PALETTE      = $1FA00       ; 256 entries x 2 bytes
VRAM_SPRITE_ATTR  = $1FC00       ; 128 sprites x 8 bytes

; The FX multiplier writes its 32-bit result to VRAM rather than to a
; register, so it needs four scratch bytes. $1F800-$1F9BF is unused in
; the VERA memory map.
	ifndef VRAM_FX_SCRATCH
VRAM_FX_SCRATCH = $1F800
	endif

VERA_PSG_VOICE_SIZE   = 4
VERA_SPRITE_ATTR_SIZE = 8

; Sprite attribute byte offsets (see VERA reference "Sprite attributes").
SPRITE_ATTR_ADDR_L   = 0    ; image address bits 12:5
SPRITE_ATTR_ADDR_H   = 1    ; bit7 = mode (0=4bpp,1=8bpp), bits 3:0 = addr 16:13
SPRITE_ATTR_X_L      = 2
SPRITE_ATTR_X_H      = 3    ; bits 1:0 = X 9:8
SPRITE_ATTR_Y_L      = 4
SPRITE_ATTR_Y_H      = 5    ; bits 1:0 = Y 9:8
SPRITE_ATTR_FLAGS    = 6    ; collision mask 7:4 | Z 3:2 | vflip 1 | hflip 0
SPRITE_ATTR_SIZE_PAL = 7    ; height 7:6 | width 5:4 | palette offset 3:0

SPRITE_MODE_4BPP  = %00000000
SPRITE_MODE_8BPP  = %10000000

SPRITE_Z_DISABLED = %00000000
SPRITE_Z_BEHIND   = %00000100   ; between background and layer 0
SPRITE_Z_MIDDLE   = %00001000   ; between layer 0 and layer 1
SPRITE_Z_FRONT    = %00001100   ; in front of layer 1
SPRITE_HFLIP      = %00000001
SPRITE_VFLIP      = %00000010

SPRITE_SIZE_8     = 0
SPRITE_SIZE_16    = 1
SPRITE_SIZE_32    = 2
SPRITE_SIZE_64    = 3

	endif
