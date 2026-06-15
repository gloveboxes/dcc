# Appendix: how `dccrtlstrip` works and code size

`DCCRTL.MAC` is a single ~16,380-line runtime, but most programs use only a
fraction of it. The normal dcc build flow runs `dccrtlstrip` before the final
L80 link to remove routines you don't call, so you only pay for what you use. This appendix
explains how it decides what to keep and — more usefully — what each library
feature actually costs in code size once its transitive dependencies are pulled
in.

## How `dccrtlstrip` decides what to keep

Most library names in the standard headers are ordinary C identifiers. During
code generation, dcc maps well-known library calls to short internal assembler
labels (for example `memcpy` becomes `__mcpy`, `strlen` becomes `__slen`). You
never write those short names yourself — you include the header and call the C
function — but these internal names are what `dccrtlstrip` sees when it scans
the generated `.MAC` file.

`dccrtlstrip` is a conservative dead-block eliminator that runs **before** L80
linking. Its flow:

1. **Split into blocks.** `DCCRTL.MAC` is split into blocks delimited by
   `public` directives. A run of consecutive `public` lines becomes a shared
   *prelude* block, and each real public label after it becomes its own block
   that *depends* on the prelude. Everything before the first `public` (the
   `org 100h`, the `extrn` declarations, the `errno` EQUs, `HDRSIZE`) is an
   unconditional preamble.
2. **Scan the app for references.** For each app `.mac`, opcodes are parsed and
   their symbol operands recorded as *roots* (`extrn`, `call`, `jp`, `jr`, `dw`,
   and `ld` forms). A fallback whole-token scan also treats any exact mention of
   a known runtime symbol as a root.
3. **Mark reachable blocks.** `start` is forced as a root. Each root's owning
   block (plus its prelude) is kept, then the kept blocks are re-scanned for
   further references, iterating to a fixpoint. **Transitive runtime-to-runtime
   dependencies are therefore pulled in automatically.**
4. **Write the output.** The preamble is emitted unconditionally, then only the
   kept blocks; `public` lines are filtered so only kept symbols are
   re-declared.

### Design consequences

- **Transitivity is automatic** — keeping `_printf` re-scans its body and pulls
  the `pf_*` helpers; keeping a float op pulls the classify helpers.
- **The fallback scan is deliberately over-conservative** — any mention of a
  runtime symbol's exact name keeps it, which is why float `printf` (`_pffio`)
  is reliably retained when `-ffloatio` is used.
- **Unused features cost nothing** — a program that never does `float`
  arithmetic keeps none of the float blocks.

## How to read the size numbers

The numbers below are **relative line counts**, not exact bytes (blocks contain
comments and blank lines). Use them to compare features and spot the
heavyweights.

- **self** = source lines in the function's own block.
- **marginal** = self + every *additional* reachable block that is not already
  in the always-present baseline. This is the true incremental cost of using
  that function in a program that otherwise wouldn't need it.
- **pulls in** = the extra runtime blocks added beyond the baseline.

## The always-present baseline (~208 lines)

Every program links these regardless of what it calls, because `start` is a
forced root:

| Block | Role |
| --- | --- |
| `start` | entry, heap init, BSS zeroing, calls `_main` |
| `__build_argv` (+ `__conout`, `__argbuf`, `argv`) | command-tail argv builder; **also holds `__conout`**, the console writer |
| `__brk`, `__hlimit` | heap state words |
| `_exit` (+ `__cpm_set_retcode`) | reached from `start` after `_main` returns |

Because `__conout` lives inside the `__build_argv` block, **console output costs
nothing extra** — `putchar`/`puts` just call into already-present code.

## Formatted I/O (`stdio.h`)

| Function | self | marginal | pulls in (beyond baseline) |
| --- | ---: | ---: | --- |
| `printf` (`-ffloatio` ⇒ `_pffio`) | 212 | **~2210** | `_printf` + full float stack (all arithmetic and int/long↔float conversions) |
| `fprintf` | 68 | **~1372** | `_printf`, `_write` + file-I/O core |
| `printf` (integer-only) | 717 | ~717 | nothing — self-contained monolith |
| `sprintf` | 31 | ~748 | `_printf` (shares its formatter body) |
| `vprintf` | 26 | ~743 | `_printf` (console sink) |
| `vsprintf` | 31 | ~748 | `_printf` (buffer sink) |
| `vfprintf` | 73 | ~1377 | `_printf`, `_write` + file-I/O core |
| `puts` | 27 | ~27 | nothing (console only) |
| `fputc`/`putc` | 63 | ~650 | `_write` + file-I/O core |
| `fputs` | 58 | ~645 | `_write` + file-I/O core |
| `putchar` | 12 | ~12 | nothing (console only) |

!!! warning "Console-only output: avoid the file-stream functions"
    `fputc`/`fputs`/`fprintf` are **not** lightweight even when you only ever
    target the console — they dispatch on the file descriptor and therefore link
    the whole low-level file-I/O core. For console-only output prefer
    `putchar`/`puts`/`printf`.

### Formatted input (`scanf` family)

| Function | self | marginal | pulls in |
| --- | ---: | ---: | --- |
| `fscanf` (shared core) | 697 | ~1288 | `__getc`, `_read` + file-I/O core |
| `scanf` / `sscanf` | 17 | ~1305 | tiny stubs that jump into the `fscanf` core |

`scanf`/`sscanf` look like 17-line stubs but jump into the shared 697-line
`fscanf` core, so using any one of them links all three plus the read path.

## Low-level file I/O — one shared core

`open`/`read`/`write`/`close`/`lseek`/`unlink`/`fsync`/`fdatasync` share a
prelude and a common FCB/DMA core. **Using any one of them links that core
(~470 lines).** The first file function is expensive; additional ones are nearly
free.

| Function | self | marginal | pulls in |
| --- | ---: | ---: | --- |
| `fread` | 65 | ~1032 | `_read` + core + integer `*` `/` `%` helpers (record math) |
| `fwrite` | 44 | ~1050 | `_write` + core + integer mul/div/mod helpers |
| `fgets` | 164 | ~712 | `_read` + core |
| `fopen` | 64 | ~607 | `_open` + core |
| `write` / `lseek` | 152 / 150 | ~587 / 585 | core |
| `read` / `open` | 113 / 108 | ~548 / 543 | core |
| `close` / `unlink` | 43 / 37 | ~478 / 472 | core |
| `fclose` | 17 | ~495 | `_close` + core |

## Memory (`stdlib.h`)

| Function | self | marginal | pulls in |
| --- | ---: | ---: | --- |
| `calloc` | 43 | ~646 | overflow-checked size math (`__ckmul`/`__divu`/`__mulu`/`__mods`/`__modu`/`__r1s`/`__r1u`) |
| `realloc` | 174 | ~478 | `__frcoal`, `__mlh` |
| `malloc` | 15 | ~199 | `__mlh` (block-coalescing helper) |
| `free` | 16 | ~136 | `__frcoal` |

## Integer arithmetic, sort/search, conversion

| Function | self | marginal | pulls in |
| --- | ---: | ---: | --- |
| `qsort` | 143 | ~383 | `__call_hl`, `__mulu`, `__r1s`, `__r1u` |
| `bsearch` | 88 | ~328 | `__call_hl`, `__mulu`, `__r1s`, `__r1u` |
| `strtol` | 528 | ~980 | 32-bit long mul/div/mod/compare + `_errno` |
| `strtoul` | 485 | ~485 | shares the long-arithmetic core with `strtol` |
| `atol` | 112 | ~112 | nothing |
| `atoi` | 71 | ~71 | nothing |
| `rand` / `srand` | 44 | ~44 | nothing |

### 32-bit `long` arithmetic

| Operation | self | marginal | pulls in |
| --- | ---: | ---: | --- |
| `long *` | 79 | ~342 | `__lmd`, `__mulu`, `__r1s`, `__r1u` |
| `long /` signed | 151 | ~295 | `__lmd`, `__lmu` |
| `long %` signed | 135 | ~279 | `__lmd`, `__lmu` |
| `long /` unsigned | 109 | ~253 | `__lmd`, `__lmu` |
| `long %` unsigned | 117 | ~144 | `__lmd` |
| `long <` signed/unsigned | 31 / 23 | ~31 / 23 | nothing |

## Float — the most expensive feature

A single `float` operator links the shared normalise/round core (~700+ lines).

| Operation | self | marginal | pulls in |
| --- | ---: | ---: | --- |
| `float +` / `-` | 9 | ~732 | `__fdiv`, `__fsub` (shared normalise/round core) |
| `float *` | 326 | ~1049 | `__fdiv`, `__fsub` |
| `float /` | 277 | ~723 | `__fsub` |
| `float <` | 59 | ~269 | `__fge`, `__fnan`, `__fzro` |
| `float ==` | 72 | ~108 | `__fnan`, `__fzro` |
| `int→float` | 19 | ~52 | `__fuf` |
| `float→int` | 30 | ~71 | `__ffu` |
| `long→float` | 32 | ~76 | `__fulf` |
| `float→long` | 42 | ~98 | `__fful` |

### `math.h` float functions

Each `math.h` routine links the shared float arithmetic core plus whatever other
math routines it calls. `powf` is the worst case because it chains `expf`,
`logf`, `frexpf`, and `ldexpf` on top of the float core.

| Function | self | marginal | pulls in |
| --- | ---: | ---: | --- |
| `powf` | 280 | ~3296 | `expf`, `logf`, `frexpf`, `ldexpf` + full float core/conversions |
| `tanhf` | 113 | ~2504 | `expf`, `ldexpf` + float core |
| `sinhf` / `coshf` | 60 | ~2451 | `expf`, `ldexpf` + float core |
| `expf` | 340 | ~2279 | `ldexpf` + float core + int/long↔float conversions |
| `tanf` | 377 | ~2110 | `fmodf` + float core + long↔float conversions |
| `acosf` | 29 | ~2085 | `asinf`, `sqrtf` + float core |
| `atan2f` | 192 | ~2083 | `atanf` + float core |
| `log10f` | 29 | ~2082 | `logf`, `frexpf` + float core |
| `asinf` | 360 | ~2056 | `sqrtf` + float core |
| `logf` | 291 | ~2053 | `frexpf` + float core + uint→float |
| `cosf` | 347 | ~2008 | `fmodf` + float core + long↔float conversions |
| `sinf` | 305 | ~1966 | `fmodf` + float core + long↔float conversions |
| `atanf` | 377 | ~1760 | float core |
| `sqrtf` | 201 | ~1640 | float core + `__fge`/`__flt`, `__fnan`/`__fzro`, `__fscale_pow2` |
| `modff` | 96 | ~1537 | `floorf`, `ceilf` + long↔float conversions |
| `fmodf` | 104 | ~1327 | `__fdiv`/`__fmul`/`__fsub` + long↔float conversions |
| `ldexpf` | 251 | ~589 | `__feq`/`__fge`/`__fgt`, `__fnan`/`__fzro` |
| `nextafterf` | 236 | ~577 | `__feq`/`__fge`/`__flt`, `__fnan`/`__fzro` |
| `floorf` / `ceilf` | 83 / 77 | ~523 / 520 | long↔float conv + compares |
| `frexpf` | 202 | ~310 | `__feq`, `__fnan`, `__fzro` |
| `fabsf` | 14 | ~22 | `__fabs` |

!!! tip "`frexpf` / `ldexpf` are the cheap float functions"
    They manipulate IEEE-754 bits directly (no arithmetic core), so they only
    pull a couple of classify helpers. Everything else drags in the full float
    arithmetic stack, and the `exp`/`log`/`pow` and hyperbolic group is the most
    expensive thing you can link — budget ~2,000–3,300 lines for any one of them.

## Strings / ctype — almost all self-contained

Most string and ctype routines pull in **nothing** beyond themselves (each
~15–100 lines), so they are cheap to use individually.

| Function | self | marginal | pulls in |
| --- | ---: | ---: | --- |
| `strdup` | 40 | ~258 | `_malloc` + `__mlh`, `__slen` |
| `strstr` | 96 | ~96 | nothing |
| `strtok` | 97 | ~185 | `__sspn` (`strspn`), `__spbr` (`strpbrk`) |
| `memset` | 42 | ~42 | nothing |
| `memcpy` | 32 | ~32 | nothing |
| `strcpy` | 21 | ~21 | nothing |
| `strlen` | 19 | ~19 | nothing |
| `isalpha` | 20 | ~20 | nothing |
| `toupper` | 21 | ~21 | nothing |

`strdup` is the notable exception: it allocates, so it drags in the whole
`malloc` chain.

## CP/M extensions and misc

| Function | self | marginal | pulls in |
| --- | ---: | ---: | --- |
| `perror` | 58 | ~162 | `_errno`, `_strerror` |
| `opendir` | 73 | ~90 | `_dcls` |
| `readdir` | 99 | ~116 | `_dcls` |
| `strerror` | 71 | ~71 | nothing |
| `setjmp` / `longjmp` | 44 / 85 | ~44 / 85 | nothing |
| `bdos` / `inp` / `outp` | 31 / 15 / 16 | ~31 / 15 / 16 | nothing |

## Optimisation takeaways

1. **Console-only output is cheap.** `putchar`, `puts`, and integer `printf`
   only touch already-present code or are self-contained. Avoid
   `fputc`/`fputs`/`fprintf` for console work — they link the file-I/O core.
2. **`printf` is a 717-line monolith** but pulls nothing else. **Float printf
   (`-ffloatio`) roughly quadruples that** by linking the entire float stack.
   `vprintf`/`vsprintf` reuse that engine for free; `vfprintf` carries the
   file-I/O core like `fprintf`.
3. **Any single low-level file call links the whole FCB/DMA core (~470 lines).**
   The first file function is expensive; additional ones are nearly free.
4. **`scanf`/`sscanf` are not small** — they share the 697-line `fscanf` core.
5. **Float is the biggest lever.** A single `float` operator links ~700+ lines;
   `sqrtf`/`fmodf` exceed 1,300 lines, and the `expf`/`logf`/`powf` and
   hyperbolic group runs ~2,000–3,300 lines.
6. **`malloc`/`calloc` pull integer mul/div/mod helpers** for size arithmetic;
   `strdup` inherits the whole `malloc` chain.
7. **String/ctype routines are individually cheap** — they pull only themselves.

The practical rule: every call either stays cheap or drags in a substantial
amount of support code. Reach for the console functions, integer-only `printf`,
and the self-contained string helpers when binary size matters, and treat float
formatting and the transcendental math functions as deliberate, budgeted
choices.

## Regenerating the size report

Run the report generator whenever a change can alter runtime reachability or
block sizes. That includes:

- editing `DCCRTL.MAC`, especially adding/removing `public` labels, moving block
   boundaries, or changing helper calls inside runtime routines,
- adding, removing, or renaming runtime functions in the C headers,
- changing compiler runtime-name mapping in `src/dcc/dcc_asmname.c`,
- changing arithmetic/codegen helper emission in the compiler or peephole
   optimizer,
- changing `dccrtlstrip` reachability logic.

From the repo root, generate a full Markdown report:

```sh
python3 scripts/dccrtl_size_report.py > dccrtl-size-report.md
```

Use that output to refresh the tables in this appendix. The default report is
grouped for documentation updates and includes `self`, `marginal`, and pulled-in
blocks for the runtime symbols most likely to matter to C programmers.

For targeted checks while editing a particular section, pass runtime public
symbols directly:

```sh
python3 scripts/dccrtl_size_report.py --symbols _printf,_pffio,_malloc,_powf
```

To inspect every public runtime symbol, sort by largest marginal cost:

```sh
python3 scripts/dccrtl_size_report.py --all-publics --sort marginal \
   > dccrtl-all-publics.md
```

Agents can request structured output for comparison or automated updates:

```sh
python3 scripts/dccrtl_size_report.py --all-publics --format json \
   > dccrtl-all-publics.json
```

After updating this appendix, rebuild the docs in strict mode:

```sh
cd docs/docs
mkdocs build --strict
```

The script parses `public`-delimited runtime blocks, computes the baseline
reachable from `start`, and reports each symbol's self and marginal line counts
using the same reachability model as `dccrtlstrip`. The counts are still a
relative source-line proxy, not exact `.COM` bytes.
