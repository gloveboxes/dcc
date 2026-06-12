/* math.h - minimal dcc single-precision math declarations */

#ifndef _MATH_H
#define _MATH_H

float fabsf(float x);
float floorf(float x);
float ceilf(float x);
float sqrtf(float x);
float nextafterf(float x);
float fmodf(float x, float y);

/*
 * C89 (4.5) spells these without the 'f' suffix and defines them on double.
 * dcc has no double (float is the only floating type), so the unsuffixed C89
 * names are provided as single-precision aliases of the 'f' variants above.
 * This lets portable C89 source that calls fabs/floor/ceil/sqrt/fmod compile
 * and link against the single-precision runtime.
 */
#define fabs(x)     fabsf(x)
#define floor(x)    floorf(x)
#define ceil(x)     ceilf(x)
#define sqrt(x)     sqrtf(x)
#define fmod(x, y)  fmodf((x), (y))

#endif /* _MATH_H */
