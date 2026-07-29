/* =====================================================================
 * x16clib :: x16/iec.h -- low-level IEC / serial bus wrappers
 * =====================================================================
 * Direct access to the classic Commodore serial bus KERNAL calls. Most
 * programs should use x16/fileio.h, x16/load.h, x16/dos.h or x16/bmx.h
 * instead; this gate is for protocols that need explicit bus control.
 *
 * Reading the drive status line by hand, the way the DOS wedge does:
 *
 *      x16_iec_talk_channel(__reg("r0") 8,
                     __reg("r2") 15);    // TALK 8, secondary $6F
 *      do { c = x16_iec_acptr(); ... } while (c != 0x0D);
 *      x16_iec_untalk();
 *
 * The composite *_channel calls OR the X16_IEC_CMD_* base into the
 * secondary for you; the raw second/tksa calls take the finished byte.
 * =====================================================================
 */

#ifndef X16_IEC_H
#define X16_IEC_H

/* Secondary-address command bases, OR'd with the channel (0-15). */
#define X16_IEC_CMD_DATA        0x60
#define X16_IEC_CMD_CLOSE       0xE0
#define X16_IEC_CMD_OPEN        0xF0

/* --- raw KERNAL wrappers ------------------------------------------- */

void x16_iec_listen (__reg("a") unsigned char device);
void x16_iec_talk (__reg("a") unsigned char device);

/* The secondary command byte after LISTEN / after TALK. */
void x16_iec_second (__reg("a") unsigned char cmd);
void x16_iec_tksa (__reg("a") unsigned char cmd);

/* One byte out to the listener / in from the talker. */
void x16_iec_ciout (__reg("a") unsigned char b);
unsigned char x16_iec_acptr (void);

void x16_iec_unlisten (void);
void x16_iec_untalk (void);

/* KERNAL SETTMO. A no-op in ROM r49, kept for completeness. */
void x16_iec_set_timeout (__reg("a") unsigned char t);

/* The serial/KERNAL status byte, as x16_fio_readst(). */
unsigned char x16_iec_readst (void);

/* X16 block transfers for the current channel (after CHKIN/CHKOUT).
** A count of 0 lets the implementation choose. Returns the byte count
** actually transferred, or -1 when the channel cannot do block
** transfers -- fall back to acptr/ciout one byte at a time.
**
** The pointer always advances. (The raw KERNAL call takes carry set to
** pin it on one address for port I/O; that mode is not exposed here.)
*/
int x16_iec_macptr (__reg("r0") unsigned char count,
               __reg("r2/r3") void *dest);
int x16_iec_mciout (__reg("r0") unsigned char count,
               __reg("r2/r3") const void *src);

/* --- composites ---------------------------------------------------- */

/* LISTEN device, then the OPEN / DATA / CLOSE secondary command. */
void x16_iec_open_channel (__reg("r0") unsigned char device,
                     __reg("r2") unsigned char secondary);
void x16_iec_data_channel (__reg("r0") unsigned char device,
                     __reg("r2") unsigned char secondary);
void x16_iec_close_channel (__reg("r0") unsigned char device,
                      __reg("r2") unsigned char secondary);

/* TALK device, then the DATA secondary command. */
void x16_iec_talk_channel (__reg("r0") unsigned char device,
                     __reg("r2") unsigned char secondary);

#endif /* X16_IEC_H */
