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
- `[mode]` — Build mode: `peep` (optimized, default) or `nopeep`

### Examples

```pwsh
pwsh ./scripts/ma.ps1 triangle
pwsh ./scripts/ma.ps1 sieve nopeep
pwsh ./scripts/ma.ps1 cobint -Mode peep -BuildDir mybuild
```

### Parameters

| Parameter | Default | Purpose |
| --------- | ------- | ------- |
| `-Name` | (required) | App name without `.c` extension |
| `-Mode` | `peep` | Build mode: `peep` or `nopeep` |
| `-BuildDir` | `build` | Build directory for artifacts |
| `-Emulator` | `ntvcm` | Emulator command for CP/M tools |

### Environment Variables

- `DCC_STACK_SIZE` — C stack reserve in bytes (default: 512)
- `DCC_FORCE_STACK_CHECK` — Force `-fstack-check` on all builds
- `DCC`, `DCCPEEP`, `DCCRTLSTRIP`, `NTVCM`, `M80`, `L80` — Tool paths

## Test Suite Runner (`runall.ps1`)

Comprehensive test suite runner: builds and runs all test applications (all
`*.c` files in tests/) with output verification against per-app baselines in
`tests/baselines/`. Uses `ma.ps1` for building and `app_overrides.json` for
test-specific arguments and stack sizes. Comparison is keyed by app name, so
test discovery order does not matter.

**Runs in parallel by default.** Each app builds in its own `build/<app>/`
subdirectory so concurrent builds don't clobber shared artifacts, and a live
`[ n/total] PASS/FAIL` status prints as each app completes. Use `-Serial` to
fall back to sequential builds in the shared `build/` directory. The lightweight
stack-overflow guard (`-fstack-check`) is **on by default**; pass
`-NoStackCheck` to build without it.

### Purpose

Validates the entire test suite by:

- Building all tests in configured modes (peep, nopeep, or both)
- Running each under the emulator with test-specific arguments
- Comparing output against per-app baselines (placeholder-aware)
- Reporting build/run status per app and overall results

### Usage

```pwsh
pwsh ./scripts/runall.ps1 [options]
```

### Examples

```pwsh
pwsh ./scripts/runall.ps1                       # parallel, stack-check on (defaults)
pwsh ./scripts/runall.ps1 -Serial               # sequential fallback
pwsh ./scripts/runall.ps1 -NoStackCheck         # build without the stack guard
pwsh ./scripts/runall.ps1 -ThrottleLimit 8      # cap concurrency
pwsh ./scripts/runall.ps1 -Emulator altair
pwsh ./scripts/runall.ps1 -Mode nopeep -BuildDir mybuild
```

### Parameters

| Parameter | Default | Purpose |
| --------- | ------- | ------- |
| `-Emulator` | `ntvcm` | Emulator command for running .COM files |
| `-NoStackCheck` | (off) | Disable `-fstack-check` (the guard is ON by default) |
| `-BuildDir` | `build` | Build directory for artifacts |
| `-BaselineDir` | `tests/baselines` | Directory of per-app `<app>.txt` baselines |
| `-Mode` | `both` | Build mode: `peep`, `nopeep`, or `both` |
| `-Serial` | (off) | Run sequentially instead of the default parallel mode |
| `-ThrottleLimit` | CPU core count | Max concurrent apps in parallel mode |

### Output

Reports:
- Total apps discovered
- Passed/failed/skipped counts
- Per-app build and execution status (live in parallel mode)
- Output verification against baseline
- Exit code 0 on success, 1 on failure

## Performance Capture (`perfcapture.ps1`)

Benchmarks all test applications (all `*.c` files in the tests/ folder) in both
optimized and unoptimized modes. Builds each app with `peep` (dccpeep
optimization) and `nopeep` (no optimization), measures execution time under the
ntvcm emulator, records binary size, and outputs results to CSV with both metrics
on a single line per app for easy comparison. **Cross-platform**: runs on macOS,
Linux, and Windows with PowerShell 7+.

### Purpose

Compares performance and binary size across different build modes for the
complete test suite to:

- Measure dccpeep optimization impact across all tests (speed vs. size trade-offs)
- Verify dcc compiler or runtime changes don't regress performance
- Track binary size changes across runtime updates
- Analyze the impact of compiler flags (e.g., `-fstack-check`)
- Build performance history over time (results append to CSV)

### Prerequisites

This script requires **PowerShell 7 or later**. Download from the
[PowerShell releases](https://github.com/PowerShell/PowerShell/releases) page or
use your platform's package manager:

=== "macOS (Homebrew)"

    ```bash
    brew install powershell
    ```

=== "Ubuntu/Debian"

    ```bash
    wget https://aka.ms/powershell-release-deb
    sudo dpkg -i powershell-release-deb
    sudo apt-get update
    sudo apt-get install powershell
    ```

=== "Windows (Chocolatey)"

    ```powershell
    choco install powershell-core
    ```

=== "Windows (scoop)"

    ```powershell
    scoop install pwsh
    ```

### Usage

=== "macOS / Linux / Windows (bash/zsh)"

    ```bash
    pwsh scripts/perfcapture.ps1
    ```

=== "Windows (PowerShell)"

    ```powershell
    pwsh scripts/perfcapture.ps1
    ```

    Or if `pwsh` is in your PATH:

    ```powershell
    .\scripts\perfcapture.ps1
    ```

No arguments required — both optimized and unoptimized benchmarks are captured
automatically.

### Output

Results are written to `perf_results.csv` in the project root directory with the
following format. **Results append to the file**, so each run adds a new row per
app:

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

### Examples

```bash
# First run - creates new CSV with header + data
pwsh scripts/perfcapture.ps1
Get-Content perf_results.csv

# Second run - appends new rows
pwsh scripts/perfcapture.ps1
Get-Content perf_results.csv    # now has 2 rows per app

# Analyze optimization impact for a specific app across all runs
Select-String "tstring" perf_results.csv
```

### Parameters

You can customize behavior with optional parameters:

```pwsh
pwsh scripts/perfcapture.ps1 -BuildDir "mybuild" -OutputFile "results.csv" -Emulator "altair"
```

| Parameter | Default | Purpose |
| --------- | ------- | ------- |
| `BuildDir` | `build` | Working directory for build artifacts |
| `OutputFile` | `perf_results.csv` | Output CSV file path (relative to project root) |
| `Emulator` | `ntvcm` | Emulator command used to run `.COM` files |

### App Overrides Lookup Table

Application-specific arguments, stack size requirements, and exclusions are defined in the
lookup table `scripts/app_overrides.json`. This JSON file eliminates hardcoded
parameter lists in the script and makes overrides easy to maintain:

**Properties:**

| Property | Values | Optional? |
| -------- | ------ | --------- |
| `name` | `ttt`, `pint`, `triangle` | Required |
| `args` | `10`, `e.pas`, `-c`, etc. | Yes |
| `stack_size` | `768`, `1536`, etc. | Yes (default: 512) |
| `ignore` | `true` | Yes (set to skip benchmarking an app) |

**Example:**

```json
{
  "apps": [
    { "name": "ttt", "args": "10" },
    { "name": "pint", "args": "e.pas" },
    { "name": "triangle", "stack_size": 768 },
    { "name": "cobint", "args": "e.cob", "stack_size": 1536 },
    { "name": "tc89fltb", "ignore": true }
  ]
}
```

To add a new app override or modify existing ones, simply edit
`scripts/app_overrides.json` and run the script again.

### Requirements

- **PowerShell 7+** (cross-platform)
- dcc compiler toolchain (`./dcc`, `./dccpeep`, `./dccrtlstrip` binaries)
- Build driver (`ma.sh` on Unix, `ma.bat` on Windows)
- ntvcm emulator (or compatible alternative)

## Stack Size Measurement (`stacksize.sh` / `stacksize.bat`)

Finds the minimum C stack reserve an app needs under dcc's lightweight
stack-overflow guard (`-fstack-check`). See the
[Building and linking](../02-build-and-link.md#measuring-the-stack-an-app-needs)
section for full documentation, or run `scripts/stacksize.sh --help`.
