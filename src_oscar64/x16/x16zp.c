// =====================================================================
// x16clib :: x16/x16zp.c -- the zero-page diagnostic
// =====================================================================
// The cc65 and llvm-mos builds share a sixteen-byte scratch block in zero
// page (X16_P0..P7 for arguments, X16_T0..T7 for private temporaries),
// because their modules are assembly and assembly needs somewhere to put
// a 16-bit operand. x16_zp_base() reports where the linker put it.
//
// THIS BUILD HAS NO SUCH BLOCK. The modules here are C, so an operand
// lives wherever Oscar64 decides -- usually an ordinary global, like
// bitmap8l.c's x16__gp_x. What zero page this library does claim is
// three pinned POINTERS (input.c's mouse scratch, load.c's start
// address, pcm.c's refill cursor), which need to be there because
// `(ptr),y` has no other addressing mode. They sit in Oscar64's own user
// region, and that region's base is a property of the target rather than
// of any one program.
//
// So the honest answer to "where is the scratch" is the base of that
// region, and it is a constant. Nothing depends on the value -- the ca65
// header says the same about its own -- and reporting the address of one
// arbitrary module's pointer instead would be both misleading and a
// reason to drag that module into every program that asks.
// =====================================================================

#include <x16/x16zp.h>

// Oscar64's user zero page on this target is $F7-$FF: nine bytes, of
// which this library uses eight.
#define X16_ZP_USER_BASE 0xF7

unsigned char x16_zp_base(void) {
    return X16_ZP_USER_BASE;
}
