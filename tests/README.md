# dcc test suite

This folder holds the C source programs used to exercise the `dcc` C89 compiler
(CP/M-80 / Z80 target) and its runtime, together with the expected-output
baselines used to verify them.

## Layout

```text
tests/
  _test_overrides.json # per-test run config (args, stack_size, ignore)
  <name>.c          # a test program (compiled + run by the harness)
  E.PAS, E.COB, ... # fixture input files consumed by some interpreters
  baselines/
    <name>.txt      # expected stdout for tests/<name>.c
```

## How tests and baselines relate

Each test program `tests/<name>.c` has a **matching baseline file**
`tests/baselines/<name>.txt`. The baseline contains the **exact expected
standard output** that the program produces when compiled with `dcc` and run
under the emulator (`ntvcm`).

The relationship is **by file name**:

| Test source            | Expected output baseline          |
| ---------------------- | --------------------------------- |
| `tests/sieve.c`        | `tests/baselines/sieve.txt`       |
| `tests/ttt.c`          | `tests/baselines/ttt.txt`         |
| `tests/cobint.c`       | `tests/baselines/cobint.txt`      |

The test runner (`scripts/runall.ps1`) builds each `*.c` file, runs it, and
compares the captured stdout against the like-named file in `baselines/`.
Because matching is keyed on the **app name** (not position in a list), the
order in which tests are discovered or run does not matter.

> Note: a few tests take command-line arguments or need a larger stack. Those
> parameters live in `tests/_test_overrides.json` (keys: `args`, `stack_size`,
> `ignore`). For example `ttt` is run with `10`, and `cobint` reads `e.cob`.

## Running the suite

From the repo root, use the platform launcher or run the Python runner directly:

```sh
# Build + run every test, verify against tests/baselines/ (parallel by default)
./scripts/runall.sh       # macOS/Linux
python3 ./scripts/runall.py

# Sequential fallback (one app at a time in the shared build/ dir)
./scripts/runall.sh --serial
python3 ./scripts/runall.py --serial

# Build only one optimization pass (default builds both)
./scripts/runall.sh --mode peep
./scripts/runall.sh --mode nopeep
python3 ./scripts/runall.py --mode peep
python3 ./scripts/runall.py --mode nopeep

# Build without the stack-overflow guard (on by default)
./scripts/runall.sh --no-stack-check
python3 ./scripts/runall.py --no-stack-check
```

On Windows, use `scripts\runall.bat` with the same Python-style options. The
PowerShell wrapper `scripts/runall.ps1` still exists for compatibility, but the
suite implementation is `scripts/runall.py` and is independent of
`scripts/ma.ps1`.

By default the suite builds each app in **both** optimization modes \u2014 `peep`
(optimized, via the dccpeep peephole optimizer) and `nopeep` (unoptimized) \u2014
and verifies both against the same baseline, so a default run does two builds
per app. Use `-Mode peep` or `-Mode nopeep` to build just one.

By default the suite runs **in parallel** (each app builds in its own
`build/<app>/` subdirectory), which is much faster on multi-core machines. The
runner discovers all `tests/*.c` files dynamically, then schedules larger source
files first so long-running apps start early; correctness is still keyed only by
app name and `baselines/<name>.txt`, not by run order. Pass `--serial` to build
sequentially in the shared `build/` directory. The lightweight stack-overflow
guard (`-fstack-check`) is on by default; use `--no-stack-check` to disable it.

A test **passes** when its program builds, runs, and its stdout matches the
corresponding `baselines/<name>.txt` byte-for-byte (line endings normalized to
LF). A test with no baseline file is still built and run, but reported as
"no baseline" rather than verified.

## Placeholders for volatile output

Some programs print values that legitimately change between builds or platforms
(timestamps, build dates, path separators). A baseline may embed **placeholder
tokens** for those parts; the harness matches them as patterns instead of
literal text. All expected content still lives in the single baseline file.

| Token      | Matches                                   | Example value |
| ---------- | ----------------------------------------- | ------------- |
| `{{DATE}}` | C `__DATE__` value (`Mmm dd yyyy`)        | `Jun 16 2026` |
| `{{TIME}}` | C `__TIME__` value (`HH:MM:SS`)           | `20:13:03`    |
| `{{SEP}}`  | path separator (`/` on Unix, `\` Windows) | `/`           |

For example, `tests/baselines/tstdc.txt` (the `__DATE__`/`__TIME__`/`__FILE__`
macro test) uses:

```text
__DATE__ {{DATE}}
__TIME__ {{TIME}}
__FILE__ tests{{SEP}}tstdc.c
__LINE__ 8
__STDC__ 1
test tstdc completed with great success
```

A baseline with no placeholder tokens is compared as an exact string. The token
definitions are global (in `scripts/runall.py`); add a new one there if you need
to mask another kind of volatile value.

## Adding a new test

1. Add the program as `tests/<name>.c`.
2. If it needs arguments, a custom stack size, or should be skipped, add an
   entry to `tests/_test_overrides.json`.
3. Generate its baseline by capturing the **known-good** output into
   `tests/baselines/<name>.txt` (exact stdout, LF line endings, no extra
   trailing content). If any output is volatile (dates, times, paths), replace
   those parts with the placeholder tokens described above.
4. Run `./scripts/runall.sh` and confirm the new test reports
   "Output matches baseline".

## Regenerating baselines from the legacy file

The baselines were originally derived from the single concatenated file
`baseline_test_dcc.txt` (output blocks separated by `test <app>` headers, in a
fixed run order). To (re)generate the per-app `baselines/` files from it:

```pwsh
pwsh ./scripts/convert-baseline.ps1
```

That converter slices the legacy file using the authoritative ordered app list
in `runall.sh` so that output lines which themselves begin with the literal
prefix `test` followed by a space (e.g. `test tstdc completed with great
success`) are not mistaken for section
headers. The split reproduces the original baseline byte-for-byte.

## Notes for automated agents

- The **source of truth** for "what a test should print" is
  `tests/baselines/<name>.txt`. Do not infer expected output from the `.c`
  source alone.
- Keep the baseline and the test in sync: if you intentionally change a
  program's output, update its baseline file in the same change.
- Do not edit `baseline_test_dcc.txt` by hand to fix a single test; prefer
  editing the per-app baseline. The legacy file is kept only for historical
  reference and round-trip regeneration.
- Per-test run parameters (arguments, stack size, ignore) are configuration,
  and live in `tests/_test_overrides.json`, not encoded in the baselines.
