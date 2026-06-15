# Memory and utilities (`stdlib.h`)

Include [`stdlib.h`](06-stdlib.md). This header covers dynamic memory, string-to
-number conversion, integer arithmetic helpers, searching and sorting, process
control, and pseudo-random numbers.

## Dynamic memory

| Function | Summary |
| --- | --- |
| `void *malloc(size_t n)` | Allocate `n` bytes; returns `NULL` on failure. |
| `void *calloc(size_t nmemb, size_t size)` | Allocate and zero `nmemb * size` bytes. |
| `void *realloc(void *p, size_t n)` | Resize a block, preserving contents. |
| `void free(void *p)` | Return a block to the heap. |

The allocator is a first-fit free list with a 3-byte block header. Freeing a
block coalesces it with adjacent free neighbours (including blocks freed via
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

!!! note "Size cost"
    `malloc`/`calloc` pull in integer multiply/divide/modulo helpers for size
    arithmetic, and `strdup` inherits the whole `malloc` chain. See the
    [appendix](appendix/01-dccrtlstrip.md).

## Conversion

| Function | Summary |
| --- | --- |
| `int atoi(const char *s)` | Parse a leading signed decimal integer (16-bit). |
| `long atol(const char *s)` | Parse a leading signed decimal integer (32-bit). |
| `long strtol(const char *s, char **end, int base)` | Parse a `long` in `base` 2..36 (0 = auto). |
| `unsigned long strtoul(const char *s, char **end, int base)` | Parse an `unsigned long` in `base` 2..36 (0 = auto). |

`atoi`/`atol` skip leading spaces/tabs, accept an optional `+`/`-` sign, then
consume decimal digits; conversion stops at the first non-digit. Overflow wraps
modulo the type width.

```c
int  n = atoi("  -123xyz");   /* -123  */
long m = atol("  -123456");   /* -123456L */
```

`strtol`/`strtoul` are the full C89 conversions. They skip leading whitespace,
accept an optional sign, honour a `0x`/`0X` prefix for base 16 and a leading `0`
for base 8 when `base` is 0, and accept digits/letters up to `base`-1 for any
base from 2 to 36. The unused tail is reported through `*end` when `end` is
non-`NULL`. On overflow they clamp to `LONG_MAX`/`LONG_MIN` (or `ULONG_MAX`) and
set `errno` to `ERANGE`.

```c
char *end;
long  v = strtol("  -0x1Ag", &end, 0);            /* v = -26, *end = 'g'  */
unsigned long u = strtoul("4294967295", NULL, 10); /* ULONG_MAX */
```

There is **no `atof`** — C89 `atof` returns `double`, and dcc has no `double`.
See [Limitations](11-limitations.md).

## Integer arithmetic helpers

| Function | Summary |
| --- | --- |
| `int abs(int j)` | Absolute value of a 16-bit signed integer. |
| `long labs(long j)` | Absolute value of a 32-bit signed long. |
| `div_t div(int numer, int denom)` | Signed 16-bit quotient and remainder. |
| `ldiv_t ldiv(long numer, long denom)` | Signed 32-bit quotient and remainder. |

`div` returns a `div_t` with `quot` and `rem` members; `ldiv` returns an
`ldiv_t` with 32-bit members. Signed division truncates toward zero; the
remainder has the same sign as the numerator.

```c
div_t  d  = div(-7, 3);          /* d.quot == -2, d.rem == -1 */
ldiv_t ld = ldiv(200000L, 7L);
```

## Searching and sorting

| Function | Summary |
| --- | --- |
| `void qsort(void *base, size_t num, size_t size, int (*cmp)(const void*, const void*))` | Sort `num` elements of `size` bytes in place. |
| `const void *bsearch(const void *key, const void *base, size_t num, size_t size, int (*cmp)(const void*, const void*))` | Binary-search a sorted array; returns the matching element or `NULL`. |

Both take the standard comparator: `cmp(a, b)` returns negative if `a` sorts
before `b`, zero if equal, positive if after. `qsort` uses an in-place,
non-recursive Shell sort, so it is **not stable**; `bsearch` requires the array
to be sorted by the same comparator. See [Worked examples](12-examples.md) for
complete programs.

## Process control

| Function | Summary |
| --- | --- |
| `void exit(int code)` | Flush, terminate, and return `code` to the OS. |

The exit code is surfaced through CP/M 3.0 BDOS call 108, which emulators such
as ntvcm reflect in their own process exit code. Returning a value from `main`
has the same effect.

## Pseudo-random numbers

| Function | Summary |
| --- | --- |
| `int rand(void)` | Next pseudo-random number in `0 .. RAND_MAX`. |
| `void srand(unsigned seed)` | Seed the generator. |

`RAND_MAX` is 32767.

```c
srand(1);
int roll = rand() % 6 + 1;     /* a die roll */
```
