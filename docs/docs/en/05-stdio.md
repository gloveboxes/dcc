# Console and file I/O (`stdio.h`)

Include [`stdio.h`](05-stdio.md). The predefined streams `stdin`, `stdout`, and
`stderr` are available.

## Types, streams, and constants

| Name | Meaning |
| --- | --- |
| `FILE` | Stream handle type; `typedef int FILE`. |
| `stdin`, `stdout`, `stderr` | Predefined console stream pointers. |
| `NULL` | Null pointer constant, defined as `0` if not already defined. |
| `EOF` | End-of-file / error return value, `-1`. |
| `SEEK_SET`, `SEEK_CUR`, `SEEK_END` | `fseek` origins: beginning, current position, end. |
| `BUFSIZ` | Default buffer size, `256`. |
| `FOPEN_MAX` | Maximum C89 stream count exposed by the header, `8`. |

`stdin`, `stdout`, and `stderr` are console pseudo-streams. They are available
for portable-looking code, but console-only output is smaller when you use
`putchar`, `puts`, or integer `printf` directly.

!!! tip "Console output is cheap"
    `putchar`, `puts`, and integer `printf` write through the console routine
    that is already part of every program. The file-stream functions
    (`fputc`/`fputs`/`fprintf`) link the full low-level file core even when you
    only target the console, so prefer the console functions for console work.
    See the [appendix](appendix/01-dccrtlstrip.md) for the size figures.

## Formatted output

| Function | Summary |
| --- | --- |
| `int printf(const char *fmt, ...)` | Formatted output to the console (`stdout`). |
| `int fprintf(FILE *fp, const char *fmt, ...)` | Formatted output to a stream. |
| `int sprintf(char *buf, const char *fmt, ...)` | Formatted output into a caller-supplied buffer. |
| `int vprintf(const char *fmt, va_list ap)` | Like `printf`, taking a started `va_list`. |
| `int vfprintf(FILE *fp, const char *fmt, va_list ap)` | Like `fprintf`, taking a `va_list`. |
| `int vsprintf(char *buf, const char *fmt, va_list ap)` | Like `sprintf`, taking a `va_list`. |

All six share one formatting engine, so they accept the same conversions. The
inline-argument forms are used the usual way:

```c
printf("count=%d name=%s hex=%04x\n", n, name, addr);
sprintf(line, "%ld bytes", total);
fprintf(stderr, "error %d\n", errno);
```

The `v…` variants take a `va_list` (from
[`stdarg.h`](09-utility-headers.md#stdargh-variadic-arguments)) so you can write
your own `printf`-style wrappers that forward a format string — see the worked
[logging-wrapper example](12-examples.md#a-printf-style-logging-wrapper).

### printf conversions

| Specifier | Meaning |
| --- | --- |
| `%d`, `%i` | Signed 16-bit decimal. |
| `%u` | Unsigned 16-bit decimal. |
| `%x`, `%X` | Unsigned 16-bit hex (lower / upper case). |
| `%c` | Single character. |
| `%s` | NUL-terminated string. |
| `%f` | 32-bit float. **Requires the `-ffloatio` compiler flag.** |
| `%%` | A literal percent sign. |

Length modifiers:

- `l` — promotes integer conversions to 32-bit `long`: `%ld`, `%li`, `%lu`,
  `%lx`, `%lX`.
- `z` — `size_t` width. Since `size_t` is 16-bit here, `%zu` / `%zd` and friends
  behave like the plain 16-bit conversions.

Field width and flags:

- A decimal **field width** (`%6d`, `%10s`) pads with spaces on the left.
- The `-` flag (`%-6d`) left-justifies within the field.
- A **precision** on integer conversions (`%.4d`) zero-fills to at least that
  many digits.

```c
printf("|%6d|%-6d|%.4d|\n", 42, 42, 42);   /* |    42|42    |0042| */
printf("%lu items, %lx flags\n", count, mask);
```

Not supported: the `+`, space, and `#` flags, and `*` (run-time) width or
precision. Use fixed widths in the format string.

## Character and string output

| Function | Summary |
| --- | --- |
| `int putchar(int c)` | Write one character to the console. |
| `int putc(int c, FILE *fp)` | Write one character to a stream. |
| `int fputc(int c, FILE *fp)` | Same as `putc`. |
| `int puts(const char *s)` | Write a string plus a newline to the console. |
| `int fputs(const char *s, FILE *fp)` | Write a string (no newline) to a stream. |

`putc` and `fputc` are separate C names that map to stream-character output;
there is no `fgetc` alias in this runtime.

## Console output buffering

Console output (`printf`, `puts`, `putchar`, `fputs`/`fwrite`/`fprintf` to
`stdout`/`stderr`) is **buffered**. Characters accumulate in a buffer and are
sent to CP/M in batches, which is far faster than one BDOS call per character.
The buffer is drained automatically:

- when it fills,
- when a newline is written (in the default line-buffered mode),
- on `fflush()`,
- before any blocking console **input** (`getchar`, `getch`, `scanf`, `fgets`),
  so a prompt printed just before input is always visible first,
- and at normal program exit (`main` return, `exit()`).

| Function | Summary |
| --- | --- |
| `int fflush(FILE *fp)` | Drain any buffered console output now. `fflush(NULL)`, `fflush(stdout)`, and `fflush(stderr)` all flush the console. Returns `0`. |
| `int setvbuf(FILE *fp, char *buf, int mode, size_t size)` | Choose the buffering mode for `stdout`/`stderr`, optionally adopting your own buffer. Returns `0` on success, non-zero only for an invalid `mode`. |
| `void setbuf(FILE *fp, char *buf)` | Shorthand: `setvbuf(fp, buf, buf ? _IOFBF : _IONBF, BUFSIZ)`. |

`setvbuf` must be called **before** any output on the stream. The `mode` is one
of:

| Mode | Meaning |
| --- | --- |
| `_IOFBF` | Fully buffered — drain only when the buffer fills, on `fflush`, before input, or at exit. |
| `_IOLBF` | Line buffered — additionally drain at each newline. This is the default. |
| `_IONBF` | Unbuffered — drain after every character. |

If you pass a non-`NULL` `buf` (with `size >= 2`), the runtime **adopts your
buffer**: console output is accumulated there and a larger buffer means fewer
BDOS calls. One byte of `size` is reserved internally, so the usable capacity is
`size - 1`. A `NULL` `buf` (or `size < 2`) uses the runtime's small internal
buffer instead.

!!! warning "Buffer lifetime"
    An adopted buffer must remain valid for as long as the stream uses it. Before
    freeing it, detach it first — e.g. `setvbuf(stdout, NULL, _IOLBF, 0)` — so
    the automatic exit-time flush does not touch freed memory.

`setvbuf`/`setbuf` only configure the **console** (`stdout`/`stderr`). Called on
a file stream they are accepted but have no effect (file writes are
write-through). `stdout` and `stderr` share the one console buffer, so their
output is interleaved in the exact order written.

```c
#include <stdio.h>
#include <stdlib.h>

char *buf = malloc(4096);
setvbuf(stdout, buf, _IOFBF, 4096);  /* batch up to ~4 KB before each flush */

for (i = 0; i < 1000; i++)
    printf("line %d\n", i);          /* accumulates; few BDOS calls */

setvbuf(stdout, NULL, _IOLBF, 0);    /* detach before freeing */
free(buf);
```

## Character input

| Function | Summary |
| --- | --- |
| `int getchar(void)` | Read one character from the console; blocks until a character is available. For non-blocking keyboard polling use `kbhit()` / `getch()` (see below). |
| `int getc(FILE *fp)` | Read one character from a stream. |
| `int kbhit(void)` | Console status poll: returns nonzero if a key is waiting, `0` otherwise. Does **not** block and does **not** consume the character. |
| `int getch(void)` | Read one key from the console without echo. Blocks (polls) until a key is available; guard it with `kbhit()` for non-blocking use. |

`getchar` is the console input form. In the runtime it calls CP/M BDOS function
1 (console input), so it waits for a character and returns that character as a
non-negative `int`. `getc` reads from a stream through the `_read` path. The
runtime does not provide `fgetc`. See
[CP/M extensions](10-system-and-cpm.md#non-blocking-console-input) for more on
the non-blocking convention.

For non-blocking console input, use `kbhit()` to test whether a key is waiting,
then `getch()` to read it. `kbhit()` never blocks; `getch()` only blocks if you
call it when no key is ready, so the two are normally paired:

```c
#include <stdio.h>

int main(void)
{
  int ch;

  puts("Press Q to quit.");
  for (;;) {
    if (kbhit()) {              /* non-blocking: a key is waiting */
      ch = getch();             /* safe: will not block here */
      if (ch == 'q' || ch == 'Q')
        break;
      putchar(ch);
    }

    /* do other periodic work here */
  }
  return 0;
}
```

Because `getch()` builds on BDOS function 6 (which uses `0` as its "no
character" sentinel), this convention is best for keyboard polling and command
loops, not for input protocols where a NUL byte is meaningful. `getch()` and
`getchar()` flush any pending buffered console output before they block, so a
prompt printed just before them is always visible first.

## File streams

| Function | Summary |
| --- | --- |
| `FILE *fopen(const char *path, const char *mode)` | Open a file. Modes: `"r"`, `"w"`, `"a"`, with optional `"+"` / `"b"`. |
| `int fclose(FILE *fp)` | Flush and close a stream. |
| `int fflush(FILE *fp)` | Flush buffered output (see [Console output buffering](#console-output-buffering)). |
| `char *fgets(char *buf, int n, FILE *fp)` | Read a line (up to `n-1` chars). |
| `size_t fread(void *buf, size_t sz, size_t n, FILE *fp)` | Read `n` elements of `sz` bytes. |
| `size_t fwrite(const void *buf, size_t sz, size_t n, FILE *fp)` | Write `n` elements of `sz` bytes. |
| `int fseek(FILE *fp, long off, int whence)` | Reposition using `SEEK_SET` / `CUR` / `END`. |
| `long ftell(FILE *fp)` | Report the current file position. |
| `void rewind(FILE *fp)` | Seek to the beginning. |
| `int feof(FILE *fp)` | Non-zero after end-of-file is reached. |
| `int ferror(FILE *fp)` | Non-zero if an error flag is set. |
| `void clearerr(FILE *fp)` | Clear the EOF and error flags. |
| `void setbuf(FILE *fp, char *buf)` | Configure console buffering (see [above](#console-output-buffering)). |
| `int setvbuf(FILE *fp, char *buf, int mode, size_t size)` | Configure console buffering (see [above](#console-output-buffering)). |
| `int remove(const char *path)` | Delete a file. |
| `void perror(const char *s)` | Print `s` plus the current error text. |

For a complete program, see the worked
[file-reading example](12-examples.md#reading-a-text-file-line-by-line).

## scanf-family input

`scanf`, `sscanf`, and `fscanf` share a small, non-floating C89 subset.

| Specifier | Meaning |
| --- | --- |
| `%d` | Signed decimal integer. |
| `%i` | Signed integer with base auto-detected (`0`, `0x`). |
| `%u` | Unsigned decimal integer. |
| `%x`, `%X` | Unsigned hexadecimal, optional `0x` prefix. |
| `%o` | Unsigned octal. |
| `%c` | Character input; does not skip leading whitespace. |
| `%s` | Non-whitespace string, NUL-terminated by the runtime. |
| `%%` | A literal percent sign. |

Supported modifiers:

- A decimal field width limits the maximum input consumed by a conversion.
- `*` suppresses assignment (input is consumed but no argument is read).
- `h` is accepted for integer conversions; since `short` is the same size as
  `int`, it behaves like the plain conversion.
- `l` stores integer conversions through a `long *` / `unsigned long *`.

```c
int  value;
char word[16];
long big;

sscanf("-12 hello 0x2a", "%d %s %i", &value, word, &value);
sscanf("123456", "%ld", &big);
```

For a complete, tested program, see the worked
[`sscanf` parsing example](12-examples.md#parsing-input-with-sscanf).

Not supported: floating input (`%f`, `%e`, `%g`), scansets (`%[...]`), `%n`, and
`%p`.
