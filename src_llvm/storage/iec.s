; =====================================================================
; x16clib :: storage/iec.s -- low-level IEC / serial bus wrappers
; =====================================================================
; Direct helpers for the classic Commodore serial bus / IEC KERNAL calls.
; Most programs should use FILEIO, LOAD, DOS, or BMX instead; this gate
; is for protocols that need explicit bus control.
;
; MACPTR and MCIOUT are X16 block transfers for the current channel:
;       A   = byte count, 0 lets the implementation choose
;       X/Y = destination/source pointer
;       X/Y = bytes transferred on return
;       C   = set when unsupported/error
; The C bindings fold that carry into a -1 return, like getchar().
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; (import dropped: popa)

        .globl  x16_iec_listen
        .globl  x16_iec_talk
        .globl  x16_iec_second
        .globl  x16_iec_tksa
        .globl  x16_iec_ciout
        .globl  x16_iec_acptr
        .globl  x16_iec_unlisten
        .globl  x16_iec_untalk
        .globl  x16_iec_set_timeout
        .globl  x16_iec_readst
        .globl  x16_iec_macptr
        .globl  x16_iec_mciout
        .globl  x16_iec_open_channel
        .globl  x16_iec_data_channel
        .globl  x16_iec_talk_channel
        .globl  x16_iec_close_channel

IEC_CMD_DATA  = $60             ; secondary data channel command base
IEC_CMD_CLOSE = $E0             ; close channel command base
IEC_CMD_OPEN  = $F0             ; open channel command base

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_iec_listen(unsigned char device)
; void __fastcall__ x16_iec_talk(unsigned char device)
; void __fastcall__ x16_iec_second(unsigned char cmd)  -- after LISTEN
; void __fastcall__ x16_iec_tksa(unsigned char cmd)    -- after TALK
; void __fastcall__ x16_iec_ciout(unsigned char b)     -- send one byte
; unsigned char x16_iec_acptr(void)                    -- receive one byte
; void x16_iec_unlisten(void)
; void x16_iec_untalk(void)
; void __fastcall__ x16_iec_set_timeout(unsigned char t) -- ROM r49 no-op
; unsigned char x16_iec_readst(void)                   -- serial status
; ---------------------------------------------------------------------
x16_iec_listen:
        jmp     LISTEN

x16_iec_talk:
        jmp     TALK

x16_iec_second:
        jmp     SECOND

x16_iec_tksa:
        jmp     TKSA

x16_iec_ciout:
        jmp     CIOUT

x16_iec_acptr:
        jsr     ACPTR
        ldx     #0
        rts

x16_iec_unlisten:
        jmp     UNLSN

x16_iec_untalk:
        jmp     UNTLK

x16_iec_set_timeout:
        jmp     SETTMO

x16_iec_readst:
        jsr     READST
        ldx     #0
        rts

; ---------------------------------------------------------------------
; int __fastcall__ x16_iec_macptr(unsigned char count, void *dest)
; int __fastcall__ x16_iec_mciout(unsigned char count, const void *src)
;   Bytes transferred, or -1 when the current channel cannot do block
;   transfers (fall back to acptr/ciout one byte at a time).
; ---------------------------------------------------------------------
x16_iec_macptr:
        ldx     __rc2                   ; dest: the pointer pair
        ldy     __rc3                   ; (A already holds count)
        clc                             ; C in = 0: ADVANCE the pointer. Set,
                                        ; it pins the address for port I/O --
                                        ; and it arrives here as stack litter.
        jsr     MACPTR                  ; out: X = low, Y = high, C = error
        bcs     .Lx16_iec_macptr_bad
        txa                             ; return A = low, X = high
        phy
        plx
        rts
.Lx16_iec_macptr_bad:
        lda     #$FF                    ; -1
        tax
        rts

x16_iec_mciout:
        ldx     __rc2                   ; src: the pointer pair
        ldy     __rc3                   ; (A already holds count)
        clc                             ; C in = 0: advance (see macptr above)
        jsr     MCIOUT                  ; out: X = low, Y = high, C = error
        bcs     .Lx16_iec_mciout_bad
        txa
        phy
        plx
        rts
.Lx16_iec_mciout_bad:
        lda     #$FF                    ; -1
        tax
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_iec_open_channel(unsigned char device,
;                                        unsigned char secondary)
; ...and data/talk/close, all with the same shape.
;
; llvm-mos puts the device in A and the secondary in X; the internals
; want A = device, Y = secondary.
; ---------------------------------------------------------------------
x16_iec_open_channel:
        phx                     ; secondary X -> Y, leaving
        ply                     ; A (the device) untouched
        jmp     iec_open_channel

x16_iec_data_channel:
        phx                     ; secondary X -> Y, leaving
        ply                     ; A (the device) untouched
        jmp     iec_data_channel

x16_iec_talk_channel:
        phx                     ; secondary X -> Y, leaving
        ply                     ; A (the device) untouched
        jmp     iec_talk_channel

x16_iec_close_channel:
        phx                     ; secondary X -> Y, leaving
        ply                     ; A (the device) untouched
        jmp     iec_close_channel

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; iec_open_channel -- LISTEN device, send OPEN secondary command
;   in: A = device number, Y = secondary channel
; ---------------------------------------------------------------------
iec_open_channel:
        jsr     LISTEN
        tya
        ora     #IEC_CMD_OPEN
        jmp     SECOND

; ---------------------------------------------------------------------
; iec_data_channel -- LISTEN device, send DATA secondary command
;   in: A = device number, Y = secondary channel
; ---------------------------------------------------------------------
iec_data_channel:
        jsr     LISTEN
        tya
        ora     #IEC_CMD_DATA
        jmp     SECOND

; ---------------------------------------------------------------------
; iec_talk_channel -- TALK device, send DATA secondary command
;   in: A = device number, Y = secondary channel
; ---------------------------------------------------------------------
iec_talk_channel:
        jsr     TALK
        tya
        ora     #IEC_CMD_DATA
        jmp     TKSA

; ---------------------------------------------------------------------
; iec_close_channel -- LISTEN device, send CLOSE secondary command
;   in: A = device number, Y = secondary channel
; ---------------------------------------------------------------------
iec_close_channel:
        jsr     LISTEN
        tya
        ora     #IEC_CMD_CLOSE
        jmp     SECOND
