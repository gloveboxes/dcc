# Building and linking

This page covers the path from a `.c` file to a runnable CP/M `.COM` program and
the compiler options that most affect the result. You typically do this in your
**own project directory** — dcc and ntvcm are general-purpose tools for building
CP/M / Z80 C apps anywhere, not just inside the dcc repo. As long as the tools
are on your `PATH` (see [Setting up the toolchain](00-setup-toolchain.md)), the
commands below work from any folder that holds your `.c` sources.

## The one-step helper script

The quickest way to build is the `ma.sh` (macOS/Linux) or `ma.bat` (Windows)
helper script. It compiles, optimizes, strips the runtime, assembles, and links
in one step. Copy `ma.sh` / `ma.bat` from the dcc repo into your own project (or
point at it by path) and run it against your source:

```sh
sh ./ma.sh foo            # builds foo.c -> FOO.COM (peephole optimized)
sh ./ma.sh foo nopeep     # same, but skip the dccpeep optimizer
```

Under the hood this runs the compiler, the optional `dccpeep` peephole
optimizer, the `dccrtlstrip` runtime trimmer, then M80 and L80. Runtime
trimming is part of the normal build path because it keeps the final `.COM`
file from carrying unused library routines. The script
resolves each tool from your `PATH` (or the `DCC` / `DCCPEEP` / `DCCRTLSTRIP`
environment variables), so it does not need to live next to the dcc binaries.

## The manual pipeline

If you'd rather drive the steps yourself — or wire them into your own build
system — the full pipeline for `foo.c` is:

```sh
dcc foo.c -o FOO.MAC                 # compile C -> M80 assembly
dccpeep -Ot FOO.MAC FOO.MAC          # optional peephole optimization
dccrtlstrip -r DCCRTL.MAC -o RTLMIN.MAC FOO.MAC   # standard runtime trimming
m80 =FOO                             # assemble FOO.MAC -> FOO.REL
l80 FOO,RTLMIN,FOO/N/E               # link -> FOO.COM
```

The `m80`/`l80` steps run under the emulator the same way the helper scripts
invoke them.

## The compiler invocation

```text
dcc [options] input.c [-o output.mac]
```

Common options:

| Option | Meaning |
| --- | --- |
| `-o file` | Write M80 assembly to `file`; default is `out.mac`, `-` is stdout. |
| `-c`, `-module` | Emit a linkable helper module, not a final program translation unit. |
| `-f`, `-ffloatio` | Enable floating-point `printf` formatting support. |
| `-s bytes`, `-stack bytes`, `--stack bytes` | Reserve stack bytes; default is 512. |
| `-s=bytes`, `-stack=bytes`, `--stack=bytes` | Equivalent attached forms for the stack size. |
| `-I dir`, `-Idir` | Add an include search directory. |
| `-D name[=value]`, `-Dname[=value]` | Predefine a macro. |
| `-U name`, `-Uname` | Undefine a preprocessor macro. |
| `-v`, `--version` | Print the compiler version and exit. |
| `-h`, `--help` | Print compiler help and exit. |

## Options that affect the runtime

- **`-f` / `-ffloatio`** — pull in floating-point support for the `printf`
  family. You **must** pass this if your format strings use `%f`; otherwise
  float formatting is not linked in. This is also the single biggest code-size
  lever — see the [appendix](appendix/01-dccrtlstrip.md).
- **`-s` / `-stack` / `--stack`** — reserve stack space (default 512; accepted
  range 0..32767). The heap used by `malloc` lives between the end of BSS and
  the bottom of the stack, so growing the stack shrinks the heap and vice versa.
  There are no runtime checks that stop the stack from smashing the heap.
- **`-Dname[=value]`** — predefine a macro. `_DCC_=1` is always defined.

## Including headers

Include the standard headers as usual:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

## Memory layout

CP/M loads `.COM` files in just one way. BSS begins immediately after the loaded
image, and the loader sets `SP` to the highest free byte. The heap grows on
demand between the end of BSS and the bottom of the stack. Because there is no
guard between them, size the stack deliberately with `-stack` for programs with
deep recursion or large frames.
