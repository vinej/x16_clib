/* =====================================================================
 * x16clib :: x16/fileio.h -- generic KERNAL file/channel I/O
 * =====================================================================
 * Streamed file/channel I/O: OPEN/CLOSE, CHKIN/CHKOUT, CHRIN/CHROUT,
 * READST, and the setup calls that feed them. For one-shot PRG LOAD/SAVE
 * use x16/load.h; when a call fails and you want to know WHY, ask the
 * command channel via x16/dos.h.
 *
 * The usual dance, writing then reading a SEQ file on device 8:
 *
 *      x16_fio_open_write("DATA.SEQ,S,W", 12, 2, 8, 2);
 *      x16_fio_chrout(...);            // as many as you like
 *      x16_fio_close_named(2);
 *
 *      x16_fio_open_read("DATA.SEQ,S,R", 12, 2, 8, 2);
 *      do { b = x16_fio_chrin(); ... } while (!x16_fio_readst());
 *      x16_fio_close_named(2);
 *
 * Filenames are (pointer, length), not NUL-terminated. Calls that can
 * fail return 0 on success, else the KERNAL error code -- the same
 * convention as x16_fs_load().
 *
 * cc65's <cbm.h> has cbm_k_* twins for the raw wrappers; these exist so
 * the same API is available in every port of the library.
 * =====================================================================
 */

#ifndef X16_FILEIO_H
#define X16_FILEIO_H

#include <x16/zpsafe.h>

#define X16_FIO_DEV_KEYBOARD    0
#define X16_FIO_DEV_SCREEN      3
#define X16_FIO_DEV_DISK        8
#define X16_FIO_LFN_COMMAND     15
#define X16_FIO_SA_NONE         0
#define X16_FIO_SA_COMMAND      15

/* End-of-file bit in x16_fio_readst()'s answer. */
#define X16_FIO_ST_EOF          0x40

/* --- raw KERNAL wrappers ------------------------------------------- */

void x16_fio_set_lfs (unsigned char lfn, unsigned char device,
                                   unsigned char secondary);
void x16_fio_set_name (const char *name, unsigned char len);

/* 0 on success, else the KERNAL error code. Uses the name and numbers
** given to the two calls above.
*/
unsigned char x16_fio_open (void);

void x16_fio_close (unsigned char lfn);

/* Select a logical file for input/output. 0 on success, else the KERNAL
** error code (3 = file not open).
*/
unsigned char x16_fio_chkin (unsigned char lfn);
unsigned char x16_fio_chkout (unsigned char lfn);

/* Back to keyboard and screen. */
void x16_fio_clrchn (void);

/* One byte from the current input channel. */
unsigned char x16_fio_chrin (void);

/* One byte to the current output channel. */
void x16_fio_chrout (unsigned char b);

/* The KERNAL status byte; X16_FIO_ST_EOF set means end of file. */
unsigned char x16_fio_readst (void);

/* One byte, 0 if nothing is waiting. On a file channel it reads like
** x16_fio_chrin().
*/
unsigned char x16_fio_getin (void);

void x16_fio_close_all (void);                          /* every file */
void x16_fio_close_device (unsigned char device);

/* --- composites ---------------------------------------------------- */

/* SETNAM + SETLFS + OPEN in one call. 0 on success, else the KERNAL
** error code.
*/
unsigned char x16_fio_open_named (const char *name,
                                               unsigned char len,
                                               unsigned char lfn,
                                               unsigned char device,
                                               unsigned char secondary);

/* ...then also select the file for input (CHKIN) or output (CHKOUT). */
unsigned char x16_fio_open_read (const char *name,
                                              unsigned char len,
                                              unsigned char lfn,
                                              unsigned char device,
                                              unsigned char secondary);
unsigned char x16_fio_open_write (const char *name,
                                               unsigned char len,
                                               unsigned char lfn,
                                               unsigned char device,
                                               unsigned char secondary);

/* CLRCHN + CLOSE. */
void x16_fio_close_named (unsigned char lfn);

#endif /* X16_FILEIO_H */
