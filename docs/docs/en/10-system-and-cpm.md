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

`open` flags from `fcntl.h`: `O_RDONLY` (0), `O_WRONLY` (1), `O_RDWR` (2),
`O_CREAT` (0100 octal), `O_TRUNC` (01000 octal). `off_t` is `long`.

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
- `EDOM` = 33, `ERANGE` = 34.

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
