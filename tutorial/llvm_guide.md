# C on the Commander X16 with llvm-mos — language, library, and CPU guide

This guide covers the **llvm-mos** half of x16clib: what dialect of C
(and C++) the compiler accepts, what the SDK's libc actually contains
on the cx16 target, and the traps a 65C02 target sets for C code. The
other toolchains are covered by [ca65_guide.md](ca65_guide.md),
[kickc_guide.md](kickc_guide.md) and [oscar64_guide.md](oscar64_guide.md);
the library API itself by
[userguide.md](userguide.md).

Everything here was verified against **clang 23.0.0git (llvm-mos), SDK
v23.0.1**, the snapshot bundled in `llvm-mos\`.

```
mos-cx16-clang -Os -mreserve-zp=16 -I include_llvm -o PROG.PRG prog.c dist_llvm\libx16c.a
```

The `-mreserve-zp=16` is **required** when linking x16clib — without it
the link fails with `section '.zp.bss' will not fit in region 'zp'`.
See §4.

---

## 1. The C language llvm-mos compiles

llvm-mos is real clang with a 6502 backend. The language is therefore
**modern C, complete**: the cx16 driver defaults to **C17**
(`__STDC_VERSION__ == 201710L`), and `-std=c23` works. Designated
initializers, compound literals, `_Generic`, anonymous unions, GNU
extensions, `__attribute__` — all there. C++ is available through
`mos-cx16-clang++` (templates, `constexpr`, classes and virtual
dispatch work; **exceptions do not** — the SDK links `-fno-exceptions`
code and a header-only subset of the standard library: `<algorithm>`,
`<array>`, `<type_traits>`, `<new>`, `<utility>`, ...).

### Type sizes (verified with `-dM -E`)

| Type | Size | Notes |
|---|---|---|
| `char` | 8 bits | **unsigned by default** (`__CHAR_UNSIGNED__`) |
| `short`, `int` | 16 bits | the native "cheap" word |
| `long` | 32 bits | |
| `long long` | 64 bits | supported, expensive |
| `float` | 32 bits | **software float — fully supported** |
| `double` | 64 bits | supported; twice the pain of `float` |
| pointers | 16 bits | no far pointers |

Floating point deserves a caveat with both edges stated:

- Arithmetic works, `printf("%f", ...)` works — a minimal float-printf
  program links to a 3.7 KB PRG.
- **`<math.h>` is almost empty**: it declares exactly `fmin`, `fmax`,
  `fminf`, `fmaxf`. There is no `sqrt`, `sin`, `pow`, nothing
  transcendental. For those, use `x16/math.h` (sine/cosine tables,
  `atan2`, lerp), `x16/fixed.h` (8.8 fixed point — the right answer
  for game math), or `x16/float.h` (the ROM's BASIC FP package: `SIN`,
  `COS`, `SQR`, `LOG`, `EXP`, `ATN`).

## 2. Calling convention and the imaginary registers

The 6502 has three 8-bit registers, so llvm-mos manufactures more: 32
**imaginary registers** `__rc0`–`__rc31`, which are zero-page bytes. On
the cx16 target, `cx16/lib/imag-regs.ld` aliases `__rc2`–`__rc29`
straight onto the KERNAL's `r0`–`r15`, making $02–$25 one contiguous
36-byte run.

Arguments are assigned **strictly left to right** through the sequence
`A`, `X`, `__rc2`, `__rc3`, ... A byte takes the next single register;
a 16-bit integer takes the next free pair, which may be `A`/`X`; a
**pointer always takes an aligned `__rc` pair**, never `A`/`X` (it has
to sit in zero page for `(zp),y` indirection). Nothing jumps the queue.
The layouts this produces should be captured, not derived (all three
below are from the library's own shims):

- `x16_mem_fill(dst, count, value)` — `dst` is a pointer, so it goes to
  `__rc2/__rc3`; `count`, an int, takes the still-free `A/X`; `value`
  lands in `__rc4`.
- `x16_gfx_text(x, y, color, s)` — `x` (16-bit int, first) takes
  `A/X`, `y` takes `__rc2`, `color` takes `__rc3`, and the string takes
  the next aligned pair, `__rc4/__rc5`.
- `x16_fs_load(name, len, device, sa, dest, end)` — `sa` lands in
  `__rc4` and breaks that pair, so `dest` skips to `__rc6/__rc7` and
  `end` to `__rc8/__rc9`.
- Returns: `char` in `A` alone (no int promotion), `int` in `A/X`,
  **pointers in `__rc2/__rc3`** — not `A/X`.

None of this appears in the headers — there is no `__fastcall__`
keyword; the convention lives in the compiler. It also means **object
code from llvm-mos and cc65 can never be mixed**.

## 3. Whole-program LTO and the static stack

llvm-mos compiles to LLVM bitcode and does the real code generation at
link time, whole-program. Two consequences:

- **Non-recursive functions get no stack frame at all.** The linker
  proves the call graph is recursion-free and turns locals into
  statically allocated memory. This is why idiomatic C is fast here:
  locals cost the same as globals.
- **Recursion (and anything the LTO pass can't prove, like calls
  through some function pointers) falls back to a soft stack** —
  markedly slower. Avoid recursion; if you take function addresses,
  expect the callees to pessimize. Avoid VLAs and `alloca` for the
  same reason.

## 4. The zero-page economy

The cx16 target leaves **90 bytes** of user zero page ($26–$7F); the
KERNAL's `r0`–`r15` and the imaginary registers share $02–$25. The
driver config passes `-mlto-zp=90`, which lets clang's LTO pass promote
the hottest globals and locals into that window — automatically, and
greedily.

x16clib keeps a 16-byte argument scratch block in that same window, and
LTO knows nothing about it. **`-mreserve-zp=16` tells LTO to leave 16
bytes alone**; the build scripts pass it, and any program linking the
library must too. Forgetting it is not a subtle bug — the link fails:

```
ld.lld: error: section '.zp.bss' will not fit in region 'zp':
        overflowed by 16 bytes
```

If your own code also claims zero page explicitly
(`__attribute__((section(".zp.bss")))`), raise the number to cover
both.

## 5. The standard library the SDK ships on cx16

The SDK carries its own libc (`mos-platform/common` plus a
`mos-platform/cx16` overlay). Checked against the archives:

| Header | State |
|---|---|
| `<stdio.h>` | `printf`/`sprintf`/`snprintf`/`vfprintf` and `scanf`/`sscanf`, **including `%f`**. `fopen`/`fread`/`fwrite`/`fgets` are real, implemented over KERNAL SETNAM/OPEN — disk I/O through stdio works on the default drive. `stdin`/`stdout` are the KERNAL console. |
| `<stdlib.h>` | `malloc`/`free`, `qsort`, `atoi`, `strtol`, `abs`, `exit`/`atexit` |
| `<string.h>`, `<ctype.h>` | complete |
| `<math.h>` | **only `fmin`/`fmax`/`fminf`/`fmaxf`** — see §1 |
| `<setjmp.h>`, `<errno.h>`, `<assert.h>`, `<time.h>`, `<inttypes.h>` | present |
| `<fixed_point.h>` | SDK-native fixed-point types — an alternative to `x16/fixed.h` |
| `<peekpoke.h>`, `<6502.h>`, `<_6522.h>` | direct hardware access helpers |
| C++ subset | `<algorithm>`, `<array>`, `<new>`, `<type_traits>`, `<utility>`, `<initializer_list>`, `<iterator>` — no exceptions |

### Platform header `<cx16.h>` — and its broken `vpoke()`

The SDK's `<cx16.h>` provides `vpeek`, `vpoke`, `waitvsync` and the
VERA register struct. **`vpoke()` in SDK v23.0.1 is broken**: it reads
the address from `__rc2/__rc3/__rc4`, but the compiler puts the first
address byte in `X`. Every write lands at `addr >> 8` — verified on
hardware: `vpoke(0xAB, 0x08000)` stores at `$00080`. `vpeek()` is
fine.

**Linking this library fixes it.** `src_llvm/core/vpoke.s` defines a
correct `vpoke`, and because `libx16c.a` is named ahead of the platform
libraries the linker takes ours and never pulls the SDK's member in.
There is no flag and nothing to rename: your `vpoke()` calls just work.
Without the library, write single VRAM bytes through `x16_vera_addr0()`
plus a `VERA_DATA0` store, or your own macro.

### Character sets: the opposite trap from cc65

llvm-mos does **not** translate string literals to PETSCII. `"HELLO"`
stays ASCII, `'S'` stays `$53`. That is exactly right for binary data
and DOS commands, and exactly wrong for `CHROUT` on the default PETSCII
screen — uppercase ASCII prints as the wrong glyphs. Choose one:

- Switch the screen to ISO mode (`x16_screen_charset(X16_CHARSET_ISO)`)
  and ASCII-ish literals print correctly.
- In C++20, `<charset.h>` converts literals at compile time (it is
  unusable from C — it's built on `constexpr` templates).
- In C on a PETSCII screen, convert yourself or use screen codes.

## 6. Writing C for a 65C02 — what the CPU punishes

The cx16 driver passes `-mcpu=mosw65c02` (the WDC 65C02), matching the
X16's W65C02S at 8 MHz, so the backend uses `stz`, `bra`, `phx`, the
`(zp)` addressing mode and friends. llvm-mos generates markedly better
code than cc65 — often several times faster — but the machine underneath
is the same, and the same advice applies:

- **`unsigned char` is the native type.** 16-bit `int` costs double on
  every operation; `long` more; `long long` and `double` are for
  correctness, not for loops.
- **No hardware multiply or divide.** 16-bit multiply is a runtime
  routine; division worse. Shifts by constants are cheap, shifts by
  variables are loops. Tables and `x16_fx_mult()` (the VERA FX
  hardware multiplier) beat the runtime.
- **Prefer unsigned.** Signed division and signed comparisons emit
  extra fix-up code.
- **16-bit pointers only.** The 512 KB–2 MB of banked RAM is a window
  at $A000–$BFFF and the 128 KB of VRAM is not addressable at all;
  reach both through `x16/bank.h`, `x16/vera.h`, `x16/load.h`.
- **The hardware stack is 256 bytes** ($0100–$01FF) and holds return
  addresses and interrupt state. The static-stack transformation (§3)
  keeps C locals off it entirely — one of llvm-mos's biggest wins —
  but only while you avoid recursion.
- **Interrupts and zero page.** An ISR must preserve every imaginary
  register it (or anything it calls) touches; because $02–$25 is one
  contiguous run on cx16, a full save is 36 bytes plus x16clib's 16.
  The library's scratch block is not reentrant: an ISR must not call
  any `x16_*` routine taking more than three arguments or any 16-bit
  argument. `x16/irq.h` documents the ISR-safe subset and installs
  handlers correctly.

### 6502 vs 65C02 vs 65816

- **NMOS 6502**: `-mcpu=mosw65c02` output will not run on one; if you
  share code with a C64 build, that target's driver picks the right
  CPU.
- **65C02**: what the X16 has and what this target generates. All good.
- **65816**: llvm-mos does **not** generate native-mode 65816 code, and
  the X16 shipped with a 65C02 anyway — no 16-bit accumulator, no
  24-bit addressing. Write for the 65C02.

## 7. x16clib specifics under llvm-mos

- Compile with `-mreserve-zp=16` (§4). `build_llvm.ps1` does.
- No `__fastcall__` anywhere — the headers are plain C, the convention
  is the compiler's.
- Link `dist_llvm\libx16c.a`. The API is identical to the cc65 build's:
  the same 29 modules, the same hand-written 6502 underneath, different
  C entry shims.
- The regression suite (`.\build_llvm.ps1 -Test`) runs 46 tests on the
  r49 emulator; library constants are transcribed from the r49 ROM.

## 8. Gotcha checklist

1. **`-mreserve-zp=16` or the link fails** — the one flag to remember.
2. The SDK's `vpoke()` writes to `addr >> 8` — but this library
   replaces it, so it is correct as long as you link `libx16c.a`.
3. `char` is **unsigned** by default.
4. `float` works, `printf("%f")` works — but `<math.h>` has no `sqrt`,
   no `sin`. Use the library's tables, fixed point, or the ROM FP.
5. String literals stay **ASCII** — the reverse of cc65's PETSCII
   translation. Pick a screen charset and stick to it.
6. Pointer returns arrive in `__rc2/__rc3`, not `A/X` — it matters the
   moment you write assembly against C.
7. Recursion, `alloca`, and gratuitous function pointers forfeit the
   static-stack optimization.
8. cc65 and llvm-mos object code **cannot be mixed** — different
   calling conventions, different object formats.
