# Utilities

Developer utility scripts and tools for the dcc compiler and runtime.

## Performance Capture (`perfcapture.sh` / `perfcapture.bat`)

Benchmarks a set of standard test applications in both optimized and unoptimized
modes. Builds each app with `peep` (dccpeep optimization) and `nopeep` (no
optimization), measures execution time under the ntvcm emulator, records binary
size, and outputs results to CSV with both metrics on a single line per app for
easy comparison.

### Purpose

Compares performance and binary size across different build modes to:

- Measure dccpeep optimization impact (speed vs. size trade-offs)
- Verify dcc compiler or runtime changes don't regress performance
- Track binary size changes across runtime updates
- Analyze the impact of compiler flags (e.g., `-fstack-check`)
- Establish baseline metrics for optimization changes

### Location

- **macOS/Linux**: `scripts/perfcapture.sh`
- **Windows**: `scripts/perfcapture.bat`

Both versions automatically capture both `peep` and `nopeep` builds and produce
the same CSV output format.

### Usage

=== "macOS / Linux"

    ```sh
    cd /path/to/dcc
    scripts/perfcapture.sh
    ```

=== "Windows"

    ```batch
    cd C:\path\to\dcc
    scripts\perfcapture.bat
    ```

No arguments required — both optimized and unoptimized benchmarks are captured
automatically.

### Output

Results are written to `perf_results.csv` in the project root directory with the
following format:

```csv
utc-timestamp,app,peep_ms,peep_size,nopeep_ms,nopeep_size
2026-06-16T07:18:39Z,tstring,17000,6400,19000,6912
2026-06-16T07:18:39Z,sieve,1000,2176,3000,2304
2026-06-16T07:18:40Z,e,1000,2560,1000,2560
2026-06-16T07:18:40Z,tm,2000,4224,11000,4352
2026-06-16T07:18:40Z,ttt,2000,2816,4000,5632
2026-06-16T07:18:42Z,pihex,80000,14464,80000,14976
2026-06-16T07:18:42Z,mm,4000,6784,4000,6912
```

**Columns:**

- `utc-timestamp` — UTC timestamp (ISO 8601 format, e.g., `2026-06-16T07:18:39Z`)
- `app` — Application name
- `peep_ms` — Execution time in milliseconds (optimized with dccpeep)
- `peep_size` — Binary size in bytes (optimized)
- `nopeep_ms` — Execution time in milliseconds (unoptimized)
- `nopeep_size` — Binary size in bytes (unoptimized)

### Examples

```sh
# Capture both modes
scripts/perfcapture.sh

# View the CSV results
cat perf_results.csv

# Analyze optimization impact for a specific app
grep tstring perf_results.csv
```

### Environment Variables

| Variable | Default | Purpose |
| -------- | ------- | ------- |
| `BUILD_DIR` | `build` | Working directory for build artifacts |
| `OUTPUT_FILE` | `perf_results.csv` | Output CSV file path (relative to project root) |
| `EMULATOR` | `ntvcm` | Emulator command used to run `.COM` files |

### Requirements

- dcc compiler toolchain (`./dcc`, `./dccpeep`, `./dccrtlstrip` binaries)
- Build driver (`ma.sh` on Unix, `ma.bat` on Windows)
- ntvcm emulator (or compatible alternative)
- **macOS/Linux**: `/usr/bin/time` for accurate millisecond timing
- **Windows**: PowerShell for timing and timestamp generation

## Stack Size Measurement (`stacksize.sh` / `stacksize.bat`)

Finds the minimum C stack reserve an app needs under dcc's lightweight
stack-overflow guard (`-fstack-check`). See the
[Building and linking](../02-build-and-link.md#measuring-the-stack-an-app-needs)
section for full documentation, or run `scripts/stacksize.sh --help`.
