# DCC C Runtime — Memory Leak & Optimisation Pass

This document records a focused pass over the DCC CP/M-80 / Z80 C runtime that
hunted for memory leaks, lifecycle defects, and optimisation opportunities. It
is a companion to [c-runtime-optimisations-bug-fixes.md](c-runtime-optimisations-bug-fixes.md)
and follows the same ground rules.

## Scope & ground rules

- **Single source of truth:** every runtime change is in [DCCRTL.MAC](DCCRTL.MAC).
  `RTLMIN.MAC` is generated per-application by `dccrtlstrip` at link time and is
  never hand-edited.
- **Correctness is the gate:** the full regression suite (`bash ./runall.sh ntvcm`)
  must match [baseline_test_dcc.txt](baseline_test_dcc.txt) in **both** `peep`
  and `nopeep` modes, except for the `__DATE__` / `__TIME__` wall-clock lines
  emitted by `tstdc`.
- **Calling convention:** standard functions use an `IX` frame (args at `IX+4`,
  `IX+6`, …), return values in `HL` (`DE:HL` for 32-bit/float), caller cleans
  the stack.
- **Heap block layout:** first-fit allocator, 3-byte header
  (`HDRSIZE = 3`): bytes 0-1 = user size (unsigned 16-bit), byte 2 = flags
  (bit 0 set ⇒ free). `__hbase`/`__brk`/`__hlimit` bound the heap.

## Summary of findings

The heap allocator (first-fit + coalesce-on-free) and the hot string/memory
primitives were already in good shape from the previous pass. This pass found
**no leaked heap blocks and no leaked file-descriptor slots.** The actionable
items were a non-functional `ferror`/`clearerr`, an unbounded re-read cost in
`fgets`, a `realloc` that always doubled peak heap, a heap high-water mark that
never fell, a couple of robustness gaps (`size_t` multiply overflow and a
`size == 0` divide-by-zero in the `fread`/`fwrite` item count), a malformed
`rewind` call frame, and a sub-standard concurrent-open-files limit.

---

## Correctness / lifecycle fixes

### 1. `clearerr` was a no-op; `feof` could not be cleared

`clearerr` previously returned immediately with a comment claiming the RTL kept
no per-stream flags — but the runtime *does* keep a per-slot `__fdeof` array
(set by `fread`/`fgets`) and a `__fderr` array. After end-of-file only
`rewind`/`fseek` could clear the EOF indicator, so the standard idiom
`clearerr(fp)` did nothing.

`_clearerr` now resolves the stream's slot (ignoring the `0/1/2` console
pseudo-streams) and clears both `__fdeof[slot]` and `__fderr[slot]`.

### 2. `ferror` always returned 0

`__fderr` was only ever *cleared* (in `fopen`) and *read* (in `ferror`); nothing
ever set it, so `ferror` could never report a problem. The write path now records
a stream error: when a CP/M random-record write fails (`wrnospc`, e.g. disk
full), `_write` sets `__fderr[slot]` before setting `errno`. `__slot` is known
valid at that point because the write already passed `__chkfd`.

Read-side EOF is intentionally **not** treated as an error (CP/M conflates "read
past end" with several non-error directory codes), so `ferror` reflects genuine
write failures rather than ordinary end-of-file.

### 2a. `rewind` never moved the file position

`rewind` is defined as `(void)fseek(fp, 0L, SEEK_SET)` plus clearing the error
and EOF indicators. `_rewind` built the call to the internal
`long lseek(int fd, long offset, int whence)` by pushing the `offset` argument
as a **single** word instead of the two words a 32-bit `long` occupies. That
misaligned the stack frame, so `_lseek` read its `whence` from the wrong slot
(effectively a garbage value), failed the `0/1/2` dispatch, and returned
`EINVAL` **without changing `__fdoff`**. Because `rewind` is `void`, the failure
was swallowed: the EOF flag was cleared but the file position never moved.

The symptom was subtle: after `fgets` read the first line, `rewind` followed by
`fread` returned `0x1A` (Ctrl-Z) padding — the data physically sitting at the
*unchanged* mid-file position inside the record — rather than the start of the
file. `fseek(fp, 0L, SEEK_SET)` worked correctly the whole time, so the two
standard-equivalent idioms behaved differently.

`_rewind` now pushes the offset as two zero words and cleans 8 bytes, exactly
matching `_fseek`'s correct frame. `rewind` then truly seeks to 0. (The earlier
build of this branch noted this as a pre-existing out-of-scope issue; it is now
fixed here.)

### 3. `calloc` / `fread` / `fwrite` size-product overflow (CWE-190)

`calloc(nmemb, size)`, `fread`, and `fwrite` computed `nmemb * size` with the
16-bit `__mulu` and no overflow check, so e.g. `calloc(4096, 16)` wrapped to a
0-byte allocation that the caller would then overrun.

A new shared helper `__ckmul` multiplies and verifies the result
(`product / b == a`, reusing the tested `__mulu` / `__divu`), returning carry set
on overflow. `calloc` now returns `NULL` on overflow; `fread`/`fwrite` return 0
items. The helper is published as its own block so `dccrtlstrip` keeps it only
when one of those three functions is linked.

### 4. `fread` / `fwrite` with `size == 0` returned 65535

Both functions ended by dividing the byte count by `size` to produce an item
count. With `size == 0` that divided by zero; `DRSU` returns `0xFFFF` for
division by zero, so `fread`/`fwrite` reported 65535 items. They now early-return
0 when `size == 0` (this also covers the `count == 0` case via the product).

---

## Performance / footprint optimisations

### 5. `fgets` issued one full-sector BDOS read per character (read sector cache)

`fgets` on a file reads one byte at a time via `_read(fp, &ch, 1)`. Each
`_read` of a single byte issued a BDOS random-record read (function 33) that
pulls the **whole 128-byte sector** into the shared `__dma` buffer and then
copies one byte out. Reading an *N*-character line therefore performed ~*N*
physical sector reads of the **same** record.

`_read` now keeps a one-record cache describing which `(slot, record)` currently
resides in `__dma`:

- `__rdcv` — valid flag; `__rdck` — key (slot byte + the FCB's 3 random-record
  bytes `r0,r1,r2`).
- In the read loop, after `__setrr` sets up the record, `__rdhit` compares the
  request against the cache. On a hit it **skips both the set-DMA and the BDOS
  read** and copies straight out of `__dma`. On a miss it performs the read and
  `__rdsto` records the new key.
- **Miss-path invalidation (correctness invariant):** a BDOS random read
  overwrites `__dma` *even when it fails* (EOF / error returning a non-zero
  code). On a miss the cache is therefore invalidated **before** the read and
  only re-validated by `__rdsto` **after** a successful read. Without this, a
  failed end-of-file read clobbers `__dma` while the cache still claims the
  previous record, so a later read of that record would falsely hit on garbage
  (observed as `0x1A` fill). This was caught by `fileops` (the
  `offset 8192 isn't a j` check) during the regression and fixed.
- The cache is also invalidated whenever `__dma` or the underlying data may
  change: on `open` (a new file may reuse a slot number), on `close`, and at the
  start of every file `write` (which reuses `__dma` as scratch). `lseek`/`fseek`
  deliberately do **not** invalidate it — seeking within the same record keeps
  the buffer valid, which is the whole point.

For the common `fgets` loop this turns ~128 sector reads per sector into one.
`fread`/`fwrite` of larger buffers are unaffected (each record is still read
once). Validated by `fileops` (extensive read/write/seek), `tfio`, `tdirent`,
and the new `tioerr`.

### 6. `realloc` always allocated-new + copied + freed-old

`realloc` unconditionally allocated a fresh block, copied, and freed the old one
— even to shrink a block or to grow the most-recently-allocated buffer. The
common "grow my buffer" loop therefore copied every time and pushed `__brk` to
roughly twice the final size. Two in-place fast paths were added before the
allocate-new fallback:

- **Shrink / same size (`new <= old`):** keep the block in place. If the slack
  is at least `HDRSIZE+1`, split a free tail and coalesce it forward (via
  `__frcoal`); otherwise keep the whole block. No allocation, no copy, same
  pointer returned.
- **Grow at the heap top (`new > old` and block end `== __brk`):** if the larger
  end still fits below `__hlimit`, extend the block and bump `__brk` in place.
  No copy, same pointer returned.

Any other grow (block not at the top) falls through to the original
allocate-new + copy + free-old path, which still coalesces the freed old block.

### 7. Heap high-water mark never fell (`__brk` trim)

Freeing the top-most block marked it free and coalesced it, but `__brk` never
dropped, so the heap ceiling only ever rose. A program that allocated then freed
a large temporary buffer kept the heap inflated, squeezing the heap↔stack gap
for the rest of its run.

`__frcoal` now trims: after coalescing, if the resulting free block ends exactly
at `__brk`, it is the top block, so `__brk` is lowered to that block's header.
The space returns to the heap/stack gap instead of being stranded as a top free
block. Because every free path (`free`, `realloc(ptr,0)`, the realloc free-old
path, and the new shrink-tail path) routes through `__frcoal`, the trim applies
uniformly.

A useful side effect: a test (or program) that frees all of its allocations
returns `__brk` to the heap base, because coalescing plus trim cascades down.

### 8. Concurrent open-files limit raised from 4 to 8 (C89 `FOPEN_MAX`)

The file layer allowed only **4** real files open at once (`NFILES = 4`, fd
`3..6`). C89 §7.9.3 requires `FOPEN_MAX` — the minimum number of streams
guaranteed open simultaneously — to be **at least 8, including the three
standard streams** (`stdin`/`stdout`/`stderr`). In this runtime those three are
console pseudo-FILEs (fd `0/1/2`) that do not occupy a real-file slot, so a
4-slot table left the library short of the standard's intent and was tight for
anything opening several files at once (merge/diff/temp-file workloads).

`NFILES` is now **8** (fd `3..10`). The change is almost entirely mechanical
because the file code already referenced the symbolic constants:

- `NFILES equ 8`, and `MAXFD` now derives as `FIRSTFD+NFILES` so the valid-fd
  range can never drift out of sync with the table size again.
- The per-slot state arrays are now sized from the constants rather than literal
  byte counts: `__fduse`/`__fdeof`/`__fderr` are `ds NFILES`, `__fdoff` is
  `ds NFILES*4` (a 32-bit position per slot), and `__fcbs` is
  `ds NFILES*FCBSIZE` (one 36-byte FCB per slot).
- The single shared 128-byte `__dma` buffer and the read sector cache are
  unchanged — the cache keys on the slot number, so it keeps working across the
  larger table.

The added cost is ~`NFILES_delta * (FCBSIZE + 7)` bytes of BSS — about **172
bytes** for the 4→8 bump — which is negligible in a CP/M TPA. A matching
`#define FOPEN_MAX 8` was added to [stdio.h](stdio.h) (it was previously absent,
so the library did not advertise the standard macro at all); a comment ties it
to `NFILES`. The C89 minimum applies to *streams including* `stdin`/`stdout`/
`stderr`, and since those are slot-free here, 8 real-file slots comfortably
meets it.

### 9. `qsort`, `bsearch`, and `atol` moved into the runtime

`stdlib.h` declared `qsort`, `bsearch`, and `atol`, but only sample `.c`
implementations existed — every program that used them had to supply its own or
`#include` the sample. They are now first-class runtime routines in
[DCCRTL.MAC](DCCRTL.MAC), each in its own public block so `dccrtlstrip` keeps
them only when referenced (verified: a program that does not call them links
none of the three).

- **`atol`** mirrors the existing `atoi` (skip spaces/tabs, optional `+`/`-`
  sign, decimal digits) but accumulates a 32-bit `long` returned in `DE:HL`;
  overflow wraps modulo 2^32, matching `atoi`'s defined 2^16 wrap. Adding it
  surfaced a small **C89 conformance bug in `atoi`**: it skipped a leading `-`
  but not a leading `+`, so `atoi("+456")` returned 0. Since C89 defines `atoi`
  as `(int)strtol(s, NULL, 10)` (which accepts either sign), `atoi` was fixed to
  skip `+` as well, keeping the two functions consistent. No existing caller
  passes a leading `+`, so no baseline output changed.
- **`bsearch`** is a size-decrementing binary search (no signed index
  arithmetic, so it is correct across the full unsigned `size_t` range). It
  calls the comparator through the runtime indirect-call helper `__call_hl`
  using the standard dcc convention — arguments pushed right-to-left, result in
  `HL`, caller cleans the stack — and returns the matching element pointer or
  `NULL`.
- **`qsort`** is an in-place, **non-recursive Shell sort** (gap sequence
  `num/2, /2, …`). Iterative-by-construction means no C-stack growth on
  adversarial input — important on the small CP/M stack, and the reason a
  recursive quicksort was avoided — and it needs no auxiliary heap. Elements of
  arbitrary `size` are exchanged with a byte-wise swap, so any element width
  works. It is **not** a stable sort.

Both `qsort` and `bsearch` hold their working state in static scratch because
the comparator may clobber every register; a consequence is that they are not
re-entrant from within their own comparator, which is acceptable for this
runtime. No [stdlib.h](stdlib.h) change was needed — the prototypes were already
present — and no [dcc.c](dcc.c) change was needed.

---

## Test coverage added

### `tallocx.c` (allocator torture test) — new cases

- `t_shrink_inplace` — `realloc` shrink returns the same pointer and preserves
  contents (no allocate/copy).
- `t_grow_top` — `realloc` growing the heap-top block grows in place (same
  pointer), and the enlarged block is fully usable.
- `t_trim` — freeing the top block lowers `__brk`: a later *larger* request
  reuses the same base address instead of growing the heap above a stranded
  free block (this fails without the trim).
- `t_calloc_overflow` — `calloc` products that overflow 16 bits (`4096*16`,
  `700*100`) return `NULL`, while a large non-overflowing product still succeeds
  and is zeroed.

Two existing cases were made trim/fast-path aware (they previously pinned
addresses that assumed a freed top block lingered for split/no-split reuse):

- `t_nosplit` now keeps a guard block above the block under test so freeing it
  does not trim, and verifies no-split by reusing the full block rather than by
  the address of the next allocation.
- `t_recoalesce` now keeps a guard block above `B` so `realloc(B, …)` takes the
  allocate-new path (instead of growing in place), preserving its coverage of
  the realloc free-old coalescing fixed in the previous pass.

`tallocx` prints only a pass/fail line, so these additions do not change its
baseline output.

### `tioerr.c` (new stdio edge-case test)

Covers the stdio fixes deterministically: `fread` with `size == 0` and
`count == 0` return 0; `fgets` file reads (which drive the read sector cache);
`feof` after end-of-file; `ferror` reporting a healthy stream as not-in-error;
`clearerr` resetting the EOF indicator; `rewind` actually seeking back to offset
0 (re-reading the first bytes rather than stale mid-file data); and `fwrite`
with `size == 0` returning 0. Added to [runall.sh](runall.sh),
[runall.bat](runall.bat), and [baseline_test_dcc.txt](baseline_test_dcc.txt).

### `terrno.c` — updated for the new open-files limit

`terrno` verifies the `EMFILE` ("too many open files") path. It previously
opened 4 files and expected the 5th to fail; it now opens **8** files (filling
`NFILES`) and expects the 9th to fail with `EMFILE`. The rewrite uses a name
table and a loop rather than per-fd locals so it tracks `NFILES` without further
edits. The visible baseline line (`PASS open too many errno=24 …`) is unchanged.

### `tqsort.c` (new `qsort` unit test)

Covers the `qsort` runtime routine (fix 9), checked against an independent
insertion-sort oracle over 40 random `int` arrays of varying length (including
`n` = 0, 1, 2), plus already-sorted, reverse-sorted, and all-equal inputs, a
descending comparator, and four element widths — `int`, a 6-byte `struct`, an
odd 5-byte record, single bytes, and `char *` pointers — to exercise the
byte-wise element swap and the function-pointer comparator path. The struct
case also verifies (via a tag field) that whole elements move and none are lost
or duplicated. Added to [runall.sh](runall.sh), [runall.bat](runall.bat), and
[baseline_test_dcc.txt](baseline_test_dcc.txt); prints only a single pass/fail
line.

### `tbsearch.c` (new `bsearch` unit test)

Covers the `bsearch` runtime routine (fix 9), probed over a sorted `int` array
for every present key, every absent gap key, below-min/above-max sentinels, and
the first/last elements, plus the empty, single-element, and two-element edges,
and a wide-element (`struct`) search. The comparator is exercised through the
runtime's indirect-call path. Added to [runall.sh](runall.sh),
[runall.bat](runall.bat), and [baseline_test_dcc.txt](baseline_test_dcc.txt);
prints only a single pass/fail line.

`atol` (and `atoi`, including the `+`-sign fix) are covered by
[tstdlib.c](tstdlib.c) alongside the other `<stdlib.h>` conversions: both are
checked for sign handling, leading whitespace, trailing junk, no-digit/empty
input, and range values up to `LONG_MIN`/`LONG_MAX` for `atol`.

### `tsoak.c` (standalone soak / stress test — not in the runner)

A long-running, self-verifying stress test for the allocator and file I/O. It
churns a 32-slot heap pool with random `malloc`/`calloc`/`realloc`/`free`
(verifying every live block's contents before each mutation and at every
checkpoint), and every few thousand iterations performs a single-file round-trip
that writes a patterned buffer and reads it back via `rewind`, random-access
`fseek`/`ftell`, and `fgets` — exercising the read sector cache, multi-record
files, and the `rewind` fix.

It also runs a **multi-file interleaved phase** (once at startup and then
periodically) that is the decisive test for the shared-DMA read sector cache: it
holds `MF_FILES` (6) files open at once — comfortably within the new 8-slot
limit — seeds each with a distinct pattern, and round-robins random reads,
sequential reads, and writes across them. Every read is checked against an
in-memory shadow model of each file, which catches the three cache failure
modes directly: a cross-file read returning another file's cached record; a
read-after-write returning stale pre-write bytes (cache not invalidated on
write); and a same-file sequential read on the cache-hit path returning stale
data. A final full re-read of every file against the shadow is the end-state
proof.

Three further CPU/heap phases (each once at startup, then periodically) drive
runtime library routines the heap/file loops do not otherwise touch, and every
working buffer is heap-allocated so any one-past-the-end write also surfaces as
allocator corruption at the next checkpoint:

- **string / memory** — builds known strings and byte patterns in malloc'd
  buffers and verifies `strlen`, `strcpy`, `strcmp`, `strchr`/`strrchr`,
  `strcat`/`strncmp`, `strstr`, `memcpy`/`memcmp`, `memset`, `memchr`,
  overlapping `memmove` (against a byte-loop shadow), and `strdup` (equal
  content, distinct independent storage).
- **printf / scanf round-trips** — formats random `int`/`unsigned`/`long`/hex
  values with `sprintf` and parses them back with `atoi` and `sscanf`
  (`%d`/`%u`/`%ld`/`%x` and a two-field format), asserting equality. (`atol` is
  not in this runtime, so longs go through `sscanf %ld`.)
- **qsort + bsearch** — sorts a heap-allocated `int` array and searches it.
  `qsort`/`bsearch` are not part of this runtime (the harness links a single
  translation unit), so compact **iterative, stack-safe** versions are bundled
  in the test; they call the comparator through a function pointer, exercising
  the runtime indirect-call path (`__call_hl`). Correctness is checked against
  an independent insertion-sort oracle, and `bsearch` is probed for every
  present key plus guaranteed-absent low/high sentinels.

Transient out-of-memory is handled softly, so the only way it stops early is a
genuine defect. It prints a periodic `[diag]` line and a final summary whose
`malloc + calloc == free` identity confirms no leak and whose `mfops`/`str`/
`fmt`/`sort` counts report the multi-file and CPU/heap phase activity. It is
intentionally **not** wired into `runall.sh` (it is long-running, not a quick
regression); build and run it directly with `./ma.sh tsoak peep` then
`ntvcm TSOAK`. The iteration count is a `#define` at the top of the file.

---

## Validation

The full `bash ./runall.sh ntvcm` regression is green in both `peep` and
`nopeep` modes; the filtered diff against the baseline (excluding the
`__DATE__` / `__TIME__` lines) is empty. `tallocx` (including the four new
cases), `tmalloch`, `tfio`, `fileops`, `tdirent`, `tcrcfix`, and the new
`tioerr` all pass in both modes.

### Resolved: `rewind` mid-file bug

An earlier build of this branch flagged, as a pre-existing out-of-scope issue,
that `fgets` followed by `rewind` + `fread` returned `0x1A` padding instead of
the original data. Root-causing it showed the defect was in `_rewind`'s call
frame to `_lseek` (offset pushed as one word instead of a 32-bit `long`), so
`rewind` cleared EOF but never seeked. It is now fixed (see fix 2a) and the new
`tioerr` `rewind read 5: abcde` line locks in the correct behaviour.


---

## Footnote — compiler tokeniser buffer raised from 127 to 512

While developing the soak test for this branch, a string literal longer than
127 characters caused the compiler to derail with misleading errors (spurious
`unexpected token` reports at later function definitions and at `<eof>`), rather
than reporting the real problem. The cause was in [dcc.c](dcc.c): the per-token
text buffer `MAX_TOK_TEXT` was `128` (127 usable characters plus the terminating
NUL), and on overflow the string scanner stopped mid-literal **without**
consuming the closing quote, leaving the lexer out of sync.

This is a compiler-side change, not a runtime change, but it is recorded here
because it surfaced during this branch's work and the fix is required to build
the longer diagnostic strings used by the soak test.

- **Fix:** `MAX_TOK_TEXT` was raised from `128` to `512`. C89 (ISO 9899:1990
  §2.2.4.1, *Translation limits*) requires an implementation to accept at least
  **509 characters** in a character-string or wide-string literal after
  concatenation, so `512` clears that minimum for a single token. Adjacent
  string literals are still joined in a separately grown buffer, so the overall
  literal length remains effectively unbounded.
- **Robustness:** both the narrow (`"…"`) and wide (`L"…"`) string scanners now
  detect an over-long literal, emit a single `string literal too long`
  diagnostic, and **drain** the rest of the literal (honouring `\` escapes) up to
  the closing quote so the lexer stays synchronised and no cascade of bogus
  errors follows.
- **Validation:** a 134-character literal now compiles cleanly, and a
  600-character literal produces exactly one diagnostic with no follow-on
  errors. The full `./runall.sh ntvcm` regression remained **green** in both
  `peep` and `nopeep` modes.

---

## Footnote — C89 header conformance fixes (independent of the no-`double` design)

A C89 conformance review of this branch confirmed that dcc is, by design, a
single-precision dialect: `float` is the only floating type and `double` is
intentionally absent. Setting that deliberate omission aside, the review found a
handful of header-level divergences that are **independent** of the `double`
decision and were tidied up. These are header-only changes — no runtime (`.MAC`)
behaviour changed — but they are recorded here because they bring the shipped
headers closer to the letter of C89.

- **[float.h](float.h) — documented the `DBL_*` / `LDBL_*` aliasing.** Because
  there is no `double` or `long double`, those macros are aliased to the `FLT_*`
  values and therefore do **not** meet the C89 (§2.2.4.2) minimums for the
  wider types (e.g. `DBL_DIG` ≥ 10, `DBL_EPSILON` ≤ 1E-9). The values are
  unchanged; a comment now states this is a deliberate consequence of the
  no-`double` design so the magnitudes are not mistaken for a bug.
- **[stddef.h](stddef.h) — added the missing C89 (§4.1.5) members.** It now
  defines `ptrdiff_t` (`int`, the result of subtracting two 16-bit pointers),
  `wchar_t` (`unsigned int`, guarded by the same `_WCHAR_T` macro as
  [stdint.h](stdint.h) so the two headers coexist), and `NULL`.
- **[stdio.h](stdio.h) — `BUFSIZ` raised from `128` to `256`.** C89 (§4.9.2)
  requires `BUFSIZ` to be at least `256`. The macro is purely declarative in
  this runtime (no fixed buffer of that size is allocated), so the change is
  safe.
- **[math.h](math.h) — added the unsuffixed C89 (§4.5) names.** `fabs`,
  `floor`, `ceil`, `sqrt`, and `fmod` are now provided as function-like macro
  aliases of the existing single-precision `…f` entry points (the `…f` spellings
  are themselves reserved by C89 §4.13.4 for future use). Portable C89 source
  that calls the unsuffixed names now compiles and links unchanged; the
  operations remain single-precision.
- **Validation:** [tptrdiff.c](tptrdiff.c) (which declares its own `wchar_t`)
  and [trig.c](trig.c) (which calls the unsuffixed `fmod` / `sqrt` / `fabs`)
  both compile cleanly, with trig's calls resolving to `_fmodf` / `_sqrtf` /
  `_fabsf`. The full `./runall.sh ntvcm` regression remained **green** in both
  `peep` and `nopeep` modes.
