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
 * =====================================================================
 */

#ifndef X16_DOS_H
#define X16_DOS_H

/* Anything below this is success. */
#define X16_DOS_OK_BELOW        20
#define X16_DOS_NO_CHANNEL      255

/* The device defaults to 8, the SD card. */
void x16_dos_set_device(__reg("a") unsigned char device);

/* The reply text of the last command, NUL-terminated. The next call
** overwrites it, so copy it if you need to keep it. */
const char *x16_dos_msg(void);

/* Read the drive's pending status line. */
unsigned char x16_dos_status(void);

/* Send a raw command and fetch the reply. A length of 0 sends nothing and
** just reads the pending status, like x16_dos_status(). */
unsigned char x16_dos_cmd(__reg("r0/r1") const char *cmd, __reg("r2") unsigned char len);

/* One-call wrappers. Each returns the status code. Filenames are
** (pointer, length), not NUL-terminated -- pass strlen(name). */
unsigned char x16_dos_delete(__reg("r0/r1") const char *name, __reg("r2") unsigned char len);
unsigned char x16_dos_mkdir(__reg("r0/r1") const char *name, __reg("r2") unsigned char len);
unsigned char x16_dos_rmdir(__reg("r0/r1") const char *name, __reg("r2") unsigned char len);

/* "//" is the root. */
unsigned char x16_dos_chdir(__reg("r0/r1") const char *name, __reg("r2") unsigned char len);

unsigned char x16_dos_rename(__reg("r0/r1") const char *oldname, __reg("r2") unsigned char oldlen,
                             __reg("r4/r5") const char *newname, __reg("r6") unsigned char newlen);

#endif /* X16_DOS_H */
