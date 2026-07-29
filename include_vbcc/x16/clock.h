/* =====================================================================
 * x16clib :: x16/clock.h -- the jiffy timer and the real-time clock
 * =====================================================================
 * Two clocks. The 24-bit jiffy timer ticks at 60 Hz under the KERNAL's
 * IRQ and wraps daily -- cc65's clock() reads the same counter, but
 * only these calls can SET it, or tick it by hand when you own the
 * interrupt. The RTC is the battery-backed date/time chip, read and
 * written as a whole through x16_date_time.
 * =====================================================================
 */

#ifndef X16_CLOCK_H
#define X16_CLOCK_H

/* Field order is the KERNAL's r0-r3 date/time layout -- do not reorder. */
typedef struct {
    unsigned char year;         /* since 1900: 126 means 2026 */
    unsigned char month;        /* 1-12 */
    unsigned char day;          /* 1-31 */
    unsigned char hours;        /* 0-23 */
    unsigned char minutes;      /* 0-59 */
    unsigned char seconds;      /* 0-59 */
    unsigned char jiffies;      /* 60ths of a second */
    unsigned char weekday;      /* 1 = Monday */
} x16_date_time;

/* Tick the jiffy timer by one, exactly as the KERNAL's IRQ does every
** frame. Call it from your own handler if you have taken VSYNC over,
** or clock() and the timer stand still.
*/
void x16_clock_update (void);

/* The jiffy counter, 0 to 0xFFFFFF. It keeps counting through a set,
** so measure intervals by subtraction.
*/
unsigned long x16_clock_get_timer (void);

/* Set the jiffy counter; bits 24-31 are ignored. */
void x16_clock_set_timer (unsigned long jiffies);

/* Read/write the RTC. The ROM does not validate -- pass sane fields. */
void x16_clock_get_date_time (__reg("a/x") x16_date_time *dt);
void x16_clock_set_date_time (__reg("a/x") const x16_date_time *dt);

#endif /* X16_CLOCK_H */
