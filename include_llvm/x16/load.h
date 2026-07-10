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
 *   reason x16_fs_vload() exists rather than an X16_SA_VRAM constant:
 *   putting 2 or 3 in the secondary address does NOT reach VRAM, it asks
 *   for a raw header-included load into system RAM.
 *
 * cc65's <cbm.h> has cbm_load() and cbm_save() for the system-RAM cases.
 * x16_fs_vload() has no cc65 equivalent.
 * =====================================================================
 */

/* ---------------------------------------------------------------------
** llvm-mos build. The API is identical to the cc65 build's; only the
** calling convention differs, and llvm-mos expresses it in the compiler
** rather than in the declaration. So there is no __fastcall__ here.
**
** llvm-mos passes byte arguments in A, then X, then __rc2, __rc3, ...
** and returns the same way. cc65 pushes all but the last argument on a
** software stack. Object code from the two toolchains cannot be mixed.
** --------------------------------------------------------------------- */

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
** you do not want it.
*/
unsigned char x16_fs_load (const char *name, unsigned char len,
                                        unsigned char device, unsigned char sa,
                                        void *dest, unsigned int *end);

/* Write [start, end) as a PRG, with the usual 2-byte load-address header.
** `end` is exclusive. Returns 0 on success, else the KERNAL error code.
*/
unsigned char x16_fs_save (const char *name, unsigned char len,
                                        unsigned char device,
                                        const void *start, const void *end);

/* Load straight into VRAM. The 2-byte PRG header is skipped and the data
** lands at `vaddr`. Returns 0 on success, else the KERNAL error code.
*/
unsigned char x16_fs_vload (const char *name, unsigned char len,
                                         unsigned char device,
                                         unsigned long vaddr);

/* KERNAL SETNAM, for driving OPEN and friends yourself. */
void x16_fs_setname (const char *name, unsigned char len);

#endif /* X16_LOAD_H */
