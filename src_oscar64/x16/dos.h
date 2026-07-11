/* =====================================================================
 * x16clib :: x16/dos.h -- the DOS command channel
 * =====================================================================
 * x16_fs_load() and x16_fs_save() report failure, but never WHY. The
 * answer lives on channel 15: every command sent there is answered with
 * a status line like "62,FILE NOT FOUND,00,00".
 *
 *      if (x16_fs_save(...) != 0) {
 *          x16_dos_status();               // why?
 *          printf("%s\n", x16_dos_msg());
 *      }
 *
 * The numeric code follows CBM DOS's convention: below 20 is success, 20
 * and up is an error. 255 means the command channel would not open at
 * all. The first status read after power-on returns 73, the DOS version
 * banner, by design.
 *
 * Nothing in cc65 exposes this: <cbm.h> has opendir/readdir and
 * cbm_open, but no command channel.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** Oscar64 build. The API is identical to the cc65 build's; what differs
** is the delivery. Oscar64 compiles the whole program at once and strips
** what goes unused, so this port is a SOURCE distribution: headers and
** implementations sit side by side in src_oscar64/x16/, and the
** `#pragma compile` at the bottom of this header pulls the matching .c
** in automatically:
**
**     oscar64 -tm=x16 -n -i=src_oscar64 -o=YOURPROG.PRG yourprog.c
** --------------------------------------------------------------------- */

#ifndef X16_DOS_H
#define X16_DOS_H

/* Anything below this is success. */
#define X16_DOS_OK_BELOW        20
#define X16_DOS_NO_CHANNEL      255

/* The device defaults to 8, the SD card. */
void x16_dos_set_device (unsigned char device);

/* The reply text of the last command, NUL-terminated. The next call
** overwrites it, so copy it if you need to keep it.
*/
const char *x16_dos_msg (void);

/* Read the drive's pending status line. */
unsigned char x16_dos_status (void);

/* Send a raw command and fetch the reply. A length of 0 sends nothing
** and just reads the pending status, like x16_dos_status().
*/
unsigned char x16_dos_cmd (const char *cmd, unsigned char len);

/* One-call wrappers. Each returns the status code. Filenames are
** (pointer, length), not NUL-terminated -- pass strlen(name).
*/
unsigned char x16_dos_delete (const char *name,
                                           unsigned char len);
unsigned char x16_dos_mkdir (const char *name,
                                          unsigned char len);
unsigned char x16_dos_rmdir (const char *name,
                                          unsigned char len);

/* "//" is the root. */
unsigned char x16_dos_chdir (const char *name,
                                          unsigned char len);

unsigned char x16_dos_rename (const char *oldname,
                                           unsigned char oldlen,
                                           const char *newname,
                                           unsigned char newlen);

/* pulls the implementation in with this header */
#pragma compile("dos.c")

#endif /* X16_DOS_H */
