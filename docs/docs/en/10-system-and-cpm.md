# System and CP/M services

This page covers low-level file descriptors, directory enumeration, error
reporting, assertions, and the CP/M-specific extensions.

## `unistd.h` / `fcntl.h` — low-level file I/O

These map onto CP/M file operations and operate on small integer file
descriptors. Include `unistd.h` and `fcntl.h`.

| Function | Summary |
| --- | --- |
| `int open(const char *path, int flags, ...)` | Open/create a file, returns a descriptor. |
| `int read(int fd, void *buf, unsigned n)` | Read up to `n` bytes. |
| `int write(int fd, const void *buf, unsigned n)` | Write up to `n` bytes. |
| `int close(int fd)` | Close a descriptor. |
| `long lseek(int fd, long off, int whence)` | Reposition the descriptor. |
| `int unlink(const char *path)` | Delete a file. |
| `int fsync(int fd)` | Flush file data to disk. |
| `int fdatasync(int fd)` | Flush file data to disk. |

`fcntl.h` also declares `open` in K&R form (`int open();`) for compatibility.
`off_t` from `unistd.h` is `long`.

| Constant | Value | Meaning |
| --- | ---: | --- |
| `O_RDONLY` | 0 | open for reading |
| `O_WRONLY` | 1 | open for writing |
| `O_RDWR` | 2 | open for reading and writing |
| `O_CREAT` | 0100 | create the file if needed |
| `O_TRUNC` | 01000 | truncate an existing file |

```c
int fd = open("OUT.BIN", O_WRONLY | O_CREAT | O_TRUNC);
if (fd >= 0) {
    write(fd, data, len);
    close(fd);
}
```

!!! tip "Single file I/O core"
    `open`/`read`/`write`/`close`/`lseek`/`unlink`/`fsync`/`fdatasync` share one
    FCB/DMA core. The first file call links that core; additional file calls are
    nearly free. See the [appendix](appendix/01-dccrtlstrip.md).

## `dirent.h` — directory enumeration

Include `dirent.h`. CP/M has no subdirectories, so this enumerates files on the
selected drive.

| Function | Summary |
| --- | --- |
| `DIR *opendir(const char *path)` | Begin a scan (`"."`, `"*.*"`, or `"A:"`). |
| `struct dirent *readdir(DIR *dirp)` | Next entry, or `NULL` at the end. |
| `int closedir(DIR *dirp)` | End the scan. |

`struct dirent` has a single member, `char d_name[13]`, holding the 8.3 name.
The public API uses POSIX-like names through macros:

| Public name | Runtime entry name |
| --- | --- |
| `opendir` | `dopn` |
| `readdir` | `drd` |
| `closedir` | `dcls` |

Call the public names. The short runtime names exist to avoid
external-symbol collisions on the M80/L80 toolchain.

```c
DIR *d = opendir("*.*");
struct dirent *e;

while ((e = readdir(d)) != NULL)
    puts(e->d_name);
closedir(d);
```

## Standard diagnostics and errors

The standard-library reference now has dedicated pages for
[error reporting](standard-lib/02-errno.md) and
[assertions](standard-lib/01-assert.md). The CP/M file runtime uses `errno` for
file-related failures, and `assert` writes its diagnostic through `stderr` before
terminating with `exit(1)`.

## CP/M extensions

The runtime exposes the raw CP/M BDOS entry point for things the standard
library doesn't cover (console status, direct disk calls, and so on). It is
declared in `stdlib.h`:

```c
int bdos(int fn, int dearg);
```

`fn` is the BDOS function number and `dearg` is the value passed in `DE`; the
byte result comes back in the low byte of the returned `int`. Calls whose useful
result is an FCB/DMA region (directory and file operations) return their data
through the memory `dearg` points at, not in the return value.

### Non-blocking console input

The standard input calls (`getchar`, `getc`, `fgets`, `scanf`) are blocking
C-style input. For games, menus, terminal UIs, and other polling loops, use the
runtime's `kbhit()` and `getch()` (declared in `stdio.h`):

- `int kbhit(void)` returns nonzero when a key is waiting and `0` otherwise. It
  never blocks and does not consume the character.
- `int getch(void)` reads one key without echo. It blocks until a key is ready,
  so it is normally guarded by `kbhit()`.

```c
#include <stdio.h>

int main(void)
{
    int ch;

    if (kbhit()) {          /* non-blocking test */
        ch = getch();       /* safe: a key is already waiting */
        if (ch)
            handle_key(ch);
    }
    return 0;
}
```

Under the hood `kbhit()` is CP/M BDOS function 11 (console status) and `getch()`
is BDOS function 6 (direct console input, `E = 0xff`). Raw calls can be made
through `bdos()`, but the named functions are clearer and also flush pending
buffered output before blocking:

```c
#include <stdlib.h>

int raw_kbhit(void)        { return bdos(11, 0) != 0; }
int raw_getch_nonblock(void) { return bdos(6, 0xff); }  /* 0 = no key ready */
```

BDOS function 6 uses `0` as the "no character" sentinel, so it is best for
keyboard-style console input rather than protocols where NUL is meaningful.
Do not mix raw `bdos()` console I/O with buffered console functions in the same
code path; direct BDOS calls bypass the console output buffer used by
`printf`/`puts`.

### Direct port I/O

For talking to hardware or an emulator's virtual devices, the runtime also
provides 8-bit port I/O, declared alongside `bdos` in `stdlib.h`:

```c
int  inp(unsigned port);                 /* IN  A,(port) -> 0..255 */
void outp(unsigned port, unsigned val);  /* OUT (port),A           */
```

`inp` runs the Z80 `IN A,(port)` instruction and returns the byte read,
zero-extended to `int` (so the result is always 0..255). `outp` runs
`OUT (port),A`, sending the low byte of `val` to the port. Only the low 8 bits
of `port` are significant. Neither is part of C89.
