/* =====================================================================
 * x16clib :: x16/load.h -- load and save
 * =====================================================================
 * Device 8 is the SD card. Filenames are (pointer, length), not
 * NUL-terminated -- pass strlen(name) or a literal count.
 *
 * Two different numbers steer a load, and they are easy to conflate:
 *
 *   The SECONDARY ADDRESS says how to treat the file. That is the `sa`
 *   argument, one of the X16_SA_* constants below.
 *
 *   Where in memory the bytes land is a separate matter, and it is the
 *   reason x16_fs_vload() exists rather than an X16_SA_VRAM constant.
 * =====================================================================
 */

#ifndef X16_LOAD_H
#define X16_LOAD_H

#define X16_DEVICE_SD   8

/* Secondary address for x16_fs_load(). */
#define X16_SA_ADDR     0       /* skip the 2-byte PRG header, load at `dest` */
#define X16_SA_HEADER   1       /* skip it, load where the header itself says */
#define X16_SA_RAW      2       /* no header: load the whole file at `dest` */

/* Returns 0 on success, else the KERNAL error code.
**
** *end receives the address one past the last byte loaded. Pass NULL if
** you do not want it. (Six arguments: dest and end ride the C soft
** stack.) */
unsigned char x16_fs_load(__reg("r0/r1") const char *name, __reg("r2") unsigned char len,
                          __reg("r4") unsigned char device, __reg("r6") unsigned char sa,
                          void *dest, unsigned int *end);

/* Write [start, end) as a PRG, with the usual 2-byte load-address header.
** `end` is exclusive. Returns 0 on success, else the KERNAL error code.
** (Five arguments: end rides the C soft stack.) */
unsigned char x16_fs_save(__reg("r0/r1") const char *name, __reg("r2") unsigned char len,
                          __reg("r4") unsigned char device,
                          __reg("r6/r7") const void *start, const void *end);

/* Load straight into VRAM. The 2-byte PRG header is skipped and the data
** lands at `vaddr`. Returns 0 on success, else the KERNAL error code.
** (vaddr is a long, passed in btmp0.) */
unsigned char x16_fs_vload(__reg("r0/r1") const char *name, __reg("r2") unsigned char len,
                           __reg("r4") unsigned char device, unsigned long vaddr);

/* KERNAL SETNAM, for driving OPEN and friends yourself. */
void x16_fs_setname(__reg("r0/r1") const char *name, __reg("r2") unsigned char len);

#endif /* X16_LOAD_H */
