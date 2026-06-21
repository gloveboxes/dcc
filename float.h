#ifndef _FLOAT_H
#define _FLOAT_H

/** Exponent radix for floating-point values. */
#define FLT_RADIX 2
/** Number of base-FLT_RADIX digits in the float significand. */
#define FLT_MANT_DIG 24
/** Decimal digits of precision for float. */
#define FLT_DIG 6
/** Difference between 1.0F and the next representable float. */
#define FLT_EPSILON 1.19209290e-07F
/** Smallest positive normalized float. */
#define FLT_MIN 1.17549435e-38F
/** Largest finite float. */
#define FLT_MAX 3.40282347e+38F
/** Minimum base-FLT_RADIX exponent for normalized float. */
#define FLT_MIN_EXP (-125)
/** Maximum base-FLT_RADIX exponent for normalized float. */
#define FLT_MAX_EXP 128
/** Minimum base-10 exponent for normalized float. */
#define FLT_MIN_10_EXP (-37)
/** Maximum base-10 exponent for normalized float. */
#define FLT_MAX_10_EXP 38

/*
 * dcc has no double or long double: 32-bit IEEE `float` is the only floating
 * type, so the DBL_* and LDBL_* macros below are deliberately aliased to the
 * FLT_* values.  As a documented consequence of the no-double design they do
 * NOT meet the C89 (2.2.4.2) minimums for double/long double (which would
 * require DBL_DIG >= 10 and DBL_EPSILON <= 1E-9).  They exist only so that
 * source referencing these names still compiles; the values intentionally
 * reflect the single-precision reality of the target.
 */
/** Alias of FLT_MANT_DIG because dcc has no double type. */
#define DBL_MANT_DIG FLT_MANT_DIG
/** Alias of FLT_DIG because dcc has no double type. */
#define DBL_DIG FLT_DIG
/** Alias of FLT_EPSILON because dcc has no double type. */
#define DBL_EPSILON FLT_EPSILON
/** Alias of FLT_MIN because dcc has no double type. */
#define DBL_MIN FLT_MIN
/** Alias of FLT_MAX because dcc has no double type. */
#define DBL_MAX FLT_MAX
/** Alias of FLT_MIN_EXP because dcc has no double type. */
#define DBL_MIN_EXP FLT_MIN_EXP
/** Alias of FLT_MAX_EXP because dcc has no double type. */
#define DBL_MAX_EXP FLT_MAX_EXP
/** Alias of FLT_MIN_10_EXP because dcc has no double type. */
#define DBL_MIN_10_EXP FLT_MIN_10_EXP
/** Alias of FLT_MAX_10_EXP because dcc has no double type. */
#define DBL_MAX_10_EXP FLT_MAX_10_EXP

/** Alias of DBL_MANT_DIG because dcc has no long double type. */
#define LDBL_MANT_DIG DBL_MANT_DIG
/** Alias of DBL_DIG because dcc has no long double type. */
#define LDBL_DIG DBL_DIG
/** Alias of DBL_EPSILON because dcc has no long double type. */
#define LDBL_EPSILON DBL_EPSILON
/** Alias of DBL_MIN because dcc has no long double type. */
#define LDBL_MIN DBL_MIN
/** Alias of DBL_MAX because dcc has no long double type. */
#define LDBL_MAX DBL_MAX
/** Alias of DBL_MIN_EXP because dcc has no long double type. */
#define LDBL_MIN_EXP DBL_MIN_EXP
/** Alias of DBL_MAX_EXP because dcc has no long double type. */
#define LDBL_MAX_EXP DBL_MAX_EXP
/** Alias of DBL_MIN_10_EXP because dcc has no long double type. */
#define LDBL_MIN_10_EXP DBL_MIN_10_EXP
/** Alias of DBL_MAX_10_EXP because dcc has no long double type. */
#define LDBL_MAX_10_EXP DBL_MAX_10_EXP

#endif
