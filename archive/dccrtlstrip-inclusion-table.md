# DCCRTL.MAC runtime inclusion reference

How `dccrtlstrip` decides which blocks of `DCCRTL.MAC` are linked into a program,
and the C-level condition that triggers each runtime block.

## How `dccrtlstrip` decides what to keep

`dccrtlstrip` is a conservative dead-block eliminator that runs **before** L80
linking. Its flow:

1. **`read_runtime()` / `build_blocks()`** — splits `DCCRTL.MAC` into blocks
   delimited by `public` directives. A run of consecutive `public` lines becomes
   a shared *prelude* block, and each real public label after it becomes its own
   block that *depends* on the prelude. Everything **before the first `public`**
   (the `org 100h`, the `extrn _main/__hstart/__bssb/__bsse/__stack_size`, the
   `errno` EQUs, `HDRSIZE`) is an unconditional preamble.
2. **`scan_app()`** — for each app `.mac` it detects requirements two ways:
   - `add_refs_from_line()` parses opcodes and records the symbol operand as a
     *root*: `extrn`, `call`, `jp`, `jr` (handles `jp cc,label`), `dw`, and `ld`
     (`ld hl,sym`, `ld a,(sym)`, `ld (sym),hl`).
   - `add_known_runtime_refs_from_line()` is a fallback: if any whole-token match
     of a known runtime symbol appears anywhere on the line, that symbol becomes
     a root.
3. **`mark_reachable()`** — forces `start` as a root, keeps each root's owning
   block (plus its prelude dependency), then re-scans the kept blocks for further
   references, iterating to a fixpoint. So **transitive** runtime-to-runtime
   dependencies are pulled in automatically.
4. **`write_output()`** — emits the preamble unconditionally, then only kept
   blocks; `public` lines are filtered so only kept symbols are re-declared.

The C→assembler symbol names are assigned by the compiler
(`src/dcc/dcc_asmname.c`) and arithmetic/codegen helpers by the peephole/codegen
passes (`src/dcc/dcc_ops.c`), so the "Condition" column below is the C-level
construct that makes the compiler emit a reference to that symbol.

## Inclusion table

### Always included (root = `start`, unconditional)

| Condition | Code included |
|---|---|
| Every program (preamble before first `public`) | `org 100h`, `extrn _main/__hstart/__bssb/__bsse/__stack_size`, `errno` EQUs, `HDRSIZE` |
| Every program (`start` is a forced root) | `start`, heap vars `__hbase`/`__brk`/`__hlimit`, BSS zeroing |
| `start` calls `__build_argv` | `__build_argv`, `__argc`, `argv`, `argv0`, `__argbuf` |
| `start` calls `__cpm_set_retcode` (in the `_exit` block) | `_exit`, `__cpm_set_retcode` |

### Codegen / peephole frame stubs (emitted by `dccpeep`)

| Condition | Code included |
|---|---|
| Function with no locals | `__en0` |
| Function with locals (prologue / epilogue) | `__entr`, `__lve` |
| Load 1st/2nd/3rd word parameter | `__la1`, `__la2`, `__la3` |
| Load local word var (offsets −2…−16) | `__lv1`…`__lv8` |
| Store local word var (offsets −2…−12) | `__sv1`…`__sv6` |
| `push hl` + frame-pointer copy | `__phix` |
| Dereference 16-bit pointer | `__ldwl` |
| 16-bit `&` | `__wand` |
| Signed 16-bit compare | `__icmp` |
| Sign-extend `int`→`int` in DE / L→HL | `__sxde`, `__sxhl` |
| Call through function pointer | `__call_hl` |

### 16-bit integer arithmetic

| Condition | Code included |
|---|---|
| `int`/`unsigned` `*` | `__mulu` |
| unsigned `/`, `%` | `__divu`, `__modu` |
| signed `/`, `%` | `__divs`, `__mods` (shares `DRSU`) |
| `% C` where C is 8-bit constant | `__r1u` (unsigned), `__r1s` (signed) |
| `/ C` or `% C` by constant (peephole rewrite) | `__q2u`/`__r2u`/`__q2s`/`__r2s` (`D16U`) |
| `abs()`, `labs()`, `div()`, `ldiv()` | `_abs`, `_labs`, `_div`, `_ldiv` |

### 32-bit `long` arithmetic

| Condition | Code included |
|---|---|
| `long` `*` | `__lmul`, `__m16u`, `__m16s`, `__mlh`, `__lmd` |
| `long` `/` unsigned / signed | `__ldu` / `__lds` |
| `long` `%` unsigned / signed | `__lmu` / `__lms` |
| `long` compare `<  <= >  >=` unsigned | `__ltu`, `__leu`, `__lgu`, `__lku` |
| `long` compare `<  <= >  >=` signed | `__lts`, `__les`, `__lgs`, `__lks` |

### `<string.h>` / `<ctype.h>`

| Condition | Code included |
|---|---|
| `strlen` (and fast-call form) | `__slen`, `__slf` |
| `strcpy` / `strncpy` | `__scpy` / `__ncpy` |
| `strcmp` / `strncmp` | `__scmp` / `__ncmp` |
| `strcat` / `strncat` | `__scat` / `__ncat` |
| `strchr` (and fast-call) / `strrchr` | `__schr`, `__chf` / `__srch` |
| `strstr` / `strdup` / `strcoll` | `__sstr` / `__sdup` / `__scol` |
| `strspn` / `strcspn` / `strpbrk` | `__sspn` / `__scsp` / `__spbr` |
| `strtok` | `__stok` |
| `memcpy` / `memmove` / `memset` | `__mcpy` / `__mmov` / `__mset` |
| `memcmp` / `memchr` | `__mcmp` / `__mchr` |
| `isalpha/isalnum/isspace/isdigit` | `__caa`/`__can`/`__csp`/`__cdg` |
| `isupper/islower/isxdigit/isprint` | `__cup`/`__clo`/`__cxd`/`__cpr` |
| `iscntrl/ispunct/toupper/tolower` | `__cct`/`__cpu`/`__ctu`/`__ctl` |

### `<stdio.h>`

| Condition | Code included |
|---|---|
| `printf` (no `-ffloatio`) | `_printf` (+ `pf_*` body, sink/buffer) |
| `printf` with `-ffloatio` | `_pffio` (compiler swaps the name; pulls float `%f` path) |
| `sprintf` / `fprintf` | `_sprintf` / `_fprintf` |
| `vprintf` / `vfprintf` / `vsprintf` | `_vprintf` / `_vfprintf` / `_vsprintf` (reuse the `pf_*` engine) |
| `puts` / `fputs` / `fgets` | `_puts` / `__fps` / `_fgets` |
| `putchar`/`putc`/`fputc` | `__pchr` / `__putc` / `__fpc` |
| `getc`/`getchar` | `__getc` / `___gchr` |
| `scanf` / `sscanf` / `fscanf` | `_scanf` / `_sscanf` / `_fscanf` |
| `fopen`/`fclose`/`fread`/`fwrite` | `_fopen` / `_fclose` / `_fread` / `_fwrite` |
| `fseek`/`ftell`/`rewind`/`feof`/`ferror` | `_fseek` / `_ftell` / `_rewind` / `_feof` / `_ferror` |
| `fflush`/`clearerr`/`setbuf`/`perror`/`remove` | `_fflush` / `_clearerr` / `_setbuf` / `_perror` / `_remove` |
| Any `stdin/stdout/stderr` use | `_stdin` / `_stdout` / `_stderr` |

### Low-level file I/O (`<fcntl.h>`/`<unistd.h>`) and CP/M dir access

| Condition | Code included |
|---|---|
| `open/read/write/close` | `_open`, `_read`, `_write`, `_close` (+ `__fcbs`, `__dma`) |
| `lseek` / `unlink` / `fsync` / `fdatasync` | `_lseek` / `_unlink` / `_fsync` / `_fdatasync` |
| `opendir`/`readdir`/`closedir` (dcc dir API) | `_dopn` / `_drd` / `_dcls` |

### `<stdlib.h>` misc & memory

| Condition | Code included |
|---|---|
| `malloc` / `calloc` / `realloc` / `free` | `_malloc` (+ `__ckmul`) / `_calloc` / `__real` / `_free` (+ `__frcoal`) |
| `atoi` / `atol` | `_atoi` / `_atol` |
| `strtol` / `strtoul` | `__stol` / `__stou` |
| `rand` / `srand` | `_rand` / `_srand` |
| `bsearch` / `qsort` | `_bsearch` / `_qsort` |
| `bdos` / `inp` / `outp` (dcc extensions) | `_bdos` / `_inp` / `_outp` |
| `strerror`, `errno` reference | `_strerror`, `_errno` |
| `setjmp` / `longjmp` | `_setjmp` / `_longjmp` |

### Float (32-bit single precision) — only when float is actually used

| Condition | Code included |
|---|---|
| `float` `+ - * /` | `__fadd`, `__fsub`, `__fmul`, `__fdiv` |
| `float` compare `== != < > <= >=` | `__feq`, `__fneq`, `__flt`, `__fgt`, `__fle`, `__fge` |
| `int`↔`float` convert | `__fif` (int→fp), `__ffi` (fp→int), `__fuf` (uint→fp), `__ffu` (fp→uint) |
| `long`↔`float` convert | `__flf` (long→fp), `__ffl` (fp→long), `__fulf` (ulong→fp), `__fful` (fp→ulong) |
| float helpers / classify (pulled transitively) | `__fzro`, `__fnan`, `__finf`, `__fabs`, `__fchs`, `__fsgn`, `__fscale_pow2` |
| `fabsf`/`floorf`/`ceilf`/`fmodf` | `_fabsf` / `_floorf` / `_ceilf` / `_fmodf` |
| `sqrtf` / `nextafterf` | `_sqrtf` / `_nextafterf` |
| `expf`/`logf`/`log10f`/`powf` | `_expf` / `_logf` / `_log10f` / `_powf` |
| `sinf`/`cosf`/`tanf` | `_sinf` / `_cosf` / `_tanf` |
| `asinf`/`acosf`/`atanf`/`atan2f` | `_asinf` / `_acosf` / `_atanf` / `_atan2f` |
| `sinhf`/`coshf`/`tanhf` | `_sinhf` / `_coshf` / `_tanhf` |
| `frexpf`/`ldexpf`/`modff` | `_frexpf` / `_ldexpf` / `_modff` |

## Notable behaviours of this design

- **Transitivity is automatic** — e.g. keeping `_printf` re-scans its body and
  pulls `__pf32_*`, `pf_div10`, etc.; keeping a float op pulls the classify
  helpers.
- **The fallback whole-token scan is deliberately over-conservative** — any
  mention of a runtime symbol's exact name in app output keeps it, even in a
  comment-stripped line, which is why `_pffio` (float printf) is reliably
  retained.
- **Negative float tests keep nothing float-related** — e.g. `tests/tc89fltb.c`
  is a negative test where dcc rejects `float` arithmetic, so none of the float
  blocks are ever referenced or kept.

---

# Detailed dependency closures and marginal `.com` size cost

The tables above answer *"what block does each C function need?"*. This section
answers the size-optimisation question *"how much runtime does calling function
X actually drag in, including everything it transitively pulls?"*

## How to read the numbers

- **self** = source lines in the function's own block.
- **marginal** = self + every *additional* block reachable from it that is **not**
  already in the always-present baseline. This is the true incremental cost of
  using that function in a program that otherwise wouldn't need it.
- **pulls in** = the extra runtime blocks added to the link beyond the baseline.

Line counts are a **relative proxy**, not exact bytes — blocks contain comments
and blank lines. Use them to compare functions and spot the heavyweights, not to
predict an exact byte total. (Measured from `DCCRTL.MAC`, ~16,380 lines total.)

## Always-present baseline (~208 lines)

Every program links these regardless of what it calls, because `start` is a
forced root:

| Block | Role |
|---|---|
| `start` | entry, heap init, BSS zeroing, calls `_main` |
| `__build_argv` (+ `__conout`, `__argbuf`, `argv`) | command-tail argv builder; **also holds `__conout`**, the console writer |
| `__brk`, `__hlimit` | heap state words |
| `_exit` (+ `__cpm_set_retcode`) | reached from `start` after `_main` returns |

Because `__conout` lives inside the `__build_argv` block, **console output costs
nothing extra** — `putchar`/`puts` just call into already-present code.

## Marginal cost, heaviest first

### Formatted I/O (`<stdio.h>`)

| Function | self | marginal | pulls in (beyond baseline) |
|---|---:|---:|---|
| `printf` (`-ffloatio` ⇒ `_pffio`) | 212 | **~2210** | `_printf` + full float stack (`__fadd/__fsub/__fmul/__fdiv`, all int/long↔float conversions) |
| `fprintf` | 68 | **~1372** | `_printf`, `_write` + file-I/O core (`__dma`, `__fcbs`, `_errno`, `_fdatasync`) |
| `printf` (integer-only) | 717 | ~717 | nothing — self-contained monolith |
| `sprintf` | 31 | ~748 | `_printf` (shares its formatter body) |
| `vprintf` | 26 | ~743 | `_printf` (reuses `__pf_run`; console sink) |
| `vsprintf` | 31 | ~748 | `_printf` (reuses `__pf_run`; buffer sink) |
| `vfprintf` | 73 | ~1377 | `_printf`, `_write` + file-I/O core (fd dispatch, like `fprintf`) |
| `puts` | 27 | ~27 | nothing (console only) |
| `fputc`/`putc` (`__fpc`) | 63 | ~650 | `_write` + file-I/O core (needed for non-console fds) |
| `fputs` (`__fps`) | 58 | ~645 | `_write` + file-I/O core |
| `putchar` (`__pchr`) | 12 | ~12 | nothing (console only) |

> **Watch-out:** `fputc`/`fputs`/`fprintf` are *not* lightweight even when you
> only ever target the console — they dispatch on the fd and therefore link the
> whole low-level file-I/O core. For console-only output prefer
> `putchar`/`puts`/`printf`.

### Formatted input (`scanf` family)

| Function | self | marginal | pulls in |
|---|---:|---:|---|
| `fscanf` (shared core) | 697 | ~1288 | `__getc`, `_read` + file-I/O core |
| `scanf` / `sscanf` | 17 | ~1305 | tiny stubs that jump into the `fscanf` core, so they cost the same |

> `scanf`/`sscanf` look 17 lines each but jump into the shared `_fscanf` core —
> using any one of them links all three plus the read path.

### Low-level file I/O — one shared core

`open/read/write/close/lseek/unlink/fsync/fdatasync` are declared in one
consecutive `public` run, so they share a prelude and a common FCB/DMA core.
**Using any one of them links that core (~470 lines: `__dma`, `__fcbs`,
`_errno`, `_fdatasync`).**

| Function | self | marginal | pulls in |
|---|---:|---:|---|
| `fread` | 65 | ~1032 | `_read` + core + integer `*` / `/` / `%` helpers (record math) |
| `fwrite` | 44 | ~1050 | `_write` + core + integer mul/div/mod helpers |
| `fgets` | 164 | ~712 | `_read` + core |
| `fopen` | 64 | ~607 | `_open` + core |
| `write`/`lseek` | 152/150 | ~587/585 | core |
| `read`/`open` | 113/108 | ~548/543 | core |
| `close`/`unlink` | 43/37 | ~478/472 | core |
| `fclose` | 17 | ~495 | `_close` + core |

### Memory (`<stdlib.h>`)

| Function | self | marginal | pulls in |
|---|---:|---:|---|
| `calloc` | 43 | ~646 | `__mlh` + `__ckmul`/`__divu`/`__mulu`/`__mods`/`__modu`/`__r1s`/`__r1u` (overflow-checked size math) |
| `realloc` (`__real`) | 174 | ~478 | `__frcoal`, `__mlh` |
| `malloc` | 15 | ~199 | `__mlh` (block-coalescing helper) |
| `free` | 16 | ~136 | `__frcoal` |

### 16-bit integer & sort/search

| Function | self | marginal | pulls in |
|---|---:|---:|---|
| `qsort` | 143 | ~383 | `__call_hl`, `__mulu`, `__r1s`, `__r1u` |
| `bsearch` | 88 | ~328 | `__call_hl`, `__mulu`, `__r1s`, `__r1u` |
| `atol` | 112 | ~112 | nothing |
| `atoi` | 71 | ~71 | nothing |
| `strtol` | 528 | ~980 | `__lmul`/`__ldu`/`__lmu`/`__lgu` (32-bit long mul/div/mod/compare) + `_errno` |
| `strtoul` | 485 | ~485 | shares the same long-arithmetic core as `strtol`; ~940 on its own |
| `rand`/`srand` | 44 | ~44 | nothing |

### 32-bit `long` arithmetic

| Operation | self | marginal | pulls in |
|---|---:|---:|---|
| `long *` (`__lmul`) | 79 | ~342 | `__lmd`, `__mulu`, `__r1s`, `__r1u` |
| `long /` signed (`__lds`) | 151 | ~295 | `__lmd`, `__lmu` |
| `long %` signed (`__lms`) | 135 | ~279 | `__lmd`, `__lmu` |
| `long /` unsigned (`__ldu`) | 109 | ~253 | `__lmd`, `__lmu` |
| `long %` unsigned (`__lmu`) | 117 | ~144 | `__lmd` |
| `long <` signed/unsigned | 31/23 | ~31/23 | nothing |

### Float (32-bit) — the most expensive feature

| Operation | self | marginal | pulls in |
|---|---:|---:|---|
| `float +`/`-` (`__fadd`) | 9 | ~732 | `__fdiv`, `__fsub` (shared normalise/round core) |
| `float *` (`__fmul`) | 326 | ~1049 | `__fdiv`, `__fsub` |
| `float /` (`__fdiv`) | 277 | ~723 | `__fsub` |
| `float <` (`__flt`) | 59 | ~269 | `__fge`, `__fnan`, `__fzro` |
| `float ==` (`__feq`) | 72 | ~108 | `__fnan`, `__fzro` |
| `int→float` (`__fif`) | 19 | ~52 | `__fuf` |
| `float→int` (`__ffi`) | 30 | ~71 | `__ffu` |
| `long→float` (`__flf`) | 32 | ~76 | `__fulf` |
| `float→long` (`__ffl`) | 42 | ~98 | `__fful` |

### `<math.h>` float functions

The transcendental functions are single-precision C implementations folded into
`DCCRTL.MAC`. Each one links the shared float arithmetic core
(`__fadd/__fsub/__fmul/__fdiv` + classify helpers) plus whatever other math
routines it calls, so they are the heaviest individual features in the runtime.
`powf` is the worst case because it chains `expf`, `logf`, `frexpf`, and
`ldexpf` on top of the float core.

| Function | self | marginal | pulls in |
|---|---:|---:|---|
| `powf` | 280 | ~3296 | `expf`, `logf`, `frexpf`, `ldexpf` + full float core/conversions |
| `tanhf` | 113 | ~2504 | `expf`, `ldexpf` + float core/conversions |
| `sinhf` | 60 | ~2451 | `expf`, `ldexpf` + float core/conversions |
| `coshf` | 60 | ~2451 | `expf`, `ldexpf` + float core/conversions |
| `expf` | 340 | ~2279 | `ldexpf` + float core + int/long↔float conversions |
| `tanf` | 377 | ~2110 | `fmodf` + float core + long↔float conversions |
| `acosf` | 29 | ~2085 | `asinf`, `sqrtf` + float core |
| `atan2f` | 192 | ~2083 | `atanf` + float core |
| `log10f` | 29 | ~2082 | `logf`, `frexpf` + float core |
| `asinf` | 360 | ~2056 | `sqrtf` + float core |
| `logf` | 291 | ~2053 | `frexpf` + float core + uint→float |
| `cosf` | 347 | ~2008 | `fmodf` + float core + long↔float conversions |
| `sinf` | 305 | ~1966 | `fmodf` + float core + long↔float conversions |
| `atanf` | 377 | ~1760 | float core (`__fadd/__fsub/__fmul/__fdiv`, compares, classify) |
| `sqrtf` | 201 | ~1640 | `__fadd/__fsub/__fmul/__fdiv`, `__fge/__flt`, `__fnan/__fzro`, `__fscale_pow2` |
| `modff` | 96 | ~1537 | `floorf`, `ceilf` + long↔float conversions + compares |
| `fmodf` | 104 | ~1327 | `__fdiv/__fmul/__fsub` + long↔float conversions |
| `ldexpf` | 251 | ~589 | `__feq/__fge/__fgt`, `__fnan/__fzro` |
| `nextafterf` | 236 | ~577 | `__feq/__fge/__flt`, `__fnan/__fzro` |
| `floorf` | 83 | ~523 | long↔float conv + compares |
| `ceilf` | 77 | ~520 | long↔float conv + compares |
| `frexpf` | 202 | ~310 | `__feq`, `__fnan`, `__fzro` |
| `fabsf` | 14 | ~22 | `__fabs` |

> **`frexpf`/`ldexpf` are the cheap ones.** They manipulate IEEE-754 bits
> directly (no arithmetic core), so they only pull a couple of classify helpers.
> Everything else drags in the full float arithmetic stack, and the
> `exp`/`log`/`pow` and hyperbolic group is the most expensive thing you can
> link — budget ~2,000–3,300 lines for any one of them.

### `<string.h>` / `<ctype.h>` — almost all self-contained

Most string and ctype routines pull in **nothing** beyond themselves (each
~15–100 lines), so they are cheap to use individually:

| Function | self | marginal | pulls in |
|---|---:|---:|---|
| `strdup` | 40 | ~258 | `_malloc` + `__mlh`, `__slen` |
| `strstr` | 96 | ~96 | nothing |
| `strtok` | 97 | ~185 | `__sspn` (`strspn`), `__spbr` (`strpbrk`) |
| `memset` | 42 | ~42 | nothing |
| `memcpy` | 32 | ~32 | nothing |
| `strcpy` | 21 | ~21 | nothing |
| `strlen` | 19 | ~19 | nothing |
| `isalpha` (`__caa`) | 20 | ~20 | nothing |
| `toupper` (`__ctu`) | 21 | ~21 | nothing |

`strdup` is the notable exception: it allocates, so it drags in the whole
`malloc` chain.

### CP/M extensions & misc

| Function | self | marginal | pulls in |
|---|---:|---:|---|
| `perror` | 58 | ~162 | `_errno`, `_strerror` |
| `opendir` (`_dopn`) | 73 | ~90 | `_dcls` |
| `readdir` (`_drd`) | 99 | ~116 | `_dcls` |
| `strerror` | 71 | ~71 | nothing |
| `setjmp`/`longjmp` | 44/85 | ~44/85 | nothing |
| `bdos`/`inp`/`outp` | 31/15/16 | ~31/15/16 | nothing |

## Optimisation takeaways

1. **Console-only output is cheap.** `putchar`, `puts`, and integer `printf`
   only touch `__conout` (already in the baseline) or are self-contained. Avoid
   `fputc`/`fputs`/`fprintf` for console work — they link the file-I/O core.
2. **`printf` is a 717-line monolith** but pulls nothing else. **Float printf
   (`-ffloatio`) roughly quadruples that** by linking the entire float stack.
   `vprintf`/`vsprintf` reuse that same engine, so they cost no more than
   `printf`/`sprintf`; `vfprintf` carries the file-I/O core like `fprintf`.
3. **Any single low-level file call links the whole FCB/DMA core (~470 lines).**
   The first file function is expensive; additional ones are nearly free.
4. **`scanf`/`sscanf` are not small** — they share the 697-line `fscanf` core.
5. **Float is the biggest lever.** A single `float` operator links the shared
   normalise/round core (~700+ lines); `sqrtf`/`fmodf` exceed 1,300 lines, and
   the `expf`/`logf`/`powf` and hyperbolic group runs ~2,000–3,300 lines once
   their transitive math dependencies are pulled in.
6. **`malloc`/`calloc` pull integer mul/div/mod helpers** for size arithmetic;
   `strdup` inherits the whole `malloc` chain.
7. **String/ctype routines are individually cheap** — pull only themselves.

> These closures mirror exactly how `dccrtlstrip` attributes blocks (publics as
> boundaries, references via `call`/`jp`/`jr`/`dw`/`ld`/`extrn`, transitive
> fixpoint). Regenerate them after editing `DCCRTL.MAC` if block boundaries
> change.
