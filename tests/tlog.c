#include <stdio.h>

/* Custom frexpf implementation using an arithmetic reduction loop */
static float frexpf(float value, int *exp) {
    *exp = 0;
    if (value == 0.0f) {
        return 0.0f;
    }
    if (value < 0.0f) {
        return -frexpf(-value, exp);
    }
    while (value >= 1.0f) {
        value *= 0.5f;
        (*exp)++;
    }
    while (value < 0.5f) {
        value *= 2.0f;
        (*exp)--;
    }
    return value;
}

/* Custom logf implementation using Taylor series centered via frexpf */
static float logf(float x) {
    float m, z, z2, sum_z, term, ln_m;
    int k;
    const float ln_2 = 0.69314718f;
    float zero = 0.0f;

    if (x < 0.0f) return zero / zero;   /* Returns NaN */
    if (x == 0.0f) return -1.0f / zero; /* Returns -Inf */

    /* Reduce argument to range [0.5, 1.0) */
    m = frexpf(x, &k);

    /* Remap to narrower range [0.7071, 1.4142) for ultra-fast convergence */
    if (m < 0.70710678f) {
        m *= 2.0f;
        k -= 1;
    }

    /* Transform area to center around 0 using z = (m-1)/(m+1) */
    z = (m - 1.0f) / (m + 1.0f);
    z2 = z * z;

    /* Taylor expansion for ln((1+z)/(1-z)) = 2 * (z + z^3/3 + z^5/5 + ...) */
    sum_z = z;
    term = z;

    term *= z2; sum_z += term / 3.0f;
    term *= z2; sum_z += term / 5.0f;
    term *= z2; sum_z += term / 7.0f;
    term *= z2; sum_z += term / 9.0f;
    term *= z2; sum_z += term / 11.0f;

    ln_m = 2.0f * sum_z;
    return ln_m + (float)k * ln_2;
}

/* Custom log10f implementation using change of base formula */
static float log10f(float x) {
    const float inv_ln10 = 0.43429448f; /* 1 / ln(10) */
    return logf(x) * inv_ln10;
}

int main(void) {
    float test_val = 15.75f;
    int exp = 0;
    float mantissa;

    printf("=== Custom C89 Float Math Verification ===\n\n");

    /* Test 1: frexpf */
    mantissa = frexpf(test_val, &exp);
    printf("[TEST 1] frexpf(%.2f):\n", test_val);
    printf("  -> Mantissa: %f\n", mantissa);
    printf("  -> Exponent: %d\n\n", exp);

    /* Test 2: logf */
    printf("[TEST 2] logf values:\n");
    printf("  -> logf(1.0)  = %f (Expected: 0.0)\n", logf(1.0f));
    printf("  -> logf(2.71) = %f (Expected: ~1.0)\n", logf(2.7182818f));
    printf("  -> logf(10.0) = %f (Expected: ~2.302585)\n\n", logf(10.0f));

    /* Test 3: log10f */
    printf("[TEST 3] log10f values:\n");
    printf("  -> log10f(10.0)   = %f (Expected: 1.0)\n", log10f(10.0f));
    printf("  -> log10f(100.0)  = %f (Expected: 2.0)\n", log10f(100.0f));
    printf("  -> log10f(1000.0) = %f (Expected: 3.0)\n\n", log10f(1000.0f));

    /* Test 4: Edge cases */
    printf("[TEST 4] Edge cases:\n");
    printf("  -> logf(0.0)  = %f\n", logf(0.0f));
    printf("  -> logf(-5.0) = %f\n", logf(-5.0f));

    return 0;
}
