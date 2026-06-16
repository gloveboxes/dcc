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

## `perfcapture.sh` / `perfcapture.bat`

Captures performance benchmarks for the benchmark suite (tstring, sieve, e, tm,
ttt, pihex, mm) in both optimized and unoptimized modes. Builds each app with
both `peep` and `nopeep` modes, measures execution time under the ntvcm emulator,
records binary size, and outputs results as CSV with both metrics on a single
line per app.

`perfcapture.sh` is the macOS/Linux version; `perfcapture.bat` is the Windows
equivalent. Both automatically capture both build modes and produce identical CSV
output.

### Purpose

Compares performance and binary size across optimized (dccpeep) vs. unoptimized
builds. Useful for:

- Measuring compiler optimization impact
- Verifying binary size trade-offs
- Tracking changes across dcc, dccpeep, or dccrtlstrip updates
- Analyzing the impact of compiler flags like `-fstack-check`

### Usage

```sh
scripts/perfcapture.sh
scripts/perfcapture.bat
```

No arguments needed — both modes (peep/nopeep) are captured automatically. Results
are written to `perf_results.csv` in the project root.

### Examples

```sh
scripts/perfcapture.sh              # Capture both modes
ls -lh perf_results.csv
cat perf_results.csv
```

### Output format

The CSV file contains UTC timestamp, app name, and metrics for both peep and
nopeep builds:

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

### Environment variables

| Variable | Default | Meaning |
| -------- | ------- | ------- |
| `BUILD_DIR` | `build` | Working directory for build artifacts. |
| `OUTPUT_FILE` | `perf_results.csv` | CSV output file path (relative to project root). |
| `EMULATOR` | `ntvcm` | Emulator command used to run `.COM` files. |

```sh
EMULATOR=altair scripts/perfcapture.sh peep
OUTPUT_FILE=bench_$(date +%Y%m%d).csv scripts/perfcapture.sh peep
```

### Requirements

- The in-repo `./dcc`, `./dccpeep`, `./dccrtlstrip` binaries
- The `ntvcm` emulator (or alternative specified via `EMULATOR`)
- `ma.sh` or `ma.bat` build driver
- macOS/Linux: `/usr/bin/time` for accurate timing (fallback uses `date +%s%N`)
- Windows: PowerShell for timing and timestamp generation
