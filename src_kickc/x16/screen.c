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
