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

        .importzp       ptr1, sreg

        .export         _x16_clock_update
        .export         _x16_clock_get_timer
        .export         _x16_clock_set_timer
        .export         _x16_clock_get_date_time
        .export         _x16_clock_set_date_time

        .segment        "CODE"

; ---------------------------------------------------------------------
; void x16_clock_update(void)
;
; Tick the jiffy timer by one, exactly as the KERNAL's IRQ does.
; ---------------------------------------------------------------------
_x16_clock_update:
        jmp     UDTIM

; ---------------------------------------------------------------------
; unsigned long x16_clock_get_timer(void)
;   returns the 24-bit jiffy counter (the high byte is always 0)
;
; RDTIM answers A/X/Y = low/mid/high -- A and X are already cc65's
; 32-bit return's low half, so only Y needs moving into sreg.
; ---------------------------------------------------------------------
_x16_clock_get_timer:
        jsr     RDTIM                   ; A = low, X = mid, Y = high
        sty     sreg
        stz     sreg+1
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_clock_set_timer(unsigned long jiffies)
;   bits 24-31 are ignored: the counter is 24 bits wide
; ---------------------------------------------------------------------
_x16_clock_set_timer:
        ldy     sreg                    ; bits 16-23; A/X arrive correct
        jmp     SETTIM

; ---------------------------------------------------------------------
; void __fastcall__ x16_clock_get_date_time(x16_date_time *dt)
;
; r0-r3 come back holding the eight bytes; copy them out in one pass.
; `a:` forces absolute,y addressing -- there is no lda zp,y.
; ---------------------------------------------------------------------
_x16_clock_get_date_time:
        sta     ptr1
        stx     ptr1+1
        jsr     CLOCK_GET_DATE_TIME
        ldy     #7
@copy:
        lda     a:r0,y
        sta     (ptr1),y
        dey
        bpl     @copy
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_clock_set_date_time(const x16_date_time *dt)
;
; Writes the RTC. Jiffies and weekday are stored too; the ROM does not
; validate, so pass sane values.
; ---------------------------------------------------------------------
_x16_clock_set_date_time:
        sta     ptr1
        stx     ptr1+1
        ldy     #7
@copy:
        lda     (ptr1),y
        sta     a:r0,y
        dey
        bpl     @copy
        jmp     CLOCK_SET_DATE_TIME
