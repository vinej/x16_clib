// =====================================================================
// x16clib :: x16/screen.c -- screen mode, text output, cursor
// =====================================================================
//
// ---------------------------------------------------------------------
// THE KERNAL REQUIRES ADDRSEL = 0.
//
// Several KERNAL screen routines write VERA_ADDR_L/M/H *before* they
// set ADDRSEL, taking it on faith that port 0 is already selected. The
// screen scroller is the clearest case (x16-rom-r49
// kernal/drivers/x16/screen.s): call it with ADDRSEL = 1 and the
// destination lands in port 1, where the source promptly overwrites
// it. So every routine here that enters a KERNAL routine which touches
// VERA forces ADDRSEL = 0 first -- that is the two-instruction
//      lda #$01 / trb $9f25
// preamble (the ca65 build's vera_addrsel macro).
//
// Note also that the KERNAL leaves DCSEL = 0, so do not expect a DCSEL
// selection to survive a call into it.
//
// One structural difference from the ca65 build: its internal routines
// ended with `jmp KERNAL_ENTRY`, a tail call. Here that jmp would skip
// the epilogue KickC appends after the asm block, so every KERNAL entry
// is a jsr instead -- one rts frame, six cycles, for not depending on
// what the code generator emits around the block.
// =====================================================================

#include <x16/screen.h>
#include <x16/vera.h>

// Pointer scratch, pinned in zero page (KickC ignores __zp on
// parameters; see x16/zpsafe.h). The cc65 build used ptr1/ptr2.
__address(0x78) const char* volatile x16__sc_p0;
__address(0x7a) char* volatile x16__sc_p1;
__address(0x7c) char* volatile x16__sc_p2;

__mem volatile char x16__sc_t0;
__mem volatile char x16__sc_t1;

// ---------------------------------------------------------------------
// KERNAL SCREEN_MODE reports failure in the carry, and takes carry
// clear to mean "set".
// ---------------------------------------------------------------------
unsigned char x16_screen_set_mode(__mem unsigned char mode) {
    __mem char r;
    asm {
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/
        lda mode
        clc
        jsr $ff5f /*SCREEN_MODE*/       // carry set = unsupported
        lda #0
        rol                             // carry -> bit 0
        eor #1                          // ...report success, not failure
        sta r
    }
    return r;
}

unsigned char x16_screen_get_mode(void) {
    __mem char r;
    asm {
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/
        sec
        jsr $ff5f /*SCREEN_MODE*/
        sta r
    }
    return r;
}

void x16_screen_reset(void) {
    asm {
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/
        jsr $ff81 /*CINT*/
    }
}

void x16_screen_cls(void) {
    asm {
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/
        lda #$93 /*PETSCII_CLS*/
        jsr $ffd2 /*CHROUT*/
    }
}

void x16_screen_chrout(__mem unsigned char c) {
    asm {
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/
        lda c
        jsr $ffd2 /*CHROUT*/
    }
}

// ---------------------------------------------------------------------
// Sets the colour used by every subsequent CHROUT. Writes the KERNAL's
// editor colour byte directly -- there is no jump-table entry for
// this. Touches no VERA state.
// ---------------------------------------------------------------------
void x16_screen_color(__mem unsigned char fg, __mem unsigned char bg) {
    asm {
        lda fg
        and #$0f
        sta x16__sc_t0
        lda bg
        and #$0f
        asl
        asl
        asl
        asl                             // background into the high nibble
        ora x16__sc_t0
        sta $0376 /*KERNAL_COLOR*/
    }
}

// ---------------------------------------------------------------------
// DC_BORDER is only visible when DCSEL = 0, so select that bank first
// (keeping ADDRSEL, never writing CTRL bit 7 -- that resets VERA).
// Does not enter the KERNAL.
// ---------------------------------------------------------------------
void x16_screen_border(__mem unsigned char color) {
    asm {
        lda $9f25 /*VERA_CTRL*/         // vera_dcsel 0
        and #$01 /*VERA_CTRL_ADDRSEL*/
        sta $9f25 /*VERA_CTRL*/
        lda color
        sta $9f2c /*VERA_DC_BORDER*/
    }
}

// ---------------------------------------------------------------------
// KERNAL PLOT takes carry clear to mean "set". No ADDRSEL guard here:
// PLOT only moves the cursor variables and never touches VERA.
// ---------------------------------------------------------------------
void x16_screen_locate(__mem unsigned char rowv, __mem unsigned char col) {
    asm {
        ldx rowv
        ldy col
        clc
        jsr $fff0 /*PLOT*/
    }
}

// `rowp`, not `row`: `row` is a reserved word in KickC's inline-asm
// grammar, and an asm operand of (row),y is a parse error.
void x16_screen_get_cursor(unsigned char *rowp, unsigned char *colp) {
    x16__sc_p1 = (char*)rowp;
    x16__sc_p2 = (char*)colp;
    asm {
        sec
        jsr $fff0 /*PLOT*/              // X = row, Y = col
        stx x16__sc_t0                  // both X and Y feed the stores,
        sty x16__sc_t1                  // so stash them first
        ldy #0
        lda x16__sc_t0
        sta (x16__sc_p1),y
        lda x16__sc_t1
        sta (x16__sc_p2),y
    }
}

// 1 = ISO, 2 = PET upper/graphics, 3 = PET upper/lower, ... 12 Katakana
void x16_screen_charset(__mem unsigned char charset) {
    asm {
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/
        lda charset
        jsr $ff62 /*SCREEN_SET_CHARSET*/
    }
}

// ---------------------------------------------------------------------
// Prints a NUL-terminated string, truncated at 255 bytes. CHROUT
// preserves A, X and Y, so the index survives the call.
// ---------------------------------------------------------------------
void x16_screen_puts(const char *s) {
    x16__sc_p0 = s;
    asm {
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/
        ldy #0
    sp_loop:
        lda (x16__sc_p0),y
        beq sp_done
        jsr $ffd2 /*CHROUT*/
        iny
        bne sp_loop
    sp_done:
    }
}

// ---------------------------------------------------------------------
// The LIVE text grid, after whatever x16_screen_set_mode() left behind
// -- not the 80x60 default. SCREEN answers in X and Y, both of which the
// stores need, so stash them first.
// ---------------------------------------------------------------------
void x16_screen_get_size(unsigned char *colsp, unsigned char *rowsp) {
    x16__sc_p1 = (char*)colsp;
    x16__sc_p2 = (char*)rowsp;
    asm {
        jsr $ffed /*SCREEN*/            // X = columns, Y = rows
        stx x16__sc_t0
        sty x16__sc_t1
        ldy #0
        lda x16__sc_t0
        sta (x16__sc_p1),y
        lda x16__sc_t1
        sta (x16__sc_p2),y
    }
}

// =====================================================================
// Direct text-map access
// =====================================================================
// CHROUT costs several hundred cycles a character once the editor's
// scroll checks, colour handling and cursor bookkeeping are paid for. A
// program that repaints a whole text screen cannot afford that, so these
// write VERA's tile map itself: x16_screen_addr() points port 0 at a
// cell with auto-increment 1, and each following pair of bytes is one
// character and its colour.
//
// The address arithmetic stays in assembly, exactly as the ca65 build
// has it. Written in C it would need a variable shift count and a
// 17-bit intermediate, neither of which KickC handles gracefully -- and
// the answer is a VRAM address, where being quietly off by a row is
// invisible until the screen looks wrong.
// =====================================================================

__mem volatile char x16__sm_a0;         // the address being built
__mem volatile char x16__sm_a1;
__mem volatile char x16__sm_a2;
__mem volatile char x16__sm_s0;         // row << shift, 16 bits
__mem volatile char x16__sm_s1;
__mem volatile char x16__sm_row;
__mem volatile char x16__sm_col;
__mem volatile char x16__sm_cnt;        // blit count
__mem volatile char x16__sm_clr;        // blit colour
__mem volatile char x16__sm_ch;         // blitfill screen code

// address of (row, column) into x16__sm_a0/a1/a2, port untouched
void x16__screen_addr_calc(unsigned char row, unsigned char col) {
    x16__sm_row = row;
    x16__sm_col = col;
    asm {

        lda $9f35 /*VERA_L1_MAPBASE*/   // map base = MAPBASE << 9
        asl                             // carry = bit 16
        sta x16__sm_a1                  // mid
        lda #0
        rol
        sta x16__sm_a2                  // high
        lda #0
        sta x16__sm_a0                  // low

        lda $9f34 /*VERA_L1_CONFIG*/    // MAP_WIDTH: 0=32 1=64 2=128 3=256
        lsr
        lsr
        lsr
        lsr
        and #3
        clc
        adc #6                          // bytes per row = 2 << (5 + width)
        tay

        lda x16__sm_row                 // row << Y
        sta x16__sm_s0
        lda #0
        sta x16__sm_s1
    sa_shift:
        asl x16__sm_s0
        rol x16__sm_s1
        dey
        bne sa_shift

        clc                             // base += row * stride
        lda x16__sm_a0
        adc x16__sm_s0
        sta x16__sm_a0
        lda x16__sm_a1
        adc x16__sm_s1
        sta x16__sm_a1
        bcc sa_nc1
        inc x16__sm_a2
    sa_nc1:
        lda x16__sm_col                 // base += column * 2
        asl
        tax
        lda #0
        rol
        tay
        txa
        clc
        adc x16__sm_a0
        sta x16__sm_a0
        tya
        adc x16__sm_a1
        sta x16__sm_a1
        bcc sa_nc2
        inc x16__sm_a2
    sa_nc2:
    }
}

// Point VERA port 0 at a character cell. Leaves ADDRSEL = 0 and the
// increment set to 1.
void x16_screen_addr(unsigned char row, unsigned char col) {
    x16__screen_addr_calc(row, col);
    asm {
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        trb $9f25 /*VERA_CTRL*/
        lda x16__sm_a0
        sta $9f20 /*VERA_ADDR_L*/
        lda x16__sm_a1
        sta $9f21 /*VERA_ADDR_M*/
        lda x16__sm_a2
        and #$01                        // bit 16 of the address
        ora #$10                        // increment 1
        sta $9f22 /*VERA_ADDR_H*/
    }
}

// The same, for VERA port 1 -- the destination when moving text around
// with x16_vera_copy(); x16_screen_scroll() is the usual reason to want it.
void x16_screen_addr1(unsigned char row, unsigned char col) {
    x16__screen_addr_calc(row, col);
    asm {
        lda #$01 /*VERA_CTRL_ADDRSEL*/
        tsb $9f25 /*VERA_CTRL*/
        lda x16__sm_a0
        sta $9f20 /*VERA_ADDR_L*/
        lda x16__sm_a1
        sta $9f21 /*VERA_ADDR_M*/
        lda x16__sm_a2
        and #$01
        ora #$10
        sta $9f22 /*VERA_ADDR_H*/
    }
}

// PETSCII to screen code, the standard CBM folding. Exposed because a
// caller building its own tile data occasionally wants it.
unsigned char x16_screen_scode(__mem unsigned char c) {
    __mem char r;
    asm {
        lda c
        cmp #$20
        bcc sc_plus80                   // $00-$1F
        cmp #$40
        bcc sc_same                     // $20-$3F
        cmp #$60
        bcc sc_minus40                  // $40-$5F
        cmp #$80
        bcc sc_minus20                  // $60-$7F
        cmp #$a0
        bcc sc_plus40                   // $80-$9F
        cmp #$c0
        bcc sc_minus40                  // $A0-$BF
        sec                             // $C0-$FF
        sbc #$80
        jmp sc_same
    sc_plus80:
        clc
        adc #$80
        jmp sc_same
    sc_minus40:
        sec
        sbc #$40
        jmp sc_same
    sc_minus20:
        sec
        sbc #$20
        jmp sc_same
    sc_plus40:
        clc
        adc #$40
    sc_same:
        sta r
    }
    return r;
}

// Write a run of characters, all one colour, at the current port-0
// address; it is left just past the last cell, so runs can be chained.
void x16_screen_blit(const char *text, unsigned char count,
                     unsigned char color) {
    unsigned char i;
    x16__sm_cnt = count;
    x16__sm_clr = color;
    for (i = 0; i < x16__sm_cnt; i++) {
        x16__sm_ch = x16_screen_scode(text[i]);
        asm {
            lda x16__sm_ch
            sta $9f23 /*VERA_DATA0*/
            lda x16__sm_clr
            sta $9f23 /*VERA_DATA0*/
        }
    }
}

// The same with one repeated character: the usual way to blank part of a
// line. The character is folded once, not once per cell.
void x16_screen_blitfill(unsigned char count, unsigned char color,
                         unsigned char ch) {
    unsigned char i;
    x16__sm_cnt = count;
    x16__sm_clr = color;
    x16__sm_ch = x16_screen_scode(ch);
    for (i = 0; i < x16__sm_cnt; i++) {
        asm {
            lda x16__sm_ch
            sta $9f23 /*VERA_DATA0*/
            lda x16__sm_clr
            sta $9f23 /*VERA_DATA0*/
        }
    }
}

// ---------------------------------------------------------------------
// Slide a rectangle of the text screen up (down = 0) or down (1).
//
// Re-rendering a whole grid to scroll one line pays for every cell, and
// for a spreadsheet or a directory listing most of that cost is
// formatting the contents rather than drawing them. Moving the picture
// inside VRAM costs one row instead of a screenful.
//
// The rows uncovered at the trailing edge keep their old contents -- the
// caller draws what belongs there. Nothing happens when the distance is
// zero, or when it is large enough that nothing would survive.
//
// Vertical only: scrolling sideways would move a row onto itself, and
// x16_vera_copy() walks forward.
//
// Plain C here rather than assembly, because the loop body is three
// library calls; the ca65 build inlines them only because it can.
// ---------------------------------------------------------------------
void x16_screen_scroll(unsigned char top, unsigned char left,
                       unsigned char height, unsigned char width,
                       unsigned char distance, unsigned char down) {
    unsigned char rows;
    unsigned char i;
    unsigned char dst;
    unsigned char src;

    if (distance == 0) { return; }
    if (distance >= height) { return; }     // nothing would survive

    rows = height - distance;
    for (i = 0; i < rows; i++) {
        if (down == 0) {
            dst = top + i;                  // up: source is further down
            src = dst + distance;
        } else {
            dst = top + height - 1 - i;     // down: source is further up
            src = dst - distance;
        }
        x16_screen_addr1(dst, left);        // port 1 = destination
        x16_screen_addr(src, left);         // port 0 = source
        x16_vera_copy((unsigned int)width * 2);
    }
}
