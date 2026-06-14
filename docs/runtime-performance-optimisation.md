# DCC C Runtime — Performance Optimisation Pass

This document records a performance-focused pass over the DCC CP/M-80 / Z80 C
runtime. The goal was to reduce runtime line count and improve execution speed
by making fuller use of the Z80 instruction set, while keeping the runtime
**bit-exact** against the established regression baseline. It is a companion to
[c-runtime-optimisations-bug-fixes.md](c-runtime-optimisations-bug-fixes.md) and
[runtime-memory-optimisations.md](runtime-memory-optimisations.md), and follows
the same ground rules.

## Scope & ground rules

- **Single source of truth:** every runtime change was made in
  [DCCRTL.MAC](DCCRTL.MAC). `RTLMIN.MAC` is generated per-application by
  `dccrtlstrip` at link time and is **never** hand-edited. One build fix in this
  pass (§10) additionally touches the `dccrtlstrip` host tool itself — never the
  generated `RTLMIN.MAC`.
- **Correctness is the gate:** an optimisation was only accepted if the full
  regression suite (`bash ./runall.sh ntvcm`) produced output identical to
  [baseline_test_dcc.txt](baseline_test_dcc.txt) in **both** `peep` and `nopeep`
  modes. The only tolerated differences are the `__DATE__` / `__TIME__`
  wall-clock lines emitted by the `tstdc` test.
- **Calling convention:** standard functions use an `IX` frame (args at `IX+4`,
  `IX+6`, …), return values in `HL` (`DE:HL` for 32-bit/float, `DE` high), and
  the caller cleans the stack.
- **Performance metric:** the `ntvcm` emulator's `-p` flag prints an exact,
  deterministic `Z80 cycles:` count. All cycle figures below are measured this
  way, so before/after deltas are precise rather than estimated.

## Result

The full `bash ./runall.sh ntvcm` regression is **green** in both `peep` and
`nopeep` modes after all four changes; the filtered diff against the baseline
(excluding `__DATE__` / `__TIME__`) is empty.

| Benchmark | Before | After | Change |
| --- | ---: | ---: | ---: |
| `tpi` (1000-digit π spigot) | 308,276,707 cycles | **82,832,821 cycles** | **−73.1%** (≈3.7× faster) |
| `sieve` (integer-only) | 19,565,267 cycles | 19,565,267 cycles | unchanged (no regression) |

[DCCRTL.MAC](DCCRTL.MAC) currently stands at ~11,092 lines versus ~11,237 at
`HEAD` before this optimisation sequence (≈145 lines net smaller). The first
four arithmetic/comparison changes took it to ~11,068 lines; the follow-up I/O
and test-coverage pass deliberately spent some of that saving to remove record
copy overhead in the file runtime.

The `tpi` benchmark is dominated by 32-bit signed multiply, divide, and modulo,
so it benefits from optimisations #1 and #2 below. The `sieve` benchmark uses no
long arithmetic and is unchanged, confirming the integer-only paths were not
disturbed.

### Follow-up runtime pass

A second pass implemented the next set of runtime hotspots identified after the
first review. These changes focus on file I/O, `memchr`, `qsort`, and `sqrtf`.

| Benchmark | Before follow-up | After follow-up | Change |
| --- | ---: | ---: | ---: |
| `tc89fmat` (`sqrtf` coverage) | 209,892 cycles | **162,057 cycles** | **-22.8%** |
| `fileops` | 10,504,674 cycles | **10,161,946 cycles** | **-3.3%** |
| `trw` | 550,846,369 cycles | **545,891,945 cycles** | **-0.9%** |
| `trw2` | 4,136,424,426 cycles | **4,083,478,634 cycles** | **-1.3%** |
| `tqsort` | 42,678,495 cycles | **42,603,050 cycles** | **-0.2%** |
| `tpi` | 82,832,821 cycles | 82,827,431 cycles | essentially unchanged |
| `sieve` | 19,565,267 cycles | 19,565,267 cycles | unchanged |

`tstring` now includes additional silent `memchr` coverage, so its total cycle
count is not directly comparable to the earlier measurement.

---

## 1. `__lmul` — register-based 32-bit multiply

**File:** [DCCRTL.MAC](DCCRTL.MAC) · **Impact:** `tpi` 308.3M → 255.1M cycles (−17.2%)

The old `__lmul` held both operands and the result in static memory
(`__lm_lhs_*`, `__lm_rhs_*`, `__lm_res_*`) and ran a 32-iteration shift-add
loop. Each iteration performed ~12 memory accesses plus a redundant
`push bc` / `pop bc` even though the loop body never touched `BC` — roughly
200 T-states of overhead per iteration.

The rewrite computes the 32×32→32 (mod 2³²) product from partial products:

$$X \cdot Y \bmod 2^{32} = X_{lo} Y_{lo} + \big((X_{lo} Y_{hi} + X_{hi} Y_{lo}) \bmod 2^{16}\big) \ll 16$$

- The base term $X_{lo} Y_{lo}$ uses a **register-only** 16×16→32 shift-add: a
  32-bit accumulator in `HL:DE`, the multiplier consumed LSB-first, the
  multiplicand constant in `BC`, 16 fixed iterations, and a 33-bit right shift
  (`rr h` / `rr l` / `rr d` / `rr e`) that captures the add carry.
- The two cross terms reuse the already-proven `__mulu` (16×16→16) helper.

Only the high bits that survive `mod 2³²` are formed, so the upper cross-term
products are never computed. Operands are loaded once; the inner loop is free of
memory traffic.

---

## 2. Signed long divide / modulo — 16-bit-divisor fast path

**File:** [DCCRTL.MAC](DCCRTL.MAC) · **Impact:** `tpi` 255.1M → 82,832,821 cycles (≈3× on top of #1)

`__lds` (signed `long` divide) and `__lms` (signed `long` modulo) always ran the
slow 32-iteration shifting loop over scratch memory, even though their unsigned
counterparts `__ldu` / `__lmu` already had a compact, register-based fast path
(`ldqr_byte`) for divisors that fit in 16 bits. In `tpi` the expressions `d / b`
and `d % b` are signed `long` operations with small divisors, so they were
needlessly paying for the full 32-bit loop.

Both signed routines already normalise their operands to non-negative magnitudes
before dividing. After that normalisation, a dispatch was added:

```asm
        ld      hl,(__ldv_dvs_h)   ; divisor magnitude high word
        ld      a,h
        or      l
        jp      z,ldivs_dvs16      ; fits in 16 bits -> byte-wise fast path
```

- `ldivs_dvs16` builds the 32-bit quotient MSB-first via the shared `ldqr_byte`
  step, writing the quotient bytes into `__ldv_quo_*`, then falls into the
  common `ldivs_apply_sign` tail.
- `lmods_dvs16` runs the same byte-wise loop but keeps the remainder in `HL`
  (the quotient bytes are discarded), zeroes the remainder high word, then joins
  `lmods_apply_sign`.

This reuses the **already-verified** `ldqr_byte` helper — no new division logic
was introduced — and the sign-application tails are shared rather than
duplicated. Negative and mixed-sign cases were confirmed correct
(e.g. `-37000 / 14 = -2642`, `280000 / -4 = -70000`), and the `tpi` output
matches the baseline π digits exactly.

---

## 3. Long comparisons — redundant `ld sp,ix` removed

**File:** [DCCRTL.MAC](DCCRTL.MAC) · **Impact:** −16 bytes, −80 T-states across the call sites

The eight 32-bit comparison routines — `__ltu`, `__leu`, `__lgu`, `__lku`
(unsigned) and `__lts`, `__lks`, `__lgs`, `__les` (signed) — ended their
epilogue with:

```asm
        ld      sp,ix
        pop     ix
        ret
```

`ld sp,ix` restores the stack pointer in case a function allocated locals or
pushed during its body. None of these comparison routines do either — their only
`push` is the prologue `push ix`, so `SP` already equals `IX` at the epilogue.
The `ld sp,ix` was therefore a no-op. Removing it from all eight functions saves
2 bytes and 10 T-states per routine (16 bytes / 80 T-states total) with no
behavioural change.

---

## 4. Float comparisons — shared ordered comparator

**File:** [DCCRTL.MAC](DCCRTL.MAC) · **Impact:** ≈130 fewer lines, no hot-path cost

The four ordered IEEE-754 single-precision comparisons — `__flt`, `__fgt`,
`__fle`, `__fge` — each carried a hot "both operands positive, non-NaN" fast
path (a direct raw-byte comparison) followed by a cold slow path of ~50 nearly
identical lines handling NaN, ±0, mixed signs, and same-sign magnitude ordering.
The slow paths differed only in how they mapped the final ordering to a boolean.

The cold paths were consolidated into a single shared classifier, `__fcmps`,
that compares the two operands once and returns a three-way result in `A`:

| `A` | meaning |
| :-: | --- |
| 0 | unordered (either operand is NaN) |
| 1 | `a < b` |
| 2 | `a == b` (including `+0 == -0`) |
| 3 | `a > b` |

Each ordered comparison now keeps its hot fast path **unchanged** and, on the
cold path, simply calls `__fcmps` and maps the result to its boolean:

- `__flt` → true iff `A == 1`
- `__fgt` → true iff `A == 3`
- `__fle` → true iff `A` is `1` or `2`
- `__fge` → true iff `A` is `2` or `3`

The old boolean raw-compare helpers `__frlt` / `__frgt` were replaced by a single
three-way raw magnitude compare, `__frcmp`, which `__fcmps` uses for the
same-sign case (reversing the sense for two negatives). Because the shared core
runs only on the cold path, there is **no hot-path performance cost** — the win
is purely in code size and maintainability.

### Verification

Beyond the standard regression, the dedup was checked against a temporary
edge-case test covering `NaN`, `+Inf`, `−Inf`, `±0`, and mixed-sign operands
across all four operators (50+ cases, all correct). That scratch test was removed
after it passed; the permanent coverage in `tc89fcmp` and the `tc89flt*` family
continues to guard these paths.

---

## 5. File I/O — direct DMA for full aligned records

**File:** [DCCRTL.MAC](DCCRTL.MAC) · **Impact:** `trw2` -1.3%, `fileops` -3.3%

CP/M file I/O works in 128-byte records. The runtime still has to preserve the
existing byte-oriented `read`/`write` API, so partial records must be merged
through the shared `__dma` buffer. But when the current file offset is
record-aligned and the requested chunk is a full 128 bytes, the old path still
performed an unnecessary bounce copy:

1. Set BDOS DMA to `__dma`.
2. Read or write the CP/M record.
3. Copy 128 bytes between `__dma` and the caller buffer with `LDIR`.

The follow-up pass adds direct full-record paths in `_read` and `_write`:

- `_read` sets BDOS DMA directly to `__rwbuf` and advances the file state with
  `__advio`, leaving the existing read-sector cache untouched because `__dma`
  was not overwritten.
- `_write` sets BDOS DMA directly to `__rwbuf` for full aligned records. Partial
  writes continue to read/merge/write through `__dma`, preserving the old CP/M
  record semantics.

The direct path avoids 128-byte `LDIR` copies for aligned bulk I/O. It was
validated with `trw`, `trw2`, and `fileops` in both `peep` and `nopeep` modes.

---

## 6. `memchr` — Z80 `CPIR` scan

**File:** [DCCRTL.MAC](DCCRTL.MAC) · **Impact:** shorter counted scan; new coverage in `tstring`

`__mchr` used a scalar counted loop: test `BC`, load one byte, compare, advance,
decrement, branch. Z80 `CPIR` performs exactly the same counted byte search in a
single block instruction, so `__mchr` now guards `n == 0`, loads the target byte
into `A`, runs `CPIR`, and returns `HL - 1` on match.

`tstring` was expanded with silent `memchr` checks for zero-length search,
successful middle-of-buffer hits, bounded misses, and absent bytes. The test
still prints the same success line, so the regression baseline does not change.

---

## 7. `qsort` — stackless byte swap

**File:** [DCCRTL.MAC](DCCRTL.MAC) · **Impact:** `tqsort` 42,678,495 -> 42,603,050 cycles

The `_qsort` element-swap loop previously used `push af` / `pop af` to hold one
byte while swapping two memory locations. The loop now uses the Z80 alternate
accumulator (`ex af,af'`) as the temporary. This removes stack traffic from each
swapped byte without changing the generic arbitrary-size element handling.

The win is deliberately small but low-risk: `tqsort` improved by about 75k
cycles, and the comprehensive qsort regression still passes in both modes.

---

## 8. `sqrtf` — scale by exponent instead of multiplying by `0.5f`

**File:** [DCCRTL.MAC](DCCRTL.MAC) · **Impact:** `tc89fmat` 209,892 -> 162,057 cycles (-22.8%)

Each Newton iteration in `_sqrtf` computes:

```c
g = (g + x / g) * 0.5f;
```

The old assembly implemented the final `* 0.5f` as a full floating-point
multiply by the constant `0x3f000000`. The runtime already has
`__fscale_pow2`, which adjusts the IEEE exponent directly. `_sqrtf` now calls
that helper with `B = 0xff` (signed -1), making the scaling exact and avoiding a
full `__fmul` per Newton step.

The correctness surface is small because the loop operates on positive finite
values; `_sqrtf` still returns zero for non-positive inputs before the iteration
starts. `tc89fmat` and `ttrig` cover the path in the full suite.

---

## Review fixes

The two items below are **correctness / build fixes**, not performance changes.
They were found while reviewing the file-I/O work above: §9 is a hazard the
direct-DMA optimisation (#5) introduced, and §10 is a latent build defect (it
reproduces at `HEAD`) that surfaced as soon as a `perror`-using test was built.

## 9. `readdir` — restore the directory DMA before search-next

**File:** [DCCRTL.MAC](DCCRTL.MAC) · **Impact:** correctness; prevents caller-buffer corruption

Optimisation #5 changed an invariant. Previously every `read`/`write` left the
BDOS DMA address pointing at the internal `__dma` buffer. With the direct-DMA
fast path, a full aligned `read`/`write` now leaves the DMA pointing at the
**caller's** buffer.

`readdir` (`_drd`) issued its BDOS *search-next* (function 18) **without** setting
the DMA, relying on it still pointing at `__drdma` from `opendir`. That was
already fragile, but after #5 a `read`/`write` interleaved between `readdir`
calls makes search-next write a 32-byte directory entry straight into the
caller's buffer — silent memory corruption.

The fix makes `_drd` self-contained: it sets the DMA to `__drdma` (function 26)
before each search-next.

```asm
        ; A read()/write() since opendir may have repointed the BDOS DMA at a
        ; caller buffer (the direct-DMA file I/O path does this), so search-next
        ; would fill that buffer instead of __drdma.  Restore our directory DMA.
        ld      c,26
        ld      de,__drdma
        call    5
        ld      c,18            ; search next
        ld      de,__drfcb
        call    5
```

The common `opendir` → `readdir` loop with no intervening file I/O is unaffected.
`tdirent` exercises this path and prints its unchanged success line.

---

## 10. `perror` / `_errno` — keep the symbol's data when it is stripped

**File:** [DCCRTL.MAC](DCCRTL.MAC), [dccrtlstrip.c](dccrtlstrip.c) · **Impact:** fixes an `U _errno` link failure for `perror` users

Building `tdirent` (which calls `perror("opendir")`) failed at link time:

```
U 004232' 052 000000   ld hl,(_errno)   ... 1 Fatal error(s)
```

This is **pre-existing** — it reproduces identically against the committed
`HEAD` runtime — and has two independent root causes, both fixed:

**(a) Runtime layout.** `dccrtlstrip` splits the runtime into blocks at `public`
boundaries. The `public _errno` directive sat in the file-ops declaration run
(next to `_open`/`_read`/`_write`), but the actual data definition
`_errno: dw 0` lived far away inside the errno-*setter* block. `perror` reads
`errno` but does not pull in the setter block, so the `_errno` storage was
stripped while the reference to it survived. `_errno` now has its **own**
standalone `public` + label + `dw` block (the same self-contained pattern used by
`_stdin` / `_stdout` / `_stderr`), so its declaration and data are kept or
dropped together.

**(b) Stripper reference scan.** The tool's `ld` operand parser recognised
`ld hl,sym` but not `ld hl,(sym)` — it never skipped the leading parenthesis on
a memory-load source operand, so `perror`'s internal `ld hl,(_errno)` was not
followed during reachability marking. The parser now skips a leading `(`:

```c
        if (q) {
            q++;
            q = skipws(q);
            /* ld hl,(sym) / ld a,(sym) loads from memory at sym: the symbol
             * is a reference even though it is wrapped in parentheses. */
            if (*q == '(')
                q++;
            if (parse_ident_token(&q, sym))
                add_root(sym);
        }
```

Either fix alone resolves the immediate `tdirent` failure, but both were applied:
(a) keeps the data physically with its declaration, and (b) makes the stripper
correctly follow `ld r,(sym)` references in general, preventing the same silent
stripping for any other memory-referenced data symbol. `pint` only built before
because its own application code happened to mention `_errno` directly; with (b)
the runtime-internal reference is honoured regardless. After rebuilding the
`dccrtlstrip` host tool, `tdirent` and `pint` both build clean in `peep` and
`nopeep`, and the full regression is unchanged.

---

## Validation protocol

Every change was validated with the same gate:

1. Build the affected smoke tests and confirm correct output
   (`tmuldiv`, `tlong`, `tlongsub`, `tmod3216` for long arithmetic;
  `tc89fcmp`, `tc89flt` for float comparisons; `tstring`, `tqsort`,
  `tc89fmat`, `trw`, `trw2`, and `fileops` for the follow-up pass;
  `tdirent` and `pint` for the review fixes in §9–§10).
2. Measure `ntvcm -p` cycle counts before and after on the relevant benchmarks.
3. Run the full regression in both modes:
   `bash ./runall.sh ntvcm` → diff against `baseline_test_dcc.txt` must be empty
   except the `tstdc` `__DATE__` / `__TIME__` lines.

All optimisations passed this gate.
