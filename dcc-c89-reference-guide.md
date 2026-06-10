# dcc C89 Reference Guide

A practical reference for the C runtime library that ships with **dcc**, the
C89 compiler that targets CP/M 2.2 on the Z80.

This first section documents the runtime functions that are actually
implemented in [DCCRTL.MAC](DCCRTL.MAC) (the assembly C runtime) and how to use
them from your C source. Functions that are declared in the standard headers
but are *not* part of the linked runtime are called out explicitly in
[Declared but not in the runtime](#declared-but-not-in-the-runtime) so you don't
get surprised by a link error.

---

## Contents

- [dcc C89 Reference Guide](#dcc-c89-reference-guide)
  - [Contents](#contents)
  - [How the runtime fits together](#how-the-runtime-fits-together)
  - [Build and link basics](#build-and-link-basics)
  - [Type sizes and conventions](#type-sizes-and-conventions)
  - [C89 keywords (reserved words)](#c89-keywords-reserved-words)
    - [Not supported](#not-supported)
    - [Semantics of the accepted-but-inert qualifiers](#semantics-of-the-accepted-but-inert-qualifiers)
    - [Beyond C89](#beyond-c89)
  - [stdio.h — console and file I/O](#stdioh--console-and-file-io)
    - [Formatted output](#formatted-output)
    - [Character and string output](#character-and-string-output)
    - [Character input](#character-input)
    - [File streams](#file-streams)
    - [printf-family conversions](#printf-family-conversions)
    - [scanf family conversions](#scanf-family-conversions)
  - [stdlib.h — memory, conversion, arithmetic, process, random](#stdlibh--memory-conversion-arithmetic-process-random)
    - [Dynamic memory](#dynamic-memory)
    - [Conversion](#conversion)
    - [Integer arithmetic helpers](#integer-arithmetic-helpers)
    - [Process control](#process-control)
    - [Pseudo-random numbers](#pseudo-random-numbers)
  - [string.h — strings and memory blocks](#stringh--strings-and-memory-blocks)
  - [ctype.h — character classification](#ctypeh--character-classification)
  - [math.h — single-precision float](#mathh--single-precision-float)
  - [setjmp.h — non-local jumps](#setjmph--non-local-jumps)
  - [stdarg.h — variadic arguments](#stdargh--variadic-arguments)
  - [stddef.h — common definitions](#stddefh--common-definitions)
  - [unistd.h / fcntl.h — low-level file I/O](#unistdh--fcntlh--low-level-file-io)
  - [dirent.h — directory enumeration](#direnth--directory-enumeration)
  - [errno.h — error reporting](#errnoh--error-reporting)
  - [assert.h — assertions](#asserth--assertions)
  - [CP/M extensions](#cpm-extensions)
  - [Declared but not in the runtime](#declared-but-not-in-the-runtime)
  - [Limitations to keep in mind](#limitations-to-keep-in-mind)

---

## How the runtime fits together

[DCCRTL.MAC](DCCRTL.MAC) is the single source of truth for the runtime. It is
written in Z80 assembly (M80/L80 syntax) for size and speed and provides:

- the `start` entrypoint that sets up the heap and parses the command line into
  `argc` / `argv` before calling your `main`,
- a small subset of the C89 library (the functions documented below),
- 16-/32-bit integer and 32-bit float arithmetic helpers the compiler calls
  implicitly.

Most library names in the standard headers are ordinary C identifiers. Because
L80/M80 only preserve a short, significant prefix of external symbols, the
compiler maps well-known library calls to short internal assembler labels (for
example `memcpy` becomes `__mcpy`, `strlen` becomes `__slen`). You never write
those short names yourself — just call the standard C functions and include the
matching header.

The optional [dccrtlstrip](dccrtlstrip.c) pass scans your program and removes
the runtime routines you don't use, producing a smaller `RTLMIN.MAC` that is
linked instead of the whole runtime. It follows symbol references
automatically, so you only pay for what you call.

## Build and link basics

Compile, optimize, strip the runtime, assemble, and link in one step with the
helper script:

```sh
sh ./ma.sh foo            # builds foo.c -> FOO.COM (peephole optimized)
sh ./ma.sh foo nopeep     # same, but skip the dccpeep optimizer
```

The underlying compiler invocation is:

```text
dcc [options] input.c [-o output.mac]
```

Common options:

| Option                                      | Meaning                                                              |
| ------------------------------------------- | -------------------------------------------------------------------- |
| `-o file`                                   | Write M80 assembly to `file`; default is `out.mac`, `-` is stdout.   |
| `-c`, `-module`                             | Emit a linkable helper module, not a final program translation unit. |
| `-f`, `-ffloatio`                           | Enable floating-point `printf` formatting support.                   |
| `-s bytes`, `-stack bytes`, `--stack bytes` | Reserve stack bytes; default is 512.                                 |
| `-s=bytes`, `-stack=bytes`, `--stack=bytes` | Equivalent attached forms for the stack size.                        |
| `-I dir`, `-Idir`                           | Add an include search directory.                                     |
| `-D name[=value]`, `-Dname[=value]`         | Predefine a macro.                                                   |
| `-U name`, `-Uname`                         | Undefine a preprocessor macro.                                       |
| `-v`, `--version`                           | Print the compiler version and exit.                                 |
| `-h`, `--help`                              | Print compiler help and exit.                                        |

Options that most directly affect the runtime:

- `-f` / `-ffloatio`: pull in floating-point support for the `printf` family. You
  **must** pass this if your format strings use `%f`; otherwise float
  formatting is not linked in. See [printf-family conversions](#printf-family-conversions).
- `-s` / `-stack` / `--stack`: reserve stack space (default 512; accepted range
  is 0..32767). The heap used by
  `malloc` lives between the end of BSS and the bottom of the stack, so growing
  the stack shrinks the heap and vice versa. There are no runtime checks that
  stop the stack from smashing the heap.
- `-Dname[=value]`: predefine a macro. `_DCC_=1` is always defined.

Include the standard headers as usual:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

## Type sizes and conventions

| Type           | Size    | Notes                                         |
| -------------- | ------- | --------------------------------------------- |
| `char`         | 8 bits  | signed by default                             |
| `int`, `short` | 16 bits | `int` is 16-bit; watch for overflow           |
| `long`         | 32 bits | use `%ld` and the `l` length modifier         |
| `float`        | 32 bits | the only floating type — **no `double`**      |
| pointer        | 16 bits | flat CP/M address space                       |
| `size_t`       | 16 bits | unsigned `int`                                |
| `FILE`         | 16 bits | `typedef int FILE`; streams are small handles |

Useful constants from the headers:

- [stdio.h](stdio.h): `EOF` = -1, `BUFSIZ` = 128, `SEEK_SET`/`SEEK_CUR`/`SEEK_END` = 0/1/2.
- [stdlib.h](stdlib.h): `EXIT_SUCCESS` = 0, `EXIT_FAILURE` = 1, `RAND_MAX` = 32767, `NULL` = 0.
- [errno.h](errno.h): `EDOM` = 33, `ERANGE` = 34.

---

## C89 keywords (reserved words)

dcc recognizes 31 of the 32 standard C89 keywords. The compiler's keyword table
lives in [dcc.c](dcc.c) (`keyword_kind`). The only C89 keyword that is **not**
recognized is `double`, because dcc has no 8-byte floating type — use `float`
instead (see [Type sizes and conventions](#type-sizes-and-conventions)).

| Category         | Keywords                                                                                               |
| ---------------- | ------------------------------------------------------------------------------------------------------ |
| Types            | `char`, `short`, `int`, `long`, `signed`, `unsigned`, `float`, `void`                                  |
| Storage class    | `auto`, `extern`, `register`, `static`, `typedef`                                                      |
| Type qualifiers  | `const`, `volatile`                                                                                    |
| Aggregates/enums | `struct`, `union`, `enum`                                                                              |
| Control flow     | `if`, `else`, `switch`, `case`, `default`, `for`, `while`, `do`, `break`, `continue`, `goto`, `return` |
| Operators        | `sizeof`                                                                                               |

### Not supported

| Keyword  | Reason                                                          |
| -------- | --------------------------------------------------------------- |
| `double` | No 8-byte floating type; `double` is unrecognized. Use `float`. |

### Semantics of the accepted-but-inert qualifiers

A few keywords are parsed so your source compiles, but they do not change code
generation:

- `const` — accepted; honored only for constant folding of const-initialized
  variables. It does not place data in read-only memory.
- `volatile` — accepted but otherwise ignored.
- `register` — accepted as a hint only; it does not force register allocation.
- `auto` — accepted; since it is already the default storage for locals, it is a
  no-op.

### Beyond C89

- `inline` — accepted as a non-C89 (C99) extension and then ignored; functions
  are always emitted normally. No other C99/C11 keywords (`restrict`, `_Bool`,
  `_Complex`, and so on) are recognized.

---

## stdio.h — console and file I/O

Include [stdio.h](stdio.h). The predefined streams `stdin`, `stdout`, and
`stderr` are available.

### Formatted output

| Function                                       | Summary                                         |
| ---------------------------------------------- | ----------------------------------------------- |
| `int printf(const char *fmt, ...)`             | Formatted output to the console (`stdout`).     |
| `int fprintf(FILE *fp, const char *fmt, ...)`  | Formatted output to a stream.                   |
| `int sprintf(char *buf, const char *fmt, ...)` | Formatted output into a caller-supplied buffer. |

All three share one formatting engine, so they accept the same conversions (see
[printf-family conversions](#printf-family-conversions)).

```c
printf("count=%d name=%s hex=%04x\n", n, name, addr);
sprintf(line, "%ld bytes", total);
fprintf(stderr, "error %d\n", errno);
```

### Character and string output

| Function                             | Summary                                       |
| ------------------------------------ | --------------------------------------------- |
| `int putchar(int c)`                 | Write one character to the console.           |
| `int putc(int c, FILE *fp)`          | Write one character to a stream.              |
| `int fputc(int c, FILE *fp)`         | Same as `putc`.                               |
| `int puts(const char *s)`            | Write a string plus a newline to the console. |
| `int fputs(const char *s, FILE *fp)` | Write a string (no newline) to a stream.      |

### Character input

| Function             | Summary                              |
| -------------------- | ------------------------------------ |
| `int getchar(void)`  | Read one character from the console. |
| `int getc(FILE *fp)` | Read one character from a stream.    |

### File streams

| Function                                                        | Summary                                                             |
| --------------------------------------------------------------- | ------------------------------------------------------------------- |
| `FILE *fopen(const char *path, const char *mode)`               | Open a file. Modes: `"r"`, `"w"`, `"a"`, with optional `"+"`/`"b"`. |
| `int fclose(FILE *fp)`                                          | Flush and close a stream.                                           |
| `int fflush(FILE *fp)`                                          | Flush buffered writes to disk.                                      |
| `char *fgets(char *buf, int n, FILE *fp)`                       | Read a line (up to `n-1` chars).                                    |
| `size_t fread(void *buf, size_t sz, size_t n, FILE *fp)`        | Read `n` elements of `sz` bytes.                                    |
| `size_t fwrite(const void *buf, size_t sz, size_t n, FILE *fp)` | Write `n` elements of `sz` bytes.                                   |
| `int fseek(FILE *fp, long off, int whence)`                     | Reposition using `SEEK_SET`/`CUR`/`END`.                            |
| `long ftell(FILE *fp)`                                          | Report the current file position.                                   |
| `void rewind(FILE *fp)`                                         | Seek to the beginning.                                              |
| `int feof(FILE *fp)`                                            | Non-zero after end-of-file is reached.                              |
| `int ferror(FILE *fp)`                                          | Non-zero if an error flag is set.                                   |
| `void clearerr(FILE *fp)`                                       | Clear the EOF and error flags.                                      |
| `void setbuf(FILE *fp, char *buf)`                              | Set a stream buffer.                                                |
| `int remove(const char *path)`                                  | Delete a file.                                                      |
| `void perror(const char *s)`                                    | Print `s` plus the current error text.                              |

```c
FILE *fp = fopen("DATA.TXT", "r");
if (fp) {
    char line[128];
    while (fgets(line, sizeof line, fp))
        fputs(line, stdout);
    fclose(fp);
}
```

### printf-family conversions

The formatter supports these conversion specifiers:

| Specifier  | Meaning                                                   |
| ---------- | --------------------------------------------------------- |
| `%d`, `%i` | Signed 16-bit decimal.                                    |
| `%u`       | Unsigned 16-bit decimal.                                  |
| `%x`, `%X` | Unsigned 16-bit hex (lower / upper case).                 |
| `%c`       | Single character.                                         |
| `%s`       | NUL-terminated string.                                    |
| `%f`       | 32-bit float. **Requires the `-ffloatio` compiler flag.** |
| `%%`       | A literal percent sign.                                   |

Length modifiers:

- `l` — promotes the integer conversions to 32-bit `long`: `%ld`, `%li`, `%lu`,
  `%lx`, `%lX`.
- `z` — `size_t` width. Since `size_t` is 16-bit here, `%zu`/`%zd`/`%zi`/`%zx`/`%zX`
  behave like the plain 16-bit conversions.

Field width and flags:

- A decimal **field width** (for example `%6d`, `%10s`) pads with spaces on the
  left to the requested width.
- The `-` flag (for example `%-6d`) left-justifies within the field.
- A **precision** on integer conversions (for example `%.4d`) zero-fills the
  number to at least that many digits.

```c
printf("|%6d|%-6d|%.4d|\n", 42, 42, 42);   /* |    42|42    |0042| */
printf("%lu items, %lx flags\n", count, mask);
```

Not supported: the `+`, space, and `#` flags, and `*` (run-time) width or
precision. Use fixed widths in the format string.

### scanf family conversions

The input formatter supports a small, non-floating C89 subset shared by
`scanf`, `sscanf`, and `fscanf`.

| Specifier      | Meaning                                               |
| -------------- | ----------------------------------------------------- |
| `%d`           | Signed decimal integer.                               |
| `%i`           | Signed integer with base auto-detected (`0`, `0x`).   |
| `%u`           | Unsigned decimal integer.                             |
| `%x`, `%X`     | Unsigned hexadecimal integer, optional `0x` prefix.   |
| `%o`           | Unsigned octal integer.                               |
| `%c`           | Character input; does not skip leading whitespace.    |
| `%s`           | Non-whitespace string, NUL-terminated by the runtime. |
| `%%`           | A literal percent sign.                               |

Supported modifiers:

- A decimal field width limits the maximum input consumed by a conversion.
- `*` suppresses assignment, so the input is consumed but no argument is read
  and the assignment count is not incremented.
- `h` is accepted for integer conversions; on dcc, `short` is the same size as
  `int`, so it behaves like the plain conversion.
- `l` stores integer conversions through a `long *` / `unsigned long *`.

Not supported: floating input (`%f`, `%e`, `%g`), scansets (`%[...]`), `%n`, and
`%p`.

```c
int n, value;
char word[16];
long big;

n = sscanf("-12 hello 0x2a", "%d %s %i", &value, word, &value);
n = sscanf("123456", "%ld", &big);
```

---

## stdlib.h — memory, conversion, arithmetic, process, random

Include [stdlib.h](stdlib.h).

### Dynamic memory

| Function                                  | Summary                                        |
| ----------------------------------------- | ---------------------------------------------- |
| `void *malloc(size_t n)`                  | Allocate `n` bytes; returns `NULL` on failure. |
| `void *calloc(size_t nmemb, size_t size)` | Allocate and zero `nmemb * size` bytes.        |
| `void *realloc(void *p, size_t n)`        | Resize a block, preserving contents.           |
| `void free(void *p)`                      | Return a block to the heap.                    |

The allocator is a first-fit free list with a 3-byte block header. Freeing a
block coalesces it with adjacent free neighbors (including blocks freed via
`realloc(p, 0)` and the old block released by a growing `realloc`), which keeps
fragmentation down. The heap grows on demand between the end of BSS and the
stack.

```c
char *p = malloc(256);
if (!p) { fputs("out of memory\n", stderr); exit(EXIT_FAILURE); }
p = realloc(p, 512);        /* old contents preserved */
free(p);
```

`realloc` follows the standard rules: `realloc(NULL, n)` behaves like
`malloc(n)`, and `realloc(p, 0)` frees `p` and returns `NULL`.

### Conversion

| Function                  | Summary                                          |
| ------------------------- | ------------------------------------------------ |
| `int atoi(const char *s)` | Parse a leading signed decimal integer (16-bit). |

```c
int n = atoi("  -123xyz");   /* -123 */
```

### Integer arithmetic helpers

| Function                              | Summary                                    |
| ------------------------------------- | ------------------------------------------ |
| `int abs(int j)`                      | Absolute value of a 16-bit signed integer. |
| `long labs(long j)`                   | Absolute value of a 32-bit signed long.    |
| `div_t div(int numer, int denom)`     | Signed 16-bit quotient and remainder.      |
| `ldiv_t ldiv(long numer, long denom)` | Signed 32-bit quotient and remainder.      |

`div` returns a `div_t` with `quot` and `rem` members. `ldiv` returns an
`ldiv_t` with 32-bit `quot` and `rem` members. dcc's runtime truncates signed
division toward zero; the remainder has the same sign as the numerator.

```c
div_t d = div(-7, 3);       /* d.quot == -2, d.rem == -1 */
ldiv_t ld = ldiv(200000L, 7L);
```

### Process control

| Function              | Summary                                        |
| --------------------- | ---------------------------------------------- |
| `void exit(int code)` | Flush, terminate, and return `code` to the OS. |

The exit code is also surfaced through CP/M 3.0 BDOS call 108, which emulators
such as ntvcm reflect in their own process exit code. Returning a value from
`main` has the same effect.

### Pseudo-random numbers

| Function                    | Summary                                       |
| --------------------------- | --------------------------------------------- |
| `int rand(void)`            | Next pseudo-random number in `0 .. RAND_MAX`. |
| `void srand(unsigned seed)` | Seed the generator.                           |

`RAND_MAX` is 32767.

```c
srand(1);
int r = rand() % 6 + 1;     /* a die roll */
```

---

## string.h — strings and memory blocks

Include [string.h](string.h). All of the following are implemented in the
runtime.

| Function                                              | Summary                                  |
| ----------------------------------------------------- | ---------------------------------------- |
| `size_t strlen(const char *s)`                        | Length excluding the NUL.                |
| `char *strcpy(char *d, const char *s)`                | Copy including the NUL.                  |
| `char *strncpy(char *d, const char *s, size_t n)`     | Copy at most `n`, NUL-pad the remainder. |
| `char *strcat(char *d, const char *s)`                | Append `s` to `d`.                       |
| `char *strncat(char *d, const char *s, size_t n)`     | Append at most `n` chars plus NUL.       |
| `int strcmp(const char *a, const char *b)`            | Lexicographic compare.                   |
| `int strncmp(const char *a, const char *b, size_t n)` | Compare at most `n` chars.               |
| `int strcoll(const char *a, const char *b)`           | Locale compare (same as `strcmp` here).  |
| `char *strchr(const char *s, int c)`                  | First occurrence of `c`.                 |
| `char *strrchr(const char *s, int c)`                 | Last occurrence of `c`.                  |
| `char *strstr(const char *h, const char *n)`          | First occurrence of substring `n`.       |
| `size_t strspn(const char *s, const char *set)`       | Length of leading run in `set`.          |
| `size_t strcspn(const char *s, const char *set)`      | Length of leading run *not* in `set`.    |
| `char *strpbrk(const char *s, const char *set)`       | First char that is in `set`.             |
| `char *strdup(const char *s)`                         | Heap copy of `s` (free it later).        |
| `void *memcpy(void *d, const void *s, size_t n)`      | Copy `n` bytes (no overlap).             |
| `void *memmove(void *d, const void *s, size_t n)`     | Copy `n` bytes (overlap safe).           |
| `void *memset(void *d, int c, size_t n)`              | Fill `n` bytes with `c`.                 |
| `int memcmp(const void *a, const void *b, size_t n)`  | Compare `n` bytes.                       |
| `void *memchr(const void *s, int c, size_t n)`        | Find `c` within `n` bytes.               |
| `char *strerror(int err)`                             | Text for an error number.                |

```c
char dst[16];
strcpy(dst, "hello");
if (strcmp(dst, "hello") == 0)
    memset(dst, 0, sizeof dst);
```

---

## ctype.h — character classification

Include [ctype.h](ctype.h). Each takes an `int` and returns non-zero for a
match (or the converted character for the two case functions).

| Function              | True when the character is…  |
| --------------------- | ---------------------------- |
| `int isalpha(int c)`  | a letter                     |
| `int isdigit(int c)`  | a decimal digit              |
| `int isalnum(int c)`  | a letter or digit            |
| `int isspace(int c)`  | whitespace                   |
| `int isupper(int c)`  | an uppercase letter          |
| `int islower(int c)`  | a lowercase letter           |
| `int isxdigit(int c)` | a hex digit                  |
| `int isprint(int c)`  | printable (including space)  |
| `int iscntrl(int c)`  | a control character          |
| `int ispunct(int c)`  | punctuation                  |
| `int toupper(int c)`  | — returns the uppercase form |
| `int tolower(int c)`  | — returns the lowercase form |

```c
for (char *p = s; *p; ++p)
    *p = toupper(*p);
```

---

## math.h — single-precision float

Include [math.h](math.h). dcc has only 32-bit `float` (no `double`), so the
math entry points are the single-precision `…f` variants.

| Function                             | Summary                              |
| ------------------------------------ | ------------------------------------ |
| `float fabsf(float x)`               | Absolute value.                      |
| `float floorf(float x)`              | Round toward negative infinity.      |
| `float ceilf(float x)`               | Round toward positive infinity.      |
| `float fmodf(float x, float y)`      | Floating-point remainder of `x / y`. |
| `float sqrtf(float x)`               | Square root.                         |
| `float nextafterf(float x, float y)` | Next representable value after `x`.  |

To print floats, compile with `-ffloatio` and use `%f`:

```c
float r = sqrtf(2.0f);
printf("%f\n", r);          /* needs -ffloatio */
```

---

## setjmp.h — non-local jumps

Include [setjmp.h](setjmp.h). `jmp_buf` is an 8-byte buffer (saved return
address, SP, and IX).

| Function                             | Summary                                                 |
| ------------------------------------ | ------------------------------------------------------- |
| `int setjmp(jmp_buf env)`            | Save the current context; returns 0 on the direct call. |
| `void longjmp(jmp_buf env, int val)` | Jump back to the `setjmp`, which returns `val` (or 1).  |

```c
jmp_buf env;
if (setjmp(env) == 0) {
    /* normal path */
    work(env);
} else {
    /* arrived here via longjmp */
    recover();
}
/* inside work(): longjmp(env, 1); */
```

---

## stdarg.h — variadic arguments

Include [stdarg.h](stdarg.h). `va_list` is a `char *` walking the stack frame.

| Macro                | Summary                                          |
| -------------------- | ------------------------------------------------ |
| `va_start(ap, last)` | Begin traversal after the named argument `last`. |
| `va_arg(ap, type)`   | Fetch the next argument of `type`.               |
| `va_end(ap)`         | Finish traversal.                                |

```c
int sum(int count, ...) {
    va_list ap;
    int total = 0;
    va_start(ap, count);
    for (int i = 0; i < count; ++i)
        total += va_arg(ap, int);
    va_end(ap);
    return total;
}
```

---

## stddef.h — common definitions

Include [stddef.h](stddef.h) for common C definitions used by library headers
and portable data-structure code.

| Name                       | Summary                                                  |
| -------------------------- | -------------------------------------------------------- |
| `size_t`                   | Unsigned 16-bit size type.                               |
| `offsetof(type, member)`   | Compile-time byte offset of a struct/union member.       |

`offsetof` is implemented by the compiler as `__offsetof(type, member)`. It
accepts struct and union member designators, including nested member access and
constant array indexes.

```c
#include <stddef.h>

struct rec {
  char tag;
  int value;
};

int off = offsetof(struct rec, value);
```

---

## unistd.h / fcntl.h — low-level file I/O

These map onto CP/M file operations and operate on small integer file
descriptors. Include [unistd.h](unistd.h) and [fcntl.h](fcntl.h).

| Function                                         | Summary                                   |
| ------------------------------------------------ | ----------------------------------------- |
| `int open(const char *path, int flags, ...)`     | Open/create a file, returns a descriptor. |
| `int read(int fd, void *buf, unsigned n)`        | Read up to `n` bytes.                     |
| `int write(int fd, const void *buf, unsigned n)` | Write up to `n` bytes.                    |
| `int close(int fd)`                              | Close a descriptor.                       |
| `long lseek(int fd, long off, int whence)`       | Reposition the descriptor.                |
| `int unlink(const char *path)`                   | Delete a file.                            |
| `int fsync(int fd)`                              | Flush file data to disk.                  |
| `int fdatasync(int fd)`                          | Flush file data to disk.                  |

`open` flags from [fcntl.h](fcntl.h): `O_RDONLY` (0), `O_WRONLY` (1), `O_RDWR`
(2), `O_CREAT` (0100 octal), `O_TRUNC` (01000 octal). `off_t` is `long`.

```c
int fd = open("OUT.BIN", O_WRONLY | O_CREAT | O_TRUNC);
if (fd >= 0) {
    write(fd, data, len);
    close(fd);
}
```

---

## dirent.h — directory enumeration

Include [dirent.h](dirent.h). CP/M has no subdirectories, so this enumerates
files on the selected drive. The POSIX-style names are macros over the short
runtime entry points (`opendir` → `dopn`, `readdir` → `drd`, `closedir` →
`dcls`).

| Function                            | Summary                                   |
| ----------------------------------- | ----------------------------------------- |
| `DIR *opendir(const char *path)`    | Begin a scan (`"."`, `"*.*"`, or `"A:"`). |
| `struct dirent *readdir(DIR *dirp)` | Next entry, or `NULL` at the end.         |
| `int closedir(DIR *dirp)`           | End the scan.                             |

`struct dirent` has a single member, `char d_name[13]`, holding the 8.3 name.

```c
DIR *d = opendir("*.*");
struct dirent *e;
while ((e = readdir(d)) != NULL)
    puts(e->d_name);
closedir(d);
```

See [cpmenumd.c](cpmenumd.c) for a complete example.

---

## errno.h — error reporting

Include [errno.h](errno.h). The runtime is single-threaded, so `errno` is a
single global `int`.

- `extern int errno;`
- `EDOM` = 33, `ERANGE` = 34.

Pair it with `perror` or `strerror` to render a message:

```c
if (!fopen("MISSING.DAT", "r"))
    perror("open");
```

---

## assert.h — assertions

Include [assert.h](assert.h). `assert(expr)` prints a diagnostic to `stderr`
and calls `exit(1)` when `expr` is false. Define `NDEBUG` before including the
header to compile assertions out.

```c
#include <assert.h>
assert(ptr != NULL);
```

---

## CP/M extensions

The runtime exposes the raw CP/M BDOS entry point for things the standard
library doesn't cover (console status, direct disk calls, and so on). It is not
declared in a header, so declare it yourself:

```c
extern int bdos(int fn, int dearg);
```

`fn` is the BDOS function number and `dearg` is the value passed in `DE`; the
result comes back in `HL`/`A`. See [tbdos.c](tbdos.c) and [crc.c](crc.c) for
working examples.

---

## Declared but not in the runtime

Some functions are declared in [stdlib.h](stdlib.h) for source compatibility
but are **not** implemented in [DCCRTL.MAC](DCCRTL.MAC). If you call them
without supplying your own definition, the link step will fail with an
unresolved external. Copy a sample implementation into your project (or write
your own):

| Function(s)                          | Where to get an implementation                           |
| ------------------------------------ | -------------------------------------------------------- |
| `bsearch`                            | [bsearch.c](bsearch.c)                                   |
| `qsort`                              | [qsort.c](qsort.c)                                       |
| `atol`                               | shown inline in [primes.c](primes.c), [pihex.c](pihex.c) |

`atof` is neither declared in [stdlib.h](stdlib.h) nor implemented in the
runtime. C89 `atof` returns `double`, and dcc does not have an 8-byte `double`
type; use a project-local `float` parser if you need decimal-to-float input.

To reuse a sample, either `#include "bsearch.c"` from your main file, or
compile it separately with `dcc -c` and link the resulting `.REL` (see
[cpmenumd.c](cpmenumd.c) and [mrel.sh](mrel.sh) for the separate-compilation
workflow).

---

## Limitations to keep in mind

- **No `double`.** Only 32-bit `float` exists; all math is single precision.
- **`int` is 16-bit.** Use `long` (and `%ld`) when you need more than ±32767.
- **`scanf` is integer/string only.** Floating input, scansets, `%n`, and `%p`
  are not implemented.
- **No `+`/space/`#` printf flags and no `*` width/precision.** Use literal
  field widths.
- **`%f` needs `-ffloatio`.** Without that flag, float formatting isn't linked.
- **No stack/heap guard.** The heap and stack share memory; size the stack with
  `-stack` and see [spsmash.c](spsmash.c) for an optional manual stack check.
- **CP/M 2.2 only.** The runtime uses BDOS functions only (no BIOS calls), plus
  CP/M 3.0 BDOS 108 for the process exit code.
