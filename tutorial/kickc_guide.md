# C on the Commander X16 with KickC — language, library, and CPU guide

This guide covers the **KickC** third of x16clib: what dialect of C the
compiler actually accepts, how a linker-less toolchain changes the shape
of a project, and the zero-page rules that keep the KERNAL from
scribbling over your variables. The other toolchains are covered by
[ca65_guide.md](ca65_guide.md), [llvm_guide.md](llvm_guide.md) and
[oscar64_guide.md](oscar64_guide.md); the
library API itself by [userguide.md](userguide.md).

Everything here was verified against **KickC 0.8.6 BETA** (the last
release, February 2022), the zip unpacked to `kickc\`, running on Java 8.
Several items below are compiler crashes found while porting this
library; each has its workaround stated.

```
kickc -p cx16 -a -I include_kickc -L src_kickc yourprog.c
```

The `-a` matters: without it the "output" file is KickAssembler source
text, whatever its extension says. With it, KickC drives its bundled
KickAssembler and produces a real PRG.

---

## 1. The C language KickC compiles

KickC is **its own compiler with its own C-like language** — closer to
C than anything else on this list of caveats suggests, but do not paste
C89 at it and expect silence. What works: functions, structs, typedefs,
pointers, function pointers, arrays, `for`/`while`/`switch`, a real
preprocessor (`#define`, `#ifdef`, `#include`), and multiplication
compiled to runtime routines.

### Type sizes

| Type | Size | Notes |
|---|---|---|
| `char` | 8 bits | **unsigned by default**, like both other toolchains |
| `int`, `short` | 16 bits | |
| `long` | 32 bits | arithmetic yes — but see the shift caveat below |
| `float`, `double` | — | **do not exist.** Use `x16/fixed.h` or `x16/float.h` |
| pointers | 16 bits | |
| `bool` | exists | but does not convert — see below |

### The strict-typing surprises

KickC's type checker is stricter than C's, and its code generator is a
library of hand-written "fragments" — one per operation-and-addressing
combination. When no fragment exists, compilation **fails** (or worse,
see §6). The patterns that bite, all verified:

- **`bool` does not convert to `char`.** `t_check(x == y, ...)` is a
  type error; write `(x == y) ? 1 : 0`. Same for assignments:
  `ok = a && b;` needs the ternary.
- **Casts are mandatory even where C converts silently** — `int w = c;`
  with a `char c` is an error; write `w = (int)c;`.
- **No runtime division.** `v / 100` on a variable is a hard error
  ("Runtime division not supported"). Repeated subtraction, shifts, or
  tables. (`x16/fixed.h` and the VERA FX multiplier cover the
  multiply-shaped holes; nothing covers division — design it out.)
- **No runtime 32-bit shifts.** `pos >> 8` on a `long` variable has no
  fragment. Constants fold fine; variables do not. Keep hot state in
  16-bit pieces (this is why `examples/bounce.c` holds its position as
  a pixel word plus a fraction byte).
- **Composite expressions run out of fragments** where the simple steps
  do not: `(int)(unsigned char)(0xC0+i)`, `*pos + x` (arithmetic
  directly on a dereference), `((int)a - b) << 1` on mixed widths. The
  fix is always the same: load into a local of the right width, one
  operation per statement.
- **Array-typed parameters crash the compiler** (a ClassCastException).
  `void f(x16_float m)` where `x16_float` is `char[5]` — spell it
  `void f(unsigned char *m)`; call sites are identical.
- **String array sizes count the terminator**: `char s[3] = "abc"` is
  an "Array length mismatch". Use `char s[] = "abc"` (four bytes).
- `static` locals with initializers, `const` in casts
  (`(const char*)p`), and `volatile` in casts are parse errors. Cast to
  the unqualified type.

### One program, no linker

KickC compiles the **whole program from one root file** and strips every
function nothing calls. There are no object files and no archives —
which is why x16clib's KickC build is a *source* distribution and there
is no `dist_kickc\`. The `-L` path is the library mechanism:
`#include <x16/foo.h>` makes KickC compile `<libdir>/x16/foo.c` when one
of its functions is used. The pairing is strictly by name — a second
implementation file is invisible, which is why `pcmstream` lives inside
`src_kickc/x16/pcm.c` and `bankalloc` inside `bank.c`. Dead-code
elimination makes the merge free: a program using only `x16_pcm_put()`
carries none of the streaming or IRQ machinery.

## 2. Calling convention: variables, not registers

KickC's default convention passes each parameter in **its own static
memory cell**, named `function::parameter`, assigned by the caller
before the `jsr`. There is no argument stack and no register protocol —
and therefore no reentrancy: recursion needs an explicit
`__stackcall` pragma and is best avoided entirely.

For x16clib this is the friendliest convention of the three: the
library's inline assembly reads parameters **by name** (`lda color`),
so the cc65 build's pop-order shims have no equivalent here — a whole
class of transposition bugs is structurally impossible. Returns:
`char` in `A` alone, `int` in `A`/`X`, pointers in `A`/`X`.

Object code from KickC, cc65 and llvm-mos can never be mixed; with
KickC there is no object code to mix anyway.

## 3. Zero page: the part that will corrupt your program

This is the section to read twice.

On the X16, the user zero page is **$22–$7F and nothing else**: $02–$21
holds the KERNAL's API registers r0–r15, and $80–$FF belongs to the
KERNAL, BASIC and DOS. cc65's `cx16.cfg` and llvm-mos's target config
both respect that. **KickC's cx16 target reserves only $FC–$FF.**

Left alone, KickC will happily allocate a variable at $83 — and the
first `CHROUT` scribbles over it. This is not theoretical: porting this
library, the test suite caught the KERNAL's screen editor corrupting a
KickC variable at exactly $83, passing or failing with every unrelated
code change. Worse, when zero page fills up KickC does **not** error —
it silently allocates into reserved ranges, and silently spills pointer
variables to main memory, where `(ptr),y` assembles into garbage.

x16clib's defence is `include_kickc/x16/zpsafe.h`, included by every
library header:

```c
#pragma zp_reserve(0x02..0x21)
#pragma zp_reserve(0x76..0xfb)
```

Three things worth knowing about it:

- **It has no include guard, deliberately.** KickC's `#define` state is
  global across the whole-program compile, but pragma state is
  per-file. A guarded header would apply the pragmas to the first file
  only and leave every library file unprotected.
- `__zp` and `__mem` are **silently ignored on parameters**, so the
  library pins every pointer it indirects through at a **fixed address**
  with `__address()`: $76/$77 is the PCM streamer's interrupt-time
  pointer, $78–$7F four slots the modules share (never live across a
  call into another module; the IRQ dispatcher saves $02–$7F around C
  callbacks). That is what the second reserve range protects.
- Your program gets the remaining **$22–$75** — 84 bytes, four short of
  what cc65 offers. If a big program runs out, KickC spills silently;
  check the generated `.asm` for `.label`s above $75 if something
  behaves impossibly.

## 4. The encoding trap

KickC's default string encoding on cx16 is PETSCII-flavoured, so the
same trap as cc65 — with the same one-line fix when you want bytes to
mean themselves:

```c
#pragma encoding(ascii)
```

The library's test harness uses it so `"PASS"` leaves the machine as
`$50 $41 $53 $53`. For data that must be exact regardless of pragmas —
DOS command channels, file magic — write explicit hex, which is what
`src_kickc/x16/dos.c` and `bmx.c` do (`0x53` for `'S'`, `$42 $4D $58`
for `"BMX"`). One more wrinkle: `#pragma encoding` inside inline asm
does not exist, and `#define` does not reach inside `asm {}` blocks at
all (§6), so a character literal in assembly is always a risk. Spell
the byte.

## 5. The standard library KickC ships

KickC brings its own headers in `kickc\include` (compiled from
`kickc\lib` on demand, the same `-L` mechanism):

| Header | State |
|---|---|
| `<conio.h>` | cc65-compatible console I/O with a cx16 backend |
| `<printf.h>` | `printf` — but **no `<stdio.h>`**, and no file I/O |
| `<string.h>`, `<stdlib.h>`, `<ctype.h>` | partial but useful |
| `<cx16.h>`, `<cx16-vera.h>`, `<cx16-veralib.h>` | platform registers and a VERA helper layer |
| `<division.h>`, `<multiply.h>` | the runtime arithmetic as callable functions |
| `<math.h>` | not floating point — there is no float type |

Two cautions. The cx16 platform headers predate ROM r49 — where they
disagree with `include_kickc/x16/*.h`, trust the latter (this library
is transcribed from the r49 ROM and tested against the r49 emulator).
And KickC's `VERA_INC_1`-style constants, like cc65's, are pre-shifted;
x16clib's `X16_INC_*` are raw indices. Never mix them.

`examples/numbers.c` stays a cc65/llvm-only example for exactly the
stdio reason.

## 6. Inline assembly: the grammar, and the optimizer

`asm {}` is how x16clib carries its hand-written 6502 into KickC, and
its parser has rules of its own — all found the hard way:

- Operands resolve against **C symbol names only**. `#define`d
  constants do not substitute inside a block (the library inlines them
  as `$9f25 /*VERA_CTRL*/`), and a `jsr` can only target a label **in
  the same block** — never another function. Cross-function assembly
  becomes a C call, or a duplicated local helper.
- `.byte` is the **only** data directive (`.word` is not). That is
  enough for `jsrfar`: `jsr $ff6e` then `.byte <entry, >entry, bank`.
- Reserved words: a parameter named **`row`** breaks the parse, one
  named **`inc`** is eaten as a mnemonic, and **`a`** or **`b`** can
  crash the compiler outright. Accumulator shifts are bare: `asl`, not
  `asl a`.
- **Two labels on consecutive lines crash the optimizer** (an NPE in
  redundant-label elimination). Keep one.
- **A read whose value goes unused is deleted** — or crashes the
  optimizer. On this machine reads have side effects: VERA's FX
  accumulator reset is triggered by *reading* a register, and four
  DATA1 reads latch the FX cache. Use `bit $9f29` instead of
  `lda $9f29`; `bit` performs the same bus cycle and no pass touches
  it. (The library's `x16_fx_copy` was silently broken by exactly this
  before the switch.)
- An `rts` mid-block returns without KickC's epilogue — store results
  to a named variable and `jmp` to an end label instead. `rts` inside a
  `jsr`'d local helper is fine.
- Comments: both `//` and `/* */` work inside blocks. `(zp)` no-index
  indirection, `bra`/`stz`/`trb`/`tsb`, `%binary` and `$hex` literals
  all parse; the cx16 target sets `.cpu _65c02`.

## 7. Writing C for a 65C02 — what the CPU punishes

The machine advice from the other two guides applies unchanged —
`unsigned char` is the native type, prefer unsigned comparisons,
shifts by constants over multiplies, 16-bit pointers only — with two
KickC-specific notes:

- What the other compilers make *slow*, KickC often makes *impossible*
  (division, long shifts). Treat §1's list as a design constraint, not
  an optimization guide.
- KickC's whole-program optimizer is genuinely good at byte-sized code:
  `examples/bounce.c` compiles to 2.6 KB against cc65's 4.3 KB. Give it
  `char`s and it rewards you.

## 8. x16clib specifics under KickC

- Build: `.\build_kickc.ps1` (or the `kickc` line at the top). No
  archive to link — `src_kickc\` is the distribution.
- The regression suite (`.\build_kickc.ps1 -Test`) runs **119 tests in
  three PRGs** — split across programs for zero-page headroom, not RAM.
- Include `<x16/x16.h>` (or individual headers); every one pulls in
  `zpsafe.h` and its pragmas automatically.
- **Call `x16_irq_remove()` before returning to BASIC.** cc65 unhooked
  the interrupt chain in an exit destructor; KickC has no destructors,
  and a stale CINV vector points into memory BASIC is about to reuse.
- IRQ callbacks may call anything: the dispatcher saves zero page
  $02–$7F around them (126 bytes, ~2% of a frame, only when a callback
  is installed).
- `hello.c` and `bounce.c` build unchanged from the shared `examples\`;
  `numbers.c` does not (stdio).

## 9. Gotcha checklist

1. **`-a` or you get assembler text**, not a PRG.
2. Every `bool`-ish expression that lands in a `char` needs
   **`? 1 : 0`**; every narrowing or widening needs an explicit cast.
3. **No division, no float, no runtime 32-bit shifts.** Design around
   them; `x16/fixed.h` and `x16_fx_mult()` are the load-bearing
   replacements.
4. One operation per statement when types mix — "Unknown fragment" is
   KickC telling you the expression is too composite.
5. Zero page: keep `zpsafe.h` included (any library header does it),
   and remember KickC **fails silently** when zp runs out.
6. `#define`s don't work inside `asm {}`; `jsr` only reaches labels in
   the same block; `.byte` is the only data directive.
7. Side-effect reads in asm must be `bit`, not `lda`, or the optimizer
   deletes them.
8. Don't name parameters `row`, `inc`, `a`, or `b` if assembly touches
   them.
9. `#pragma encoding(ascii)` when bytes must mean themselves; explicit
   hex when they *really* must.
10. Call `x16_irq_remove()` on the way out.
11. KickC object code mixes with nothing — and has no objects to mix.
