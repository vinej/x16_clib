; =====================================================================
; x16clib :: core/const_kernal.inc -- the $FExx/$FFxx KERNAL jump table
; =====================================================================
; Pure symbol file. Safe to .include any number of times.
;
; Transcribed from x16-rom-r49/kernal/vectors.s (segment JMPTBL, which
; cfg/kernal-x16.cfgtpl places at $FEA8). These are the *stable public*
; entry points -- do not call the implementation addresses in the .sym
; files, they move between ROM revisions.
;
; Every ROM bank carries a bridge stub for this table (see the ROM's
; kernsup/ directory), so `jsr CHROUT` works whatever ROM bank is active.
; =====================================================================

	ifndef X16_CONST_KERNAL_INC
X16_CONST_KERNAL_INC = 1

; --- X16 extensions ($FEA8-$FF7D) ----------------------------------
EXTAPI16                = $FEA8
EXTAPI                  = $FEAB
MCIOUT                  = $FEB1
I2C_BATCH_READ          = $FEB4
I2C_BATCH_WRITE         = $FEB7
SAVEHL                  = $FEBA
KBDBUF_PEEK             = $FEBD
KBDBUF_GET_MODIFIERS    = $FEC0
KBDBUF_PUT              = $FEC3
I2C_READ_BYTE           = $FEC6
I2C_WRITE_BYTE          = $FEC9
MONITOR                 = $FECC
ENTROPY_GET             = $FECF
KEYMAP                  = $FED2
CONSOLE_SET_PAGING_MESSAGE = $FED5
CONSOLE_PUT_IMAGE       = $FED8
CONSOLE_INIT            = $FEDB
CONSOLE_PUT_CHAR        = $FEDE
CONSOLE_GET_CHAR        = $FEE1
MEMORY_FILL             = $FEE4
MEMORY_COPY             = $FEE7
MEMORY_CRC              = $FEEA
MEMORY_DECOMPRESS       = $FEED
SPRITE_SET_IMAGE        = $FEF0
SPRITE_SET_POSITION     = $FEF3

; Framebuffer API
FB_INIT                 = $FEF6
FB_GET_INFO             = $FEF9
FB_SET_PALETTE          = $FEFC
FB_CURSOR_POSITION      = $FEFF
FB_CURSOR_NEXT_LINE     = $FF02
FB_GET_PIXEL            = $FF05
FB_GET_PIXELS           = $FF08
FB_SET_PIXEL            = $FF0B
FB_SET_PIXELS           = $FF0E
FB_SET_8_PIXELS         = $FF11
FB_SET_8_PIXELS_OPAQUE  = $FF14
FB_FILL_PIXELS          = $FF17
FB_FILTER_PIXELS        = $FF1A
FB_MOVE_PIXELS          = $FF1D

; Graphics API (lives in BANK_GRAPH, reached through these stubs)
GRAPH_INIT              = $FF20
GRAPH_CLEAR             = $FF23
GRAPH_SET_WINDOW        = $FF26
GRAPH_SET_COLORS        = $FF29
GRAPH_DRAW_LINE         = $FF2C
GRAPH_DRAW_RECT         = $FF2F
GRAPH_MOVE_RECT         = $FF32
GRAPH_DRAW_OVAL         = $FF35
GRAPH_DRAW_IMAGE        = $FF38
GRAPH_SET_FONT          = $FF3B
GRAPH_GET_CHAR_SIZE     = $FF3E
GRAPH_PUT_CHAR          = $FF41

MACPTR                  = $FF44
ENTER_BASIC             = $FF47
CLOSE_ALL               = $FF4A
CLOCK_SET_DATE_TIME     = $FF4D
CLOCK_GET_DATE_TIME     = $FF50
JOYSTICK_SCAN           = $FF53
JOYSTICK_GET            = $FF56
LKUPLA                  = $FF59
LKUPSA                  = $FF5C
SCREEN_MODE             = $FF5F
SCREEN_SET_CHARSET      = $FF62
MOUSE_CONFIG            = $FF68
MOUSE_GET               = $FF6B
JSRFAR                  = $FF6E   ; jsr JSRFAR : .word addr : .byte bank
MOUSE_SCAN              = $FF71
INDFET                  = $FF74
STASH                   = $FF77
PRIMM                   = $FF7D

; --- classic C64-compatible table ($FF81-$FFF3) --------------------
CINT                    = $FF81   ; restore default text mode
IOINIT                  = $FF84
RAMTAS                  = $FF87
RESTOR                  = $FF8A
VECTOR                  = $FF8D
SETMSG                  = $FF90
SECOND                  = $FF93
TKSA                    = $FF96
MEMTOP                  = $FF99
MEMBOT                  = $FF9C
SCNKEY                  = $FF9F
SETTMO                  = $FFA2
ACPTR                   = $FFA5
CIOUT                   = $FFA8
UNTLK                   = $FFAB
UNLSN                   = $FFAE
LISTEN                  = $FFB1
TALK                    = $FFB4
READST                  = $FFB7
SETLFS                  = $FFBA
SETNAM                  = $FFBD
OPEN                    = $FFC0
CLOSE                   = $FFC3
CHKIN                   = $FFC6
CHKOUT                  = $FFC9
CLRCHN                  = $FFCC
CHRIN                   = $FFCF
CHROUT                  = $FFD2
LOAD                    = $FFD5
SAVE                    = $FFD8
SETTIM                  = $FFDB
RDTIM                   = $FFDE
STOP                    = $FFE1
GETIN                   = $FFE4
CLALL                   = $FFE7
UDTIM                   = $FFEA
SCREEN                  = $FFED
PLOT                    = $FFF0
IOBASE                  = $FFF3

; ---------------------------------------------------------------------
; KERNAL editor variables.
;
; NOT part of the jump table -- these are internal addresses, verified
; against x16-rom-r49 (kernal/cbm/editor.s, kernal.sym). They can move
; between ROM revisions, unlike everything above.
; ---------------------------------------------------------------------
KERNAL_COLOR = $0376    ; active text colour: fg | bg<<4

; ---------------------------------------------------------------------
; KERNAL indirect vectors.
; ---------------------------------------------------------------------
CINV  = $0314           ; IRQ handler vector
CBINV = $0316           ; BRK handler vector
NMINV = $0318           ; NMI handler vector

; ---------------------------------------------------------------------
; Selected PETSCII / control codes.
; ---------------------------------------------------------------------
PETSCII_WHITE     = $05
PETSCII_RETURN    = $0D
PETSCII_LOWERCASE = $0E
PETSCII_CLS       = $93
PETSCII_HOME      = $13
PETSCII_UPPERCASE = $8E

	endif
