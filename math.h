/* math.h - minimal dcc single-precision math declarations */

#ifndef _MATH_H
#define _MATH_H

float fabsf(float x);
float floorf(float x);
float ceilf(float x);
float sqrtf(float x);
float nextafterf(float x, float y);
float fmodf(float x, float y);

/* exponential and logarithmic */
float expf(float x);
float logf(float x);
float log10f(float x);
float powf(float x, float y);

/* trigonometric */
float sinf(float x);
float cosf(float x);
float tanf(float x);
float asinf(float x);
float acosf(float x);
float atanf(float x);
float atan2f(float y, float x);

/* hyperbolic */
float sinhf(float x);
float coshf(float x);
float tanhf(float x);

/* decomposition */
float frexpf(float x, int *eptr);
float ldexpf(float x, int n);
float modff(float x, float *iptr);

/*
 * C89 (4.5) spells these without the 'f' suffix and defines them on double.
 * dcc has no double (float is the only floating type), so the unsuffixed C89
 * names are provided as single-precision aliases of the 'f' variants above.
 * This lets portable C89 source that calls fabs/floor/ceil/sqrt/fmod (etc.)
 * compile and link against the single-precision runtime.
 */
#define fabs(x)     fabsf(x)
#define floor(x)    floorf(x)
#define ceil(x)     ceilf(x)
#define sqrt(x)     sqrtf(x)
#define fmod(x, y)  fmodf((x), (y))

#define exp(x)      expf(x)
#define log(x)      logf(x)
#define log10(x)    log10f(x)
#define pow(x, y)   powf((x), (y))

#define sin(x)      sinf(x)
#define cos(x)      cosf(x)
#define tan(x)      tanf(x)
#define asin(x)     asinf(x)
#define acos(x)     acosf(x)
#define atan(x)     atanf(x)
#define atan2(y, x) atan2f((y), (x))

#define sinh(x)     sinhf(x)
#define cosh(x)     coshf(x)
#define tanh(x)     tanhf(x)

#define frexp(x, e)  frexpf((x), (e))
#define ldexp(x, n)  ldexpf((x), (n))
#define modf(x, ip)  modff((x), (ip))

#endif /* _MATH_H */
