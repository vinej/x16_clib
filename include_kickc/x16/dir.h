/* =====================================================================
 * x16clib :: x16/dir.h -- reading a directory
 * =====================================================================
 * A drive hands its directory over as a BASIC program listing -- link
 * words, line numbers, quoted names. These routines walk that so you
 * never see it:
 *
 *      char name[40];
 *      if (!x16_dir_open(0, 0, 8)) return;     // "$", device 8
 *      while (x16_dir_next(name, sizeof name)) {
 *          if (x16_dir_type() == X16_DIR_TYPE_PRG) {
 *              // name, x16_dir_blocks() ...
 *          }
 *      }
 *      x16_dir_close();
 *
 * The header line naming the volume comes back as X16_DIR_TYPE_HOST and
 * the trailing "BLOCKS FREE." line as X16_DIR_TYPE_NONE with an empty
 * name, rather than being hidden -- a file browser wants to skip them, a
 * disk info panel wants to show them, and this way neither has to
 * re-parse anything.
 *
 * The directory occupies logical file 3, clear of the loader's 1 and the
 * command channel's 15. Only one directory can be open at a time.
 *
 * cc65's <cbm.h> has cbm_opendir/cbm_readdir over its own file table;
 * these stand alone and also classify the header and trailer lines.
 * =====================================================================
 */

#ifndef X16_DIR_H
#define X16_DIR_H

#include <x16/zpsafe.h>

/* What x16_dir_type() reports for the entry x16_dir_next() just read. */
#define X16_DIR_TYPE_NONE       0       /* no name on the line: "BLOCKS FREE." */
#define X16_DIR_TYPE_PRG        1
#define X16_DIR_TYPE_SEQ        2
#define X16_DIR_TYPE_USR        3
#define X16_DIR_TYPE_REL        4
#define X16_DIR_TYPE_DIR        5
#define X16_DIR_TYPE_HOST       6       /* the header line naming the volume */

/* Open a directory for reading. A length of 0 asks for "$", the current
** directory; otherwise `path` names one, (pointer, length) style, not
** NUL-terminated. Returns 1 if the directory opened, 0 if not.
*/
unsigned char x16_dir_open (const char *path, unsigned char len,
                                         unsigned char device);

/* Read the next entry. The name arrives NUL-terminated in `buf`,
** truncated to fit (size 2-255). Returns 1 if an entry was read, 0 at
** the end of the listing.
*/
unsigned char x16_dir_next (char *buf, unsigned char size);

/* Describe the entry x16_dir_next() just read. */
unsigned char x16_dir_type (void);      /* X16_DIR_TYPE_* */
unsigned int x16_dir_blocks (void);     /* the listing's block count */

/* Finished with the directory. */
void x16_dir_close (void);

#endif /* X16_DIR_H */
