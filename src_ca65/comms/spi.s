; =====================================================================
; x16clib :: comms/spi.s -- VERA SPI controller helpers
; =====================================================================
; VERA's SPI controller is exposed at VERA_SPI_DATA/CTRL. Writing DATA
; starts one full-duplex transfer; BUSY stays set until the received byte
; can be read back from DATA. SELECT asserts chip-select when set.
;
; The SD card hangs off this bus, and the KERNAL's DOS drives it through
; these same two registers -- do not run transfers of your own while a
; KERNAL load or save is in flight.
;
; Buffer routines use r0 = RAM pointer and r1 = byte count. They advance
; r0 to one byte past the buffer and leave r1 = 0.
; =====================================================================

        .include        "macros.inc"

        .import         popax

        .export         _x16_spi_get_ctrl
        .export         _x16_spi_set_ctrl
        .export         _x16_spi_wait
        .export         _x16_spi_select
        .export         _x16_spi_deselect
        .export         _x16_spi_slow
        .export         _x16_spi_fast
        .export         _x16_spi_autotx_on
        .export         _x16_spi_autotx_off
        .export         _x16_spi_transfer
        .export         _x16_spi_write
        .export         _x16_spi_read
        .export         _x16_spi_autotx_read
        .export         _x16_spi_read_bytes
        .export         _x16_spi_write_bytes

        .segment        "CODE"

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_spi_get_ctrl(void)
; void __fastcall__ x16_spi_set_ctrl(unsigned char bits)
; ---------------------------------------------------------------------
_x16_spi_get_ctrl:
        jsr     spi_get_ctrl
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; The remaining control entries take nothing and return nothing: the
; internal routines already ARE the C ABI shape, so they get dual labels
; or a bare jmp rather than a shim.
; ---------------------------------------------------------------------

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_spi_transfer(unsigned char out)
; unsigned char x16_spi_read(void)
; unsigned char x16_spi_autotx_read(void)
;   Byte returns need X cleared for int-promoting callers.
; ---------------------------------------------------------------------
_x16_spi_transfer:
        jsr     spi_transfer
        ldx     #0
        rts

_x16_spi_read:
        jsr     spi_read
        ldx     #0
        rts

_x16_spi_autotx_read:
        jsr     spi_autotx_read
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_spi_read_bytes(void *dest, unsigned int count)
; void __fastcall__ x16_spi_write_bytes(const void *src, unsigned int count)
;   The count (rightmost arg) arrives in A/X and goes straight to r1;
;   only then is popax free to fetch the pointer into A/X for r0.
; ---------------------------------------------------------------------
_x16_spi_read_bytes:
        sta     r1L                     ; count
        stx     r1H
        jsr     popax                   ; dest: A = low, X = high
        sta     r0L
        stx     r0H
        jmp     spi_read_bytes

_x16_spi_write_bytes:
        sta     r1L                     ; count
        stx     r1H
        jsr     popax                   ; src: A = low, X = high
        sta     r0L
        stx     r0H
        jmp     spi_write_bytes

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; spi_get_ctrl -- read SPI_CTRL
;   out: A = VERA_SPI_* control/status bits
; ---------------------------------------------------------------------
spi_get_ctrl:
        lda     VERA_SPI_CTRL
        rts

; ---------------------------------------------------------------------
; spi_set_ctrl -- write SPI_CTRL
;   in: A = VERA_SPI_SELECT/SLOWCLK/AUTOTX bits
; ---------------------------------------------------------------------
_x16_spi_set_ctrl:
spi_set_ctrl:
        sta     VERA_SPI_CTRL
        rts

; ---------------------------------------------------------------------
; spi_wait -- wait for the active transfer to finish
; ---------------------------------------------------------------------
_x16_spi_wait:
spi_wait:
        bit     VERA_SPI_CTRL           ; BUSY is bit 7: bit copies it to N
        bmi     spi_wait
        rts

; ---------------------------------------------------------------------
; spi_select / spi_deselect -- assert or release chip-select
; ---------------------------------------------------------------------
_x16_spi_select:
spi_select:
        lda     VERA_SPI_CTRL
        ora     #VERA_SPI_SELECT
        sta     VERA_SPI_CTRL
        rts

_x16_spi_deselect:
spi_deselect:
        lda     VERA_SPI_CTRL
        and     #<~VERA_SPI_SELECT
        sta     VERA_SPI_CTRL
        rts

; ---------------------------------------------------------------------
; spi_slow / spi_fast -- select ~390 kHz or ~12.5 MHz SPI clock
; ---------------------------------------------------------------------
_x16_spi_slow:
spi_slow:
        lda     VERA_SPI_CTRL
        ora     #VERA_SPI_SLOWCLK
        sta     VERA_SPI_CTRL
        rts

_x16_spi_fast:
spi_fast:
        lda     VERA_SPI_CTRL
        and     #<~VERA_SPI_SLOWCLK
        sta     VERA_SPI_CTRL
        rts

; ---------------------------------------------------------------------
; spi_autotx_on / spi_autotx_off
;   Auto-TX makes each SPI_DATA read start a new $FF transfer.
; ---------------------------------------------------------------------
_x16_spi_autotx_on:
spi_autotx_on:
        lda     VERA_SPI_CTRL
        ora     #VERA_SPI_AUTOTX
        sta     VERA_SPI_CTRL
        rts

_x16_spi_autotx_off:
spi_autotx_off:
        lda     VERA_SPI_CTRL
        and     #<~VERA_SPI_AUTOTX
        sta     VERA_SPI_CTRL
        rts

; ---------------------------------------------------------------------
; spi_transfer -- transmit A, wait, then return the received byte
;   in:  A = byte to transmit
;   out: A = received byte
; ---------------------------------------------------------------------
spi_transfer:
        sta     VERA_SPI_DATA
        jsr     spi_wait
        lda     VERA_SPI_DATA
        rts

; ---------------------------------------------------------------------
; spi_write -- transmit A and wait; received byte is discarded
; ---------------------------------------------------------------------
_x16_spi_write:
spi_write:
        sta     VERA_SPI_DATA
        jmp     spi_wait

; ---------------------------------------------------------------------
; spi_read -- transmit $FF, wait, then return the received byte
;   out: A = received byte
; ---------------------------------------------------------------------
spi_read:
        lda     #$FF
        jmp     spi_transfer

; ---------------------------------------------------------------------
; spi_autotx_read -- wait, then read DATA in Auto-TX mode
;   out: A = received byte; the read starts the next $FF transfer
; ---------------------------------------------------------------------
spi_autotx_read:
        jsr     spi_wait
        lda     VERA_SPI_DATA
        rts

; ---------------------------------------------------------------------
; spi_read_bytes -- read bytes into RAM
;   in:  r0 = destination pointer, r1 = byte count
;   out: r0 advanced, r1 = 0
; ---------------------------------------------------------------------
spi_read_bytes:
        lda     r1L
        ora     r1H
        beq     @done
@loop:
        jsr     spi_read
        ldy     #0
        sta     (r0),y
        inc     r0L
        bne     @dec
        inc     r0H
@dec:
        lda     r1L
        bne     @dec_lo
        dec     r1H
@dec_lo:
        dec     r1L
        lda     r1L
        ora     r1H
        bne     @loop
@done:
        rts

; ---------------------------------------------------------------------
; spi_write_bytes -- write bytes from RAM
;   in:  r0 = source pointer, r1 = byte count
;   out: r0 advanced, r1 = 0
; ---------------------------------------------------------------------
spi_write_bytes:
        lda     r1L
        ora     r1H
        beq     @done
@loop:
        ldy     #0
        lda     (r0),y
        jsr     spi_write
        inc     r0L
        bne     @dec
        inc     r0H
@dec:
        lda     r1L
        bne     @dec_lo
        dec     r1H
@dec_lo:
        dec     r1L
        lda     r1L
        ora     r1H
        bne     @loop
@done:
        rts
