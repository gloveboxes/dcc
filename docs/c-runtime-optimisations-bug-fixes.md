# DCC C Runtime — Optimisations & Bug Fixes

This document records the performance optimisations, runtime additions, and
correctness / fragmentation fixes applied to the DCC C runtime library. All
changes target the CP/M-80 / Z80 runtime and were validated to be **bit-exact**
against the established baseline.

## Scope & ground rules

- **Single source of truth:** every change was made in [DCCRTL.MAC](DCCRTL.MAC).
  `RTLMIN.MAC` is generated per-application by `dccrtlstrip` at link time to
  minimise the final `.COM` size and is **never** hand-edited.
- **Correctness is the gate:** an optimisation was only accepted if the full
  regression suite (`./runall.sh ntvcm`) produced output identical to
  [baseline_test_dcc.txt](baseline_test_dcc.txt) in **both** `peep` and
  `nopeep` modes. The only tolerated differences are the `__DATE__` / `__TIME__`
  wall-clock lines emitted by the `tstdc` test.
- **Calling convention:** standard functions use an `IX` frame (args at `IX+4`,
  `IX+6`, …), return values in `HL` (32-bit/float in `DE:HL`, `DE` high), and the
  caller cleans the stack.

## Result

The full `./runall.sh ntvcm` regression is **green** in both `peep` and `nopeep`
modes after all changes. The filtered diff against the baseline (excluding
`__DATE__` / `__TIME__`) is empty.

This branch has also been synced with the fork's updated `origin/main` through
commit `945036a` (`fix various integer promotion bugs. allow longer macros.
small floating point scale by power of two improvements`). That upstream sync
brings in the new `offsetof` support, `rand` / performance-chart updates, float
field-initialisation and float-scaling fixes, integer-promotion fixes, longer
macro replacement handling, and the upstream regression apps `toffset`,
`tc89fini`, `tmod3216`, `tpromo2`, and `tunaryp`. The local runtime additions
in this report remain present in the merged branch and the runner/baseline now
include both upstream and local regression coverage.

---

## Correctness fix

### `__lmu` (long unsigned modulo) — wrong result for interior zero bytes

**Severity: real bug, fixed.**

While reworking the long-division routines (optimisation #8), a genuine defect
was found in `__lmu`'s `lmodu_dvs16` path. The old code used `call nz` to skip
dividend bytes that were zero. That is incorrect for a zero byte that appears
**after** a non-zero byte: a zero byte still has to shift the running remainder,
so skipping it corrupts the result.

- **Symptom:** `16777217 % 7` returned **3** instead of the correct **2**.
- **Fix:** all four dividend bytes are now processed unconditionally via the
  shared `ldqr_byte` helper; the buggy `call nz` shortcut and the old
  `lmodu_byte` routine were removed.
- **Regression impact:** none — no baseline test relied on the incorrect value,
  and the full long-integer suite remained green after the fix.

---

## Implemented optimisations

### Integer division / printf

#### #1 — `printf` constant-time ÷10 (`pf_div10`)
The 16-bit decimal-conversion path previously called the general division
routine (`DRSU`) once per digit. It now uses a dedicated `pf_div10` that returns
quotient in `HL` and remainder in `A` while preserving `BC`/`DE`/`IX`.
**Why:** digit extraction is a hot inner loop; a purpose-built ÷10 is much
cheaper than the general divider and avoids disturbing the printf running state.

#### #2 — `printf` build-once digit emission (`pf_build_u`)
The old code counted digits in one pass and then printed them in a second pass
(`pf_count_u_digits` + `pf_print_u`). These were replaced by a single
`pf_build_u` that builds the digits once into a buffer, plus a `pf_emit_z`
helper. **Why:** halves the per-number work and removes duplicated logic.
The digit count is derived from the buffer pointer difference, **not** from a
register, so the printf character counter `BC` is never disturbed.

#### #3 — 32-bit `printf` register-based ÷10 (`pf_divu32_10`)
The 32-bit decimal path was rewritten to divide by 10 in registers
(`pf_divu32_10` / `pf_build_u32`), and the dead `__pf32_*` scratch variables
were removed. **Why:** same rationale as #1 extended to `long`; also reclaims
static RAM.

#### #4 — General divide: bounded hybrid (`DRSU`)
`DRSU` now caps repeated-subtraction at 40 iterations (via a `djnz`
down-counter) and falls back to the fixed 16-iteration shift/subtract divider
(`D16U`) beyond that. **Why:** the original kept a fast path for small
quotients (good for `e.c`, `pihex`) but had an **unbounded** worst case. The
hybrid keeps the fast path while guaranteeing a bounded cost.

### Multiply

#### #5 — `__mulu` small-operand fast path
If either operand's high byte is zero, the operands are swapped so the small one
is the multiplier, and an 8-iteration loop is used (`sla e` / `rl d` instead of
the `ex`/`add`/`ex` dance). **Why:** most real multiplies have at least one
small operand; this roughly halves the common case.

### Memory & string

#### #6 — `memmove` backward copy via `LDDR`
The overlapping backward-copy case in `__mmov` was changed from a hand-rolled
per-byte loop to a single `LDDR` block move. The top-level `n != 0` check
guarantees no 65536-byte wrap. **Why:** `LDDR` is the canonical, fastest Z80
block move. (Validated specifically by `trtl2`'s overlapping-`memmove` test.)

#### #12 — `strcspn` / `strspn`: remove the running counter
Both functions kept the match count in a static memory variable
(`__spn_count`), updating memory on every character. The count is simply
`current_s - initial_s`, so the variable was removed entirely and the result is
now computed with a single `sbc hl,de` at the end. **Why:** eliminates two
memory accesses per character and reclaims the static word. `strpbrk` was left
unchanged (it returns a pointer, not a count).

#### #14 — `atoi`: inline ×10
The accumulator update previously did `ld de,10` / `call __mulu` per digit. It
is now inlined as `add hl,hl` ×3 plus `add hl,de` (i.e. ×8 + ×2), then add the
digit. **Why:** removes a subroutine call (and the surrounding `push af`/`pop af`)
from the per-digit loop. The result is bit-identical modulo 2¹⁶, matching the
previous wrap-around behaviour.

### Heap

#### #11 — `malloc` free-block coalescing
The allocator coalesces adjacent free blocks on **every** free, via a shared
internal helper `__frcoal` (see #19; `_free` and both `realloc` free paths call
it). The accepted design deliberately keeps the `__mlh` malloc walk simple and
does the merge from a known freed block instead:

1. mark the current block free;
2. scan from `__hbase` to find the block immediately preceding the freed block;
3. if that predecessor is free, use it as the coalescing anchor;
4. merge forward while the immediately following block exists and is also free.

The forward merge has an explicit safety guard: it computes the next header and
does **not** read that header unless `next < __brk`. `next == __brk` means the
anchor is the tail block. This is the key invariant that prevents the allocator
from reading or merging beyond the valid heap.

**Why:** without coalescing, adjacent free blocks remain fragmented forever. In
the `tmalloch` scenario, freeing a 32768-byte block, allocating/freeing a small
block from its front, and then requesting another 32768 bytes fails despite the
memory being physically adjacent. Coalescing restores the original usable block
and lets the allocator reuse it.

**Validation:** the original `tmalloch` wrap-check regression now passes in both
`peep` and `nopeep`. A temporary probe confirmed the expected layout:
`p1 == q1 == p2 == q2` and `malloc(65000U) == 0`. A second temporary probe freed
adjacent blocks in reverse order and then successfully allocated their combined
payload from the predecessor address, confirming that predecessor coalescing is
working as well as forward coalescing. A standalone torture test,
[tallocx.c](tallocx.c), now covers exact split/no-split thresholds, forward and
reverse coalescing, bridge coalescing across three adjacent blocks, the
32768-byte fragmentation scenario, calloc zeroing on fresh and reused blocks,
realloc grow/shrink/null/zero behavior, realloc free-path coalescing (both the
grow/shrink old-block free and `realloc(ptr, 0)`), wrap rejection, and a
deterministic 420-step mixed malloc/calloc/realloc/free stress pattern.

#### #19 — `realloc` free paths coalesce (shared `__frcoal`)
**Severity: real fragmentation defect, fixed.**

`realloc` has two paths that release heap space: `realloc(ptr, 0)` frees the
block and returns `NULL`, and the normal grow/shrink path frees the *old* block
after copying into the newly allocated one (`rl_free_old`). Both paths used to
mark the block free by writing the flag byte directly, **without** coalescing.
Because the `__mlh` malloc walk never merges adjacent free blocks itself, a
realloc-freed block left next to an existing free neighbour could never be
reused — the heap fragmented and `malloc` was forced to extend `__brk` (or fail)
even though physically adjacent free space existed.

- **Symptom:** with A then B allocated adjacently, `free(a)` followed by
  `realloc(b, smaller)` reuses and splits A's block; the freed old-B block and
  the split-off tail fragment then stayed as two separate free blocks, so a
  following `malloc` that should have reused them instead extended the heap (a
  probe printed `FRAGMENTED (heap extended)`).
- **Fix:** the coalescing logic that previously lived inline in `_free` was
  extracted into a shared internal helper `__frcoal` (mark free + predecessor
  scan + forward merge, keeping the same `next < __brk` guard). `_free`,
  `realloc(ptr, 0)`, and `rl_free_old` now all `call __frcoal`, so **every** free
  path keeps the heap coalesced.
- **Bonus:** the free logic is no longer duplicated, so the runtime is marginally
  smaller; `dccrtlstrip` correctly keeps `__frcoal` whenever `_free` or `realloc`
  is referenced.
- **Validation:** the probe above flipped to `COALESCED (reused fragment)` in
  both `peep` and `nopeep` (the post-realloc `malloc` reused the fragment at the
  exact expected address). Two permanent regression tests were added to
  [tallocx.c](tallocx.c): `t_recoalesce` (a grow/shrink realloc fragment merges
  with the freed old block) and `t_rezero_coalesce` (`realloc(ptr, 0)` merges
  with an adjacent free block); both assert exact reuse addresses.

### C89 stdlib additions

#### #20 — `abs`, `labs`, `div`, and `ldiv` runtime implementations

The C89 integer helper functions declared by [stdlib.h](stdlib.h) now have
runtime implementations in [DCCRTL.MAC](DCCRTL.MAC):

- `_abs` implements 16-bit signed absolute value and returns the result in `HL`.
- `_labs` implements 32-bit signed absolute value and returns the result in
  `DE:HL`.
- `_div` fills the hidden struct-return destination for `div_t` by calling the
  existing signed 16-bit quotient/remainder helpers (`__divs` and `__mods`).
- `_ldiv` fills the hidden struct-return destination for `ldiv_t` by calling the
  existing signed 32-bit helpers (`__lds` and `__lms`).

**Why:** these functions were declared for source compatibility but previously
required each program to supply its own implementation. Adding them to the
runtime completes a small C89 stdlib gap while reusing the already-tested signed
division/modulo machinery. No [dcc.c](dcc.c) change was needed: the compiler
already supports the prototypes and the hidden struct-return ABI used for
`div_t` / `ldiv_t`. As with the rest of the runtime, `dccrtlstrip` keeps these
symbols only when referenced.

**Semantics:** signed division truncates toward zero and the remainder has the
same sign as the numerator, matching the existing `__divs`/`__mods` and
`__lds`/`__lms` behavior. `abs(INT_MIN)` and `labs(LONG_MIN)` remain the usual C
overflow cases; the two's-complement value is returned unchanged modulo the type
width.

**Validation:** a new standalone [tstdlib.c](tstdlib.c) regression covers
positive, negative, and zero cases for `abs`/`labs`, all sign combinations for
`div` and `ldiv`, and recomposition checks (`quot * denom + rem == numer`). It
was added to [runall.sh](runall.sh), [runall.bat](runall.bat), and
[baseline_test_dcc.txt](baseline_test_dcc.txt), and passes in both `peep` and
`nopeep` modes.

#### #21 — `scanf`, `sscanf`, and `fscanf` non-floating input parser

The `scanf` family is no longer a link-only stub. `_scanf`, `_sscanf`, and
`_fscanf` now share a small input-format engine in [DCCRTL.MAC](DCCRTL.MAC),
using the same IX-frame vararg convention as the printf engine. The supported
subset is intentionally practical rather than fully C89-complete:

- whitespace and literal matching;
- `%%`;
- assignment suppression with `*`;
- decimal field widths;
- `h` and `l` length modifiers for integer stores (`short` is the same size as
  `int` on dcc);
- `%d`, `%i`, `%u`, `%x`/`%X`, `%o`, `%c`, and `%s`.

The scanner uses a tiny source abstraction over strings (`sscanf`) and
FILE/stdin input (`fscanf`/`scanf`), plus a one-character pushback slot so a
numeric conversion can stop at the first non-matching character without losing
it for the next conversion.

**Not included:** floating input (`%f`/`%e`/`%g`), scansets, `%n`, and `%p`.
Those were deliberately left out to avoid pulling in a decimal-to-float parser
and the larger C89 edge-case surface.

**Validation:** a new standalone [tscanf.c](tscanf.c) regression covers
`sscanf` integer/string/character parsing, base autodetection, width-limited
fields, assignment suppression, literal matching, mismatch/EOF returns, and
pushback across adjacent conversions. It also writes and reopens a temporary
CP/M file to exercise `fscanf` through the FILE input path. The test was added
to [runall.sh](runall.sh), [runall.bat](runall.bat), and
[baseline_test_dcc.txt](baseline_test_dcc.txt), and passes in both `peep` and
`nopeep` modes.

### Register-allocation sweeps

#### #7 — `ld r,(ix+d)` direct loads
Across all the long-integer routines (`__lmul`, `__ldu`, `__lmu`, `__lds`,
`__lms` prologues and the eight comparisons `__ltu`/`__leu`/`__lgu`/`__lku`/
`__lts`/`__lks`/`__lgs`/`__les`), sequences of `ld a,(ix+d)` / `ld r,a` were
collapsed to a single `ld r,(ix+d)`. **Why:** `A` was only acting as a courier.
The carry flag that links the two `sbc hl,de` halves of each compare is
preserved because `ld` does not affect flags.

#### #13 — `__r1u` register loop counter
The 16-bit-by-8-bit modulo helper kept its bit counter in the alternate
accumulator (`ex af,af'` on every iteration). It now uses `ld d,16` with
`dec d` / `jr nz`. `D` was otherwise free (the divisor lives only in `E`), and
`push de`/`pop de` were added to honour the “preserve everything but `HL`”
contract. **Why:** removes two `ex af,af'` instructions per bit.

### Floating point

#### #8 — `__ldu` 16-bit-divisor fast path
`__ldu` (long unsigned divide) gained a fast path (`ldivu_dvs16`) for divisors
that fit in 16 bits, sharing a new `ldqr_byte` helper that builds each quotient
byte in place with `sla a` and forms the 16-bit remainder with `adc hl,hl` plus a
trial subtraction. **Why:** the common case of a small divisor no longer pays for
the full 32-bit division. (This is the same change during which the `__lmu` bug
above was found and fixed.)

#### #9 — `__fadd` exponent-alignment byte shift
When aligning two floats for addition, the mantissa right-shift (`FSRA`/`FSRB`)
is a **truncating** shift (the LSB is discarded; there is no guard/round bit).
Therefore eight single-bit shifts are exactly one whole-byte move down
(`AM0←AM1`, `AM1←AM2`, `AM2←0`). The alignment loop now performs whole-byte moves
while the shift count is ≥ 8, then bit-shifts the remainder. **Why:** a large
exponent difference (≥ 8) used to cost dozens of single-bit shifts; this is now a
couple of byte moves. Provably identical to the original truncating shift.

#### #17 — `sqrtf` early-exit on convergence
The Newton–Raphson loop ran a fixed iteration count. It now compares the new
iterate against the stored `g` (a 4-byte compare) and exits as soon as they are
bit-identical. **Why:** Newton's iteration here is a deterministic function, so
once two consecutive iterates match, every later iterate would match too — the
early exit is bit-identical to running the full count, but skips the remaining
(redundant) float divide/add/multiply work for inputs that converge early.

### Prologue cleanups

#### #10 — `ctype` IX-free leaf prologues
All twelve `ctype` leaf functions (`__ctu`, `__csp`, `__cdg`, `__caa`, `__can`,
`__cup`, `__clo`, `__cxd`, `__cpr`, `__cct`, `__cpu`, `__ctl`) no longer build an
`IX` frame. They read the argument directly:
- single-byte functions use `ld hl,2` / `add hl,sp` / `ld a,(hl)`;
- `toupper`/`tolower` (`__ctu`/`__ctl`), which also need the full `int` to return
  unchanged, additionally do `inc hl` / `ld h,(hl)` / `ld l,a`.

Every `pop ix` before `ret` was removed. **Why:** these are tiny leaf routines;
the `IX` frame setup/teardown was pure overhead.

---

## Considered but **not** applied

### Rejected variant — coalescing during the malloc walk
The first #11 attempt added a coalescing loop inside `__mlh`, merging adjacent
following free blocks while walking the heap for a first-fit allocation. That
variant was reverted because it corrupted the heap in the `tmalloch`
fragmentation test: after coalescing made a later `malloc(32768U)` succeed,
`malloc(65000U)` incorrectly returned a wrapped non-NULL pointer whose header sat
below the heap base.

The accepted solution moved coalescing to `_free`, where the runtime starts from
a known block header and can guard every forward read with `next < __brk`. That
preserves the existing `__mlh` extend/wrap checks and removes the failure mode
seen in the malloc-walk variant.

### #9 (partial) — `__fmul` product-width narrowing (unsafe)
Narrowing the multiply's intermediate product was rejected. The product is an
**exact 48-bit** value; low-order carries propagate up into the top 24 bits, so
truncating the working width would change the final truncated result and break
bit-exactness.

### #15 — `fread`/`fwrite` `size == 1` special case
Skipped. These paths are dominated by the BDOS read/write call; skipping the
`size * count` multiply and the `bytes / size` divide saves a negligible amount
while adding code.

### #16 — frame-pointer stub functions
Skipped on size grounds — the code-size cost outweighed the benefit.

### #18 — `rand`
Left as-is. The generator is already an efficient 16-bit xorshift; the `<< 7`
step cannot be simplified to `<< 8 >> 1` because that would drop bit 8.

---

## Validation

- Per-change spot checks were run with a helper (`qtest.sh`) that builds each app
  in both `peep` and `nopeep` modes, runs it under `ntvcm`, normalises CR/LF, and
  diffs against the corresponding block of [baseline_test_dcc.txt](baseline_test_dcc.txt).
- Representative apps exercised per area: `e`, `tmuldiv`, `tlong*`, `pihex`,
  `primes`, `sieve`, `nqueens`, `tc89f*` (float), `tphi`, `tpi`, `mm`, `tprintf`,
  `tsprintf`, `tfio`, `tstr`/`tstr2`/`tstr3`/`tstri2`, `tctype`, `tap`, `ttt`,
  `tchess`, `wumpus`, `tmalloch`, `tallocx`, `tstdlib`, `tscanf`, `toffset`,
  `tc89fini`, `tmod3216`, `tpromo2`, `tunaryp`.
- Two temporary allocator probes were also used for #11: one confirmed that the
  split 32768-byte block coalesced and still rejected `malloc(65000U)`, and one
  confirmed reverse-free-order coalescing by freeing A then B and allocating the
  combined A+B payload from A's address.
- A third temporary probe was used for #19: it confirmed that a realloc-split
  fragment plus the freed old block coalesce (the post-realloc `malloc` reused
  the fragment instead of extending the heap), flipping `FRAGMENTED` →
  `COALESCED` in both modes.
- The standalone [tallocx.c](tallocx.c) allocator torture test — including the
  new `t_recoalesce` and `t_rezero_coalesce` cases — passed in both `peep` and
  `nopeep` modes.
- The standalone [tstdlib.c](tstdlib.c) C89 stdlib test for `abs`, `labs`,
  `div`, and `ldiv` passed in both `peep` and `nopeep` modes.
- The standalone [tscanf.c](tscanf.c) stdio input test for `sscanf` and `fscanf`
  passed in both `peep` and `nopeep` modes.
- Final gate: `./runall.sh ntvcm` — **green** in both modes (only `__DATE__` /
  `__TIME__` differ, as expected).
