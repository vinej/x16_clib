; =====================================================================
; x16clib :: core/macros.inc -- inlined plumbing
; =====================================================================
; Pure macro file. Safe to .include any number of times.
; Requires --cpu 65C02 (uses trb/tsb).
;
; The ACME original's `basic_stub` macro is gone: cc65's cx16 runtime
; emits the load address and the `10 SYS` header itself.
; =====================================================================

.ifndef X16_MACROS_INC
X16_MACROS_INC = 1

        .include        "const_zp.inc"
        .include        "const_vera.inc"
        .include        "const_kernal.inc"
        .include        "const_rom.inc"

; ---------------------------------------------------------------------
; vera_addrsel port         select which data port the ADDR registers
;                           refer to. Clobbers A and flags.
;
; ADDRSEL is bit 0 of CTRL. DCSEL is bits 6:1. A plain
; `lda #0 : sta VERA_CTRL` would silently reset DCSEL too, so this does
; a read-modify-write via the 65C02's trb/tsb.
;
; NOTE: DATA0 always talks to port 0 and DATA1 always to port 1,
; regardless of ADDRSEL. ADDRSEL only picks which port's ADDR_L/M/H are
; visible at $9F20-$9F22. That is what lets vera_copy stream through
; both ports without touching CTRL in the inner loop.
; ---------------------------------------------------------------------
.macro  vera_addrsel port
        lda     #VERA_CTRL_ADDRSEL
.if (port) = 0
        trb     VERA_CTRL               ; clear bit 0
.else
        tsb     VERA_CTRL               ; set bit 0
.endif
.endmacro

; ---------------------------------------------------------------------
; vera_dcsel n              select the $9F29-$9F2C register bank (0-63).
;                           Clobbers A and flags. Preserves ADDRSEL.
;                           Never writes bit 7 (that resets VERA).
; ---------------------------------------------------------------------
.macro  vera_dcsel n
.if (n) > 63
        .fatal  "DCSEL must be 0-63"
.endif
        lda     VERA_CTRL
        and     #VERA_CTRL_ADDRSEL
        ora     #((n) << 1)
        sta     VERA_CTRL
.endmacro

; ---------------------------------------------------------------------
; vera_addr port, addr, inc     point a data port at a 17-bit VRAM
;                               address. Clobbers A and flags.
;
; `inc` is an INDEX, not a byte count -- use the VERA_INC_* constants
; from const_vera.inc (NOT the pre-shifted ones in cc65's cx16.h).
;   vera_addr 0, VRAM_TEXT, VERA_INC_1
;   vera_addr 1, VRAM_BITMAP + 320, VERA_INC_320
; ---------------------------------------------------------------------
.macro  vera_addr port, addr, inc
.if (addr) > $1FFFF
        .fatal  "VRAM address must be 17-bit"
.endif
.if (inc) > 15
        .fatal  "use a VERA_INC_* constant, not a byte count"
.endif
        vera_addrsel port
        lda     #<(addr)
        sta     VERA_ADDR_L
        lda     #>(addr)
        sta     VERA_ADDR_M
        lda     #((^(addr)) & VERA_ADDR_H_BANK) | ((inc) << 4)
        sta     VERA_ADDR_H
.endmacro

; Same, but decrementing.
.macro  vera_addr_decr port, addr, inc
        vera_addrsel port
        lda     #<(addr)
        sta     VERA_ADDR_L
        lda     #>(addr)
        sta     VERA_ADDR_M
        lda     #((^(addr)) & VERA_ADDR_H_BANK) | VERA_ADDR_H_DECR | ((inc) << 4)
        sta     VERA_ADDR_H
.endmacro

; ---------------------------------------------------------------------
; vpoke_const addr, value   one-off VRAM byte write. Clobbers A, flags.
;
; Named `vpoke_const` rather than `vpoke` so it cannot be confused with
; cc65's runtime vpoke() function, which takes variables.
; ---------------------------------------------------------------------
.macro  vpoke_const addr, value
        vera_addr 0, addr, VERA_INC_0
        lda     #(value)
        sta     VERA_DATA0
.endmacro

; ---------------------------------------------------------------------
; Bank registers.
; ---------------------------------------------------------------------
.macro  set_rambank n
        lda     #(n)
        sta     RAM_BANK
.endmacro

.macro  set_rombank n
        lda     #(n)
        sta     ROM_BANK
.endmacro

; ---------------------------------------------------------------------
; jsrfar addr, bank         call a routine in another ROM/RAM bank.
;
; This is the KERNAL's own mechanism ($FF6E). It saves the caller's ROM
; bank, switches, calls, then restores -- preserving A, X, Y and flags,
; and it is reentrant (safe from an IRQ handler).
;
; Do NOT hand-roll this. A naive
;       lda ROM_BANK : pha : lda #bank : sta ROM_BANK : jsr entry
; cannot restore the bank afterwards without destroying the callee's
; return values in A/X/Y, because the saved byte is buried under them on
; the stack. jsrfar solves it by reserving a stack slot up front.
;
;   jsrfar rom_ym_playnote, BANK_AUDIO
; ---------------------------------------------------------------------
.macro  jsrfar addr, bank
        jsr     JSRFAR
        .word   addr
        .byte   bank
.endmacro

; ---------------------------------------------------------------------
; rom_call_fast bank, entry     ~40 cycles cheaper than jsrfar, but it
;                               CLOBBERS A and leaves ROM_BANK set to
;                               `bank`. You must restore it yourself.
;                               Not safe to use from an IRQ handler that
;                               may interrupt another bank switch.
; ---------------------------------------------------------------------
.macro  rom_call_fast bank, entry
        lda     #(bank)
        sta     ROM_BANK
        jsr     entry
.endmacro

.endif
