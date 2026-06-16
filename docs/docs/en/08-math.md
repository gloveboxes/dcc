# Floating point math (`math.h`)

Include [`math.h`](08-math.md). dcc has only 32-bit `float` (no `double`), so
the math entry points are the single-precision `…f` variants.

!!! warning "Float is the biggest size lever"
    A single `float` operator links the shared normalise/round core, and the
    transcendental functions (`expf`/`logf`/`powf` and the trig/hyperbolic
    families) are the heaviest individual features in the runtime. Budget
    accordingly and see the [appendix](appendix/01-dccrtlstrip.md).

## Rounding, remainder, and roots

| Function | Summary |
| --- | --- |
| `float fabsf(float x)` | Absolute value. |
| `float floorf(float x)` | Round toward negative infinity. |
| `float ceilf(float x)` | Round toward positive infinity. |
| `float fmodf(float x, float y)` | Floating-point remainder of `x / y`. |
| `float sqrtf(float x)` | Square root. |
| `float nextafterf(float x, float y)` | Next representable value after `x`. |

## Exponential and logarithmic

| Function | Summary |
| --- | --- |
| `float expf(float x)` | Base-*e* exponential, e<sup>x</sup>. |
| `float logf(float x)` | Natural logarithm (base *e*). |
| `float log10f(float x)` | Base-10 logarithm. |
| `float powf(float x, float y)` | `x` raised to the power `y`. |

## Trigonometric

| Function | Summary |
| --- | --- |
| `float sinf(float x)` | Sine of `x` (radians). |
| `float cosf(float x)` | Cosine of `x` (radians). |
| `float tanf(float x)` | Tangent of `x` (radians). |
| `float asinf(float x)` | Arc sine, result in `[-π/2, π/2]`. |
| `float acosf(float x)` | Arc cosine, result in `[0, π]`. |
| `float atanf(float x)` | Arc tangent, result in `[-π/2, π/2]`. |
| `float atan2f(float y, float x)` | Arc tangent of `y/x` using the signs of both. |

## Hyperbolic

| Function | Summary |
| --- | --- |
| `float sinhf(float x)` | Hyperbolic sine. |
| `float coshf(float x)` | Hyperbolic cosine. |
| `float tanhf(float x)` | Hyperbolic tangent. |

## Decomposition

| Function | Summary |
| --- | --- |
| `float frexpf(float x, int *e)` | Split `x` into a normalized fraction in `[0.5, 1)` and exponent `*e`. |
| `float ldexpf(float x, int n)` | Compute `x * 2^n`. |
| `float modff(float x, float *ip)` | Split `x` into integer part `*ip` and the returned fraction. |

## Unsuffixed aliases

For convenience, the unsuffixed C89 names are provided as macro aliases of the
`…f` variants, so portable C89 source compiles unchanged (the operations remain
single-precision): `fabs`, `floor`, `ceil`, `sqrt`, `fmod`, `exp`, `log`,
`log10`, `pow`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sinh`,
`cosh`, `tanh`, `frexp`, `ldexp`, and `modf`.

## Accuracy

The transcendental routines (`exp`/`log`/`pow`, the trig and hyperbolic
families) are single-precision polynomial approximations: expect roughly 5–6
correct decimal digits, not full `float` round-trip accuracy. The
range-reduction in `sinf`/`cosf`/`tanf` uses `fmodf`, so accuracy gradually
degrades for very large arguments.

To print floats, compile with `-ffloatio` and use `%f`:

```c
float r = sqrtf(2.0f);
printf("%f\n", r);          /* needs -ffloatio */
```

## Precision and mixed-type gotchas

- **`float` carries about 7 decimal digits (a 24-bit significand).** Integers up
  to 2<sup>24</sup> (16,777,216) are exact; beyond that only some integers are
  representable. `(long)(float)16777217L` is `16777216`, not `16777217`. The
  same rounding applies in constant expressions, `case` labels, and array
  bounds.
- **Comparing a wide integer against a `float` happens in `float`.** The integer
  side converts first, so `16777216L < (float)16777217L` is **false** (the cast
  rounded down to `16777216.0f`). Compare as integers when you need full 32-bit
  precision.
- **Any `float` arm makes a `?:` a `float` expression.** `cond ? 2 : 3.5f`
  yields a `float`, not an `int`; cast the result if you need an integer.
- **A `float` is "true" when its magnitude is nonzero.** `if (f)`, `f ? a : b`,
  `!f`, `f && g`, and `while (f)` test against zero, and both `+0.0f` and
  `-0.0f` count as false.
