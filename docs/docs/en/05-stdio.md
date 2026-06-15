# Console and file I/O (`stdio.h`)

Include [`stdio.h`](05-stdio.md). The predefined streams `stdin`, `stdout`, and
`stderr` are available.

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
your own `printf`-style wrappers that forward a format string:

```c
#include <stdarg.h>

void logmsg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
```

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

## Character input

| Function | Summary |
| --- | --- |
| `int getchar(void)` | Read one character from the console. |
| `int getc(FILE *fp)` | Read one character from a stream. |

## File streams

| Function | Summary |
| --- | --- |
| `FILE *fopen(const char *path, const char *mode)` | Open a file. Modes: `"r"`, `"w"`, `"a"`, with optional `"+"` / `"b"`. |
| `int fclose(FILE *fp)` | Flush and close a stream. |
| `int fflush(FILE *fp)` | Flush buffered writes to disk. |
| `char *fgets(char *buf, int n, FILE *fp)` | Read a line (up to `n-1` chars). |
| `size_t fread(void *buf, size_t sz, size_t n, FILE *fp)` | Read `n` elements of `sz` bytes. |
| `size_t fwrite(const void *buf, size_t sz, size_t n, FILE *fp)` | Write `n` elements of `sz` bytes. |
| `int fseek(FILE *fp, long off, int whence)` | Reposition using `SEEK_SET` / `CUR` / `END`. |
| `long ftell(FILE *fp)` | Report the current file position. |
| `void rewind(FILE *fp)` | Seek to the beginning. |
| `int feof(FILE *fp)` | Non-zero after end-of-file is reached. |
| `int ferror(FILE *fp)` | Non-zero if an error flag is set. |
| `void clearerr(FILE *fp)` | Clear the EOF and error flags. |
| `void setbuf(FILE *fp, char *buf)` | Set a stream buffer. |
| `int remove(const char *path)` | Delete a file. |
| `void perror(const char *s)` | Print `s` plus the current error text. |

```c
FILE *fp = fopen("DATA.TXT", "r");
if (fp) {
    char line[128];
    while (fgets(line, sizeof line, fp))
        fputs(line, stdout);
    fclose(fp);
}
```

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

Not supported: floating input (`%f`, `%e`, `%g`), scansets (`%[...]`), `%n`, and
`%p`.
