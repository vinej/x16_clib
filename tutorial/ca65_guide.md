# C on the Commander X16 with cc65 — language, library, and CPU guide

This guide covers the **cc65** half of x16clib: what dialect of C the
compiler actually accepts, what standard library `-t cx16` gives you,
and the traps a 65C02 target sets for C code. The other toolchains are
covered by [llvm_guide.md](llvm_guide.md),
[kickc_guide.md](kickc_guide.md) and [oscar64_guide.md](oscar64_guide.md);
the library API itself by
[userguide.md](userguide.md).

Everything here was verified against **cc65 V2.19 (Git db178e5)**, the
snapshot bundled in `ca65\`.

```
cl65 -t cx16 -O -I include_ca65 -o PROG.PRG prog.c dist_ca65\x16c.lib
```

---

## 1. The C language cc65 compiles

cc65 is a **C89/C90 compiler with a few C99 conveniences bolted on**.
It is not C99, and nothing later. `--standard` selects `c89`, `c99`
(incomplete) or `cc65` (the default: C89 plus the extensions below and
the supported C99 pieces).

### Type sizes

| Type | Size | Notes |
|---|---|---|
| `char` | 8 bits | **unsigned by default** (`--signed-chars` flips it) |
| `short`, `int` | 16 bits | the native "cheap" word |
| `long` | 32 bits | via runtime calls; noticeably slower |
| `long long` | — | **not supported** |
| `float`, `double` | — | **not supported** — see below |
| pointers | 16 bits | plain data; no far pointers |
| `size_t`, `ptrdiff_t` | 16 bits | |

### No floating point, at all

The compiler stops dead:

```
fc.c:1: Fatal: Floating point type is currently unsupported
```

There is no `<math.h>` in cc65's headers either. Your options, in order
of preference:

1. **`x16/fixed.h`** — 8.8 fixed point with a 16×16 multiply. Right for
   almost all game math.
2. **`x16/math.h`** — precomputed sine/cosine tables, `atan2`, `lerp`,
   a PRNG.
3. **`x16/float.h`** — bindings to the ROM's BASIC floating point
   package (40-bit MFLPT). Slow, but real transcendentals: `SIN`,
   `COS`, `SQR`, `LOG`, `EXP`, `ATN`.

### C99 pieces that do work

`//` comments, `<stdint.h>`, `<stdbool.h>`, `<inttypes.h>`, `<iso646.h>`.

### C99 pieces that do not

Variable-length arrays, designated initializers, compound literals,
`long long`, floating point, declarations mixed freely into statements
(accepted only in `--standard c99`/`cc65` modes, and not everywhere).
When in doubt, write C89.

### cc65 extensions you will actually use

- `__fastcall__` — the default calling convention for named functions;
  x16clib declares every entry point with it explicitly.
- `__asm__("...")` — inline assembly, with `%b`/`%w` operand formats.
- `register` — with `-r` (or `--register-vars`), puts a variable in
  zero page for the life of the function. Worth it for hot pointers.
- `-Cl` / `--static-locals` — allocates locals statically instead of on
  the parameter stack. Faster code, **but every function becomes
  non-reentrant**: no recursion, and no calling the function from both
  main line and interrupt.
- `#pragma charmap(index, code)` — remap the literal translation table
  (see §4).
- `-Os` / `--inline-stdfuncs` — inlines `memcpy`, `strlen` and friends.

## 2. Calling convention and the two stacks

cc65's `__fastcall__` passes the **rightmost** argument in registers —
`A` for a char, `A/X` for an int or pointer, `sreg:A/X` for a long —
and pushes every other argument, left to right, onto a **software
parameter stack**. Variadic functions fall back to `__cdecl__`
(everything on the stack).

Returns come back the same way: char in `A` (with `X` zeroed for int
promotion), int/pointer in `A/X`, long in `sreg:A/X`.

There are two stacks and neither is big:

- The **hardware stack** ($0100–$01FF, 256 bytes) holds return
  addresses and interrupt state only. Deep call chains plus an IRQ can
  genuinely overflow it.
- The **software C stack** holds parameters and locals. `cx16.cfg`
  gives it `__STACKSIZE__ = $0800` (2 KB) at the top of RAM, carved
  out of your program space. Override at link time:
  `cl65 ... -Wl -D,__STACKSIZE__=$0400`.

Recursion is legal but pays software-stack rent on every call. Prefer
iteration; if you must recurse, keep frames tiny.

## 3. Memory map under `-t cx16`

| Region | Range | What |
|---|---|---|
| Zero page (C runtime) | $0022–$007F | cc65's `sp`, `ptr1..4`, `tmp1..4`, `sreg`, `regbank`, plus `EXTZP` (x16clib's 16-byte scratch lands here) |
| Hardware stack | $0100–$01FF | JSR/RTS, IRQ |
| Program | $0801–$9EFF | code, rodata, data, BSS, then the heap |
| C stack | top 2 KB below $9F00 | grows down |
| I/O | $9F00–$9FFF | VERA, VIAs, YM2151 |
| Banked RAM | $A000–$BFFF | 256 banks × 8 KB — see `x16/bank.h` |
| KERNAL/BASIC ROM | $C000–$FFFF | 16 banks |

That leaves roughly **38 KB minus stack** for a flat C program. The
heap (`malloc`) automatically uses whatever sits between BSS and the C
stack. Everything beyond the flat 64 KB — the 512 KB–2 MB of banked
RAM, the 128 KB of VRAM — is invisible to C pointers and reached
through `x16/bank.h`, `x16/vera.h` and `x16/load.h`.

## 4. The PETSCII trap

**`-t cx16` translates string and character literals from ASCII to
PETSCII, silently, in both C and assembly.** `'S'` compiles to `$D3`,
not `$53`. `"HELLO"` is stored as PETSCII bytes. This is what makes
`printf` come out readable on the default screen — and what corrupts
any byte that was *data* rather than text: file magics, DOS command
channel strings, protocol bytes. The DOS answers a mistranslated
scratch command with `30,SYNTAX ERROR` and deletes nothing; nothing
warns you at build time.

Rules of thumb:

- Literals that go to the **screen or the DOS command channel as
  text**: leave them alone, the translation is what you want.
- Bytes that must be **exact ASCII** (file magic, binary formats):
  write the constant — the library does `#define CH_S 0x53` — or remap
  with `#pragma charmap`.
- cc65's `<ctype.h>` tables are PETSCII-aware on CBM targets, so
  `isalpha()` etc. agree with the translated literals.
- If you switch the screen to ISO mode (`x16_screen_charset
  (X16_CHARSET_ISO)`), the KERNAL now expects ISO-8859-15 — and your
  PETSCII-translated literals print wrong. Pick one charset per
  program.

## 5. The standard library `-t cx16` ships

cc65 comes with a real, if small, libc plus CBM- and X16-specific
headers. All of it lives in `ca65\include` and `cx16.lib`.

### Standard C

| Header | State |
|---|---|
| `<stdio.h>` | `printf`/`sprintf`/`scanf` families (no `%f`, no `long long`); `FILE*` I/O via KERNAL — `fopen("data.bin","rb")` opens on the default drive |
| `<stdlib.h>` | `malloc`/`free`/`realloc`, `rand`, `atoi`/`atol`, `qsort`, `bsearch`, `abs`, `labs`, `exit`/`atexit` |
| `<string.h>` | complete; `memcpy`/`memset` are good hand assembly |
| `<ctype.h>` | complete, PETSCII-aware |
| `<time.h>` | `time()`/`clock()` off the X16's RTC and jiffy clock |
| `<setjmp.h>`, `<errno.h>`, `<assert.h>`, `<limits.h>`, `<stdarg.h>` | present |
| `<math.h>` | **absent** — no floating point |
| `<locale.h>`, `<signal.h>` | stubs, C89 minimum |

### Platform headers

| Header | What |
|---|---|
| `<cx16.h>` | `vpeek`/`vpoke`, `waitvsync`, `videomode`, the `VERA` register struct, bank macros. **Trap:** its `VERA_INC_1` is pre-shifted (0x10); x16clib's `X16_INC_1` is the raw index (1). Never mix the two constant families. |
| `<conio.h>` | fast direct-to-screen text: `cputs`, `gotoxy`, `cprintf` |
| `<cbm.h>` | KERNAL wrappers: `cbm_load`, `cbm_save`, `cbm_open`, ... |
| `<joystick.h>`, `<mouse.h>` | loadable/statically-linked drivers |
| `<tgi.h>` | portable graphics driver (320×240×256) |
| `<em.h>` | extended memory driver over banked RAM |
| `<peekpoke.h>` | `PEEK`/`POKE` macros |

Where cc65 does a job well, x16clib deliberately does not duplicate it:
use conio for text, `printf` for formatting, cc65's `vpoke` for single
VRAM bytes. The library adds what cc65 lacks — sound, sprites, layers,
VERA FX, banked-RAM tooling, LZSA2/ZX0 depacking (see the README).

## 6. Writing C for a 65C02 — what the CPU punishes

The `-t cx16` target sets `.setcpu "65C02"` (verified in the generated
assembly), matching the X16's W65C02 at 8 MHz. The generated code may
use `stz`, `bra` and the rest — it will not run on an NMOS 6502
machine, which on the X16 costs you nothing.

The 65C02 is an 8-bit CPU with three registers and no multiply. C hides
none of this well:

- **`unsigned char` is the native type.** An `int` loop counter costs
  double on every increment and compare; a `long` costs a runtime call.
  Use `unsigned char` for anything that fits in 0–255, and prefer
  unsigned over signed everywhere — signed comparisons and divisions
  emit extra code.
- **Multiplication and division are subroutines**, hundreds of cycles
  for 16-bit operands. Replace with shifts where you can, tables where
  you can't, or hand the work to hardware: `x16_fx_mult()` uses the
  VERA FX multiplier.
- **Variable shift counts are loops.** `x << 3` is fine; `x << n` is a
  loop of `n` iterations.
- **Every parameter and local on the C stack** is accessed through a
  zero-page pointer with an index — several times slower than a global
  or a `register` variable. For hot functions: few parameters, `-Cl`
  or file-scope `static` state, `register` on the busiest pointer.
- **Structs are passed and returned by pointer.** Passing a `struct`
  by value, or returning one, works but copies through memory; pass
  pointers.
- **16-bit pointers only.** There is no far pointer. Banked RAM is a
  window at $A000; `x16/bank.h` handles bank-crossing copies, and
  `x16_bank_alloc()` manages whole banks.
- **Interrupts + zero page don't mix freely.** x16clib keeps a 16-byte
  argument scratch block in zero page. It is not reentrant: an ISR must
  not call any `x16_*` routine that takes more than three arguments or
  any 16-bit argument. `x16/irq.h` documents the ISR-safe subset.

### 6502 vs 65C02 vs 65816

- **NMOS 6502**: not relevant to the X16, but if you also target C64,
  build that code with `--cpu 6502` — the cx16 default emits 65C02
  opcodes.
- **65C02**: what the X16 has (a WDC W65C02S) and what `-t cx16`
  generates. All good.
- **65816**: cc65 **never generates** native 65816 code. `ca65` can
  assemble it (`.setcpu "65816"`, `--cpu 65816`) if you write it
  yourself, but the X16 shipped with a 65C02 — there is no 16-bit
  accumulator, no 24-bit addressing, no extra bank of stack to plan
  for. Write for the 65C02 and stop worrying.

## 7. x16clib specifics under cc65

- Every library entry point is `__fastcall__` — cc65's default, so your
  calls just work. The distinction only matters if you take a
  function's address or write assembly against it.
- Link `dist_ca65\x16c.lib`; ld65 pulls only the modules you call.
  `#include <x16/x16.h>` costs nothing at run time.
- The regression suite (`.\build_ca65.ps1 -Test`) runs 158 tests on the
  r49 emulator; the library's constants are transcribed from the r49
  ROM.

## 8. Gotcha checklist

1. `char` is **unsigned** by default.
2. No `float`, no `double`, no `<math.h>`, no `long long`.
3. `'S'` is `$D3` after PETSCII translation — write `0x53` when you
   mean ASCII.
4. cc65's `VERA_INC_1` (0x10) ≠ x16clib's `X16_INC_1` (1).
5. The C stack is 2 KB and the hardware stack 256 bytes — recursion and
   big locals are luxuries.
6. `-Cl` makes everything non-reentrant; so does the library's
   zero-page scratch block, from an ISR's point of view.
7. cc65 and llvm-mos object code **cannot be mixed** — different
   calling conventions, different object formats.
