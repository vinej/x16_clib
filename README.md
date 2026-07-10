# x16clib

A C library for the Commander X16. Write games and demos in C without
re-deriving the machine's hardware surface every time.

This is a routine-by-routine port of [x16lib](../x16_library), an ACME
assembly library, to a linkable archive with C headers. The assembly is
preserved, not rewritten: every hot loop is the same hand-written 6502.
What is new is a thin shim in front of each routine, and a set of headers
that say what the hardware actually does.

## Two toolchains

The same 28 modules and the same API build under **cc65** and under
**llvm-mos**. Pick either; they share no object code.

```
include_ca65/  src_ca65/  test_ca65/  build_ca65.ps1     cc65      158 tests
include_llvm/  src_llvm/  test_llvm/  build_llvm.ps1     llvm-mos   46 tests
examples/      doc/       emulator/   tools/             shared
```

```powershell
.\build_ca65.ps1 -Test                 # cc65:     158/158
.\build_llvm.ps1 -Test                 # llvm-mos:  46/46
.\build_llvm.ps1 -Source examples\bounce.c -Run
```

The two trees hold the *same* assembly for the internal routines --
`tools/ca65_to_llvm.py` translates the mechanical half -- and completely
different C entry points, because the calling conventions have nothing in
common:

|  | cc65 | llvm-mos |
|---|---|---|
| arguments | rightmost in `A`/`X`; the rest pushed on a software stack | assigned left to right into `A`, `X`, `__rc2`, `__rc3`, … |
| pointers | two bytes like anything else | a whole aligned `__rc` **pair** |
| `char` return | `A`, plus `ldx #0` for int promotion | `A` alone |
| pointer return | `A`/`X` | **`__rc2`/`__rc3`** |
| declaration | `__fastcall__` | nothing |

**Under llvm-mos, compile with `-mreserve-zp=16`** (`build_llvm.ps1` does).
The cx16 target leaves only ninety bytes of zero page, clang's LTO claims
as many as it likes, and this library reserves sixteen for its argument
block. Without the flag the link fails outright with `section '.zp.bss'
will not fit in region 'zp'`. See [include_llvm/x16/x16.h](include_llvm/x16/x16.h).

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
| `x16/bitmap.h` | 320x240x256 pset, lines, rects, Bresenham, **circles, discs, text, flood fill** |
| `x16/verafx.h` | VERA FX: hardware multiply, 4x fills, **hardware lines, filled triangles, blits, transparency** |
| `x16/psg.h` | the 16-voice PSG, and **ASR envelopes** |
| `x16/ym.h` | the YM2151 FM chip |
| `x16/pcm.h` | the PCM FIFO, and **AFLOW-driven streaming** |
| `x16/adpcm.h` | **IMA ADPCM: 4:1 compressed audio** |
| `x16/input.h` | joystick, mouse, keyboard |
| `x16/irq.h` | VSYNC frame counter, **raster interrupts, hardware sprite collision** |
| `x16/bank.h` | banked RAM, boundary-crossing copies, **a whole-bank allocator** |
| `x16/mem.h` | **KERNAL block ops: fill, copy, CRC-16, and LZSA2 depacking** |
| `x16/load.h` | load and save, including straight into VRAM |
| `x16/dos.h` | **the DOS command channel: status, delete, rename, mkdir, chdir** |
| `x16/bmx.h` | **BMX, the X16's native bitmap file format** |
| `x16/zx0.h` | **ZX0 depacking, tighter than LZSA2** |
| `x16/fixed.h` | 8.8 fixed point, 16x16 multiply |
| `x16/math.h` | **PRNG, sine/cosine tables, atan2, lerp** |
| `x16/collide.h` | 8- and 16-bit bounding-box overlap |
| `x16/clip.h` | **Cohen-Sutherland line clipping** |
| `x16/buffers.h` | **a ring buffer and a stack** |
| `x16/float.h` | a binding to the ROM's floating point library |

Several of those are things the machine can do that nothing else exposes
to C. `x16_mem_decompress()` is an **LZSA2 depacker sitting in ROM**, and
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
to VERA, leaving the CPU one store per pixel; `x16_fx_copy()` moves VRAM
to VRAM 32 bits at a time. `x16_pcm_stream_start()` plays a buffer longer
than the 4 KB FIFO, refilled from the AFLOW interrupt. `x16_bmx_load()`
reads the image format the community's tools and Prog8 write, which no C
library could read before. And `x16_dos_status()` is the difference
between "the save failed" and "the save failed because the disk is full".

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

**To just use the library, nothing needs rebuilding.** The finished
archives are committed, one per toolchain -- `dist_ca65\x16c.lib` and
`dist_llvm\libx16c.a` -- along with the headers in `include_ca65\` and
`include_llvm\`. A compiler for *your* program is all you need:

```
cl65 -t cx16 -O -I include_ca65 -o PROG.PRG prog.c dist_ca65\x16c.lib
```

To rebuild the library itself, or run its test suite, read on.

Three third-party things are expected but **not** committed:

| Path | What | Where from |
|---|---|---|
| `ca65\` (or any cc65 install) | one compiler | <https://cc65.github.io/> (a Windows snapshot zip) |
| `llvm-mos\` | the other | <https://github.com/llvm-mos/llvm-mos-sdk/releases> (the combined `llvm-mos-windows.7z`, not the compiler-only build) |
| `emulator\x16emu.exe` + `rom.bin` | X16 emulator r49 | <https://github.com/X16Community/x16-emulator>, ROM from <https://github.com/X16Community/x16-rom> |

You need only the toolchain you intend to use.

`build_ca65.ps1` finds cc65 through the repo-local `.\ca65\bin` first,
then `%CC65_HOME%\bin`, then `C:\Emulator\cc65\bin`, then `C:\cc65\bin`,
then `PATH`. `build_llvm.ps1` finds llvm-mos through `%LLVM_MOS_HOME%\bin`,
then `.\llvm-mos\bin`, then `C:\llvm-mos\bin`.

### Recompiling the library

Point the script at a cc65 install using any of the paths above. The
repo-local option is a `ca65\` folder (gitignored) shaped like this --
`bin` alone is not enough, because `cl65` locates the runtime pieces
relative to its own exe:

```
ca65\bin\        ca65.exe, cc65.exe, cl65.exe, ld65.exe, ar65.exe
ca65\asminc\     assembler includes
ca65\cfg\        linker configs (cx16.cfg)
ca65\include\    the C standard headers
ca65\lib\        cx16.lib
ca65\target\     cx16 data files (conio drivers)
```

(`html\`, `samples\`, and the other targets' files under `lib\` and
`target\` can be left out.) Then a full recompile is just:

```powershell
.\build_ca65.ps1                 # rebuild x16c.lib + examples\hello.c
.\build_ca65.ps1 -Test           # rebuild and run the regression suite
```

The script rebuilds only what changed (it compares timestamps against
the objects in `build_ca65\obj`); delete the `build_ca65` folder to
force everything from scratch. Intermediates stay in the gitignored
`build_ca65\`; the finished archive lands in the committed
`dist_ca65\`, so expect git to see `dist_ca65\x16c.lib` as modified
after a rebuild. `build_llvm.ps1` does the same with `build_llvm\` and
`dist_llvm\libx16c.a`.

Use the **r49** emulator and ROM: the constants in `src_ca65/core/` and
`src_llvm/core/` are transcribed from the r49 ROM sources, and both test
suites assert against r49 behaviour.

## Quick start

```powershell
.\build_ca65.ps1                         # build the lib + examples\hello.c
.\build_ca65.ps1 -Run                    # ...and run it windowed
.\build_ca65.ps1 -Source examples\bounce.c -Run
.\build_ca65.ps1 -Test                   # the headless regression suite
```

Swap `build_ca65.ps1` for `build_llvm.ps1` to do the same with llvm-mos;
the examples build unchanged under both.

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
cl65 -t cx16 -O -I include_ca65 -o PROG.PRG prog.c dist_ca65\x16c.lib
```

...or, with llvm-mos:

```
mos-cx16-clang -Os -mreserve-zp=16 -I include_llvm \
    -o PROG.PRG prog.c dist_llvm\libx16c.a
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

**A C interrupt callback would corrupt the zero page** three times over.
cc65's compiled code uses `sp`, `sreg`, `ptr1-4`, `tmp1-4` and `regbank`
(26 bytes), and the code it interrupted owns all of them mid-expression.
This library's own `X16_P`/`X16_T` scratch adds 16. And the KERNAL's
virtual registers `r0`-`r15` at `$02-$21` are another 32, clobbered by any
KERNAL call the handler makes. cc65's interruptor mechanism saves none of
the three. So this library saves all 74 bytes, only when a callback is
installed. That is what lets a raster handler be written in C, and call
any `x16_*` routine, rather than being usually-correct.

**`OPEN` succeeds for a file that does not exist.** CBM DOS reports
`62,FILE NOT FOUND` on the command channel, and the KERNAL only surfaces
it in `READST` once a read has been attempted -- so `OPEN` and `CHKIN`
both return carry clear, and the first `CHRIN` hands back junk. Any reader
that trusts carry alone will misdiagnose a missing file as a corrupt one.
`x16_bmx_load()` checks `READST` after the header and answers
`X16_BMX_ERR_IO`. Use `x16_dos_status()` for the reason.

**`x16_psg_set_freq()` leaves VERA's DECR bit set on port 0.** It writes
the frequency high byte first, walking the address backwards, so that the
pitch never passes through a garbage intermediate value that clicks. It
does not clear the decrement flag afterwards. Nothing in this library
cares -- every routine that uses port 0 sets `ADDR_H` whole -- and neither
does cc65's `vpoke()`. But hand-written `VERA_DATA0` writes after a
`set_freq` will walk the wrong way.

**`x16_f_to_str()` copies.** The ROM writes its answer into `$0100`, the
bottom of the stack page. The raw ROM call's buffer would not survive the
next deep call; the wrapper copies it into yours before returning.

## Tests

```powershell
.\build_ca65.ps1 -Test           # cc65, headless: 158 in a few seconds
.\build_ca65.ps1 -Test -Windowed # ...with video: 165, one skip
.\build_llvm.ps1 -Test           # llvm-mos: 46, all ABI-focused
```

**The suite is two programs.** All 28 library modules plus 160-odd test
functions no longer fit in the X16's 38.6 KB of program RAM — the code
alone reaches 33 KB, leaving nothing for BSS. So `test_ca65/runner.c`
compiles twice: `test_ca65/runner2.c` is three lines that set `SUITE` to 2 and
`#include` it, and the groups behind `#if SUITE == 2` move to the second
PRG, which then links only the modules its half reaches. `build_ca65.ps1` runs
both and sums the results. Each half is self-contained, so
`-Source test\runner2.c` runs it alone.

Every test drives the library one way and verifies through an
**independent path** -- write through a library call on port 0, read back
with cc65's `vpeek()` -- so a bug in the address plumbing cannot hide
behind itself. Any `FAIL`, a pass count that disagrees with the total, a
missing `DONE` line, or a timeout fails the build with a real exit code.

**Run it both ways.** Headless `-testbench` has no video, so VERA raises
neither VSYNC nor a raster interrupt and the CPU takes no IRQ at all. Eight
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
interrupt disarmed itself; `BMX_MISSING` asks for a file that is not
there, because `OPEN` succeeds anyway and the naive reader calls it a
format error.

**A test that cannot fail is not a test.** Two of these were written
weakly and only found out under mutation. `DOS_DELETE_MISSING` asserted
merely "some error", which a mistranslated command channel satisfies with
`30,SYNTAX ERROR`; it now asserts `62`. And `PSG_ENV_RELEASE` ramped 40
down by steps of 10, landing on zero exactly and never reaching the
underflow clamp it existed to check; the step is now 12, and the test also
checks the pan bits, which an unclamped volume overwrites.

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
src_ca65/core/   constants, macros, the zero-page block
src/*/           one .s per module -> one .o -> one member of x16c.lib
examples/        hello.c, bounce.c, numbers.c
test_ca65/       runner.c, runner2.c, testlib.h, fsroot/
build_ca65/      objects, PRGs, transcripts             (gitignored)
dist_ca65/       x16c.lib, the finished archive         (committed)
src_llvm/ include_llvm/ test_llvm/ build_llvm/ dist_llvm/  the llvm-mos half
tools/           ca65_to_llvm.py
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
window, so `src_ca65/core/x16zp.s` declares the block and the linker places it.

Four more differences bit during the port and are not visible in the table.

**`ca65 -t cx16` translates character literals to PETSCII.** ACME did not.
`lda #'S'` assembles to `$D3`, not `$53` — so the DOS command channel,
fed a scratch command it cannot parse, answers `30,SYNTAX ERROR` and the
delete silently does nothing. Every byte in `src/storage/dos.s` and
`src/storage/bmx.s` that goes to a drive or into a file magic is therefore
written as an explicit constant (`CH_S = $53`) rather than a literal.
Nothing warns you: the code assembles, links and runs.

**ca65 has no assembly-time floating point.** ACME's
`!for i, 0, 255 { !byte int(sin(i * ...) * 127) }` has no translation at
all, so `src/util/math.s` carries a precomputed 256-byte sine table and a
33-byte arctangent table as literal data.

**A plain label ends the enclosing cheap-local (`@`) scope**, where ACME's
zone-locals did not. Self-modifying code that needs a named operand — the
`lda $xxxx` in `pcmstream.s` and `gfx_text` — must therefore use plain
labels throughout that routine, not a mix.

**`.byte -1` is a range error**; write `$FF`. And a nonzero initialiser
belongs in `.segment "DATA"`, because crt0's `zerobss` wipes `BSS`.

## Porting notes: ca65 to llvm-mos

`tools/ca65_to_llvm.py` does the mechanical half — `.segment "CODE"` to
`.section .text,"ax",@progbits`, `.export` to `.globl`, `.import` deleted
(ELF resolves externs by name), `.res` to `.zero`, `.fatal` to `.error`,
`^(a)` to `((a) >> 16)`, macro parameters to `\arg`, `_x16_foo` to
`x16_foo`, and ca65's cheap locals (`@loop`, scoped to the preceding real
label and reused across routines) to `.L<parent>_<name>`.

It deliberately does **not** translate the C entry points. It leaves
cc65's `popa`, `popax`, `ptr1` and `sreg` exactly where they are, so any
shim that has not been rewritten by hand fails at *link* time with an
undefined symbol, and it prints every such line when it runs. A
plausible-looking automatic translation of a shim would be silently
wrong, which is the one failure this project cannot afford.

Four more things that bite:

**Argument position decides everything.** Integers and pointers draw from
the same `__rc` space, left to right. `x16_mem_fill(dst, count, value)`
puts the pointer in `__rc2/__rc3` and so pushes `value` out to `__rc4`;
`x16_gfx_text(x, y, color, s)` fills `__rc2` and `__rc3` with `y` and
`color` first, so the *string* lands in `__rc4/__rc5`. And
`x16_fs_load(name, len, device, sa, dest, end)` has `sa` take `__rc4`,
breaking that pair, so `dest` skips to `__rc6/__rc7` and `end` to
`__rc8/__rc9`. Do not reason about this — capture the registers.

**A pointer return comes back in `__rc2/__rc3`**, not `A`/`X`. Three
routines return pointers (`x16_mem_decompress`, `x16_zx0_decompress`,
`x16_dos_msg`), and a shim that leaves the address in `A`/`X` returns
whatever `__rc2` happened to hold — usually a plausible address in the
same page.

**`.byte "AB"` silently assembles to a single `00`** under LLVM, where
ca65 emits `41 42`. Use `.ascii`. Nothing in this library relied on it,
but it is the same genus as ca65's PETSCII charmap.

**cc65's `.destructor` becomes `.fini_array`.** `irq.s` needs to unhook
`CINV` before the program returns to BASIC; llvm-mos has no directive for
it, so the module puts a `.word irq_remove` in `.fini_array` and the
runtime's `__do_fini_array` walks it on the way out.

The interrupt zero-page save gets *simpler* on llvm-mos, not harder. cc65
needed three blocks (its 26-byte runtime, our 16, the KERNAL's 32). Here
`cx16/lib/imag-regs.ld` aliases `__rc2`–`__rc29` straight onto the
KERNAL's `r0`–`r15`, so `$02`–`$25` is one contiguous run covering all
thirty-two imaginary registers and all sixteen KERNAL registers at once:
36 bytes, plus our 16.

## A bug in llvm-mos-sdk v23.0.1

**The cx16 target's `vpoke()` is broken.** `vpeek(addr)` correctly reads
the address from `A`, `X` and `__rc2`. `vpoke(data, addr)` reads it from
`__rc2`, `__rc3` and `__rc4` — never touching `X`, where the compiler puts
`addr`'s first byte. Every write therefore lands at `addr >> 8`. Verified
on the machine: `vpoke(0xAB, 0x08000)` stores at `$00080`, and
`vpoke(0xCD, 0x01234)` at `$00012`.

`test_llvm/testlib.h` defines its own `t_vpoke()` and keeps the SDK's
`vpeek()` as the independent read-back path. If you use `vpoke()` in your
own cx16 program under this SDK, it is not doing what you think.
