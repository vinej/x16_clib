/* =====================================================================
 * x16clib :: x16/x16zp.h -- the zero-page diagnostic
 * =====================================================================
 * Parity with the cc65 and llvm-mos builds, which keep a shared
 * sixteen-byte scratch block in zero page and report its address here.
 *
 * This build has no such block -- its modules are C, so operands live
 * wherever the compiler puts them. What comes back is the base of the
 * zero-page region Oscar64 reserves for user code, which is where the
 * library's three pinned pointers live. See x16zp.c for why that is the
 * honest answer rather than one module's variable.
 * =====================================================================
 */

#ifndef X16_X16ZP_H
#define X16_X16ZP_H

/* Diagnostic: the base of the library's zero-page footprint. Nothing
** depends on the value.
*/
unsigned char x16_zp_base (void);

/* pulls the implementation in with this header */
#pragma compile("x16zp.c")

#endif /* X16_X16ZP_H */
