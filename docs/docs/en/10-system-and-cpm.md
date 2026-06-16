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

!!! tip "One shared core"
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

You normally call the public names. The short runtime names exist to avoid
external-symbol collisions on the M80/L80 toolchain.

```c
DIR *d = opendir("*.*");
struct dirent *e;

while ((e = readdir(d)) != NULL)
    puts(e->d_name);
closedir(d);
```

## `errno.h` — error reporting

Include `errno.h`. The runtime is single-threaded, so `errno` is a single global
`int`.

- `extern int errno;`
- C89 requires `EDOM` and `ERANGE`; dcc also defines file-related values used by
    the CP/M file runtime.

| Macro | Value | Meaning |
| --- | ---: | --- |
| `EPERM` | 1 | operation not permitted |
| `ENOENT` | 2 | no such file or directory |
| `ESRCH` | 3 | no such process |
| `EINTR` | 4 | interrupted call |
| `EIO` | 5 | I/O error |
| `ENXIO` | 6 | no such device or address |
| `E2BIG` | 7 | argument list too long |
| `ENOEXEC` | 8 | exec format error |
| `EBADF` | 9 | bad file descriptor |
| `ECHILD` | 10 | no child processes |
| `EAGAIN` | 11 | try again |
| `ENOMEM` | 12 | not enough memory |
| `EACCES` | 13 | permission denied |
| `EFAULT` | 14 | bad address |
| `EBUSY` | 16 | resource busy |
| `EEXIST` | 17 | file exists |
| `EXDEV` | 18 | cross-device link |
| `ENODEV` | 19 | no such device |
| `ENOTDIR` | 20 | not a directory |
| `EISDIR` | 21 | is a directory |
| `EINVAL` | 22 | invalid argument |
| `ENFILE` | 23 | system file table full |
| `EMFILE` | 24 | too many open files |
| `ENOTTY` | 25 | inappropriate device control |
| `EFBIG` | 27 | file too large |
| `ENOSPC` | 28 | no space left |
| `ESPIPE` | 29 | illegal seek |
| `EROFS` | 30 | read-only file system |
| `EMLINK` | 31 | too many links |
| `EPIPE` | 32 | broken pipe |
| `EDOM` | 33 | domain error |
| `ERANGE` | 34 | range error |
| `ENOTEMPTY` | 39 | directory not empty |

Pair it with `perror` or `strerror` to render a message:

```c
if (!fopen("MISSING.DAT", "r"))
    perror("open");
```

## `assert.h` — assertions

Include `assert.h`. `assert(expr)` prints a diagnostic to `stderr` and calls
`exit(1)` when `expr` is false. Define `NDEBUG` before including the header to
compile assertions out.

```c
#include <assert.h>
assert(ptr != NULL);
```

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

The standard input calls (`getchar`, `getc`, `fgets`, `scanf`) should be treated
as blocking C-style input. For games, menus, terminal UIs, and other polling
loops, use CP/M BDOS directly:

- `bdos(11, 0)` checks console status and returns nonzero when a character is
    waiting.
- `bdos(6, 0xff)` performs direct console input and returns `0` when no
    character is ready, otherwise the character value. It does not echo.

```c
#include <stdlib.h>

int cpm_kbhit(void)
{
    return bdos(11, 0) != 0;
}

int cpm_getch_nonblock(void)
{
    return bdos(6, 0xff);       /* 0 means no character is ready */
}

int main(void)
{
    int ch;

    if (cpm_kbhit()) {
    ch = cpm_getch_nonblock();
    if (ch)
        handle_key(ch);
    }
    return 0;
}
```

BDOS function 6 uses `0` as the "no character" sentinel, so it is best for
keyboard-style console input rather than protocols where NUL is meaningful.
Avoid mixing direct console I/O with buffered console input in the same code
path, because direct BDOS calls can bypass console buffers used by other input
functions.

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
