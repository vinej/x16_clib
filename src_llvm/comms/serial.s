; =====================================================================
; x16clib :: comms/serial.s -- the serial / WiFi card UARTs
; =====================================================================
; The Commander X16 serial / WiFi card carries up to two 16C550-style
; UARTs in the expansion I/O window. They live on 8-byte boundaries
; between $9F60 and $9FF8; the standard card populates $9F60 (UART 0)
; and $9F68 (UART 1). The WiFi half is an ESP32 running ZiModem, driven
; as an AT-command modem over UART 0 -- but that is a protocol on top of
; these bytes, not something this module knows about (comms/zimodem.s is).
;
; The register file (offset from the UART's base address):
;   0  RHR/THR   receive / transmit holding    (DLL when DLAB=1)
;   1  IER       interrupt enable              (DLM when DLAB=1)
;   2  IIR/FCR   read: interrupt id / write: FIFO control
;   3  LCR       line control (word size, parity, stop, DLAB)
;   4  MCR       modem control (DTR/RTS/loop/auto-flow)
;   5  LSR       line status (DR, THRE, errors)
;   6  MSR       modem status
;   7  SCR       scratch (no hardware effect -- used to fingerprint)
;
; ser_init programs 8N1 + FIFOs + auto-flow and leaves interrupts OFF:
; this module POLLS the UART, so it never goes near system/irq.s's CINV
; chain and is safe to use alongside it. It remembers the UART it was
; handed; ser_put/ser_get/... all talk to that one. Point them elsewhere
; by calling ser_init again.
;
; Reads have side effects on real UARTs -- reading RHR pops the RX FIFO,
; reading LSR clears the sticky error bits -- so this module never lets
; an indexed store's dummy read fall on RHR: byte writes to THR go out
; through `sta (ptr)` (no index, no dummy read on the 65C02).
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; (import dropped: popax)

        .globl  x16_ser_detect
        .globl  x16_ser_uart0
        .globl  x16_ser_uart1
        .globl  x16_ser_init
        .globl  x16_ser_avail
        .globl  x16_ser_get
        .globl  x16_ser_get_wait
        .globl  x16_ser_put
        .globl  x16_ser_puts
        .globl  x16_ser_write
        .globl  x16_ser_read_until
        .globl  x16_ser_discard_until

; comms/zimodem.s frames its AT commands through these. Nothing else
; should: the C API is the x16_ser_* layer above.
        .globl  ser_init
        .globl  ser_put
        .globl  ser_puts
        .globl  ser_get_wait
        .globl  ser_read_until
        .globl  ser_discard_until

; --- register offsets ------------------------------------------------
SER_RHR = 0                     ; = THR on write, = DLL when DLAB set
SER_IER = 1                     ; = DLM when DLAB set
SER_FCR = 2                     ; write: FIFO control (reads IIR)
SER_LCR = 3
SER_MCR = 4
SER_LSR = 5
SER_MSR = 6
SER_SCR = 7

; --- LSR bits --------------------------------------------------------
SER_LSR_DR   = %00000001        ; a received byte is ready
SER_LSR_THRE = %00100000        ; the transmit holding register is empty

SER_SCAN_FIRST = $9F60          ; first candidate UART base
SER_SCAN_LAST  = $9FF8          ; last candidate UART base
SER_SCAN_STEP  = 8              ; UARTs sit on 8-byte boundaries

        .section .text,"ax",@progbits

; =====================================================================
; C entry points
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_ser_detect(void)
; unsigned int x16_ser_uart0(void)
; unsigned int x16_ser_uart1(void)
; ---------------------------------------------------------------------
x16_ser_detect:
        jsr     ser_detect
        ldx     #0                      ; high byte, for int-promoting callers
        rts

x16_ser_uart0:
        lda     ser_u0
        ldx     ser_u0+1
        rts

x16_ser_uart1:
        lda     ser_u1
        ldx     ser_u1+1
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_ser_init(unsigned int base, unsigned int divisor)
;   The divisor (rightmost arg) arrives in A/X and goes to X16_P0/P1,
;   which frees A/X for the popped base -- exactly what ser_init wants.
; ---------------------------------------------------------------------
x16_ser_init:
        ldy     mos8(__rc2)             ; divisor, moved through Y so the
        sty     mos8(X16_P0)            ; base stays put in A/X, which is
        ldy     mos8(__rc3)             ; where the internal routine wants
        sty     mos8(X16_P1)            ; it
        jmp     ser_init

; ---------------------------------------------------------------------
; unsigned char x16_ser_avail(void)
; ---------------------------------------------------------------------
x16_ser_avail:
        jsr     ser_avail
        lda     #0
        adc     #0                      ; A = the carry: 1 = a byte waits
        ldx     #0
        rts

; ---------------------------------------------------------------------
; int x16_ser_get(void)  -- the byte, or -1 if the RX FIFO was empty
; ---------------------------------------------------------------------
x16_ser_get:
        jsr     ser_get
        bcs     .Lx16_ser_get_empty
        ldx     #0
        rts
.Lx16_ser_get_empty:
        lda     #$FF                    ; -1
        tax
        rts

; ---------------------------------------------------------------------
; unsigned char x16_ser_get_wait(void)
; ---------------------------------------------------------------------
x16_ser_get_wait:
        jsr     ser_get_wait
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_ser_put(unsigned char b)
; void __fastcall__ x16_ser_puts(const char *s)
; void __fastcall__ x16_ser_discard_until(const char *match)
;   All three already arrive in the registers the internals want.
; ---------------------------------------------------------------------
x16_ser_put:
        jmp     ser_put

x16_ser_puts:
        lda     mos8(__rc2)             ; a lone pointer never lands in
        ldx     mos8(__rc3)             ; A/X -- it takes an __rc pair,
                                        ; and the routine below wants A/X
        jmp     ser_puts

x16_ser_discard_until:
        lda     mos8(__rc2)             ; a lone pointer never lands in
        ldx     mos8(__rc3)             ; A/X -- it takes an __rc pair,
                                        ; and the routine below wants A/X
        jmp     ser_discard_until

; ---------------------------------------------------------------------
; void __fastcall__ x16_ser_write(const void *data, unsigned char len)
;   popax clobbers Y, so the length rides the hardware stack across the
;   pointer pop and lands in Y at the last moment.
; ---------------------------------------------------------------------
x16_ser_write:
        tay                             ; Y = len (it arrives in A)
        lda     mos8(__rc2)             ; A/X = data
        ldx     mos8(__rc3)
        jmp     ser_write

; ---------------------------------------------------------------------
; unsigned int __fastcall__ x16_ser_read_until(char *buf, unsigned int max,
;                                              const char *match)
;   Returns the byte count actually stored, the needle included.
; ---------------------------------------------------------------------
x16_ser_read_until:
        sta     mos8(X16_P2)            ; max, straight from A/X
        stx     mos8(X16_P3)
        lda     mos8(__rc2)             ; buf
        sta     mos8(X16_P0)
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        lda     mos8(__rc4)             ; match, into A/X for the call
        ldx     mos8(__rc5)
        jsr     ser_read_until
        lda     mos8(X16_P4)            ; bytes actually stored
        ldx     mos8(X16_P5)
        rts

; =====================================================================
; Internal routines
; =====================================================================

; ---------------------------------------------------------------------
; ser_detect -- scan the expansion window for UART chips.
;   out: A = number found (0, 1 or 2)
;        carry clear if at least one was found, set if none
;        ser_u0 = first UART base (0 if none)
;        ser_u1 = second UART base (0 if none)
;
; The probe writes and reads back three registers whose behaviour a UART
; is required to have and bare bus is not: the top nibble of IER always
; reads 0, the top two bits of MCR always read 0, and the scratch
; register holds whatever you put in it. Two different scratch patterns
; make a floating bus very unlikely to answer by accident. Interrupts
; are held off across the probe so an IRQ handler never sees the UART
; mid-fingerprint.
; ---------------------------------------------------------------------
ser_detect:
        stz     ser_u0
        stz     ser_u0+1
        stz     ser_u1
        stz     ser_u1+1
        lda     #<SER_SCAN_FIRST        ; X16_TPTR1 walks the candidate bases
        sta     mos8(X16_T2)
        lda     #>SER_SCAN_FIRST
        sta     mos8(X16_T3)
        php
        sei
.Lser_detect_scan:
        jsr     serial_probe
        bcc     .Lser_detect_next                   ; carry set from serial_probe = a UART is here
        lda     ser_u0+1                ; first slot still empty?
        ora     ser_u0
        bne     .Lser_detect_have_first
        lda     mos8(X16_T2)            ; store as UART 0
        sta     ser_u0
        lda     mos8(X16_T3)
        sta     ser_u0+1
        bra     .Lser_detect_next
.Lser_detect_have_first:
        lda     mos8(X16_T2)            ; store as UART 1 and stop
        sta     ser_u1
        lda     mos8(X16_T3)
        sta     ser_u1+1
        bra     .Lser_detect_done
.Lser_detect_next:
        clc                             ; advance the base by SER_SCAN_STEP
        lda     mos8(X16_T2)
        adc     #SER_SCAN_STEP
        sta     mos8(X16_T2)
        bcc     .Lser_detect_nohi
        inc     mos8(X16_T3)
.Lser_detect_nohi:
        lda     mos8(X16_T3)            ; past SER_SCAN_LAST?
        cmp     #>SER_SCAN_LAST
        bcc     .Lser_detect_scan
        bne     .Lser_detect_done
        lda     mos8(X16_T2)
        cmp     #<SER_SCAN_LAST
        bcc     .Lser_detect_scan
        beq     .Lser_detect_scan                   ; include SER_SCAN_LAST itself
.Lser_detect_done:
        plp
        ldx     #0                      ; count the non-zero slots
        lda     ser_u0
        ora     ser_u0+1
        beq     .Lser_detect_c0
        inx
.Lser_detect_c0:
        lda     ser_u1
        ora     ser_u1+1
        beq     .Lser_detect_c1
        inx
.Lser_detect_c1:
        txa
        beq     .Lser_detect_none                   ; count 0: nothing found
        clc                             ; carry clear = at least one UART
        rts
.Lser_detect_none:
        sec
        rts

; probe the UART whose base is in X16_TPTR1 (X16_T2/T3).
;   out: carry set = a UART answered, carry clear = nothing there
; Leaves IER and MCR at 0 either way.
serial_probe:
        ldy     #SER_IER
        lda     #$F0
        sta     (X16_T2),y
        lda     (X16_T2),y
        and     #$F0                    ; the high nibble must read back as 0
        bne     serial_no
        lda     #0
        sta     (X16_T2),y
        ldy     #SER_MCR
        lda     #$FF
        sta     (X16_T2),y
        lda     (X16_T2),y
        cmp     #$3F                    ; bits 7,6 of MCR always read 0
        bne     serial_no_mcr
        lda     #0
        sta     (X16_T2),y
        ldy     #SER_SCR                ; scratch holds two distinct patterns
        lda     #$A5
        sta     (X16_T2),y
        lda     (X16_T2),y
        cmp     #$A5
        bne     serial_no
        lda     #$5A
        sta     (X16_T2),y
        lda     (X16_T2),y
        cmp     #$5A
        bne     serial_no
        sec
        rts
serial_no_mcr:
        lda     #0                      ; leave MCR clean before bailing
        sta     (X16_T2),y
serial_no:
        clc
        rts

; ---------------------------------------------------------------------
; ser_init -- program a UART for 8N1, FIFOs on, auto-flow, no interrupts.
;   in:  A = UART base low, X = UART base high
;        X16_P0/P1 = baud divisor (an X16_SER_BAUD_* constant)
; The UART becomes "the current one" for ser_put/ser_get/etc.
; ---------------------------------------------------------------------
ser_init:
        sta     ser_base
        stx     ser_base+1
        jsr     serial_load_ptr

        ldy     #SER_LCR                ; DLAB = 1 to reach the divisor latch
        lda     #$80
        sta     (X16_T0),y
        ldy     #SER_RHR                ; DLL
        lda     mos8(X16_P0)
        sta     (X16_T0),y
        ldy     #SER_IER                ; DLM
        lda     mos8(X16_P1)
        sta     (X16_T0),y
        ldy     #SER_LCR                ; 8 bits, no parity, 1 stop, DLAB = 0
        lda     #$03
        sta     (X16_T0),y
        ldy     #SER_FCR                ; FIFO enable + reset both, RX trigger 8
        lda     #$87
        sta     (X16_T0),y
        ldy     #SER_MCR                ; DTR+RTS, auto-flow, OUT2 (ZiModem stream)
        lda     #$27
        sta     (X16_T0),y
        ldy     #SER_IER                ; no interrupts: this module polls
        lda     #$00
        sta     (X16_T0),y
        rts

; ---------------------------------------------------------------------
; ser_avail -- is a received byte waiting?
;   out: carry set = yes (LSR data-ready), carry clear = no
; ---------------------------------------------------------------------
ser_avail:
        jsr     serial_load_ptr
        ldy     #SER_LSR
        lda     (X16_T0),y
        and     #SER_LSR_DR
        beq     .Lser_avail_none
        sec
        rts
.Lser_avail_none:
        clc
        rts

; ---------------------------------------------------------------------
; ser_get -- read one byte without blocking.
;   out: carry clear + A = byte if one was waiting;
;        carry set if the RX FIFO was empty (A undefined)
; ---------------------------------------------------------------------
ser_get:
        jsr     serial_load_ptr
        ldy     #SER_LSR
        lda     (X16_T0),y
        and     #SER_LSR_DR
        beq     .Lser_get_empty
        ldy     #SER_RHR
        lda     (X16_T0),y              ; this read pops the RX FIFO
        clc
        rts
.Lser_get_empty:
        sec
        rts

; ---------------------------------------------------------------------
; ser_get_wait -- read one byte, blocking until one arrives.
;   out: A = byte
; Spins on the UART: only sane once something is actually connected.
; ---------------------------------------------------------------------
ser_get_wait:
        jsr     serial_load_ptr
.Lser_get_wait_wait:
        ldy     #SER_LSR
        lda     (X16_T0),y
        and     #SER_LSR_DR
        beq     .Lser_get_wait_wait
        ldy     #SER_RHR
        lda     (X16_T0),y
        rts

; ---------------------------------------------------------------------
; ser_put -- send one byte, waiting for room in the transmit FIFO.
;   in:  A = byte
; Preserves nothing but is safe to call in a tight loop.
; ---------------------------------------------------------------------
ser_put:
        pha
        jsr     serial_load_ptr
.Lser_put_wait:
        ldy     #SER_LSR
        lda     (X16_T0),y
        and     #SER_LSR_THRE
        beq     .Lser_put_wait                   ; hold until the holding register is empty
        pla
        sta     (X16_T0)                ; THR write: no index, so no dummy read
        rts

; ---------------------------------------------------------------------
; ser_puts -- send a NUL-terminated string.
;   in:  A = string low, X = string high
; ---------------------------------------------------------------------
ser_puts:
        sta     mos8(X16_P2)
        stx     mos8(X16_P3)
        ldy     #0
.Lser_puts_loop:
        lda     (X16_P2),y
        beq     .Lser_puts_done
        phy
        jsr     ser_put
        ply
        iny
        bne     .Lser_puts_loop
.Lser_puts_done:
        rts

; ---------------------------------------------------------------------
; ser_write -- send a counted (binary-safe) run of bytes.
;   in:  A = data low, X = data high, Y = length (1..255; 0 = 256)
; ---------------------------------------------------------------------
ser_write:
        sta     mos8(X16_P2)
        stx     mos8(X16_P3)
        sty     mos8(X16_P4)            ; remaining count
        ldy     #0
.Lser_write_loop:
        phy
        lda     (X16_P2),y
        jsr     ser_put
        ply
        iny
        dec     mos8(X16_P4)
        bne     .Lser_write_loop
        rts

; ---------------------------------------------------------------------
; ser_read_until -- read into a buffer until a match string is seen.
;   in:  A = match low, X = match high (NUL-terminated needle)
;        X16_P0/P1 = buffer address
;        X16_P2/P3 = max bytes to store
;   out: X16_P4/P5 = bytes actually stored
; The matched needle is included in the buffer. Stops at the match or at
; max bytes. Blocks on the UART between bytes -- for real hardware.
; ---------------------------------------------------------------------
ser_read_until:
        sta     mos8(X16_T4)            ; X16_TPTR2 = match base (needle start)
        stx     mos8(X16_T5)
        lda     mos8(X16_T4)            ; X16_P6/P7 = the moving needle cursor
        sta     mos8(X16_P6)
        lda     mos8(X16_T5)
        sta     mos8(X16_P7)
        stz     mos8(X16_P4)            ; stored count = 0
        stz     mos8(X16_P5)
.Lser_read_until_loop:
        lda     mos8(X16_P5)            ; stored >= max ?  (16-bit compare)
        cmp     mos8(X16_P3)
        bcc     .Lser_read_until_room
        bne     .Lser_read_until_done
        lda     mos8(X16_P4)
        cmp     mos8(X16_P2)
        bcs     .Lser_read_until_done
.Lser_read_until_room:
        jsr     ser_get_wait            ; A = next byte
        ldy     #0
        sta     (X16_P0),y              ; store it
        inc     mos8(X16_P0)
        bne     .Lser_read_until_nostorehi
        inc     mos8(X16_P1)
.Lser_read_until_nostorehi:
        inc     mos8(X16_P4)            ; ++stored (16-bit)
        bne     .Lser_read_until_cmp
        inc     mos8(X16_P5)
.Lser_read_until_cmp:
        cmp     (X16_P6)                ; does it continue the needle?
        bne     .Lser_read_until_reset
        inc     mos8(X16_P6)            ; advance the needle cursor
        bne     .Lser_read_until_noneedlehi
        inc     mos8(X16_P7)
.Lser_read_until_noneedlehi:
        lda     (X16_P6)                ; needle fully matched (next char NUL)?
        beq     .Lser_read_until_done
        bra     .Lser_read_until_loop
.Lser_read_until_reset:
        lda     mos8(X16_T4)            ; mismatch: rewind the needle cursor
        sta     mos8(X16_P6)
        lda     mos8(X16_T5)
        sta     mos8(X16_P7)
        bra     .Lser_read_until_loop
.Lser_read_until_done:
        rts

; ---------------------------------------------------------------------
; ser_discard_until -- read and throw away bytes until a match is seen.
;   in:  A = match low, X = match high (NUL-terminated needle)
; The matched needle is discarded too. Blocks on the UART -- hardware.
; ---------------------------------------------------------------------
ser_discard_until:
        sta     mos8(X16_T4)            ; needle base
        stx     mos8(X16_T5)
        lda     mos8(X16_T4)            ; moving cursor in X16_P6/P7
        sta     mos8(X16_P6)
        lda     mos8(X16_T5)
        sta     mos8(X16_P7)
.Lser_discard_until_loop:
        jsr     ser_get_wait
        cmp     (X16_P6)
        bne     .Lser_discard_until_reset
        inc     mos8(X16_P6)
        bne     .Lser_discard_until_nohi
        inc     mos8(X16_P7)
.Lser_discard_until_nohi:
        lda     (X16_P6)
        beq     .Lser_discard_until_done                   ; hit the NUL: whole needle matched
        bra     .Lser_discard_until_loop
.Lser_discard_until_reset:
        lda     mos8(X16_T4)
        sta     mos8(X16_P6)
        lda     mos8(X16_T5)
        sta     mos8(X16_P7)
        bra     .Lser_discard_until_loop
.Lser_discard_until_done:
        rts

; copy the current UART base into X16_TPTR0 for (zp),y register access
serial_load_ptr:
        lda     ser_base
        sta     mos8(X16_T0)
        lda     ser_base+1
        sta     mos8(X16_T0+1)
        rts

; ---------------------------------------------------------------------
; Zeroed by cc65's crt0 before main runs -- the upstream `.byte 0, 0`
; initialisers become plain BSS here.
; ---------------------------------------------------------------------
        .section .bss,"aw",@nobits

ser_base:       .zero  2                  ; the UART ser_init last selected
ser_u0:         .zero  2                  ; ser_detect results
ser_u1:         .zero  2
