/* math.h - minimal dcc single-precision math declarations */

#ifndef _MATH_H
#define _MATH_H

/** Value returned on overflow; equals FLT_MAX (dcc has no double). */
#define HUGE_VAL 3.40282347e+38F

/** Absolute value. */
float fabsf(float x);
/** Round toward negative infinity. */
float floorf(float x);
/** Round toward positive infinity. */
float ceilf(float x);
/** Square root. */
float sqrtf(float x);
/** Next representable value after x in the direction of y. */
float nextafterf(float x, float y);
/** Floating-point remainder of x / y. */
float fmodf(float x, float y);

/* exponential and logarithmic */
/** Base-e exponential, e raised to x. */
float expf(float x);
/** Natural logarithm, base e. */
float logf(float x);
/** Base-10 logarithm. */
float log10f(float x);
/** x raised to the power y. */
float powf(float x, float y);

/* trigonometric */
/** Sine of x, in radians. */
float sinf(float x);
/** Cosine of x, in radians. */
float cosf(float x);
/** Tangent of x, in radians. */
float tanf(float x);
/** Arc sine of x. */
float asinf(float x);
/** Arc cosine of x. */
float acosf(float x);
/** Arc tangent of x. */
float atanf(float x);
/** Arc tangent of y / x using the signs of both arguments. */
float atan2f(float y, float x);

/* hyperbolic */
/** Hyperbolic sine. */
float sinhf(float x);
/** Hyperbolic cosine. */
float coshf(float x);
/** Hyperbolic tangent. */
float tanhf(float x);

/* decomposition */
/** Split x into a normalized fraction and exponent. */
float frexpf(float x, int *eptr);
/** Compute x multiplied by 2 raised to n. */
float ldexpf(float x, int n);
/** Split x into integer and fractional parts. */
float modff(float x, float *iptr);

/*
 * C89 (4.5) spells these without the 'f' suffix and defines them on double.
 * dcc has no double (float is the only floating type), so the unsuffixed C89
 * names are provided as single-precision aliases of the 'f' variants above.
 * This lets portable C89 source that calls fabs/floor/ceil/sqrt/fmod (etc.)
 * compile and link against the single-precision runtime.
 */
/** Single-precision alias for fabsf. */
#define fabs(x)     fabsf(x)
/** Single-precision alias for floorf. */
#define floor(x)    floorf(x)
/** Single-precision alias for ceilf. */
#define ceil(x)     ceilf(x)
/** Single-precision alias for sqrtf. */
#define sqrt(x)     sqrtf(x)
/** Single-precision alias for fmodf. */
#define fmod(x, y)  fmodf((x), (y))

/** Single-precision alias for expf. */
#define exp(x)      expf(x)
/** Single-precision alias for logf. */
#define log(x)      logf(x)
/** Single-precision alias for log10f. */
#define log10(x)    log10f(x)
/** Single-precision alias for powf. */
#define pow(x, y)   powf((x), (y))

/** Single-precision alias for sinf. */
#define sin(x)      sinf(x)
/** Single-precision alias for cosf. */
#define cos(x)      cosf(x)
/** Single-precision alias for tanf. */
#define tan(x)      tanf(x)
/** Single-precision alias for asinf. */
#define asin(x)     asinf(x)
/** Single-precision alias for acosf. */
#define acos(x)     acosf(x)
/** Single-precision alias for atanf. */
#define atan(x)     atanf(x)
/** Single-precision alias for atan2f. */
#define atan2(y, x) atan2f((y), (x))

/** Single-precision alias for sinhf. */
#define sinh(x)     sinhf(x)
/** Single-precision alias for coshf. */
#define cosh(x)     coshf(x)
/** Single-precision alias for tanhf. */
#define tanh(x)     tanhf(x)

/** Single-precision alias for frexpf. */
#define frexp(x, e)  frexpf((x), (e))
/** Single-precision alias for ldexpf. */
#define ldexp(x, n)  ldexpf((x), (n))
/** Single-precision alias for modff. */
#define modf(x, ip)  modff((x), (ip))

#endif /* _MATH_H */
