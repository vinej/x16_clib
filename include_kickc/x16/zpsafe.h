/* =====================================================================
 * x16clib :: x16/zpsafe.h -- keep KickC off the KERNAL's zero page
 * =====================================================================
 * Every x16clib header includes this first. It exists because KickC's
 * cx16 target only reserves $FC-$FF, but on the X16 the USER zero page
 * is $22-$7F and nothing else:
 *
 *   $02-$21  the KERNAL's API registers r0-r15
 *   $80-$FF  KERNAL, BASIC and DOS internals
 *
 * Left unreserved, KickC happily allocates a variable at, say, $83 --
 * and the first CHROUT scribbles over it. That is not a theoretical
 * hazard: this library's own test suite caught the KERNAL's screen
 * editor corrupting a KickC variable exactly there, failing or passing
 * with every layout shift. cc65's cx16 configuration reserves the same
 * ranges for the same reason.
 *
 * The second range starts at $76, not $80: $76-$7F is the library's own
 * pinned block -- $76/$77 the PCM streamer's interrupt-time pointer,
 * $78-$7F four pointer slots the modules share (one module's slots are
 * never live across a call into another; the IRQ dispatcher saves
 * $02-$7F around C callbacks). They are pinned with __address() because
 * KickC ignores __zp/__mem on parameters, floods zero page with them,
 * and then silently spills a __zp global to main memory -- where
 * (ptr),y assembles to garbage.
 *
 * This leaves KickC 84 bytes of the 94 cc65 gets. If your program wants
 * to gamble on more, you know where these lines live.
 *
 * NO INCLUDE GUARD, deliberately. KickC's preprocessor #define state is
 * global across every file of the whole-program compile, but pragma
 * state is per-file: a guard would let the pragmas into the first file
 * only, and every library file compiled after it would allocate zero
 * page unreserved. Repeating the pragmas is harmless.
 * =====================================================================
 */

#pragma zp_reserve(0x02..0x21)
#pragma zp_reserve(0x76..0xfb)
