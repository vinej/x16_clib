// =====================================================================
// x16clib :: x16/clock.c -- KERNAL jiffy timer and RTC date/time
// =====================================================================
// The 24-bit timer is the classic KERNAL 60 Hz jiffy counter. UDTIM
// ticks it; the KERNAL's IRQ calls UDTIM once a frame, so
// x16_clock_update() only matters if you have taken the interrupt over.
//
// The date/time calls talk to the battery-backed RTC over I2C, through
// CLOCK_GET_DATE_TIME/CLOCK_SET_DATE_TIME, which use the virtual
// registers r0-r3:
//
//       r0L year since 1900     r2L minutes
//       r0H month (1-12)        r2H seconds
//       r1L day (1-31)          r3L jiffies (60ths)
//       r1H hours (0-23)        r3H weekday (1 = Monday)
//
// That byte order is exactly the x16_date_time struct in x16/clock.h,
// so the copies are a straight 8-byte move: r0..r3 sit contiguously at
// $02-$09.
// =====================================================================

#include <x16/clock.h>

// Pointer scratch, pinned in zero page (KickC ignores __zp on
// parameters; see x16/zpsafe.h). The cc65 build used ptr1.

// The timer's four bytes, low first. KickC has no runtime 32-bit shift,
// so the long is assembled and taken apart with one dword move through
// this buffer -- the same trick bcd.c uses for its 32-bit operands.
volatile unsigned char x16__ck_b4[4];

// ---------------------------------------------------------------------
// Tick the jiffy timer by one, exactly as the KERNAL's IRQ does.
// ---------------------------------------------------------------------
void x16_clock_update(void) {
    __asm {
        jsr 0xffea                      // UDTIM
    }
}

// ---------------------------------------------------------------------
// The 24-bit jiffy counter. The high byte is always 0.
// ---------------------------------------------------------------------
unsigned long x16_clock_get_timer(void) {
    unsigned long *p = (unsigned long *)x16__ck_b4;
    __asm {
        jsr 0xffde             // A = low, X = mid, Y = high (RDTIM)
        sta x16__ck_b4+0
        stx x16__ck_b4+1
        sty x16__ck_b4+2
        lda #0
        sta x16__ck_b4+3                // the counter is 24 bits wide
    }
    return *p;
}

// ---------------------------------------------------------------------
// Set it. Bits 24-31 are ignored.
// ---------------------------------------------------------------------
void x16_clock_set_timer(unsigned long jiffies) {
    unsigned long *p = (unsigned long *)x16__ck_b4;
    *p = jiffies;
    __asm {
        lda x16__ck_b4+0
        ldx x16__ck_b4+1
        ldy x16__ck_b4+2                // bits 24-31 are ignored
        jsr 0xffdb                      // SETTIM
    }
}

// ---------------------------------------------------------------------
// Read the RTC into `dt`. r0-r3 come back holding the eight bytes.
// The absolute form is spelled $0002 rather than $02: there is no
// lda zp,y, and the four-digit address stops the assembler reaching
// for one.
// ---------------------------------------------------------------------
void x16_clock_get_date_time(x16_date_time *dt) {
    __asm {
        jsr 0xff50                      // CLOCK_GET_DATE_TIME
        ldy #7
    ck_get_copy:
        lda 0x0002,y                     // r0..r3
        sta (dt),y
        dey
        bpl ck_get_copy
    }
}

// ---------------------------------------------------------------------
// Write it. Jiffies and weekday are stored too; the ROM does not
// validate, so pass sane values.
// ---------------------------------------------------------------------
void x16_clock_set_date_time(const x16_date_time *dt) {
    __asm {
        ldy #7
    ck_set_copy:
        lda (dt),y
        sta 0x0002,y                     // r0..r3
        dey
        bpl ck_set_copy
        jsr 0xff4d                      // CLOCK_SET_DATE_TIME
    }
}
