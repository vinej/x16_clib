/* =====================================================================
 * x16clib :: x16/bmx.h -- the X16's native bitmap file format
 * =====================================================================
 * BMX version 1: a 16-byte header, then the palette in VERA's own
 * layout, then the pixels. It is the format the community's tools and
 * Prog8 write, and nothing in C could read it before.
 *
 *      x16_gfx8l_init();
 *      if (x16_bmx_load("TITLE.BMX", 9, X16_DEVICE_SD, X16_VRAM_BITMAP)) {
 *          ...error...
 *      }
 *
 * x16_bmx_load() fills the palette and the pixels, and publishes the
 * header through x16_bmx_get_info(). It reads through the KERNAL's
 * MACPTR block call, streaming each row straight into VERA's data port
 * rather than one CHRIN per byte; a device that cannot block-read falls
 * back to CHRIN automatically.
 *
 * A file that is not there, or that stops before the header says it
 * should, returns X16_BMX_ERR_IO -- not ERR_FORMAT. OPEN succeeds for a
 * missing file, so a reader that trusts it would blame the contents for
 * the absence.
 *
 * ROWS ARE `stride` BYTES APART IN VRAM, 320 by default -- the
 * full-screen bitmap's stride. So a 320-wide image loads contiguously,
 * and a narrower one lands as a "stamp" with the surrounding pixels
 * untouched. Set the stride yourself for anything else.
 *
 * CAVEAT for x16_bmx_save(): VERA's palette region reads back the last
 * value the HOST wrote, not the hardware's state. The palette you save
 * is only meaningful if this program set those entries itself.
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

#ifndef X16_BMX_H
#define X16_BMX_H

#define X16_BMX_ERR_IO      1   /* open, read or write failed */
#define X16_BMX_ERR_FORMAT  2   /* not a BMX, or not version 1 */
#define X16_BMX_ERR_PACKED  3   /* compressed data is not supported */

/* The image description. The fields are block-copied to and from the
** assembly's operand block, so the order is load-bearing. Do not reorder.
*/
typedef struct {
    unsigned int  width;
    unsigned int  height;
    unsigned char bpp;          /* 1, 2, 4 or 8 */
    unsigned char palstart;     /* first palette index */
    unsigned int  palcount;     /* 1-256 entries */
    unsigned char border;
    unsigned int  stride;       /* VRAM bytes between row starts */
} x16_bmx_info;

/* Read what the last load found, or what the next save will write. */
void x16_bmx_get_info (x16_bmx_info *out);

/* Describe the image before saving it. bpp defaults to 8, palcount to
** 256 and stride to 320.
*/
void x16_bmx_set_info (const x16_bmx_info *in);

/* Palette into the VERA palette, pixels into VRAM at `vaddr`. Returns 0
** on success, else an X16_BMX_ERR_* code. The header fields are
** published through x16_bmx_get_info().
*/
unsigned char x16_bmx_load (const char *name, unsigned char len,
                                         unsigned char device,
                                         unsigned long vaddr);

/* Write a BMX file from VRAM. Describe the image first. */
unsigned char x16_bmx_save (const char *name, unsigned char len,
                                         unsigned char device,
                                         unsigned long vaddr);

/* Load a BMX into the VERA_2 640x480 SDRAM bitmap instead (the gfx8h
** engine): the palette goes to the VERA_2 palette, the pixel rows land
** 640 bytes apart from SDRAM offset 0, so a full-width 640x480x8 image
** is a plain contiguous load. Select the mode first (x16_gfx8h_init()).
**
** REQUIRES THE VERA_2 LAYER -- feature-detect with x16_gfx8h_has() from
** x16/bitmap8h.h first. On stock hardware (and the emulator) the file
** still parses, but every pixel write goes to open bus.
**
** Unlike x16_bmx_load() there is no length argument: `name` is an
** ordinary NUL-terminated C string. Returns 0 on success, else an
** X16_BMX_ERR_* code, and publishes the header fields either way.
*/
unsigned char x16_bmx_load_hires (const char *name,
                                               unsigned char device);

/* Why the last x16_bmx_* call failed: the X16_BMX_ERR_* it returned, or
** 0 after a call that worked. For call sites that could not keep the
** return value.
*/
unsigned char x16_bmx_lasterr (void);

#endif /* X16_BMX_H */
