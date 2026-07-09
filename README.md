# x16clib

A C library for the Commander X16. Write games and demos in C without
re-deriving the machine's hardware surface every time.

This is a routine-by-routine port of [x16lib](../x16_library), an ACME
assembly library, to a cc65-linkable archive with C headers. The assembly
is preserved, not rewritten: every hot loop is the same hand-written 6502.
What is new is a thin `__fastcall__` shim in front of each routine, and a
set of headers that say what the hardware actually does.

## What it gives you that cc65 does not

cc65's `cx16` target already ships `conio`, `vpeek`/`vpoke`, `waitvsync`,
`videomode`, and joystick/mouse/TGI drivers. It ships **no sound API at
all**, and only enable-toggles for sprites and layers.

| Header | Covers |
|---|---|
| `x16/vera.h` | VRAM data ports: fast fill, port-to-port copy, FX probe |
| `x16/screen.h` | screen mode (including 320x240 bitmap), text, cursor |
| `x16/palette.h` | 256 entries of 12-bit colour, single and bulk |
| `x16/tile.h` | tilemap cells, layer config, 12-bit hardware scroll |
| `x16/sprite.h` | all 128 hardware sprites |
| `x16/bitmap.h` | 320x240x256 pset, lines, rects, Bresenham |
| `x16/verafx.h` | VERA FX: hardware multiply, 4x fills, **hardware lines and filled triangles** |
| `x16/psg.h` | the 16-voice PSG |
| `x16/ym.h` | the YM2151 FM chip |
| `x16/pcm.h` | the PCM FIFO, and **AFLOW-driven streaming** |
| `x16/input.h` | joystick, mouse, keyboard |
| `x16/irq.h` | VSYNC frame counter, **raster interrupts, hardware sprite collision** |
| `x16/bank.h` | banked RAM, boundary-crossing copies, **a whole-bank allocator** |
| `x16/mem.h` | **KERNAL block ops: fill, copy, CRC-16, and LZSA2 depacking** |
| `x16/load.h` | load and save, including straight into VRAM |
| `x16/fixed.h` | 8.8 fixed point, 16x16 multiply |
| `x16/collide.h` | 8- and 16-bit bounding-box overlap |
| `x16/float.h` | a binding to the ROM's floating point library |

Four of those are things the machine can do that nothing else exposes to
C. `x16_mem_decompress()` is an **LZSA2 depacker sitting in ROM**, and
because KERNAL block operations do not increment an address in
`$9F00-$9FFF`, it unpacks a compressed asset **straight into video
memory**:

```c
x16_vera_addr0(X16_INC_1, X16_VRAM_SPRITE_DATA);
x16_mem_decompress(tiles_lzsa, X16_VERA_DATA0);   /* no staging buffer */
```

`x16_irq_line_install()` gives raster splits, and `x16_sprite_collisions()`
reads VERA's own collision hardware instead of comparing rectangles in
software. `x16_fx_line()` and `x16_fx_triangle()` hand the Bresenham error
to VERA, leaving the CPU one store per pixel. `x16_pcm_stream_start()`
plays a buffer longer than the 4 KB FIFO, refilled from the AFLOW
interrupt.

What it deliberately does **not** port: the assembly library's `int16`,
`int32`, `number` and `bits` modules. C already has `int`, `long`,
`printf` and bitwise operators, and cc65 implements them better than a
hand-rolled fixed-register API would. The `float` module is kept because
binding the ROM's FP library saves linking cc65's own, several thousand
bytes.

For music rather than sound effects, use [ZSMKit](https://github.com/mooinglemur/zsmkit)
alongside this. It is the community's ZSM player and already ships cc65
demos.

## Prerequisites

Two third-party things are expected but **not** committed:

| Path | What | Where from |
|---|---|---|
| cc65 | the compiler | <https://cc65.github.io/> |
| `emulator\x16emu.exe` + `rom.bin` | X16 emulator r49 | <https://github.com/X16Community/x16-emulator>, ROM from <https://github.com/X16Community/x16-rom> |

`build.ps1` finds cc65 through `%CC65_HOME%\bin`, then `C:\Emulator\cc65\bin`,
then `C:\cc65\bin`, then `PATH`.

Use the **r49** emulator and ROM: the constants in `src/core/` are
transcribed from the r49 ROM sources, and the test suite asserts against
r49 behaviour.

## Quick start

```powershell
.\build.ps1                              # build the lib + examples\hello.c
.\build.ps1 -Run                         # ...and run it windowed
.\build.ps1 -Source examples\bounce.c -Run
.\build.ps1 -Test                        # the headless regression suite
```

`-Run` runs the emulator **windowed**. `-Test` is headless and raises no
VSYNC interrupt, so anything calling `x16_vsync_wait()` would hang there.

## Writing a program

```c
#include <x16/vera.h>
#include <x16/screen.h>

int main(void)
{
    x16_screen_cls();
    x16_screen_puts("HELLO FROM X16CLIB\r");

    /* A text cell is two bytes: screen code, then colour. Stepping the
    ** data port by 2 writes only the screen codes.
    */
    x16_vera_addr0(X16_INC_2, X16_VRAM_TEXT);
    x16_vera_fill(0x2A, 80);            /* '*' across the top row */

    return 0;
}
```

Build it:

```
cl65 -t cx16 -O -I include -o PROG.PRG prog.c build\x16c.lib
```

`<x16/x16.h>` pulls in everything, and costs nothing at run time: ld65
links only the library modules your program actually calls. That is what
replaced the assembly library's `X16_USE_*` gates -- ACME has no linker
and could not strip dead code, so the modules had to be selected by hand.

## Conventions

**Every symbol is prefixed `x16_`.** cc65's `<cx16.h>`, `<conio.h>` and
`<cbm.h>` stay usable alongside it. Where cc65 already does a job well
this library does not duplicate it: use cc65's `vpeek`/`vpoke` for single
VRAM bytes, `conio` for text, `printf` for formatting.

**All entry points are `__fastcall__`**, cc65's default.

**Errors** come back as a value, not a flag: `0` for failure where a
boolean makes sense (`x16_screen_set_mode`, `x16_ym_init`), or the KERNAL
error code where there is one (`x16_fs_load`).

## Things the hardware will get you wrong

Carried over from the assembly library's notes, plus what the C boundary
adds.

**`VERA_INC_*` is not `X16_INC_*`.** cc65's `<cx16.h>` defines constants
already shifted into `ADDR_H`'s bits 7:4, so its `VERA_INC_1` is `0x10`.
This library's `X16_INC_1` is the raw index, `1`. Same bits on the wire,
sixteen times apart as constants. Pass `X16_INC_*` to these functions and
`VERA_INC_*` to cc65's; never mix them.

**The KERNAL requires ADDRSEL = 0.** Several KERNAL screen routines write
VERA's address registers *before* selecting a port, assuming port 0 is
current. Call them with port 1 selected -- which `x16_vera_addr1()` and
`x16_vera_copy()` leave behind -- and the display corrupts. Every routine
in `x16/screen.h` clears ADDRSEL first. Raw `CHROUT` does not; use
`x16_screen_chrout()`.

**The scratch block is not reentrant.** The library keeps 16 zero-page
bytes that the linker places in the `$22-$7F` user window. An interrupt
handler must not call any `x16_*` routine that touches it -- in practice,
anything with more than three arguments or any 16-bit argument. `x16/irq.h`
lists what is safe.

**Sprite coordinates are 640x480**, not the 320x240 of the bitmap modes.
The KERNAL leaves HSCALE and VSCALE at 128 in the default text modes.

**VERA's `$1F9C0-$1FFFF` is write-only** -- PSG, palette, sprite
attributes. A read returns the last value *this program* wrote. So
`x16_sprite_z()`, a read-modify-write, is only meaningful after
`x16_sprite_init_all()` has given every record a known shadow.

**The YM2151's ROM driver takes the channel in `.A` and the payload in
`.X`** -- the reverse of the register-level `x16_ym_write()`, and the
reverse of intuition. The wrappers hide it, which is why `x16_ym_write()`
reads `(register, value)` while everything else reads `(channel, ...)`.
Get it backwards and the chip plays a valid-looking note on the wrong
channel: it fails silently.

**Three ROM floating-point routines are documented backwards.**
`fp_fsub` computes `mem - FAC`, not `FAC - mem`; same for `fp_fdiv`.
`x16_f_sub()` and `x16_f_div()` present the intuitive direction at the
cost of two extra bank crossings; `x16_f_rsub()` and `x16_f_rdiv()` expose
the raw order, which is what you want for `1/x`. And `fp_negfac`, despite
its name, is not a negate: it is an internal helper of the add/subtract
path that denormalises a normal FAC. The real unary minus is `fp_negop`.

**`x16_key_peek()`'s character is unspecified when the queue is empty.**
Only the count is meaningful.

**VERA's `$9F28` is IRQ_LINE on write and SCANLINE on read**, so the
raster line you programmed can never be read back. `IEN` is two registers
in one, too: bit 7 is IRQ_LINE's bit 8 (read and write), bit 6 is
SCANLINE's bit 8 (read-only). That is what makes `tsb`/`trb` on `IEN`
safe -- their read-modify-write only ever puts bit 6 back, and bit 6
ignores writes.

**A carry survives an FX operation.** Writing an increment register seeds
the subpixel accumulator to half a pixel but leaves the position's carry
bit alone, whatever the FX reference implies. Draw a filled triangle and
then a line, and the line's first minor-axis step is eaten unless X_POS is
zeroed explicitly. `x16_fx_line()` does. (A line after a line does *not*
reproduce it -- only the polygon filler, which steps two edge accumulators
twice per row, leaves the carry set.)

**AFLOW cannot be acknowledged.** It clears only by refilling the FIFO, so
a refiller that runs out of data must disable AFLOW in `IEN` or the
interrupt storms forever. This is also why `x16_irq_remove()` stops a
stream: with the handler unhooked, nothing would ever clear it.

**A C interrupt callback would corrupt the zero page**, because cc65's
compiled code uses `sp`, `sreg`, `ptr1-4`, `tmp1-4` and `regbank`, and the
code it interrupted owns all of them mid-expression. cc65's own
interruptor mechanism does not save them. So this library does: 42 bytes,
about 950 cycles, and only when a callback is installed. That is what lets
a raster handler be written in C, and call any `x16_*` routine, rather
than being usually-correct.

**`x16_f_to_str()` copies.** The ROM writes its answer into `$0100`, the
bottom of the stack page. The raw ROM call's buffer would not survive the
next deep call; the wrapper copies it into yours before returning.

## Tests

```powershell
.\build.ps1 -Test              # headless: 112 tests in a few seconds
.\build.ps1 -Test -Windowed    # with video: 118, nothing skipped but one
```

Every test drives the library one way and verifies through an
**independent path** -- write through a library call on port 0, read back
with cc65's `vpeek()` -- so a bug in the address plumbing cannot hide
behind itself. Any `FAIL`, a pass count that disagrees with the total, a
missing `DONE` line, or a timeout fails the build with a real exit code.

**Run it both ways.** Headless `-testbench` has no video, so VERA raises
neither VSYNC nor a raster interrupt and the CPU takes no IRQ at all. Six
tests therefore skip -- and say so rather than passing quietly. Each uses
the KERNAL's jiffy clock as an independent oracle, to tell "no interrupts
here at all" (skip) apart from "the interrupt ran and our handler did not"
(a real failure). `-Windowed` opens a display at real speed and every one
of them runs.

That distinction is not academic. A shim that clobbers a handler's address
is invisible to any headless test, because the vector is only dereferenced
when the interrupt fires.

Several tests exist to pin bugs found and fixed in the assembly library,
so a regression cannot slip back in: `FS_VLOAD` (the bank must reach
`LOAD`'s `.A`, not the secondary address), `F_NEG` (`fp_negfac` is not a
negate), `F_FROM_U8` (`fp_float` is signed), `GFX_CLEAR` (checks the
*last* pixel, since a truncated count clears only the top 35 rows),
`PAL_LOAD_ZERO`, `SPRITE_SIZE_PAL_MASK`, and `IRQ_PRESERVES_IFLAG`.

Others pin hardware behaviour that is easy to get wrong and impossible to
notice: `FX_LINE_CARRY` draws a triangle before a line, because that is
the only sequence that leaves the FX accumulator's carry set;
`IRQ_LINE_AT_300` proves the raster line landed by asking the beam where
it is, since IRQ_LINE cannot be read back; `IRQ_ZP_PRESERVED` parks a
sentinel in the scratch block and lets an interrupt handler overwrite it;
`PCM_STREAM_EXHAUST` plays a real buffer to its end and checks the refill
interrupt disarmed itself.

### The ABI tests

Thirteen tests exist only because of the C boundary. A shim that pops its
arguments in the wrong order does not crash -- it quietly does the wrong
thing. A shim that pops the wrong *number* of bytes corrupts cc65's stack
pointer and crashes several calls later, nowhere near the cause.

So each `ABI_*` test is built to be provably capable of failing, and each
was checked by introducing the exact bug it targets. `ABI_COLLIDE8_ARGORDER`
is the sharpest: its geometry is chosen so that transposing *any* adjacent
argument pair, or reversing the whole pop order, flips the answer.

```
A spans x[10,30) y[100,104)      x16_collide8(10, 100, 20, 4,
B spans x[28,30) y[70,105)                    28,  70,  2, 35) == 1
```

Make A tall instead of wide, or B wide instead of tall, and the overlap
vanishes. The ordinary collision tests use symmetric 10x10 boxes and never
notice.

## Layout

```
include/x16/     the C API, one header per module
src/core/        constants, macros, the zero-page block
src/*/           one .s per module -> one .o -> one member of x16c.lib
examples/        hello.c, bounce.c, numbers.c
test/            runner.c, testlib.h, fsroot/
build/           objects, x16c.lib, PRGs        (gitignored)
```

Each `.s` holds the ported routine under its original bare label plus an
exported `_x16_*` shim in front of it, so the tail-call between them is a
local `jmp` and the C world never sees the internal names.

## Porting notes

If you are looking at the ACME original alongside this:

| ACME | ca65 |
|---|---|
| `!cpu 65c02` | `--cpu 65C02` on the command line |
| `!source "f"` | `.include "f.inc"` for constants and macros only; code modules link |
| `!zone` + `.label` | plain unexported labels |
| `@label` | `@label`, identical |
| `!macro m .a {}` / `+m x` | `.macro m a … .endmacro` / `m x` |
| `!byte`/`!word`/`!text`/`!fill n,v` | `.byte`/`.word`/`.byte "…"`/`.res n,v` |
| `!if` / `!error` | `.if` / `.fatal` |
| `!ifdef X !eof` guard | `.ifndef X_INC` … `.endif` around the file |
| `* = $0801`, `+basic_stub` | deleted; cc65's crt0 emits both |
| `!addr` (force absolute) | no equivalent: segment placement plus `.importzp` |
| `X16_USE_*` gates | deleted; ld65 pulls library members by symbol |

Two traps in that table are worth naming. `!addr` has no ca65 counterpart:
addressing is decided by which segment a symbol lives in and whether it
was imported with `.importzp` or `.import`. Import a page-zero symbol with
plain `.import` and ca65 assumes absolute addressing, emitting a
three-byte instruction against an address the linker resolves below
`$0100`. And the assembly library's hardcoded `$22` zero-page block is
gone: cc65's `cx16.cfg` maps its `ZEROPAGE` segment onto exactly that
window, so `src/core/x16zp.s` declares the block and the linker places it.
