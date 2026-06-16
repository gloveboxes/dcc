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
system — the full pipeline for `foo.c` is shown below. `dcc`, `dccpeep`, and
`dccrtlstrip` are host tools; `m80.com` and `l80.com` are CP/M programs, so run
them through `ntvcm` (or another CP/M emulator):

=== "macOS / Linux"

    Define the same CRLF helper used by `ma.sh`: prefer `unix2dos` if it is
    installed, otherwise use Perl (available on macOS and common Linux systems).

    ```sh
    to_crlf() {
        if command -v unix2dos >/dev/null 2>&1; then
            unix2dos "$1" >/dev/null 2>&1 || true
        else
            perl -0pi -e 's/\r?\n/\r\n/g' "$1"
        fi
    }
    ```

    Compile the C source to M80 assembly, then optionally run the peephole
    optimizer.

    ```sh
    dcc -I /path/to/dcc -stack 512 foo.c -o FOO.MAC
    dccpeep FOO.MAC _PEEPOUT.MAC
    mv _PEEPOUT.MAC FOO.MAC
    ```

    Convert the app assembly to CP/M CRLF text and assemble it with M80 under
    `ntvcm`.

    ```sh
    to_crlf FOO.MAC
    ntvcm m80 "=FOO.MAC" /X /O /Z /L
    ```

    Copy and trim the runtime to only the blocks used by the app.

    ```sh
    cp /path/to/dcc/DCCRTL.MAC DCCRTL.MAC
    to_crlf DCCRTL.MAC
    dccrtlstrip -r DCCRTL.MAC -o RTLMIN.MAC FOO.MAC
    ```

    Convert, assemble, and link the trimmed runtime with the app.

    ```sh
    to_crlf RTLMIN.MAC
    ntvcm m80 "=RTLMIN.MAC" /X /O /Z
    ntvcm l80 "/P:100,RTLMIN,FOO,FOO/N/E"
    ```

=== "Windows PowerShell"

    Define a CRLF helper using PowerShell/.NET APIs.

    ```powershell
    function Convert-ToCrlf($Path) {
        $text = [IO.File]::ReadAllText($Path) -replace "`r?`n", "`r`n"
        [IO.File]::WriteAllText($Path, $text)
    }
    ```

    Compile the C source to M80 assembly, then optionally run the peephole
    optimizer.

    ```powershell
    dcc -I C:\path\to\dcc -stack 512 foo.c -o FOO.MAC
    dccpeep FOO.MAC _PEEPOUT.MAC
    Move-Item -Force _PEEPOUT.MAC FOO.MAC
    ```

    Convert the app assembly to CP/M CRLF text and assemble it with M80 under
    `ntvcm`.

    ```powershell
    Convert-ToCrlf FOO.MAC
    ntvcm m80 "=FOO.MAC" /X /O /Z /L
    ```

    Copy and trim the runtime to only the blocks used by the app.

    ```powershell
    Copy-Item C:\path\to\dcc\DCCRTL.MAC DCCRTL.MAC
    Convert-ToCrlf DCCRTL.MAC
    dccrtlstrip -r DCCRTL.MAC -o RTLMIN.MAC FOO.MAC
    ```

    Convert, assemble, and link the trimmed runtime with the app.

    ```powershell
    Convert-ToCrlf RTLMIN.MAC
    ntvcm m80 "=RTLMIN.MAC" /X /O /Z
    ntvcm l80 "/P:100,RTLMIN,FOO,FOO/N/E"
    ```

The helper scripts stage `m80.com` and `l80.com` before invoking `ntvcm`. For a
manual build, keep those `.COM` files and `DCCRTL.MAC` in the working directory
where you run the pipeline, or adjust the paths to match your layout. Replace
`/path/to/dcc` (or `C:\path\to\dcc`) with the dcc repo path that contains the
standard headers; if you run from the dcc repo root, the explicit `-I` is usually
unnecessary.

M80 expects CP/M-style CRLF text files; LF-only files can be misread. The Unix
helper above mirrors `ma.sh`; the Windows helper shown above uses PowerShell/.NET
APIs.

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
