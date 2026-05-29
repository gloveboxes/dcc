#include <stdio.h>
#include <math.h>

/* --- FUNCTION DECLARATIONS & IMPLEMENTATIONS (Strict C89) --- */

/* Core loopless ATAN polynomial function used by both atanf and atan2f */
static float my_core_atanf(float x) 
{
    float sign = 1.0f;
    float bias = 0.0f;
    float x2;
    float poly;

    if (x < 0.0f) {
        sign = -1.0f;
        x = -x;
    }

    if (x > 2.41421356f) {         
        bias = 1.57079632f;        
        x = -1.0f / x;
    } else if (x > 0.41421356f) {  
        bias = 0.78539816f;        
        x = (x - 1.0f) / (x + 1.0f);
    }

    x2 = x * x;
    poly = x + x * x2 * (-0.33333145f + x2 * (0.19993550f + x2 * (-0.14208899f + x2 * 0.10656263f)));

    return sign * (bias + poly);
}

float my_production_atanf(float x) 
{
    return my_core_atanf(x);
}

float my_production_cosf(float x) 
{
    const float PI = 3.14159265f;
    const float TWO_PI = 6.28318531f;
    float sign = 1.0f;
    float x2;
    float result;
    
    x = (float)fmod(x, TWO_PI);
    if (x > PI)  x -= TWO_PI;
    if (x < -PI) x += TWO_PI;

    if (x > 1.57079632f) {
        x = PI - x;
        sign = -1.0f;
    } else if (x < -1.57079632f) {
        x = -PI - x;
        sign = -1.0f;
    }

    x2 = x * x;
    result = 1.0f + x2 * (-0.49999286f + x2 * (0.04163351f + x2 * (-0.00136122f)));

    return sign * result;
}

/* Standalone, independent Remez Minimax Polynomial for Sine */
float my_production_sinf(float x) 
{
    const float PI = 3.14159265f;
    const float TWO_PI = 6.28318531f;
    float sign = 1.0f;
    float x2;
    float result;

    /* 1. Argument reduction to [-PI, PI] */
    x = (float)fmod(x, TWO_PI);
    if (x > PI)  x -= TWO_PI;
    if (x < -PI) x += TWO_PI;

    /* 2. Map quadrants to [-PI/2, PI/2] */
    if (x > 1.57079632f) {
        x = PI - x;
    } else if (x < -1.57079632f) {
        x = -PI - x;
    }

    /* 3. Evaluate 4-term Sinf Minimax Polynomial via Horner's scheme */
    x2 = x * x;
    result = x + x * x2 * (-0.16665724f + x2 * (0.00831348f + x2 * (-0.00018521f)));

    return result;
}

float my_production_tanf(float x) 
{
    const float PI = 3.14159265f;
    const float HALF_PI = 1.57079632f;
    const float QUARTER_PI = 0.78539816f;
    float x_reduced;
    int invert = 0;
    float x2;
    float num;
    float den;
    float result;

    x_reduced = (float)fmod(x, PI);
    if (x_reduced > HALF_PI) {
        x_reduced -= PI;
    } else if (x_reduced < -HALF_PI) {
        x_reduced += PI;
    }

    if (x_reduced > QUARTER_PI) {
        x_reduced = HALF_PI - x_reduced;
        invert = 1;
    } else if (x_reduced < -QUARTER_PI) {
        x_reduced = -HALF_PI - x_reduced;
        invert = 1;
    }

    x2 = x_reduced * x_reduced;
    num = x_reduced * (15.0f - x2);
    den = 15.0f - 6.0f * x2;

    if (den == 0.0f) return 0.0f;
    result = num / den;

    if (invert) {
        if (result == 0.0f) return 0.0f;
        result = 1.0f / result;
    }

    return result;
}

float my_production_atan2f(float y, float x) 
{
    const float PI = 3.14159265f;
    const float HALF_PI = 1.57079632f;
    float ratio;

    if (x == 0.0f) {
        if (y > 0.0f) return  HALF_PI; 
        if (y < 0.0f) return -HALF_PI; 
        return 0.0f;                   
    }

    ratio = y / x;

    if (x > 0.0f) {
        return my_core_atanf(ratio);
    } else {
        if (y >= 0.0f) {
            return my_core_atanf(ratio) + PI; 
        } else {
            return my_core_atanf(ratio) - PI; 
        }
    }
}

float my_production_asinf(float x) 
{
    const float HALF_PI = 1.57079632f;
    float sign = 1.0f;
    float x2;
    float poly;

    if (x < 0.0f) {
        sign = -1.0f;
        x = -x;
    }

    if (x > 1.0f) return 0.0f;

    if (x > 0.5f) {
        float transform = (float)sqrt(0.5f * (1.0f - x));
        float sub_asin = my_production_asinf(transform);
        return sign * (HALF_PI - 2.0f * sub_asin);
    }

    x2 = x * x;
    poly = x + x * x2 * (0.16666752f + x2 * (0.07495300f + x2 * (0.04547002f + x2 * 0.02417036f)));

    return sign * poly;
}

float my_production_acosf(float x) 
{
    const float HALF_PI = 1.57079632f;
    return HALF_PI - my_production_asinf(x);
}


/* --- UNIT TEST ENGINE --- */

static int tests_run = 0;
static int tests_failed = 0;

static void verify_result(const char* func_name, float input, float custom_res, float standard_res) 
{
    float epsilon = 0.001f; 
    float diff = (float)fabs(custom_res - standard_res);
    
    tests_run++;
    if (diff > epsilon) {
        printf("  [FAIL] %s at input %f: Custom=%f, Standard=%f (Diff=%f)\n", 
               func_name, input, custom_res, standard_res, diff);
        tests_failed++;
    }
}

int main(void) 
{
    float angles[11];
    float inverse_inputs[9];
    float pairs_y[9];
    float pairs_x[9];
    int i;

    angles[0] = 0.0f;   angles[1] = 0.5f;   angles[2] = 1.0f; 
    angles[3] = 1.5f;   angles[4] = -0.5f;  angles[5] = -1.0f; 
    angles[6] = -1.5f;  angles[7] = 3.0f;   angles[8] = -3.0f; 
    angles[9] = 5.0f;   angles[10] = -5.0f;

    inverse_inputs[0] = 0.0f;   inverse_inputs[1] = 0.25f;  inverse_inputs[2] = 0.5f;
    inverse_inputs[3] = 0.75f;  inverse_inputs[4] = 1.0f;   inverse_inputs[5] = -0.25f;
    inverse_inputs[6] = -0.5f;  inverse_inputs[7] = -0.75f; inverse_inputs[8] = -1.0f;

    pairs_y[0] = 0.0f;  pairs_x[0] = 1.0f;
    pairs_y[1] = 1.0f;  pairs_x[1] = 1.0f;
    pairs_y[2] = 1.0f;  pairs_x[2] = 0.0f;
    pairs_y[3] = 1.0f;  pairs_x[3] = -1.0f;
    pairs_y[4] = 0.0f;  pairs_x[4] = -1.0f;
    pairs_y[5] = -1.0f; pairs_x[5] = -1.0f;
    pairs_y[6] = -1.0f; pairs_x[6] = 0.0f;
    pairs_y[7] = -1.0f; pairs_x[7] = 1.0f;
    pairs_y[8] = 0.0f;  pairs_x[8] = 0.0f;

    printf("=========================================\n");
    printf("STARTING COMPREHENSIVE TRIGONOMETRY TESTS\n");
    printf("=========================================\n\n");

    /* 1. Direct Sweep Testing: sinf, cosf, tanf, atanf */
    for (i = 0; i < 11; i++) {
        float test_val = angles[i];
        verify_result("sinf",  test_val, my_production_sinf(test_val),  (float)sin(test_val));
        verify_result("cosf",  test_val, my_production_cosf(test_val),  (float)cos(test_val));
        verify_result("tanf",  test_val, my_production_tanf(test_val),  (float)tan(test_val));
        verify_result("atanf", test_val, my_production_atanf(test_val), (float)atan(test_val));
    }

    /* 2. Inverse Bound Sweep Testing: asinf, acosf */
    for (i = 0; i < 9; i++) {
        float test_val = inverse_inputs[i];
        verify_result("asinf", test_val, my_production_asinf(test_val), (float)asin(test_val));
        verify_result("acosf", test_val, my_production_acosf(test_val), (float)acos(test_val));
    }

    /* 3. Coordinate System Testing: atan2f */
    for (i = 0; i < 9; i++) {
        float y = pairs_y[i];
        float x = pairs_x[i];
        verify_result("atan2f", y, my_production_atan2f(y, x), (float)atan2(y, x));
    }

    printf("-----------------------------------------\n");
    printf("TEST SUITE SUMMARY:\n");
    printf("  Total Checks Executed: %d\n", tests_run);
    printf("  Failed Assertions:     %d\n", tests_failed);
    printf("-----------------------------------------\n");
    
    if (tests_failed == 0) {
        printf("  >>> SUCCESS: All 7 functions passed verification. <<<\n");
        return 0;
    } else {
        printf("  >>> FAILURE: Accuracy discrepancies detected. <<<\n");
        return 1;
    }
}
