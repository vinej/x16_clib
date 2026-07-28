/* =====================================================================
 * x16clib :: test_llvm/runner9.c -- comms: SPI, serial UART, ZiModem
 * =====================================================================
 * Standalone suite: .\build_llvm.ps1 -Test -Source test_ca65\runner9.c
 *
 * Headless reality, module by module:
 *
 * SPI. The emulator models VERA's SPI engine (BUSY is a byte-time
 * countdown, the CTRL bits latch), but with -fsroot there is NO SD card
 * on the bus: exchanged bytes come back $FF and reach nothing, so a
 * CMD0-style select + $40 exchange exercises the real transfer path
 * without any filesystem to wedge. Every transfer waits on BUSY, which
 * the emulator clears by itself; the suite's 120 s harness timeout is
 * the backstop if that model ever changes.
 *
 * SERIAL. The emulator has no 16C550, so x16_ser_detect() finding
 * nothing is itself the probe's negative test. Everything else runs
 * against a FAKE UART: eight bytes of RAM handed to x16_ser_init() as
 * the base address. RAM holds whatever the driver writes, so the init
 * register program becomes visible as final state, and pre-setting the
 * fake LSR's DR/THRE bits lets every "blocking" path complete on the
 * first poll -- no spin can outlive a bounded test.
 *
 * ZIMODEM. The AT flows block reading an ESP32 that isn't there; those
 * are skipped, JOY_PRESENT-style. What does run headless is the real
 * logic: zi_cmd's transmit framing (through the fake UART) and the
 * zi_hexdecode oracle.
 * =====================================================================
 */

#include "testlib.h"
#include <cbm.h>
#include <x16/spi.h>
#include <x16/serial.h>
#include <x16/zimodem.h>

/* The fake UART: 8 bytes of RAM standing in for a 16C550's register
** file. fake[5] is the LSR the driver polls; writes to THR land in
** fake[0], where the test can read them back.
*/
static unsigned char fake[8];

#define FAKE_LSR_DR     0x01
#define FAKE_LSR_THRE   0x20

static unsigned char buf[16];

/* ------------------------------------------------------------------ */
/* SPI                                                                */
/* ------------------------------------------------------------------ */

static unsigned char spi_ctrl0;         /* boot state, restored at the end */

static void test_spi_select(void)
{
    x16_spi_select();
    t_check((x16_spi_get_ctrl() & X16_SPI_SELECT) == X16_SPI_SELECT,
            "SPI_SELECT");
    x16_spi_deselect();
    t_check((x16_spi_get_ctrl() & X16_SPI_SELECT) == 0, "SPI_DESELECT");
}

/* The independent oracle for the clock bit: a RAW register write, no
** library involved. x16emu latches SELECT and AUTOTX but not SLOWCLK
** (it does not model clock speed), and a library bug must not hide
** behind that -- only a device that provably drops the bit may skip.
**
** The readback goes through a real function call: cc65's -O store-load
** elimination removes a load that directly follows a store to the same
** address, volatile or not, and a folded probe would answer "modeled"
** from its own written value.
*/
#define SPI_CTRL_REG    (*(volatile unsigned char *)0x9F3F)

static unsigned char spi_ctrl_raw_read(void)
{
    return SPI_CTRL_REG;
}

static unsigned char spi_slowclk_modeled(void)
{
    unsigned char r;

    SPI_CTRL_REG = X16_SPI_SLOWCLK;
    r = spi_ctrl_raw_read() & X16_SPI_SLOWCLK;
    SPI_CTRL_REG = 0;
    return r != 0;
}

static void test_spi_clock(void)
{
    if (!spi_slowclk_modeled()) {
        t_skip("SPI_SLOW");
        t_skip("SPI_FAST");
        return;
    }
    x16_spi_slow();
    t_check((x16_spi_get_ctrl() & X16_SPI_SLOWCLK) == X16_SPI_SLOWCLK,
            "SPI_SLOW");
    x16_spi_fast();
    t_check((x16_spi_get_ctrl() & X16_SPI_SLOWCLK) == 0, "SPI_FAST");
}

static void test_spi_autotx(void)
{
    x16_spi_autotx_on();
    t_check((x16_spi_get_ctrl() & X16_SPI_AUTOTX) == X16_SPI_AUTOTX,
            "SPI_AUTOTX_ON");
    x16_spi_autotx_off();
    t_check((x16_spi_get_ctrl() & X16_SPI_AUTOTX) == 0, "SPI_AUTOTX_OFF");
}

/* set_ctrl and get_ctrl see the same register: write a multi-bit
** pattern through one, read it back through the other. SELECT and
** AUTOTX rather than SLOWCLK -- see spi_slowclk_modeled above.
*/
static void test_spi_ctrl_roundtrip(void)
{
    x16_spi_set_ctrl(X16_SPI_AUTOTX | X16_SPI_SELECT);
    t_check((x16_spi_get_ctrl() & 0x07)
                == (X16_SPI_AUTOTX | X16_SPI_SELECT), "SPI_CTRL_RT");
    x16_spi_set_ctrl(0);
    t_check((x16_spi_get_ctrl() & 0x07) == 0, "SPI_CTRL_CLEAR");
}

/* No transfer is running, so BUSY must read clear -- and if it did not,
** the exchange test below could spin, so this gates it.
*/
static void test_spi_idle(void)
{
    t_check((x16_spi_get_ctrl() & X16_SPI_BUSY) == 0, "SPI_IDLE");
}

/* A CMD0-shaped exchange with NO card on the bus (hostfs mode): every
** byte comes back $FF, and each call returning at all proves the
** emulator released BUSY. x16_spi_write shares that wait; x16_spi_read
** is transfer($FF) by another name.
*/
static void test_spi_exchange(void)
{
    unsigned char r, i, ff;

    x16_spi_select();
    r = x16_spi_transfer(0x40);         /* CMD0's first byte */
    x16_spi_write(0x00);                /* ...and one argument byte */
    ff = 1;
    for (i = 0; i < 4; ++i) {
        if (x16_spi_read() != 0xFF) {
            ff = 0;
        }
    }
    x16_spi_deselect();
    t_check(r == 0xFF && ff, "SPI_XFER_NO_CARD");
}

static void test_spi_read_bytes(void)
{
    unsigned char i, ok;

    for (i = 0; i < 6; ++i) {
        buf[i] = 0x11;                  /* poison */
    }
    x16_spi_select();
    x16_spi_read_bytes(buf, 5);
    x16_spi_deselect();
    ok = 1;
    for (i = 0; i < 5; ++i) {
        if (buf[i] != 0xFF) {           /* no card: all idle bytes */
            ok = 0;
        }
    }
    t_check(ok && buf[5] == 0x11, "SPI_READ_BYTES");
}

static void test_spi_write_bytes(void)
{
    buf[0] = 0xFF;
    buf[1] = 0x95;                      /* CMD0's CRC, off to nobody */
    x16_spi_select();
    x16_spi_write_bytes(buf, 2);
    x16_spi_deselect();
    t_check((x16_spi_get_ctrl() & X16_SPI_BUSY) == 0, "SPI_WRITE_BYTES");
}

/* ------------------------------------------------------------------ */
/* Serial                                                             */
/* ------------------------------------------------------------------ */

/* The emulator maps no 16C550 anywhere in $9F60-$9FF8, and the probe's
** fingerprint (IER high nibble, MCR bits 7:6, two scratch patterns) is
** built so that no constant bus value can pass it.
*/
static void test_ser_detect_none(void)
{
    unsigned char n = x16_ser_detect();

    t_check(n == 0 && x16_ser_uart0() == 0 && x16_ser_uart1() == 0,
            "SER_DETECT_NONE");
}

/* In RAM every register write sticks, so ser_init's whole program is
** checkable as final state: divisor low $60 in the DLL (fake[0]), FCR
** $87, LCR left at 8N1/DLAB=0, MCR $27, IER 0 (the last DLM overwrite
** -- this module polls). A swapped base/divisor shim cannot pass this.
*/
static void test_ser_init_regs(void)
{
    unsigned char i;

    for (i = 0; i < 8; ++i) {
        fake[i] = 0;
    }
    x16_ser_init((unsigned int)fake, X16_SER_BAUD_9600);
    t_check(fake[0] == 0x60 && fake[1] == 0x00 && fake[2] == 0x87
                && fake[3] == 0x03 && fake[4] == 0x27, "SER_INIT_REGS");
}

static void test_ser_avail(void)
{
    fake[5] = 0;
    t_check(x16_ser_avail() == 0, "SER_AVAIL_NO");
    fake[5] = FAKE_LSR_DR;
    t_check(x16_ser_avail() == 1, "SER_AVAIL_YES");
}

static void test_ser_get(void)
{
    fake[5] = FAKE_LSR_DR;
    fake[0] = 0xAB;
    t_check(x16_ser_get() == 0xAB, "SER_GET");
    fake[5] = 0;
    t_check(x16_ser_get() == -1, "SER_GET_EMPTY");
}

static void test_ser_get_wait(void)
{
    fake[5] = FAKE_LSR_DR;              /* data already "ready": no spin */
    fake[0] = 0x77;
    t_check(x16_ser_get_wait() == 0x77, "SER_GET_WAIT");
}

static void test_ser_put(void)
{
    fake[5] = FAKE_LSR_THRE;            /* room already there: no spin */
    fake[0] = 0;
    x16_ser_put(0x55);
    t_check(fake[0] == 0x55, "SER_PUT");
}

static void test_ser_puts(void)
{
    fake[5] = FAKE_LSR_THRE;
    fake[0] = 0;
    x16_ser_puts("HI");                 /* ASCII: testlib's charmap */
    t_check(fake[0] == 0x49, "SER_PUTS");       /* 'I' went last */
}

static void test_ser_write(void)
{
    static const unsigned char data[3] = { 0x11, 0x22, 0x00 };

    fake[5] = FAKE_LSR_THRE;
    fake[0] = 0xEE;
    x16_ser_write(data, 3);             /* binary-safe: sends the NUL too */
    t_check(fake[0] == 0x00, "SER_WRITE");
}

/* RX and TX ready at once; RHR pinned at 'X'. The needle "X" matches on
** the first byte, so the read stops after storing exactly it.
*/
static void test_ser_read_until(void)
{
    unsigned int n;

    fake[5] = FAKE_LSR_DR | FAKE_LSR_THRE;
    fake[0] = 0x58;                     /* 'X' */
    buf[0] = 0;
    n = x16_ser_read_until((char *)buf, 8, "X");
    t_check(n == 1 && buf[0] == 0x58, "SER_READ_UNTIL");
}

/* A needle that never comes: the max-bytes bound is what stops it. */
static void test_ser_read_until_max(void)
{
    unsigned int n;

    fake[5] = FAKE_LSR_DR | FAKE_LSR_THRE;
    fake[0] = 0x58;
    n = x16_ser_read_until((char *)buf, 4, "Q");
    t_check(n == 4 && buf[3] == 0x58, "SER_READ_UNTIL_MAX");
}

static void test_ser_discard_until(void)
{
    fake[5] = FAKE_LSR_DR;
    fake[0] = 0x58;
    x16_ser_discard_until("X");         /* returning at all is the pass */
    t_check(1, "SER_DISCARD_UNTIL");
}

/* ------------------------------------------------------------------ */
/* ZiModem                                                            */
/* ------------------------------------------------------------------ */

/* zi_cmd = the command text + CR LF, through the same fake UART. The
** last byte on the wire must be the LF -- proof the framing went out
** through ser_puts and that the module's CRLF bytes survived PETSCII.
*/
static void test_zi_cmd(void)
{
    fake[5] = FAKE_LSR_THRE;
    fake[0] = 0;
    x16_zi_cmd("AT");
    t_check(fake[0] == 0x0A, "ZI_CMD_CRLF");
}

static void test_zi_hexdecode(void)
{
    unsigned char n;

    buf[4] = 0x77;                      /* overrun sentinel */
    n = x16_zi_hexdecode(buf, "DEADBEEF", 8);
    t_check(n == 4 && buf[0] == 0xDE && buf[1] == 0xAD && buf[2] == 0xBE
                && buf[3] == 0xEF && buf[4] == 0x77, "ZI_HEXDECODE");
}

/* Every digit, both nibble positions. */
static void test_zi_hexdecode_digits(void)
{
    unsigned char n;

    n = x16_zi_hexdecode(buf, "0123456789ABCDEF", 16);
    t_check(n == 8 && buf[0] == 0x01 && buf[3] == 0x67 && buf[4] == 0x89
                && buf[5] == 0xAB && buf[7] == 0xEF, "ZI_HEXDECODE_DIGITS");
}

static void test_zi_hexdecode_empty(void)
{
    buf[0] = 0x33;
    t_check(x16_zi_hexdecode(buf, "", 0) == 0 && buf[0] == 0x33,
            "ZI_HEXDECODE_EMPTY");
}

/* ~40 ms once, real time; -warp compresses it. Bounded by construction. */
static void test_zi_delay(void)
{
    x16_zi_delay(1);
    t_check(1, "ZI_DELAY");
}

/* ------------------------------------------------------------------ */

int main(void)
{
    t_init();

    spi_ctrl0 = x16_spi_get_ctrl();

    test_spi_select();
    test_spi_clock();
    test_spi_autotx();
    test_spi_ctrl_roundtrip();
    test_spi_idle();
    test_spi_exchange();
    test_spi_read_bytes();
    test_spi_write_bytes();

    /* Leave the bus as the KERNAL's DOS left it. */
    x16_spi_set_ctrl(spi_ctrl0 & 0x07);

    test_ser_detect_none();
    test_ser_init_regs();
    test_ser_avail();
    test_ser_get();
    test_ser_get_wait();
    test_ser_put();
    test_ser_puts();
    test_ser_write();
    test_ser_read_until();
    test_ser_read_until_max();
    test_ser_discard_until();

    test_zi_cmd();
    test_zi_hexdecode();
    test_zi_hexdecode_digits();
    test_zi_hexdecode_empty();
    test_zi_delay();

    /* A real card answering the probe, and the AT flows that block on
    ** an ESP32's replies (zi_init/reset/wait_ok/get_ip/hex_*): hardware
    ** only, an emulator oracle proved the device absent above.
    */
    t_skip("SER_CARD_PRESENT");
    t_skip("ZI_INIT_OK");
    t_skip("ZI_GET_IP");
    t_skip("ZI_HEX_TRANSFER");

    t_done();
    return 0;
}
