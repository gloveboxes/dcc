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
| `ptrdiff_t` | 16 bits | signed `int`; result of subtracting two pointers |
| `wchar_t` | 16 bits | unsigned `int`; shared by `stddef.h` and `stdint.h` |
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

## Fixed-width integer names (`stdint.h`)

`stdint.h` provides fixed-width typedefs that match the target model:

| Name | Definition |
| --- | --- |
| `int8_t` | signed 8-bit `char` |
| `uint8_t` | unsigned 8-bit `char` |
| `int16_t` | signed 16-bit `int` |
| `uint16_t` | unsigned 16-bit `int` |
| `int32_t` | signed 32-bit `long` |
| `uint32_t` | unsigned 32-bit `long` |
| `wchar_t` | unsigned 16-bit `int` |

## Integer limits (`limits.h`)

| Macro | Value |
| --- | ---: |
| `CHAR_BIT` | 8 |
| `SCHAR_MIN` / `SCHAR_MAX` | -128 / 127 |
| `UCHAR_MAX` | 255 |
| `CHAR_MIN` / `CHAR_MAX` | -128 / 127 |
| `SHRT_MIN` / `SHRT_MAX` | -32768 / 32767 |
| `USHRT_MAX` | 65535 |
| `INT_MIN` / `INT_MAX` | -32768 / 32767 |
| `UINT_MAX` | 65535 |
| `LONG_MIN` / `LONG_MAX` | -2147483648 / 2147483647 |
| `ULONG_MAX` | 4294967295 |
| `UINT32_MAX` | 4294967295 |

## Floating limits (`float.h`)

`float.h` describes dcc's single-precision reality:

| Macro | Value |
| --- | ---: |
| `FLT_RADIX` | 2 |
| `FLT_MANT_DIG` | 24 |
| `FLT_DIG` | 6 |
| `FLT_EPSILON` | 1.19209290e-07F |
| `FLT_MIN` | 1.17549435e-38F |
| `FLT_MAX` | 3.40282347e+38F |
| `FLT_MIN_EXP` / `FLT_MAX_EXP` | -125 / 128 |
| `FLT_MIN_10_EXP` / `FLT_MAX_10_EXP` | -37 / 38 |

dcc has no `double` or `long double`. The `DBL_*` and `LDBL_*` macros are
defined as aliases of the `FLT_*` values so source that references those names
still compiles, but they intentionally reflect the single-precision target:

- `DBL_MANT_DIG`, `DBL_DIG`, `DBL_EPSILON`, `DBL_MIN`, `DBL_MAX`,
  `DBL_MIN_EXP`, `DBL_MAX_EXP`, `DBL_MIN_10_EXP`, `DBL_MAX_10_EXP`
- `LDBL_MANT_DIG`, `LDBL_DIG`, `LDBL_EPSILON`, `LDBL_MIN`, `LDBL_MAX`,
  `LDBL_MIN_EXP`, `LDBL_MAX_EXP`, `LDBL_MIN_10_EXP`, `LDBL_MAX_10_EXP`

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
