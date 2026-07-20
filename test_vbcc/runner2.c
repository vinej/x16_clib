/* =====================================================================
 * x16clib :: test_vbcc/runner2.c -- v0.8.0 curve shapes (2bpp)
 * =====================================================================
 * Split out of runner.c: the curve-shape code pushes the all-in-one
 * runner past the vbcc ram region (ends at RAMEND-STACKLEN), so the
 * polygon / rounded-rect / arc / pie / bezier checks live in their own
 * small binary.
 * =====================================================================
 */
#include "testlib.h"
#include <x16/vera.h>
#include <x16/bitmap2.h>
#include <x16/shapes.h>

static void test_curves(void)
{
    static const unsigned int bpts[8] = { 20, 20, 30, 20, 40, 20, 50, 20 };

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 160UL * 41);
    x16_gfx2_fpolygon(40, 20, 12, 4, 0, 2);         /* filled square */
    t_check(x16_gfx2_read(40, 20) == 2 &&
            x16_gfx2_read(40, 7) == 0, "CURVE_POLYGON");

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 160UL * 41);
    x16_gfx2_frrect(20, 4, 40, 30, 8, 2);
    t_check(x16_gfx2_read(40, 19) == 2 &&
            x16_gfx2_read(20, 4) == 0, "CURVE_RRECT");

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 160UL * 41);
    x16_gfx2_arc(40, 20, 15, 0, 64, 3);             /* east -> south */
    t_check(x16_gfx2_read(55, 20) == 3 &&
            x16_gfx2_read(40, 35) == 3 &&
            x16_gfx2_read(25, 20) == 0, "CURVE_ARC");

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 160UL * 41);
    x16_gfx2_pie(40, 20, 15, 0, 64, 3);
    t_check(x16_gfx2_read(40, 20) == 3 &&
            x16_gfx2_read(45, 25) == 3 &&
            x16_gfx2_read(35, 15) == 0, "CURVE_PIE");

    x16_vera_addr0(X16_INC_1, X16_VRAM_BITMAP);
    x16_vera_fill(0x00, 160UL * 41);
    x16_gfx2_bezier(bpts, 3);                        /* collinear -> line */
    t_check(x16_gfx2_read(20, 20) == 3 &&
            x16_gfx2_read(50, 20) == 3 &&
            x16_gfx2_read(35, 20) == 3, "CURVE_BEZIER");
}

int main(void)
{
    t_init();
    test_curves();
    t_done();
    return 0;
}
