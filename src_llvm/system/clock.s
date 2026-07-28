; =====================================================================
; x16clib :: system/clock.s -- KERNAL jiffy timer and RTC date/time
; =====================================================================
; The 24-bit timer is the classic KERNAL 60 Hz jiffy counter -- the same
; one cc65's clock() reads. UDTIM ticks it; the KERNAL's IRQ calls UDTIM
; once a frame, so x16_clock_update() only matters if you have taken the
; interrupt over.
;
; The date/time calls talk to the battery-backed RTC over I2C, through
; the KERNAL's CLOCK_GET_DATE_TIME/CLOCK_SET_DATE_TIME, which use the
; virtual registers r0-r3:
;
;       r0L year since 1900     r2L minutes
;       r0H month (1-12)        r2H seconds
;       r1L day (1-31)          r3L jiffies (60ths)
;       r1H hours (0-23)        r3H weekday (1 = Monday)
;
; That byte order is exactly the x16_date_time struct in x16/clock.h, so
; the shims are a straight 8-byte copy: r0..r3 sit contiguously at
; $02-$09.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"

; (import dropped: X16_TPTR0, sreg)

        .globl  x16_clock_update
        .globl  x16_clock_get_timer
        .globl  x16_clock_set_timer
        .globl  x16_clock_get_date_time
        .globl  x16_clock_set_date_time

        .section .text,"ax",@progbits

; ---------------------------------------------------------------------
; void x16_clock_update(void)
;
; Tick the jiffy timer by one, exactly as the KERNAL's IRQ does.
; ---------------------------------------------------------------------
x16_clock_update:
        jmp     UDTIM

; ---------------------------------------------------------------------
; unsigned long x16_clock_get_timer(void)
;   returns the 24-bit jiffy counter (the high byte is always 0)
;
; RDTIM answers A/X/Y = low/mid/high -- A and X are already the low half
; of llvm-mos's 32-bit return, so only Y needs moving into __rc2.
; ---------------------------------------------------------------------
x16_clock_get_timer:
        jsr     RDTIM                   ; A = low, X = mid, Y = high
        sty     __rc2
        stz     __rc3
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_clock_set_timer(unsigned long jiffies)
;   bits 24-31 are ignored: the counter is 24 bits wide
; ---------------------------------------------------------------------
x16_clock_set_timer:
        ldy     __rc2                   ; bits 16-23; A/X arrive correct
        jmp     SETTIM

; ---------------------------------------------------------------------
; void __fastcall__ x16_clock_get_date_time(x16_date_time *dt)
;
; r0-r3 come back holding the eight bytes; copy them out in one pass.
; There is no lda zp,y form, so this assembles as absolute,y.
; ---------------------------------------------------------------------
x16_clock_get_date_time:
        lda     __rc2
        sta     X16_TPTR0
        lda     __rc3
        sta     X16_TPTR0+1
        jsr     CLOCK_GET_DATE_TIME
        ldy     #7
.Lx16_clock_get_date_time_copy:
        lda     r0,y
        sta     (X16_TPTR0),y
        dey
        bpl     .Lx16_clock_get_date_time_copy
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_clock_set_date_time(const x16_date_time *dt)
;
; Writes the RTC. Jiffies and weekday are stored too; the ROM does not
; validate, so pass sane values.
; ---------------------------------------------------------------------
x16_clock_set_date_time:
        lda     __rc2
        sta     X16_TPTR0
        lda     __rc3
        sta     X16_TPTR0+1
        ldy     #7
.Lx16_clock_set_date_time_copy:
        lda     (X16_TPTR0),y
        sta     r0,y
        dey
        bpl     .Lx16_clock_set_date_time_copy
        jmp     CLOCK_SET_DATE_TIME
