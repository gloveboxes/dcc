/* math.h - minimal dcc single-precision math declarations */

#ifndef _MATH_H
#define _MATH_H

float fabsf(float x);
float floorf(float x);
float ceilf(float x);
float sqrtf(float x);
float nextafterf(float x);

float fmodf(float x, float y)
{
    float quotient;
    float trnc_quotient;

    /* Handle division by zero */
    if (y == 0.0) {
        float result;
#ifdef _DCC_
        return 0.0 / 0.0;
#else
        return NAN;
#endif
    }

    quotient = x / y;

    /* C89 casting from float to long truncates toward zero */
    /* Note: This assumes the quotient fits within a long */
    trnc_quotient = (float)((long)quotient);

    return x - (trnc_quotient * y);
}

#endif /* _MATH_H */
