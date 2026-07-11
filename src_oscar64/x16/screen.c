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
// the epilogue the compiler appends after the asm block, so every KERNAL entry
// is a jsr instead -- one rts frame, six cycles, for not depending on
// what the code generator emits around the block.
// =====================================================================

#include <x16/screen.h>

// PLOT hands row/col back in X/Y; they stage through these globals on
// the way out through the rowp/colp parameters, which the assembly
// indirects directly.

volatile char x16__sc_t0;
volatile char x16__sc_t1;

// ---------------------------------------------------------------------
// KERNAL SCREEN_MODE reports failure in the carry, and takes carry
// clear to mean "set".
// ---------------------------------------------------------------------
unsigned char x16_screen_set_mode(unsigned char mode) {
    return __asm {
        lda 0x9f25  /*VERA_CTRL*/
        and #0xfe
        sta 0x9f25
        lda mode
        clc
        jsr 0xff5f       /* carry set = unsupported (SCREEN_MODE) */
        lda #0
        rol                             /* carry -> bit 0 */
        eor #1                          /* ...report success, not failure */
        sta accu
    };
}

unsigned char x16_screen_get_mode(void) {
    return __asm {
        lda 0x9f25  /*VERA_CTRL*/
        and #0xfe
        sta 0x9f25
        sec
        jsr 0xff5f                      /* SCREEN_MODE */
        sta accu
    };
}

void x16_screen_reset(void) {
    __asm {
        lda 0x9f25  /*VERA_CTRL*/
        and #0xfe
        sta 0x9f25
        jsr 0xff81                      /* CINT */
    }
}

void x16_screen_cls(void) {
    __asm {
        lda 0x9f25  /*VERA_CTRL*/
        and #0xfe
        sta 0x9f25
        lda #0x93                       /* PETSCII_CLS */
        jsr 0xffd2                      /* CHROUT */
    }
}

void x16_screen_chrout(unsigned char c) {
    __asm {
        lda 0x9f25  /*VERA_CTRL*/
        and #0xfe
        sta 0x9f25
        lda c
        jsr 0xffd2                      /* CHROUT */
    }
}

// ---------------------------------------------------------------------
// Sets the colour used by every subsequent CHROUT. Writes the KERNAL's
// editor colour byte directly -- there is no jump-table entry for
// this. Touches no VERA state.
// ---------------------------------------------------------------------
void x16_screen_color(unsigned char fg, unsigned char bg) {
    __asm {
        lda fg
        and #0x0f
        sta x16__sc_t0
        lda bg
        and #0x0f
        asl
        asl
        asl
        asl                             /* background into the high nibble */
        ora x16__sc_t0
        sta 0x0376                      /* KERNAL_COLOR */
    }
}

// ---------------------------------------------------------------------
// DC_BORDER is only visible when DCSEL = 0, so select that bank first
// (keeping ADDRSEL, never writing CTRL bit 7 -- that resets VERA).
// Does not enter the KERNAL.
// ---------------------------------------------------------------------
void x16_screen_border(unsigned char color) {
    __asm {
        lda 0x9f25         /* vera_dcsel 0 (VERA_CTRL) */
        and #0x01                       /* VERA_CTRL_ADDRSEL */
        sta 0x9f25                      /* VERA_CTRL */
        lda color
        sta 0x9f2c                      /* VERA_DC_BORDER */
    }
}

// ---------------------------------------------------------------------
// KERNAL PLOT takes carry clear to mean "set". No ADDRSEL guard here:
// PLOT only moves the cursor variables and never touches VERA.
// ---------------------------------------------------------------------
void x16_screen_locate(unsigned char rowv, unsigned char col) {
    __asm {
        ldx rowv
        ldy col
        clc
        jsr 0xfff0                      /* PLOT */
    }
}

// `rowp`, not `row`: `row` collided with KickC's inline-asm grammar,
// and the name is kept so all the ports read alike.
void x16_screen_get_cursor(unsigned char *rowp, unsigned char *colp) {
    __asm {
        sec
        jsr 0xfff0              /* X = row, Y = col (PLOT) */
        stx x16__sc_t0                  /* both X and Y feed the stores, */
        sty x16__sc_t1                  /* so stash them first */
        ldy #0
        lda x16__sc_t0
        sta (rowp),y
        lda x16__sc_t1
        sta (colp),y
    }
}

// 1 = ISO, 2 = PET upper/graphics, 3 = PET upper/lower, ... 12 Katakana
void x16_screen_charset(unsigned char charset) {
    __asm {
        lda 0x9f25  /*VERA_CTRL*/
        and #0xfe
        sta 0x9f25
        lda charset
        jsr 0xff62                      /* SCREEN_SET_CHARSET */
    }
}

// ---------------------------------------------------------------------
// Prints a NUL-terminated string, truncated at 255 bytes. CHROUT
// preserves A, X and Y, so the index survives the call.
// ---------------------------------------------------------------------
void x16_screen_puts(const char *s) {
    __asm {
        lda 0x9f25  /*VERA_CTRL*/
        and #0xfe
        sta 0x9f25
        ldy #0
    sp_loop:
        lda (s),y
        beq sp_done
        jsr 0xffd2                      /* CHROUT */
        iny
        bne sp_loop
    sp_done:
    }
}
