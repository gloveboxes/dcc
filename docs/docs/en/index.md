# Introduction

**dcc** is a C89 compiler that targets **CP/M 2.2 on the Z80**. It reads a `.c`
file and generates an M80-syntax `.MAC` assembly file that is assembled by M80
and linked by L80 to produce a CP/M `.COM` program.

dcc and the [ntvcm](https://github.com/davidly/ntvcm) emulator are
general-purpose tools: although the dcc repo bundles a test suite and build
scripts, you will normally use them to build your **own** CP/M / Z80 C apps in
independent projects. Once the tools are on your `PATH`, you can compile and run
from any directory — you don't need to work inside the dcc repo.

This documentation is a practical guide to the language dcc accepts and the C
runtime library it ships with. It is split into focused topics so you can jump
straight to the area you need:

- [Setting up the toolchain](00-setup-toolchain.md) — build dcc and ntvcm and
  put the tools on your `PATH` for macOS, Linux, and Windows.
- [Agent skills](00-agent-skills.md) — let an AI coding assistant write correct
  dcc/CP/M/Z80 code by loading the bundled skill.
- [C89 conformance and C99 extensions](01-c89-conformance.md) — which keywords
  and features are supported, and the handful of post-C89 conveniences.
- [Building and linking](02-build-and-link.md) — how a `.c` file becomes a
  `.COM` file and the options that matter most.
- [Types and conventions](03-types-and-conventions.md) — sizes of `int`,
  `long`, `float`, pointers, and the constants you can rely on.
- [Operators](04-operators.md) — the full operator set and the 16-bit gotchas.
- Library reference: [stdio](05-stdio.md), [stdlib](06-stdlib.md),
  [strings and ctype](07-strings-and-ctype.md), [math](08-math.md),
  [utility headers](09-utility-headers.md), and
  [system / CP/M services](10-system-and-cpm.md).
- [Limitations](11-limitations.md) and [worked examples](12-examples.md).
- An [appendix on `dccrtlstrip`](appendix/01-dccrtlstrip.md) explaining how the
  runtime is trimmed and what each library call costs in code size.

## How the runtime fits together

`DCCRTL.MAC` is the single source of truth for the runtime. It is written in Z80
assembly (M80/L80 syntax) for size and speed and provides:

- the `start` entrypoint that sets up the heap and parses the command line into
  `argc` / `argv` before calling your `main`,
- a near complete coverage of the C89 library (documented across these pages),
- 16-/32-bit integer and 32-bit float arithmetic helpers that the compiler
  calls implicitly.

Most library names in the standard headers are ordinary C identifiers. The
compiler maps well-known library calls to short internal assembler labels (for
example `memcpy` becomes `__mcpy`, `strlen` becomes `__slen`). You never write
those short names yourself — you just call the standard C functions and include
the matching header.

The optional `dccrtlstrip` pass scans your program and removes the runtime
routines you don't use, producing a smaller `RTLMIN.MAC` that is linked instead
of the whole runtime. It follows symbol references automatically, so you only
pay for what you call. See the [appendix](appendix/01-dccrtlstrip.md) for the
full picture and the size implications.

!!! note "Single source of truth"
    Functions that are declared in the standard headers but are **not** part of
    the linked runtime are called out explicitly in
    [Limitations](11-limitations.md) so a missing function never surprises you
    at link time.
