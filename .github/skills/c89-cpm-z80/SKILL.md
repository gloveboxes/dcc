---
name: c89-cpm-z80
description: 'Write, build, test, and debug C89 code for the dcc compiler targeting CP/M 2.2 on the Z80 (e.g. run under the ntvcm Altair 8800 emulator). Use when working on .c/.h sources compiled with dcc, or when the task mentions dcc, CP/M, CP/M 2.2, Z80, ntvcm, DCCRTL, ma.sh, VT100/ANSI terminal apps for CP/M, or the no-double single-precision-float constraint. dcc is TRUE C89 (prototypes, void, const, typedef, enum, and K&R definitions all accepted) and also accepts some C99 conveniences (for-init declarations, // line comments, block-scoped variables, inline). Hard constraints to respect: no double (32-bit float is the only floating type), 16-bit int/short, 32-bit long, 16-bit pointers, 16-bit size_t. Full library/printf/scanf inventory and hard-won pitfalls are in the reference files.'
argument-hint: 'Describe the C89/CP-M task (write code, build, run under ntvcm, debug a failure)'
---

# C89 for dcc (CP/M 2.2 / Z80)

dcc is a C89 compiler that emits Z80 assembly for CP/M 2.2.
The runtime is [DCCRTL.MAC](DCCRTL.MAC). Programs run on
real CP/M hardware or an emulator such as **ntvcm** (Altair 8800).

## When to use

- Writing, porting, or reviewing C source intended to be compiled by `dcc`.
- Building/running/debugging a dcc program (`ma.sh`, `ntvcm`).
- Any task touching CP/M file I/O, VT100/ANSI console UIs, or DCCRTL.

## Golden rules

1. **No `double`. Ever.** `float` (32-bit IEEE single) is the *only* floating
   type. The `double` keyword is not recognised — using it is a compile error.
   Write `float`, and use `f`-suffixed literals (`3.14f`). Unsuffixed float
   constants are already `float`, not `double`.
2. **`int` is 16-bit** (±32767). Use `long` (32-bit) + `%ld` when you need
   more range. Watch for silent overflow in loops, sizes, and accumulators.
3. **Pointers and `size_t` are 16-bit.** `ptrdiff_t` is `int`. Flat 64 KB space.
4. **`char` is signed by default.** Use `unsigned char` for byte/`>=0x80` work.
5. **The library is a subset.** No `atof`/`strtod` (both return `double`), no
   `<locale.h>`/`<signal.h>`/`<time.h>`. `<math.h>` now has the full
   single-precision set (trig, exp/log, hyperbolic, decomposition); `strtol`/
   `strtoul`/`strtok` are present. See
   [references/library.md](./references/library.md) before assuming a function exists.
6. **`%f` needs `-ffloatio`.** Float `printf` formatting isn't linked unless you
   pass that flag. Without it, `%f` won't work.
7. **CP/M filenames are 8.3, uppercase.** The `.COM` is named after the source
   (`foo.c` → `FOO.COM`); run it as `ntvcm FOO`.

## Type model

At a glance: `char` 8-bit (signed), `short`/`int` 16-bit, `long` 32-bit,
`float` 32-bit (the only floating type), pointers / `size_t` / `ptrdiff_t` /
`wchar_t` all 16-bit, `FILE` is `int`. Full annotated table in
[references/library.md](./references/library.md).

## Language support

dcc is C89 at heart but **accepts a handful of C99 conveniences**: `for`-init
declarations with real loop scope (`for (int i = 0; ...)`), `//` line comments,
block-scoped variable storage (inner `{ ... }` declarations shadow outer ones),
and `inline` (parsed, ignored). Details below.

- **31 of 32 C89 keywords.** Only `double` is missing. Everything else
  (`struct`, `union`, `enum`, `typedef`, `const`, `volatile`, `sizeof`, all
  control flow, all storage classes) is recognised.
- **Inert but accepted:** `const` (constant-folds initializers only; not
  read-only memory), `volatile` (ignored), `register` (hint only), `auto`
  (no-op). `inline` is accepted as a C99 extension and ignored.
- **C99 `for`-init declarations are supported** with real loop scope:
  `for (int i = 0; i < n; i++)` declares `i` only for the loop, so it shadows
  (does not clobber) an outer same-named variable and is invisible after the
  loop. Multiple declarators (`for (int i = 0, j = n; ...)`) and pointer/array
  declarators scope the same way.
- **Block scope is fully modeled.** A variable declared in an inner `{ ... }`
  block (including `if`/`while`/`for`/`switch` bodies) shadows an outer
  same-named local or parameter for that block only and is invisible once the
  block ends — sibling blocks may safely reuse a name, and an inner block may
  shadow a `for`-init loop variable.
- **C99 `//` line comments are supported** alongside C89 `/* ... */` block
  comments — including trailing comments on `#define` lines (stripped like
  block comments before macro replacement). Both forms are correctly ignored
  inside string and character literals (e.g. `"http://..."` stays intact).
- **K&R function definitions are accepted** (dcc's typing is lenient), but
  prefer prototypes for new code. Mixed decls/statements and block-local
  variables are fine (real C89).
- **Full C89 operator set**, including bitwise `&` `|` `^` `~` `<<` `>>` and
  their compound forms (`&=` `|=` `^=` `<<=` `>>=`). `>>` is **arithmetic**
  (sign-extending) on signed operands and **logical** (zero-fill) on unsigned —
  use `unsigned`/`unsigned long` when you need a guaranteed zero-fill shift.
  Shifts act at the operand width (16-bit for `int`; cast/promote to `long`
  for wider shifts).
- No other C99/C11 features (`restrict`, `_Bool`, VLAs, compound literals,
  designated initializers).

## Identifier significance

C89 guarantees significance of at least **31 characters** for internal
identifiers and at least **6** (case-insensitive) for external (global /
function) ones. dcc satisfies both, so you never need to shorten names for the
toolchain (this is *not* BDS C's 7-char rule):

- Internal names (locals, `static` file-scope names) keep their full spelling.
- External names stay distinct well past the 6 characters C89 requires (verified
  to at least the first 13 characters in this toolchain). Only externals sharing
  a very long prefix (~16+ identical leading characters, far beyond what C89
  promises or normal code uses) can silently collide at link time; make such a
  one-file helper `static` if it ever matters.

## Floating point: single precision only

- `float` is 32-bit; there is no `double` or `long double`.
- `<math.h>` provides a full single-precision library: rounding/remainder/roots
  (`fabsf`, `floorf`, `ceilf`, `sqrtf`, `fmodf`, `nextafterf`), exp/log
  (`expf`, `logf`, `log10f`, `powf`), trig (`sinf`, `cosf`, `tanf`, `asinf`,
  `acosf`, `atanf`, `atan2f`), hyperbolic (`sinhf`, `coshf`, `tanhf`), and
  decomposition (`frexpf`, `ldexpf`, `modff`) — each with an **unsuffixed alias**
  (`sin`, `cos`, `exp`, `pow`, …) that stays single-precision. The
  transcendentals are polynomial approximations (~5–6 digits), not full `float`
  accuracy.
- No `atof`/`strtod`. To parse decimals to `float`, write a small parser
  (`strtol`/`strtoul` handle integers).
- `printf("%f", x)` requires the `-ffloatio` compile flag and consumes a 32-bit
  `float` (there is no float→double default promotion, because there's no double).

## Build and run

The standard helper scripts live in the dcc repo. **Set these env vars first**
(they make `ma.sh` use the local binaries):

```sh
export PATH="/Users/<USER_NAME_FOLDER>/GitHub/ntvcm:/Users/<USER_NAME_FOLDER>/GitHub/dcc:$PATH"
export DCC=./dcc DCCPEEP=./dccpeep DCCRTLSTRIP=./dccrtlstrip
```

**Build/run one program** (compile → peephole → strip runtime → M80 → L80):

```sh
./ma.sh foo peep      # foo.c -> FOO.COM (peephole optimised); use 'nopeep' to skip
ntvcm FOO             # run it (uppercase, no extension)
ntvcm FOO ARG1 ARG2   # with CP/M command-line args
```

**Useful `dcc` options:** `-o file` (output .mac), `-c`/`-module` (linkable
module), `-f`/`-ffloatio` (float printf), `-stack N`/`-s N`/`--stack N` (reserve
stack; default 512 — heap and stack share memory, **no guard**), `-I dir` (or
joined `-Idir`; repeatable), `-Dname[=v]`,
`-Uname`, `-v`, `-h`. `_DCC_=1` is always predefined.

**Finding the standard headers (`-I`).** dcc resolves `#include <stdio.h>` by
checking the current directory first, then each `-I` directory in order. The
bundled headers (`stdio.h`, `stdlib.h`, `string.h`, `math.h`, …) live in the
**dcc repo root**, so:

- Building **inside** the dcc repo (as `ma.sh` does): they're found
  automatically via the current directory — no `-I` needed.
- Building **elsewhere**: point dcc at the repo, e.g.
  `dcc -I /path/to/dcc myapp.c -o myapp.mac` (repeat `-I` for more dirs).

Gotcha: a `<...>` header that isn't found is **silently ignored** (you lose its
prototypes and fall back to implicit `int`, so calls still compile and link via
the runtime but without type-checking); a missing `"..."` header is a fatal
error. If standard calls compile yet misbehave, check that `-I` actually
resolves the dcc headers.

Notes: M80 needs CRLF (`ma.sh` converts). `RTLMIN.MAC` is generated per-app by
`dccrtlstrip` during the build — don't hand-edit it.

## Top pitfalls (details in references/pitfalls.md)

- **No `double`** — `float` is the only floating type; no `atof`/`strtod`.
  (`<math.h>` *does* now provide single-precision transcendentals.)
- **`%f` silently does nothing without `-ffloatio`.**
- **16-bit `int`/`size_t` overflow** in sizes/indices/accumulators — promote to
  `long` (and `%ld`).
- **`char` is signed** — use `unsigned char` for bytes `>= 0x80` and table indices.
- **CP/M filenames are 8.3** and ntvcm asserts on extensions `> 3` chars — delete
  stray `.DS_Store` before running a directory-enumerating program.
- **No stack/heap guard** — size the stack with `-stack N`; keep big buffers
  `static`/global.

See [references/pitfalls.md](./references/pitfalls.md) for worked examples and
[references/library.md](./references/library.md) for the full function inventory,
`printf`/`scanf` conversion tables, and which headers/functions are absent.

## Workflow

1. **Confirm the constraint surface.** If the code uses floating point, plan for
   single precision (no `double`); if it parses decimals, plan a `float` parser
   (no `atof`). If it needs `time`/`signal`/`locale`, those headers don't exist.
2. **Check the library** in [references/library.md](./references/library.md)
   before calling anything you haven't verified — a missing function is a link
   error (`unresolved external`), not a compile error.
3. **Match repo conventions.** Read a nearby working program first. In the dcc
   repo, the authoritative, exhaustive reference is
   [dcc-c89-reference-guide.md](dcc-c89-reference-guide.md) at the repo root.
4. **Write idiomatic C89** for the 16-bit model; prefer `long`/`%ld` where range
   matters; use `unsigned char` for byte work.
5. **Build and run**: `./ma.sh <name> peep && ntvcm <NAME>`. Add `-ffloatio`
   (via the program's build path) if you use `%f`.
6. **Verify behaviour** by running the program under `ntvcm` (redirect stdin
   for interactive apps). Compare against expected output for your own program.
