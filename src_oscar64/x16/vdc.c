// =====================================================================
// x16clib :: x16/vdc.c -- VERA display composer helpers
// =====================================================================
// Four composer registers live at $9F29-$9F2C, and which four you get
// depends on DCSEL in VERA_CTRL:
//
//   DCSEL 0    DC_VIDEO   HSCALE   VSCALE   BORDER
//   DCSEL 1    HSTART     HSTOP    VSTART   VSTOP
//   DCSEL 63   VER0       VER1     VER2     VER3   (read-only)
//
// So every routine here selects a bank, touches its registers, and puts
// DCSEL back to 0 -- the state the rest of the library assumes.
//
// The registers are volatile pointers: the read-modify-writes below must
// actually re-read the hardware, and DCSEL means the SAME address is a
// different register from one statement to the next, which is precisely
// what a non-volatile access lets the compiler reorder or fold.
// =====================================================================

#include <x16/vdc.h>

#define VDC_CTRL (*(volatile unsigned char *)0x9F25)
#define VDC_R9   (*(volatile unsigned char *)0x9F29)
#define VDC_RA   (*(volatile unsigned char *)0x9F2A)
#define VDC_RB   (*(volatile unsigned char *)0x9F2B)
#define VDC_RC   (*(volatile unsigned char *)0x9F2C)

#define VDC_LAYER_MASK  (X16_VDC_LAYER0 | X16_VDC_LAYER1 | X16_VDC_SPRITES)

#define VDC_DCSEL_VERSION  63
#define VDC_VERSION_MAGIC  0x56         // 'V' in DC_VER0

// DCSEL is bits 1-6 of VERA_CTRL; bit 0 is ADDRSEL and belongs to
// whoever set it, so it survives.
static void vdc_dcsel(unsigned char n) {
    VDC_CTRL = (unsigned char)((VDC_CTRL & 0x01) | (n << 1));
}

// ---------------------------------------------------------------------
// The raw DC_VIDEO byte. Bit 7 is the read-only interlace field, so a
// set never writes it back.
// ---------------------------------------------------------------------
unsigned char x16_vdc_get_video(void) {
    vdc_dcsel(0);
    return VDC_R9;
}

void x16_vdc_set_video(unsigned char video) {
    vdc_dcsel(0);
    VDC_R9 = (unsigned char)(video & 0x7F);
}

// ---------------------------------------------------------------------
// Output mode alone, leaving chroma/240p/layer bits as they were.
// ---------------------------------------------------------------------
void x16_vdc_set_output(unsigned char mode) {
    vdc_dcsel(0);
    VDC_R9 = (unsigned char)((VDC_R9 & 0x7C) | (mode & 0x03));
}

// ---------------------------------------------------------------------
// The three enable bits: replace all of them, or set/clear a mask.
// ---------------------------------------------------------------------
void x16_vdc_set_layers(unsigned char mask) {
    vdc_dcsel(0);
    VDC_R9 = (unsigned char)((VDC_R9 & 0x0F) | (mask & VDC_LAYER_MASK));
}

void x16_vdc_layer_on(unsigned char mask) {
    vdc_dcsel(0);
    VDC_R9 = (unsigned char)(VDC_R9 | (mask & VDC_LAYER_MASK));
}

void x16_vdc_layer_off(unsigned char mask) {
    vdc_dcsel(0);
    VDC_R9 = (unsigned char)(VDC_R9 & ~(mask & VDC_LAYER_MASK));
}

// ---------------------------------------------------------------------
// Pixel scaling. $80 is 1:1; halving it doubles the pixels.
// ---------------------------------------------------------------------
unsigned int x16_vdc_get_scale(void) {
    unsigned char h, v;

    vdc_dcsel(0);
    h = VDC_RA;
    v = VDC_RB;
    return (unsigned int)h | ((unsigned int)v << 8);
}

void x16_vdc_set_scale(unsigned char hscale, unsigned char vscale) {
    vdc_dcsel(0);
    VDC_RA = hscale;
    VDC_RB = vscale;
}

// ---------------------------------------------------------------------
// The border colour index.
// ---------------------------------------------------------------------
unsigned char x16_vdc_get_border(void) {
    vdc_dcsel(0);
    return VDC_RC;
}

void x16_vdc_set_border(unsigned char index) {
    vdc_dcsel(0);
    VDC_RC = index;
}

// ---------------------------------------------------------------------
// The active window, in the composer's own units.
// ---------------------------------------------------------------------
void x16_vdc_get_active_raw(x16_vdc_active *out) {
    unsigned char a, b, c, d;

    vdc_dcsel(1);
    a = VDC_R9;
    b = VDC_RA;
    c = VDC_RB;
    d = VDC_RC;
    vdc_dcsel(0);

    out->hstart = a;
    out->hstop  = b;
    out->vstart = c;
    out->vstop  = d;
}

// One place stores the four registers, so the pixel and raw entries
// cannot drift apart.
static void vdc_store_active(unsigned char hstart, unsigned char hstop,
                             unsigned char vstart, unsigned char vstop) {
    vdc_dcsel(1);
    VDC_R9 = hstart;
    VDC_RA = hstop;
    VDC_RB = vstart;
    VDC_RC = vstop;
    vdc_dcsel(0);
}

void x16_vdc_set_active_raw(const x16_vdc_active *in) {
    vdc_store_active(in->hstart, in->hstop, in->vstart, in->vstop);
}

// ---------------------------------------------------------------------
// The same window in real pixels. Horizontal registers hold pixel / 4,
// vertical ones pixel / 2 -- so 640 wide is 160 and 480 tall is 240.
// ---------------------------------------------------------------------
void x16_vdc_set_active(unsigned int hstart, unsigned int hstop,
                        unsigned int vstart, unsigned int vstop) {
    vdc_store_active((unsigned char)(hstart >> 2),
                     (unsigned char)(hstop >> 2),
                     (unsigned char)(vstart >> 1),
                     (unsigned char)(vstop >> 1));
}

void x16_vdc_fullscreen(void) {
    vdc_store_active(0, 160, 0, 240);   // 0,0 to 640,480
}

// ---------------------------------------------------------------------
// The bitstream version. DC_VER0 reads 'V' on every bitstream that has
// these registers; on the older ones it does not, and the caller gets
// zeros and a 0 return. Same magic x16_vera_has_fx() probes, so the two
// always agree.
// ---------------------------------------------------------------------
unsigned char x16_vdc_get_version(x16_vdc_version *out) {
    unsigned char magic, major, minor, build;

    vdc_dcsel(VDC_DCSEL_VERSION);
    magic = VDC_R9;
    major = VDC_RA;                     // read while the bank is still
    minor = VDC_RB;                     // selected, whatever the magic
    build = VDC_RC;
    vdc_dcsel(0);

    if (magic != VDC_VERSION_MAGIC) {
        out->major = 0;
        out->minor = 0;
        out->build = 0;
        return 0;
    }
    out->major = major;
    out->minor = minor;
    out->build = build;
    return 1;
}
