# Types and conventions

dcc uses a compact 16-bit model. Knowing the exact widths up front avoids most
overflow and precision surprises.

## Type sizes

| Type | Size | Notes |
| --- | --- | --- |
| `char` | 8 bits | signed by default |
| `int`, `short` | 16 bits | `int` is 16-bit; watch for overflow |
| `long` | 32 bits | use `%ld` and the `l` length modifier |
| `float` | 32 bits | the only floating type — **no `double`** |
| pointer | 16 bits | flat CP/M address space |
| `size_t` | 16 bits | unsigned `int` |
| `FILE` | 16 bits | `typedef int FILE`; streams are small handles |

The practical consequences:

- Use `long` (and `%ld`) whenever a value can exceed ±32767.
- `float` carries about 7 decimal digits (a 24-bit significand). Integers up to
  2<sup>24</sup> (16,777,216) are exact; beyond that, converting a large `long`
  to `float` rounds to the nearest single. See [Floating point math](08-math.md)
  for the full set of precision gotchas.

## Useful constants

From [stdio](05-stdio.md):

- `EOF` = -1, `BUFSIZ` = 256, `SEEK_SET` / `SEEK_CUR` / `SEEK_END` = 0 / 1 / 2.

From [stdlib](06-stdlib.md):

- `EXIT_SUCCESS` = 0, `EXIT_FAILURE` = 1, `RAND_MAX` = 32767, `NULL` = 0.

From `errno.h`:

- `EDOM` = 33, `ERANGE` = 34.

## Zero-initialized data

Objects with static storage duration (globals and `static` locals) live in BSS,
which the `start` entrypoint zeroes before `main` runs. So an uninitialized
global array is guaranteed to be all zeros, as C89 requires:

```c
char buffer[4096];   /* in BSS, guaranteed zero at program start */

int main(void)
{
    return buffer[0]; /* always 0 */
}
```
