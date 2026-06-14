# mathf — `<math.h>` source for the dcc C runtime

`mathf.c` is the **source of truth** for the single-precision transcendental
math routines that ship inside the dcc C runtime, `DCCRTL.MAC`. It is not a
standalone CP/M program; it exists to *generate* the Z80 assembly for those
functions, which is then folded into the runtime by hand.

## What it provides

Portable dcc C89 implementations, written on top of the runtime's existing
float primitives (`__fmul`/`__fadd`/… and `_sqrtf`/`_floorf`/`_ceilf`/`_fmodf`):

- exponential / logarithm: `expf`, `logf`, `log10f`, `powf`
- hyperbolic: `sinhf`, `coshf`, `tanhf`
- trig / inverse-trig: `sinf`, `cosf`, `tanf`, `atanf`, `atan2f`, `asinf`, `acosf`
- decomposition: `frexpf`, `ldexpf`, `modff`

The trig and inverse-trig polynomials are the verified approximations from
`trig.c`. All routines honour the dcc constraints: no `double` (32-bit float
only), 16-bit `int`, 32-bit `long`.

## How it becomes runtime assembly

`ma.sh` runs `dccpeep` on application `.mac` files but **not** on `DCCRTL.MAC`,
so the blocks merged into the runtime must be pre-optimized here. The procedure
(also documented in the file header and the repo docs) is:

1. `dcc -c mathf.c -o mathf.mac`
2. `dccpeep mathf.mac mathf.peep.mac` — peephole-optimize before merging.
3. Strip: `extrn` lines (those helpers live in `DCCRTL.MAC` and resolve
   locally), the `; dcc stage-1d` banner, the `cseg` line, and everything from
   `; string literals` to the end.
4. Rename local labels `L<n>` → `MFL<n>`
   (`perl -pe 's/\bL(\d+)\b/MFL$1/g'`). **Required:** dcc names locals
   `L1, L2, …`; every app does too, and `dccrtlstrip`'s whole-token fallback
   scan would otherwise match an app's `L<n>` against a merged block's `L<n>`
   label and pull the entire math closure into `RTLMIN.MAC` (bloating it until
   L80 runs out of memory) for programs that never call math.
5. Splice the remaining public blocks into `DCCRTL.MAC` before `end start`,
   between the `BEGIN/END generated math functions` markers.

Each routine is an ordinary (non-static, public) function so `dccrtlstrip` can
treat it as its own keep/strip block and pull shared helpers (`expf`, `logf`,
`atanf`, …) in transitively via their call references.

## Tests

`tests/tmathf.c` verifies these routines under `ntvcm`. Accuracy is roughly
5–6 significant digits; bit-twiddling routines (`frexpf`/`ldexpf`/`modff`) are
exact. See `docs/dcc-c89-reference-guide.md` for the public API and
`docs/dccrtlstrip-inclusion-table.md` for the per-symbol code-size cost.
