# dcc helper scripts

Developer utility scripts for the `dcc` (CP/M-80 / Z80) toolchain.

## `stacksize.sh` / `stacksize.bat`

Finds the minimum **C stack reserve** an app needs under dcc's lightweight
stack-overflow guard (`-fstack-check`).

`stacksize.sh` is the macOS/Linux version; `stacksize.bat` is the equivalent for
Windows (`cmd.exe`). They take the same arguments, honour the same environment
variables, and produce the same report — the Windows version drives `ma.bat`
instead of `ma.sh`.

### Purpose

On the Z80/CP/M target the C stack and the `malloc` heap share memory with no
hardware protection. If a program recurses too deeply (or is built with too
small a `-stack` reserve) the stack can grow down into the heap and silently
corrupt it. The `-fstack-check` guard turns that silent corruption into a clean
`?stack overflow` message and exit.

`stacksize.sh` uses that guard to **measure** how much stack an app actually
needs: it builds the app with the guard forced on, runs it, and sweeps the
`-stack` reserve upward until the program runs without tripping the guard. The
first size that runs clean is the minimum requirement; the script also prints a
rounded-up recommendation with a little headroom.

### Usage

```sh
scripts/stacksize.sh <app> [-- emulator-args...]
```

- `<app>` — test/app name as passed to `ma.sh` (e.g. `triangle`, `cobint`).
- `-- args...` — arguments to pass to the emulated program (e.g. a data file
  such as `e.cob`). Everything after `--` is forwarded to the emulator.

Run it from the repo root (or anywhere — it locates the repo root relative to
the script). The in-repo `./dcc`, `./dccpeep`, `./dccrtlstrip` binaries and the
`ntvcm` emulator must be available.

### Examples

```sh
scripts/stacksize.sh triangle          # simple app
scripts/stacksize.sh cobint -- e.cob   # app that needs a data-file argument
```

Sample output:

```
Finding minimum stack for 'triangle' (guard on): start=256 step=64 max=8192 mode=peep

  stack=256    : overflow
  stack=320    : overflow
  ...
  stack=640    : OK

Minimum passing stack reserve : 640 bytes
Recommended (with headroom)   : 768 bytes

Build it with:  DCC_STACK_SIZE=768 ./ma.sh triangle peep
Or compile direct:  ./dcc -fstack-check -stack 768 tests/triangle.c -o TRIANGLE.mac
```

### Options (environment variables)

| Variable | Default | Meaning |
| -------- | ------- | ------- |
| `START`  | `256`   | First stack size to try (bytes). |
| `STEP`   | `64`    | Increment between tries (bytes). |
| `MAX`    | `8192`  | Give up after this size. Prevents an infinite loop on apps that overflow on purpose (e.g. `spsmash`). |
| `MODE`   | `peep`  | Build mode passed to `ma.sh` (`peep` or `nopeep`). |
| `EMU`    | `ntvcm` | Emulator command used to run the `.COM`. |

```sh
START=512 STEP=128 scripts/stacksize.sh triangle
MAX=16384 scripts/stacksize.sh somedeepapp
```

### Exit status

- `0` — a passing stack size was found (printed as the minimum + recommendation).
- `1` — no passing size up to `MAX`. The app may overflow on purpose (deliberate
  unbounded recursion, like `spsmash`), or it genuinely needs more than `MAX`
  bytes — raise `MAX=...` and retry.

### How it ties into the build

The sweep varies the reserve through the `DCC_STACK_SIZE` hook honored by
`ma.sh`, and forces the guard on with `DCC_FORCE_STACK_CHECK=1`. Once you know
the size, bake it in by building with `DCC_STACK_SIZE=<n> ./ma.sh <app>` or, for
the regression suite, add it to the per-app `stack_size_for` table in
`runall.sh` (and the matching block in `runall.bat`).

## `ma.ps1`

Cross-platform build driver (PowerShell 7+ equivalent of `ma.sh`). Compiles a
single test app with optional peephole optimization, strips runtime symbols,
and links to produce a `.COM` executable. The complete pipeline:

1. Compile source with `dcc` (auto-detect floatio and stack-check flags)
2. Optimize with `dccpeep` (optional, peep mode only)
3. Assemble app.MAC with M80
4. Strip DCCRTL runtime using dccrtlstrip
5. Assemble stripped RTLMIN.MAC
6. Link app + RTLMIN with L80

### Usage

```pwsh
pwsh ./scripts/ma.ps1 <name> [peep|nopeep]
```

- `<name>` — test app name (e.g., `triangle`, `sieve`, `ttt`)
- `[mode]` — build mode: `peep` (optimized, default) or `nopeep` (unoptimized)

### Examples

```pwsh
pwsh ./scripts/ma.ps1 triangle
pwsh ./scripts/ma.ps1 sieve nopeep
pwsh ./scripts/ma.ps1 cobint -Mode peep -BuildDir mybuild
```

### Parameters

| Parameter | Default | Meaning |
| --------- | ------- | ------- |
| `-Name` | (required) | Test app name (without `.c` extension) |
| `-Mode` | `peep` | Build mode: `peep` or `nopeep` |
| `-BuildDir` | `build` | Working directory for build artifacts |
| `-Emulator` | `ntvcm` | Emulator command for running CP/M tools |

### Environment Variables

- `DCC_STACK_SIZE` — C stack reserve in bytes (default: 512)
- `DCC_FORCE_STACK_CHECK` — Enable `-fstack-check` for all builds
- `DCC`, `DCCPEEP`, `DCCRTLSTRIP`, `NTVCM`, `M80`, `L80` — Tool paths

## `runall.ps1`

Comprehensive test suite: builds and runs all test applications with output
verification against per-app baselines in `tests/baselines/`. Uses `ma.ps1` to
build each app and `tests/_test_overrides.json` for test-specific arguments and stack
sizes. Comparison is keyed by app name, so test discovery order does not matter.
See [`tests/README.md`](../tests/README.md) for the test/baseline relationship.

**Runs in parallel by default** (each app builds in its own `build/<app>/`
subdirectory so concurrent builds don't clobber shared artifacts). Use
`-Serial` to fall back to sequential builds in the shared `build/` directory.
The lightweight stack-overflow guard (`-fstack-check`) is **on by default**;
pass `-NoStackCheck` to build without it.

### Usage

```pwsh
pwsh ./scripts/runall.ps1 [options]
```

Run with no options, the suite uses these defaults: **parallel** execution, the
**stack-overflow guard on** (`-fstack-check`), and **both build modes** (`peep`
and `nopeep`). Use the switches below to change any of these.

### Parameters

| Parameter | Default | Meaning |
| --------- | ------- | ------- |
| `-Emulator` | `ntvcm` | Emulator command for running .COM files |
| `-NoStackCheck` | (off) | Disable `-fstack-check` (the guard is ON by default) |
| `-BuildDir` | `build` | Working directory for artifacts |
| `-BaselineDir` | `tests/baselines` | Directory of per-app `<app>.txt` baselines |
| `-Mode` | `both` | Build mode: `peep` (optimized), `nopeep` (unoptimized), or `both` |
| `-Serial` | (off) | Run sequentially instead of the default parallel mode |
| `-ThrottleLimit` | CPU core count | Max concurrent apps in parallel mode |

### Build modes

The `-Mode` parameter selects which optimization pass(es) to build and verify.
**The default is `both`.**

- **`peep`** — optimized: runs the `dccpeep` peephole optimizer after compiling.
- **`nopeep`** — unoptimized: skips `dccpeep`.
- **`both`** (default) — builds and verifies each app **twice**, once in each
  mode, against the same baseline. A default run therefore performs two builds
  per app (this catches optimizer bugs that change a program's output).

### Examples

```pwsh
pwsh ./scripts/runall.ps1                       # all defaults
pwsh ./scripts/runall.ps1 -Serial               # sequential fallback
pwsh ./scripts/runall.ps1 -NoStackCheck         # build without the stack guard
pwsh ./scripts/runall.ps1 -ThrottleLimit 8      # cap concurrency
pwsh ./scripts/runall.ps1 -Mode peep            # optimized build only
pwsh ./scripts/runall.ps1 -Mode nopeep          # unoptimized build only
```

Parallel mode is markedly faster on multi-core machines. Each app builds in its
own `build/<app>/` subdirectory so concurrent builds don't clobber shared
artifacts, and a live `[ n/total] PASS/FAIL` status prints as each app
completes.

### Output

Reports:
- Total apps discovered
- Passed/failed/skipped counts
- Per-app build and execution status (live in parallel mode)
- Output verification against baseline (if available)
- Exit code 0 on full success, 1 if any test fails

### Exit Status

- `0` — All tests passed
- `1` — One or more tests failed

## `stress-runall.ps1`

Stress-tests the suite by running `runall.ps1` repeatedly, stopping on the first
failure. Useful for shaking out intermittent/flaky failures under parallel load.
The failing iteration's full log is kept for inspection.

### Usage

```pwsh
pwsh ./scripts/stress-runall.ps1                # up to 50 parallel runs
pwsh ./scripts/stress-runall.ps1 -Iterations 100
pwsh ./scripts/stress-runall.ps1 -Serial -Iterations 10
pwsh ./scripts/stress-runall.ps1 -ThrottleLimit 8 -KeepLogs
```

### Parameters

| Parameter | Default | Meaning |
| --------- | ------- | ------- |
| `-Iterations` | `50` | Maximum number of suite runs |
| `-Serial` | (off) | Run the suite serially each iteration |
| `-ThrottleLimit` | CPU core count | Max concurrent apps in parallel mode |
| `-LogDir` | temp folder | Where per-iteration logs are written |
| `-KeepLogs` | (off) | Keep all logs, not just the failing one |

### Exit Status

- `0` — All iterations passed
- `1` — An iteration failed (loop stopped early)

## `convert-baseline.ps1`

Splits the legacy concatenated baseline `baseline_test_dcc.txt` into per-app
baseline files under `tests/baselines/` (one `<app>.txt` per test). Used to
(re)generate the baselines consumed by `runall.ps1`.

### Usage

```pwsh
pwsh ./scripts/convert-baseline.ps1
```

### Parameters

| Parameter | Default | Meaning |
| --------- | ------- | ------- |
| `-InputFile` | `baseline_test_dcc.txt` | Legacy concatenated baseline |
| `-OutputDir` | `tests/baselines` | Destination for per-app baseline files |
| `-AppList` | (from `runall.sh`) | Ordered app list used as split boundaries |

The converter slices the legacy file using the authoritative ordered app list
(`APPLIST` in `runall.sh`) so that output lines beginning with `test ` (e.g.
`test tstdc completed with great success`) are not mistaken for section headers.
The split reproduces the original baseline byte-for-byte.

## `perfcapture.ps1`

Captures performance benchmarks for all test applications (all `*.c` files in the
tests/ folder). Builds each app in both optimized and unoptimized modes, measures
execution time under the ntvcm emulator, records binary size, and outputs results
as CSV with both metrics on a single line per app. **Cross-platform**: runs on
macOS, Linux, and Windows with PowerShell 7+.

### Purpose

Compares performance and binary size across optimized (dccpeep) vs. unoptimized
builds for the complete test suite. Useful for:

- Measuring compiler optimization impact across all tests
- Verifying dcc, dccpeep, or dccrtlstrip changes don't regress performance
- Tracking binary size changes across runtime updates
- Analyzing the impact of compiler flags like `-fstack-check`
- Building performance history over time (results append to CSV)

### Prerequisites: Installing PowerShell 7

This script requires **PowerShell 7 or later** (cross-platform).

#### macOS

Install via Homebrew:

```bash
brew install powershell
```

#### Linux (Ubuntu/Debian)

```bash
# Add Microsoft repository
wget https://aka.ms/powershell-release-deb
sudo dpkg -i powershell-release-deb

# Install
sudo apt-get update
sudo apt-get install powershell

# Invoke
pwsh scripts/perfcapture.ps1
```

For other Linux distributions, see [PowerShell Linux installation](https://learn.microsoft.com/en-us/powershell/scripting/install/installing-powershell-on-linux).

#### Windows

Download and run the installer from [PowerShell releases](https://github.com/PowerShell/PowerShell/releases),
or install via package manager:

```powershell
# Using Windows Package Manager
winget install Microsoft.PowerShell
```

### Usage

```pwsh
pwsh scripts/perfcapture.ps1
```

No arguments needed — both modes (peep/nopeep) are captured automatically. Results
are written to `perf_results.csv` in the project root. **Results append to the
file**, so each run adds a new row per app for tracking performance over time.

### Examples

```pwsh
# Capture both modes
pwsh scripts/perfcapture.ps1

# Check results
Get-Content perf_results.csv | Select-Object -Last 10

# Run again later to append more results
pwsh scripts/perfcapture.ps1

# View all results
Get-Content perf_results.csv
```

### Output format

The CSV file contains machine name, UTC timestamp, app name, and metrics for both peep and
nopeep builds:

```csv
machine,utc-timestamp,app,peep_ms,peep_size,nopeep_ms,nopeep_size
mycomputer,2026-06-16T07:18:39Z,tstring,17000,6400,19000,6912
mycomputer,2026-06-16T07:18:39Z,sieve,1000,2176,3000,2304
mycomputer,2026-06-16T07:18:40Z,e,1000,2560,1000,2560
mycomputer,2026-06-16T07:18:40Z,tm,2000,4224,11000,4352
mycomputer,2026-06-16T07:18:40Z,ttt,2000,2816,4000,5632
mycomputer,2026-06-16T07:18:42Z,pihex,80000,14464,80000,14976
mycomputer,2026-06-16T07:18:42Z,mm,4000,6784,4000,6912
```

**Columns:**

- `machine` — Name of the machine running the benchmark
- `utc-timestamp` — UTC timestamp (ISO 8601 format, e.g., `2026-06-16T07:18:39Z`)
- `app` — Application name
- `peep_ms` — Execution time in milliseconds (optimized with dccpeep)
- `peep_size` — Binary size in bytes (optimized)
- `nopeep_ms` — Execution time in milliseconds (unoptimized)
- `nopeep_size` — Binary size in bytes (unoptimized)

### Parameters

```pwsh
pwsh scripts/perfcapture.ps1 -BuildDir "mybuild" -OutputFile "results.csv" -Emulator "altair"
```

| Parameter | Default | Meaning |
| --------- | ------- | ------- |
| `BuildDir` | `build` | Working directory for build artifacts. |
| `OutputFile` | `perf_results.csv` | CSV output file path (relative to project root). |
| `Emulator` | `ntvcm` | Emulator command used to run `.COM` files. |

### App Overrides

Application-specific arguments, stack size requirements, and exclusions are stored in
[`tests/_test_overrides.json`](../tests/_test_overrides.json). Each app entry specifies:

- `name` — Application name (e.g., `ttt`, `pint`, `triangle`) (required)
- `args` — Command-line arguments to pass when running the app (optional, e.g., `10`, `e.pas`, `-c`)
- `stack_size` — C stack reserve in bytes (optional, e.g., `768`, `1536`), defaults to 512
- `ignore` — Set to `true` to skip benchmarking this app (optional, e.g., for tests that don't compile)

To add or modify overrides, simply edit the JSON file:

```json
{
  "apps": [
    { "name": "ttt", "args": "10" },
    { "name": "triangle", "stack_size": 768 },
    { "name": "cobint", "args": "e.cob", "stack_size": 1536 },
    { "name": "tc89fltb", "ignore": true }
  ]
}
```

### Requirements

- **PowerShell 7+** (cross-platform)
- The in-repo `./dcc`, `./dccpeep`, `./dccrtlstrip` binaries
- The `ntvcm` emulator (or alternative specified via `-Emulator` parameter)
- `ma.sh` build driver (on macOS/Linux) or `ma.bat` (on Windows)
