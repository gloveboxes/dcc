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
