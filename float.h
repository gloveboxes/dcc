#ifndef _FLOAT_H
#define _FLOAT_H

#define FLT_RADIX 2
#define FLT_MANT_DIG 24
#define FLT_DIG 6
#define FLT_EPSILON 1.19209290e-07F
#define FLT_MIN 1.17549435e-38F
#define FLT_MAX 3.40282347e+38F
#define FLT_MIN_EXP (-125)
#define FLT_MAX_EXP 128
#define FLT_MIN_10_EXP (-37)
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
#define DBL_MANT_DIG FLT_MANT_DIG
#define DBL_DIG FLT_DIG
#define DBL_EPSILON FLT_EPSILON
#define DBL_MIN FLT_MIN
#define DBL_MAX FLT_MAX
#define DBL_MIN_EXP FLT_MIN_EXP
#define DBL_MAX_EXP FLT_MAX_EXP
#define DBL_MIN_10_EXP FLT_MIN_10_EXP
#define DBL_MAX_10_EXP FLT_MAX_10_EXP

#define LDBL_MANT_DIG DBL_MANT_DIG
#define LDBL_DIG DBL_DIG
#define LDBL_EPSILON DBL_EPSILON
#define LDBL_MIN DBL_MIN
#define LDBL_MAX DBL_MAX
#define LDBL_MIN_EXP DBL_MIN_EXP
#define LDBL_MAX_EXP DBL_MAX_EXP
#define LDBL_MIN_10_EXP DBL_MIN_10_EXP
#define LDBL_MAX_10_EXP DBL_MAX_10_EXP

#endif
