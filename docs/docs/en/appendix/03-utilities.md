# Utilities

Developer utility scripts and tools for the dcc compiler and runtime.

## Build Driver (`ma.ps1`)

Cross-platform build driver for compiling a single test application. PowerShell 7+
equivalent of `ma.sh`. Handles the complete pipeline: compile → optimize
(optional) → assemble → strip runtime → link.

### Purpose

Builds a single test app with optional dccpeep optimization, producing a final
`.COM` executable. Automatically detects floatio requirements, handles stack-check
markers, and manages CP/M tool integration through the ntvcm emulator.

### Usage

```pwsh
pwsh ./scripts/ma.ps1 <name> [mode] [options]
```

- `<name>` — Test app name (e.g., `triangle`, `sieve`, `ttt`)
- `[mode]` — Build mode: `full` (both builds, default), `fast` (optimized), or
  `nopeep` (unoptimized)

### Examples

```pwsh
pwsh ./scripts/ma.ps1 triangle
pwsh ./scripts/ma.ps1 sieve nopeep
pwsh ./scripts/ma.ps1 cobint -Mode fast -BuildDir mybuild
```

### Parameters

| Parameter | Default | Purpose |
| --------- | ------- | ------- |
| `-Name` | (required) | App name without `.c` extension |
| `-Mode` | `full` | Build mode: `full`, `fast`, or `nopeep` |
| `-BuildDir` | `build` | Build directory for artifacts |
| `-Emulator` | `ntvcm` | Emulator command for CP/M tools |

### Environment Variables

- `DCC_STACK_SIZE` — C stack reserve in bytes (default: 512)
- `DCC_FORCE_STACK_CHECK` — Force `-fstack-check` on all builds
- `DCC`, `DCCPEEP`, `DCCRTLSTRIP`, `NTVCM`, `M80`, `L80` — Tool paths

## Test Suite Runner (`runall.ps1`)

Comprehensive test suite runner: builds and runs all test applications (all
`*.c` files in tests/) with output verification against per-app baselines in
`tests/baselines/`. Uses `ma.ps1` for building and `tests/_test_overrides.json` for
test-specific arguments and stack sizes. Comparison is keyed by app name, so
test discovery order does not matter.

**Runs in parallel by default.** Key behaviors:

- Each app builds in its own `build/<app>/` subdirectory so concurrent builds
  don't clobber shared artifacts.
- A live `[ n/total] PASS/FAIL` status prints as each app completes.
- Use `-Serial` to fall back to sequential builds in the shared `build/`
  directory.
- The lightweight stack-overflow guard (`-fstack-check`) is **on by default**;
  pass `-NoStackCheck` to build without it.
- Pass `-Report` to append per-app run-time and `.COM` size measurements to a
  CSV report while the suite runs. Report mode disables stack checking so the
  measurements reflect normal builds. When using `ntvcm`, report mode also runs
  measured apps at a fixed 1 GHz emulator clock by default; normal app runs use
  `-s:0` for full speed.

### Purpose

Validates the entire test suite by:

- Building all tests in configured modes (fast, nopeep, or full)
- Running each under the emulator with test-specific arguments
- Comparing output against per-app baselines (placeholder-aware)
- Reporting build/run status per app and overall results

### Usage

```pwsh
pwsh ./scripts/runall.ps1 [options]
```

Run with no options, the suite uses these defaults: **parallel** execution,
the **stack-overflow guard on** (`-fstack-check`), and the **fast
optimized-only** build mode. Use `-Mode full` when you want both fast and nopeep
builds.

### Examples

```pwsh
pwsh ./scripts/runall.ps1                       # quick optimized-only default
pwsh ./scripts/runall.ps1 -Help                 # show help and exit
pwsh ./scripts/runall.ps1 -Serial               # sequential fallback
pwsh ./scripts/runall.ps1 -NoStackCheck         # build without the stack guard
pwsh ./scripts/runall.ps1 -ThrottleLimit 8      # cap concurrency
pwsh ./scripts/runall.ps1 -Emulator altair
pwsh ./scripts/runall.ps1 -Mode fast            # optimized build only
pwsh ./scripts/runall.ps1 -Mode nopeep          # unoptimized build only
pwsh ./scripts/runall.ps1 -Report               # also append perf_results.csv
```

### Build Modes

The `-Mode` parameter selects which optimization pass(es) to build and verify.
**The default is `fast`.**

- **`fast`** — optimized: runs the `dccpeep` peephole optimizer after compiling.
  This produces the optimized CP/M Z80 binary.
- **`nopeep`** — unoptimized: skips `dccpeep`. This produces the unoptimized
  CP/M Z80 binary.
- **`full`** — builds and verifies each app **twice**, once in each mode,
  against the same baseline. This catches optimizer bugs that change a program's
  output.

### Parameters

| Parameter | Default | Purpose |
| --------- | ------- | ------- |
| `-Emulator` | `ntvcm` | Emulator command for running .COM files |
| `-NoStackCheck` | (off) | Disable `-fstack-check` (the guard is ON by default) |
| `-BuildDir` | `build` | Build directory for artifacts |
| `-BaselineDir` | `tests/baselines` | Directory of per-app `<app>.txt` baselines |
| `-Mode` | `fast` | Build mode: `fast` (optimized), `nopeep` (unoptimized), or `full` |
| `-Help` | (off) | Show help text and exit without building or running tests |
| `-Serial` | (off) | Run sequentially instead of the default parallel mode |
| `-ThrottleLimit` | CPU core count | Max concurrent apps in parallel mode |
| `-Report` | (off) | Append per-app execution time and `.COM` size metrics to a CSV report; implies `-NoStackCheck` |
| `-ReportFile` | `perf_results.csv` | CSV path used by `-Report` |
| `-ReportClockHz` | `1000000000` | ntvcm clock speed used for measured app runs in report mode; set to `0` for full-speed report runs |

### Output

Reports:
- Total apps discovered
- Passed/failed/skipped counts
- Per-app build and execution status (live in parallel mode)
- Output verification against baseline
- Optional CSV performance report when `-Report` is passed
- Exit code 0 on success, 1 on failure

## Test Overrides (`tests/_test_overrides.json`)

Per-test run configuration used by `runall.ps1`.
It lives in the `tests/` folder (alongside the test sources it configures) and
is named with a leading underscore so it sorts to the top of the directory.

Most tests need no entry — they compile cleanly, take no arguments, and use the
default 512-byte stack. This file only lists the exceptions.

### Schema

The file is a single JSON object with an `apps` array. Each element configures
one test, keyed by `name`:

```json
{
  "apps": [
    { "name": "<app>", "args": "<string>", "stdin": "<string>", "stack_size": <int>, "ignore": <bool> }
  ]
}
```

| Property | Type | Required | Default | Purpose |
| -------- | ---- | -------- | ------- | ------- |
| `name` | string | yes | — | Test name, without the `.c` extension (e.g. `ttt`, `cobint`) |
| `args` | string | no | `""` | Command-line arguments passed to the program when run. Multi-token strings are split on whitespace (e.g. `"a bb ccc"`) |
| `stdin` | string | no | `""` | Text piped to the program's standard input during execution (for keyboard/input-driven tests) |
| `stack_size` | integer | no | `512` | C stack reserve in bytes, passed to `dcc` as `-stack`. Used by recursive apps that need more headroom |
| `ignore` | boolean | no | `false` | When `true`, the test is skipped entirely (not built or run) |

Entries with none of the optional properties have no effect, so an app only
appears here if it overrides at least one default.

### Example

```json
{
  "apps": [
    { "name": "ttt", "args": "10" },
    { "name": "pint", "args": "e.pas" },
    { "name": "tkbd", "stdin": "x" },
    { "name": "cobint", "args": "e.cob", "stack_size": 1536 },
    { "name": "triangle", "stack_size": 768 },
    { "name": "na", "ignore": true },
    { "name": "tc89fltb", "ignore": true },
    { "name": "spsmash", "ignore": true }
  ]
}
```

### Common reasons to add an entry

- **Program reads a data file** — interpreters like `pint`, `cobint`, `forint`
  take a fixture filename as `args` (e.g. `e.pas`, `e.cob`).
- **Program reads from stdin** — set `stdin` for tests that require scripted
  keyboard/input text (for example, `tkbd` expects `x`).
- **Deep recursion** — apps such as `triangle` (768) and `cobint` (1536) need a
  larger `stack_size` than the 512-byte default, especially under
  `-fstack-check`.
- **Cannot be auto-tested** — set `ignore: true` for interactive programs
  (`na`, an editor that waits for keystrokes), tests that intentionally fail to
  compile (`tc89fltb`), or deliberate stack-smashers (`spsmash`).

To change a test's run behavior, edit `tests/_test_overrides.json` and re-run
the suite. See also `tests/README.md` in the repository for how tests,
baselines, and this file relate.

## Performance Reporting (`runall.ps1 -Report`)

`runall.ps1 -Report` collects performance data during the normal verified test
suite. It appends per-app execution time and `.COM` size metrics to a CSV report,
while still checking output against the usual baselines.

Report mode implies `-NoStackCheck` so timings reflect normal builds. When using
`ntvcm`, measured app runs use a fixed 1 GHz emulator clock by default; set
`-ReportClockHz 0` for full-speed report runs.

```pwsh
pwsh ./scripts/runall.ps1 -Report
pwsh ./scripts/runall.ps1 -Report -ReportFile results.csv
pwsh ./scripts/runall.ps1 -Report -ReportClockHz 0
```

Results are written to `perf_results.csv` by default. **Results append to the
file**, so each report run adds a new row per app:

```csv
machine,utc-timestamp,app,peep_ms,peep_size,nopeep_ms,nopeep_size
mycomputer,2026-06-16T07:18:39Z,tstring,17000,6400,19000,6912
mycomputer,2026-06-16T07:18:39Z,sieve,1000,2176,3000,2304
mycomputer,2026-06-16T07:24:21Z,tstring,17000,6400,17000,6912
mycomputer,2026-06-16T07:24:21Z,sieve,1000,2176,3000,2304
```

**Columns:**

- `machine` — Name of the machine running the benchmark
- `utc-timestamp` — UTC timestamp (ISO 8601 format, e.g., `2026-06-16T07:18:39Z`)
- `app` — Application name
- `peep_ms` — Execution time in milliseconds (optimized with dccpeep)
- `peep_size` — Binary size in bytes (optimized)
- `nopeep_ms` — Execution time in milliseconds (unoptimized)
- `nopeep_size` — Binary size in bytes (unoptimized)

The `-ReportFile` parameter controls the output path. The `-Mode` parameter
controls which CSV columns are populated: `-Mode full` fills both `peep_*` and
`nopeep_*`; single-mode runs fill only the selected mode's columns. In the CSV,
`peep_*` columns hold optimized-build measurements.

## Stack Size Measurement (`stacksize.sh` / `stacksize.bat`)

Finds the minimum C stack reserve an app needs under dcc's lightweight
stack-overflow guard (`-fstack-check`). See the
[Building and linking](../02-build-and-link.md#measuring-the-stack-an-app-needs)
section for full documentation, or run `scripts/stacksize.sh --help`.
