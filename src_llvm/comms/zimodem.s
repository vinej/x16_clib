; =====================================================================
; x16clib :: comms/zimodem.s -- ZiModem (ESP32 WiFi) over the serial card
; =====================================================================
; The WiFi half of the serial card is an ESP32 running ZiModem firmware.
; You drive it as a Hayes-style modem: send "AT..." command lines over
; UART 0 and read the replies back, "OK\r\n" on success. This layer is a
; thin skin over comms/serial.s's ser_* primitives -- it frames the AT
; commands and matches the replies; it is not the ESP32 firmware.
;
;       x16_zi_init(x16_ser_uart0(), X16_SER_BAUD_115200);
;       x16_zi_cmd("atd\"host:port\"");   // dial
;       x16_zi_wait_ok();
;
; zi_init leaves the same UART selected that ser_init did, so every
; x16_ser_* call keeps working alongside these.
;
; A NOTE ON TESTING. ZiModem is an interactive protocol: nearly every
; routine here blocks reading the ESP32's reply (through
; ser_discard_until / ser_read_until / ser_get_wait). The emulator has no
; ESP32 and never fills the receive FIFO, so those flows cannot run
; headless -- they are verified on real hardware. What the test suite
; DOES pin on-target is the real logic: zi_hexdecode (the hex-mode
; payload decoder) and zi_cmd's transmit path. The rest is documented,
; not emulator-run.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; (import dropped: popax)
; (import dropped: ser_init)
; (import dropped: ser_put)
; (import dropped: ser_puts)
; (import dropped: ser_get_wait)
; (import dropped: ser_read_until)
; (import dropped: ser_discard_until)

        .globl  x16_zi_init
        .globl  x16_zi_reset
        .globl  x16_zi_cmd
        .globl  x16_zi_wait_ok
        .globl  x16_zi_get_ip
        .globl  x16_zi_hex_open
        .globl  x16_zi_hex_chunk
        .globl  x16_zi_hex_close
        .globl  x16_zi_hexdecode
        .globl  x16_zi_delay

; ---------------------------------------------------------------------
; ca65 -t cx16 TRANSLATES CHARACTER AND STRING LITERALS TO PETSCII. The
; ESP32 speaks ASCII: `.byte "atz"` would leave as $41 $54 $5A -- which
; happens to be "ATZ", but uppercase text and every compared character
; would silently shift. Every byte that goes over the wire, or that is
; compared against one coming back, is therefore written as its explicit
; ASCII value here.
; ---------------------------------------------------------------------

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_zi_init(unsigned int uart, unsigned int divisor)
;   The divisor (rightmost arg) arrives in A/X and goes to X16_P0/P1,
;   which frees A/X for the popped UART base -- what zi_init wants.
; ---------------------------------------------------------------------
x16_zi_init:
        ldy     mos8(__rc2)             ; divisor, moved through Y so the
        sty     mos8(X16_P0)            ; uart base stays put in A/X, which is
        ldy     mos8(__rc3)             ; where the internal routine wants
        sty     mos8(X16_P1)            ; it
        jmp     zi_init

; ---------------------------------------------------------------------
; void x16_zi_reset(void)
; void x16_zi_wait_ok(void)
; void x16_zi_hex_close(void)
; void __fastcall__ x16_zi_cmd(const char *cmd)
; void __fastcall__ x16_zi_get_ip(char *buf)
; void __fastcall__ x16_zi_delay(unsigned char ticks)
;   All arrive in the registers the internals want.
; ---------------------------------------------------------------------
x16_zi_reset:
        jmp     zi_reset

x16_zi_wait_ok:
        jmp     zi_wait_ok

x16_zi_hex_close:
        jmp     zi_hex_close

x16_zi_cmd:
        lda     mos8(__rc2)             ; a lone pointer never lands in
        ldx     mos8(__rc3)             ; A/X -- it takes an __rc pair,
                                        ; and the routine below wants A/X
        jmp     zi_cmd

x16_zi_get_ip:
        lda     mos8(__rc2)             ; a lone pointer never lands in
        ldx     mos8(__rc3)             ; A/X -- it takes an __rc pair,
                                        ; and the routine below wants A/X
        jmp     zi_get_ip

x16_zi_delay:
        jmp     zi_delay

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_zi_hex_open(const char *name)
;   0 = transfer started, 1 = file not found (the internal carry).
; ---------------------------------------------------------------------
x16_zi_hex_open:
        jsr     zi_hex_open
        lda     #0
        adc     #0                      ; A = the carry: 1 = not found
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_zi_hex_chunk(unsigned char *buf)
;   Bytes decoded into buf (up to 44); 0 when the file is done.
; ---------------------------------------------------------------------
x16_zi_hex_chunk:
        jsr     zi_hex_chunk
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_zi_hexdecode(unsigned char *dest,
;                                             const char *src,
;                                             unsigned char ndigits)
;   popax clobbers Y, so the digit count rides the hardware stack across
;   BOTH pointer pops and lands in Y at the last moment; the source
;   pointer rides it across the second.
; ---------------------------------------------------------------------
x16_zi_hexdecode:
        tay                             ; Y = ndigits (it arrives in A)
        lda     mos8(__rc2)             ; dest, into the library's own
        sta     mos8(X16_P0)            ; block -- no aliasing there
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        lda     mos8(__rc4)             ; src, into A/X for the call
        ldx     mos8(__rc5)
        jsr     zi_hexdecode
        ldx     #0
        rts

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; zi_init -- put the ESP32 into a known command state.
;   in:  A = UART base low, X = UART base high
;        X16_P0/P1 = baud divisor (an X16_SER_BAUD_* constant)
; Programs the UART (ser_init), lets the board settle, aborts any stream
; left running with CTRL-C, then applies the standard ZiModem config
; (echo off, verbose result codes, stream mode) and waits for "OK".
; ---------------------------------------------------------------------
zi_init:
        jsr     ser_init                ; program + select the UART
        lda     #4                      ; the ESP32 may still be booting
        jsr     zi_delay
        lda     #$03                    ; CTRL-C: abort any prior file stream
        jsr     ser_put
        lda     #2
        jsr     zi_delay
        lda     #<zi_cfg                ; ate0q0v1x1f0r1s45=3&p0&k3
        ldx     #>zi_cfg
        jsr     zi_cmd
        jmp     zi_wait_ok

; ---------------------------------------------------------------------
; zi_reset -- issue ATZ, returning the modem to its saved profile.
; ---------------------------------------------------------------------
zi_reset:
        lda     #$03
        jsr     ser_put
        lda     #2
        jsr     zi_delay
        lda     #<zi_atz
        ldx     #>zi_atz
        jsr     zi_cmd
        jmp     zi_wait_ok

; ---------------------------------------------------------------------
; zi_cmd -- send an AT command line.
;   in:  A = command low, X = command high (NUL-terminated, no CR)
; Appends the CR/LF the firmware expects. Pure transmit -- it does NOT
; read the reply; follow with zi_wait_ok (or your own read) when the
; command answers with "OK".
; ---------------------------------------------------------------------
zi_cmd:
        jsr     ser_puts                ; the command text
        lda     #<zi_crlf
        ldx     #>zi_crlf
        jmp     ser_puts                ; the line ending

; ---------------------------------------------------------------------
; zi_wait_ok -- read and discard the reply up to and including "OK\r\n".
; Blocks on the UART -- for a connected board.
; ---------------------------------------------------------------------
zi_wait_ok:
        lda     #<zi_ok
        ldx     #>zi_ok
        jmp     ser_discard_until

; ---------------------------------------------------------------------
; zi_get_ip -- fetch the current IPv4 address as a NUL-terminated string.
;   in:  A = buffer low, X = buffer high (>= 25 bytes)
; Sends ATI2, reads the reply, and trims it at the first whitespace so
; the buffer holds just the dotted-quad. Blocks -- hardware.
; ---------------------------------------------------------------------
zi_get_ip:
        sta     zi_dest
        stx     zi_dest+1
        lda     #<zi_ati2
        ldx     #>zi_ati2
        jsr     zi_cmd                  ; ATI2 -> the board prints its IP then OK
        lda     zi_dest                 ; read the reply into the caller's buffer
        sta     mos8(X16_P0)
        lda     zi_dest+1
        sta     mos8(X16_P1)
        lda     #24
        sta     mos8(X16_P2)
        stz     mos8(X16_P3)
        lda     #<zi_ok
        ldx     #>zi_ok
        jsr     ser_read_until          ; up to and including "OK\r\n"
        lda     zi_dest                 ; a zero-page cursor to walk the reply
        sta     mos8(X16_T0)
        lda     zi_dest+1
        sta     mos8(X16_T0+1)
        ldy     #0                      ; trim at the first control/space char
.Lzi_get_ip_scan:
        lda     (X16_T0),y
        cmp     #$21                    ; ' '+1: anything <= space ends the address
        bcc     .Lzi_get_ip_cut
        iny
        bne     .Lzi_get_ip_scan
.Lzi_get_ip_cut:
        lda     #0
        sta     (X16_T0),y
        rts

; ---------------------------------------------------------------------
; zi_hex_open -- begin a hex-mode file download.
;   in:  A = filename/URL low, X = filename/URL high (NUL-terminated)
;   out: carry clear = transfer started, carry set = file not found
; Switches the board to hex transfer, requests the file, and eats the
; "[..header..]" line. Then pull the payload with zi_hex_chunk until it
; returns 0, and finish with zi_hex_close. Blocks -- hardware.
; ---------------------------------------------------------------------
zi_hex_open:
        sta     zi_fname
        stx     zi_fname+1
        lda     #<zi_ats45              ; ats45=1 : enable hex-mode transfer
        ldx     #>zi_ats45
        jsr     zi_cmd
        jsr     zi_wait_ok
        lda     #<zi_atg                ; at&g"
        ldx     #>zi_atg
        jsr     ser_puts
        lda     zi_fname
        ldx     zi_fname+1
        jsr     ser_puts                ; the filename
        lda     #<zi_qcrlf              ; " CR LF
        ldx     #>zi_qcrlf
        jsr     ser_puts
        jsr     ser_get_wait            ; '[' opens the header, anything else errs
        cmp     #$5B                    ; ASCII '['
        bne     .Lzi_hex_open_err
        lda     #<zi_crlf               ; skip the rest of the header line
        ldx     #>zi_crlf
        jsr     ser_discard_until
        clc
        rts
.Lzi_hex_open_err:
        lda     #<zi_rrerr              ; drain to the end of the "ERROR" line
        ldx     #>zi_rrerr
        jsr     ser_discard_until
        sec
        rts

; ---------------------------------------------------------------------
; zi_hex_chunk -- read the next payload chunk of a hex-mode download.
;   in:  A = buffer low, X = buffer high (must hold >= 44 bytes)
;   out: A = bytes decoded into the buffer, 0 when the file is done
; One hex line -> up to 44 raw bytes. Blocks on the UART -- hardware.
; ---------------------------------------------------------------------
zi_hex_chunk:
        sta     zi_dest
        stx     zi_dest+1
        lda     #<zi_linebuf            ; read one CR/LF-terminated line
        sta     mos8(X16_P0)
        lda     #>zi_linebuf
        sta     mos8(X16_P1)
        lda     #90
        sta     mos8(X16_P2)
        stz     mos8(X16_P3)
        lda     #<zi_crlf
        ldx     #>zi_crlf
        jsr     ser_read_until          ; P4/P5 = bytes stored (incl. the CR/LF)
        lda     mos8(X16_P5)
        bne     .Lzi_hex_chunk_data
        lda     mos8(X16_P4)            ; "OK\r\n" (4 bytes, starts 'O') ends it
        cmp     #4
        bne     .Lzi_hex_chunk_data
        lda     zi_linebuf
        cmp     #$4F                    ; ASCII 'O'
        bne     .Lzi_hex_chunk_data
        lda     #0
        rts
.Lzi_hex_chunk_data:
        lda     mos8(X16_P4)            ; digits = line length minus the CR/LF
        sec
        sbc     #2
        tay
        lda     zi_dest                 ; decode into the caller's buffer
        sta     mos8(X16_P0)
        lda     zi_dest+1
        sta     mos8(X16_P1)
        lda     #<zi_linebuf
        ldx     #>zi_linebuf
        jmp     zi_hexdecode            ; returns A = bytes produced

; ---------------------------------------------------------------------
; zi_hex_close -- swallow the trailing "OK" after the payload.
; ---------------------------------------------------------------------
zi_hex_close:
        jmp     zi_wait_ok

; ---------------------------------------------------------------------
; zi_hexdecode -- turn a run of ASCII hex digits into packed bytes.
;   in:  A = source low, X = source high (uppercase hex text)
;        Y = number of digits (even)
;        X16_P0/P1 = destination pointer
;   out: A = bytes written (Y / 2); X16_P0/P1 advanced past them
; The one piece of ZiModem logic with an independent oracle, so it is a
; standalone routine the test suite drives directly.
; ---------------------------------------------------------------------
zi_hexdecode:
        sta     mos8(X16_T4)            ; T4/T5 = source cursor
        stx     mos8(X16_T5)
        sty     mos8(X16_T6)            ; T6 = digits left
        stz     mos8(X16_T7)            ; T7 = bytes produced
.Lzi_hexdecode_loop:
        lda     mos8(X16_T6)
        beq     .Lzi_hexdecode_done
        ldy     #0
        lda     (X16_T4),y              ; high nibble digit
        jsr     zimodem_nib
        asl
        asl
        asl
        asl
        sta     mos8(X16_T3)
        ldy     #1
        lda     (X16_T4),y              ; low nibble digit
        jsr     zimodem_nib
        ora     mos8(X16_T3)
        sta     (X16_P0)                ; store the packed byte
        inc     mos8(X16_P0)
        bne     .Lzi_hexdecode_dst
        inc     mos8(X16_P1)
.Lzi_hexdecode_dst:
        lda     mos8(X16_T4)            ; source += 2
        clc
        adc     #2
        sta     mos8(X16_T4)
        bcc     .Lzi_hexdecode_src
        inc     mos8(X16_T5)
.Lzi_hexdecode_src:
        inc     mos8(X16_T7)
        dec     mos8(X16_T6)
        dec     mos8(X16_T6)
        bra     .Lzi_hexdecode_loop
.Lzi_hexdecode_done:
        lda     mos8(X16_T7)
        rts

; one ASCII hex digit in A -> its 0..15 value (uppercase A-F)
zimodem_nib:
        sec
        sbc     #$30                    ; ASCII '0'
        cmp     #10
        bcc     zimodem_nib_lo
        sbc     #7                      ; 'A'-'0'-10: fold 'A'..'F' onto 10..15
zimodem_nib_lo:
        rts

; ---------------------------------------------------------------------
; zi_delay -- a coarse busy-wait so the ESP32 can keep up.
;   in:  A = ticks (~40 ms each at 8 MHz; timing is approximate)
; Self-contained (no jiffy IRQ, no KERNAL), so it works in any context.
; ---------------------------------------------------------------------
zi_delay:
        tax
        beq     .Lzi_delay_done
.Lzi_delay_tick:
        lda     #0                      ; A: 256-step middle counter
.Lzi_delay_mid:
        ldy     #0
.Lzi_delay_inner:
        iny
        bne     .Lzi_delay_inner                  ; 256 inner steps
        inc     a
        bne     .Lzi_delay_mid                    ; 256 middle steps -> 65536 inner
        dex
        bne     .Lzi_delay_tick
.Lzi_delay_done:
        rts

; ---------------------------------------------------------------------
; The AT commands, in explicit ASCII (see the PETSCII note above).
; ---------------------------------------------------------------------
        .section .rodata,"a",@progbits

; "ate0q0v1x1f0r1s45=3&p0&k3" -- echo off, quiet off, verbose, extended
; result codes, flow control, stream mode
zi_cfg:   .byte $61,$74,$65,$30,$71,$30,$76,$31,$78,$31,$66,$30
          .byte $72,$31,$73,$34,$35,$3D,$33,$26,$70,$30,$26,$6B,$33,$00
zi_atz:   .byte $61,$74,$7A,$00                     ; "atz"
zi_ati2:  .byte $61,$74,$69,$32,$00                 ; "ati2"
zi_ats45: .byte $61,$74,$73,$34,$35,$3D,$31,$00     ; "ats45=1"
zi_atg:   .byte $61,$74,$26,$67,$22,$00             ; at&g"
zi_qcrlf: .byte $22,$0D,$0A,$00                     ; " CR LF
zi_crlf:  .byte $0D,$0A,$00
zi_ok:    .byte $4F,$4B,$0D,$0A,$00                 ; "OK" CR LF
zi_rrerr: .byte $52,$52,$4F,$52,$0D,$0A,$00         ; "RROR" CR LF

        .section .bss,"aw",@nobits

zi_dest:    .zero  2                      ; caller's current destination buffer
zi_fname:   .zero  2
zi_linebuf: .zero  90                     ; one hex line, read before decoding
