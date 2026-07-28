/* =====================================================================
 * x16clib :: x16/vdc.h -- VERA display composer helpers
 * =====================================================================
 * The display composer is VERA's own output stage: video mode, layer
 * enables, pixel scaling, border colour and the active display window
 * ($9F29-$9F2C behind DCSEL 0/1). This drives VERA itself -- not the
 * C128's 8563 chip that the VDC name usually means.
 *
 *      x16_vdc_set_scale(0x40, 0x40);        // 2x pixels: 320x240
 *      x16_vdc_set_border(6);                // blue border
 *      x16_vdc_set_active(32, 608, 24, 456); // letterbox the picture
 *
 * Every call leaves DCSEL at 0, the state the rest of the library
 * assumes, so these can be mixed freely with the other x16_* video
 * calls.
 * =====================================================================
 */

#ifndef X16_VDC_H
#define X16_VDC_H

/* DC_VIDEO bits, for get/set_video and set_layers/layer_on/off. */
#define X16_VDC_OUTPUT_OFF      0x00
#define X16_VDC_OUTPUT_VGA      0x01
#define X16_VDC_OUTPUT_NTSC     0x02
#define X16_VDC_OUTPUT_RGB      0x03
#define X16_VDC_CHROMA_DISABLE  0x04
#define X16_VDC_240P            0x08
#define X16_VDC_LAYER0          0x10
#define X16_VDC_LAYER1          0x20
#define X16_VDC_SPRITES         0x40
#define X16_VDC_FIELD           0x80    /* read-only interlace field bit */

/* The raw DC_VIDEO byte. Setting ignores bit 7 (FIELD is read-only). */
unsigned char x16_vdc_get_video (void);
void x16_vdc_set_video (unsigned char video);

/* Change only the output mode bits (X16_VDC_OUTPUT_*), or only the
** three enable bits. x16_vdc_set_layers() replaces all three at once;
** layer_on/layer_off set and clear the ones in `mask`.
*/
void x16_vdc_set_output (unsigned char mode);
void x16_vdc_set_layers (unsigned char mask);
void x16_vdc_layer_on (unsigned char mask);
void x16_vdc_layer_off (unsigned char mask);

/* Pixel scaling: $80 = one output pixel per input pixel (640x480),
** $40 = 2x (320x240), $20 = 4x. The get packs HSCALE in the low byte,
** VSCALE in the high.
*/
unsigned int x16_vdc_get_scale (void);
void x16_vdc_set_scale (unsigned char hscale,
                                     unsigned char vscale);

/* The border palette index, shown outside the active window. */
unsigned char x16_vdc_get_border (void);
void x16_vdc_set_border (unsigned char index);

/* ---------------------------------------------------------------------
 * The active display window.
 *
 * The raw registers hold native coordinates with low bits omitted:
 * HSTART/HSTOP = pixel / 4, VSTART/VSTOP = pixel / 2.
 * x16_vdc_set_active() takes real pixel coordinates (0-640, 0-480)
 * and converts. x16_vdc_fullscreen() restores the whole screen.
 * ------------------------------------------------------------------ */
typedef struct {
    unsigned char hstart;       /* pixels / 4 */
    unsigned char hstop;        /* pixels / 4 */
    unsigned char vstart;       /* pixels / 2 */
    unsigned char vstop;        /* pixels / 2 */
} x16_vdc_active;

void x16_vdc_get_active_raw (x16_vdc_active *out);
void x16_vdc_set_active_raw (const x16_vdc_active *in);
void x16_vdc_set_active (unsigned int hstart,
                                      unsigned int hstop,
                                      unsigned int vstart,
                                      unsigned int vstop);
void x16_vdc_fullscreen (void);

/* ---------------------------------------------------------------------
 * The VERA bitstream version (DCSEL=63). Returns 1 with the fields
 * filled in when the registers answer; 0 and zeros on bitstreams from
 * before they existed. Agrees with x16_vera_has_fx(), which probes the
 * same magic.
 * ------------------------------------------------------------------ */
typedef struct {
    unsigned char major;
    unsigned char minor;
    unsigned char build;
} x16_vdc_version;

unsigned char x16_vdc_get_version (x16_vdc_version *out);

#endif /* X16_VDC_H */
