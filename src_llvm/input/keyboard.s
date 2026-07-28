; =====================================================================
; x16clib :: input/keyboard.s -- keyboard buffer and layout helpers
; =====================================================================
; The X16-specific keyboard surface beyond input.s's key_get/key_peek:
; injecting keys into the KERNAL buffer, reading the modifier bitfield,
; and querying or switching the active keyboard layout.
;
; Ported from x16_library input/keyboard.asm. Two upstream routines are
; NOT duplicated here because input.s already wraps them:
;
;       upstream kbd_peek  -> x16_key_peek()  (x16/input.h)
;       (GETIN consumption) -> x16_key_get() / x16_key_wait()
;
; The KEYMAP name lives in the KERNAL's variable space in RAM BANK 0
; ($A000-$BFFF), so the pointer KEYMAP returns is useless to a C caller
; whose RAM bank is anything else. x16_kbd_get_keymap therefore copies
; the name out with bank 0 mapped, into a caller buffer.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; cc65 lends ptr1/ptr2 for indirection; llvm-mos has no such
; runtime, so these use the library's own scratch pairs.

        .globl  x16_kbd_scan
        .globl  x16_kbd_put
        .globl  x16_kbd_get_modifiers
        .globl  x16_kbd_get_keymap
        .globl  x16_kbd_set_keymap

; The KERNAL's kbdnam buffer is KBDNAM_LEN = 14 bytes, NUL included
; (x16-rom-r49 kernal/drivers/x16/ps2kbd.s). Mirrored as
; X16_KBD_KEYMAP_LEN in the header.
KEYMAP_LEN = 14

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void x16_kbd_scan(void)
;
; Scan the keyboard once. The KERNAL's own IRQ already does this every
; frame; you only need it if you have taken the interrupt over.
; ---------------------------------------------------------------------
x16_kbd_scan:
        jmp     SCNKEY

; ---------------------------------------------------------------------
; void __fastcall__ x16_kbd_put(unsigned char key)
;
; Append a PETSCII key to the keyboard buffer, as if it had been typed.
; Read it back with x16_key_get()/x16_key_peek() or plain GETIN.
; ---------------------------------------------------------------------
x16_kbd_put:
        jmp     KBDBUF_PUT

; ---------------------------------------------------------------------
; unsigned char x16_kbd_get_modifiers(void)
;   returns the X16_KBD_MOD_* bits
; ---------------------------------------------------------------------
x16_kbd_get_modifiers:
        jsr     KBDBUF_GET_MODIFIERS
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_kbd_get_keymap(char *name)
;   returns the layout index; copies the NUL-terminated layout name
;   (at most X16_KBD_KEYMAP_LEN bytes, NUL included) into `name`
;
; KEYMAP with carry set answers A = index, X/Y = name pointer -- but the
; name sits in RAM bank 0. Copy it out with that bank mapped, then put
; the caller's bank back. The KERNAL IRQ saves the bank itself, so an
; interrupt in the middle is harmless.
; ---------------------------------------------------------------------
x16_kbd_get_keymap:
        lda     mos8(__rc2)             ; name buffer: the pointer pair
        sta     mos8(X16_TPTR0)
        lda     mos8(__rc3)
        sta     mos8(X16_TPTR0+1)

        sec
        jsr     KEYMAP                  ; A = index, X = name lo, Y = name hi
        stx     mos8(X16_TPTR1)
        sty     mos8(X16_TPTR1+1)
        pha                             ; park the index

        lda     RAM_BANK
        pha
        stz     RAM_BANK                ; kbdnam lives in bank 0

        ldy     #0
.Lx16_kbd_get_keymap_copy:
        lda     (X16_TPTR1),y
        sta     (X16_TPTR0),y
        beq     .Lx16_kbd_get_keymap_done                   ; NUL copied
        iny
        cpy     #KEYMAP_LEN - 1
        bne     .Lx16_kbd_get_keymap_copy
        lda     #0                      ; ROM guarantees a NUL, but never
        sta     (X16_TPTR0),y                ; trust it past the buffer's end
.Lx16_kbd_get_keymap_done:
        pla
        sta     RAM_BANK

        pla                             ; A = index
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_kbd_set_keymap(const char *name)
;   returns 1 on success, 0 if the layout is unknown
;
; `name` is the NUL-terminated layout identifier, byte-for-byte as the
; ROM stores it (e.g. "en-us"). It must sit below $A000: the KERNAL
; compares it while its own RAM bank is mapped. On failure the previous
; layout stays active.
; ---------------------------------------------------------------------
x16_kbd_set_keymap:
        ldx     mos8(__rc2)             ; KEYMAP wants X = lo, Y = hi
        ldy     mos8(__rc3)

        clc
        jsr     KEYMAP                  ; carry set on unknown layout
        lda     #0
        ldx     #0
        bcs     .Lx16_kbd_set_keymap_fail
        lda     #1
.Lx16_kbd_set_keymap_fail:
        rts
