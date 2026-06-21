# Limitations

Keep these constraints in mind; they follow directly from the 16-bit CP/M target
and the single source-of-truth runtime.

## Language and type limits

- **No `double`.** Only 32-bit `float` exists; all math is single precision.
- **`float` precision is ~24 bits.** Integers past about ±16,777,216 are not
  all representable, so converting a large `long` to
  `float` — or comparing a `long` against a `float` — rounds to the nearest
  single. Keep values as integers when you need full 32-bit precision. See
  [Floating point math](standard-lib/08-math.md).
- **`%` is integer-only.** Use `fmodf` for a floating-point remainder;
  `float % float` is a compile error.
- **`int` is 16-bit.** Use `long` (and `%ld`) when you need more than ±32767.

## Library limits

- **`scanf` is integer/string only.** Floating input, scansets, `%n`, and `%p`
  are not implemented.
- **No `+`/space/`#` printf flags and no `*` width/precision.** Use literal
  field widths.
- **`%f` needs `-ffloatio`.** Without that flag, float formatting isn't linked.

## Runtime and environment limits

- **No stack/heap guard by default.** The heap and stack share memory; size the
  stack with `-stack`. There is no protection at runtime *unless* you opt in to
  the lightweight stack-overflow guard with **`-fstack-check`**, which makes an
  overflow exit cleanly with a `?stack overflow` message instead of silently
  corrupting the heap. See [Building and linking](02-build-and-link.md) for the
  flag and the `stacksize` utility that measures the reserve an app needs.
- **CP/M 2.2 only.** The runtime uses BDOS functions only (no BIOS calls), plus
  CP/M 3.0 BDOS 108 for the process exit code.

## Declared but not in the runtime

Some functions are declared in `stdlib.h` for source compatibility but are
**not** implemented in `DCCRTL.MAC`. If you call them without supplying your own
definition, the link step fails with an unresolved external.

| Function | Notes |
| --- | --- |
| `atof` | Neither declared nor implemented. C89 `atof` returns `double`, which dcc does not have; use a project-local `float` parser if you need decimal-to-float input. |

If you need to supply your own implementation of an unimplemented function,
either `#include` its `.c` from your main file, or compile it separately with
`dcc -c` and link the resulting `.REL` (the separate-compilation workflow).
