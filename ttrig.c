#include <stdio.h>
#include <math.h>

/* results compare to msvc's results aside from the occasional least-significant digit */

/* Core loopless ATAN polynomial function used by both atanf and atan2f */
float atanf(float x) 
{
    float sign = 1.0f;
    float bias = 0.0f;

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

    float x2 = x * x;
    float poly = x + x * x2 * (-0.33333145f + x2 * (0.19993550f + x2 * (-0.14208899f + x2 * 0.10656263f)));

    return sign * (bias + poly);
}

float cosf(float x) 
{
    const float PI = 3.14159265f;
    const float TWO_PI = 6.28318531f;
    float sign = 1.0f;
    
    x = fmodf(x, TWO_PI);
    if (x > PI)  x -= TWO_PI;
    if (x < -PI) x += TWO_PI;

    if (x > 1.57079632f) {
        x = PI - x;
        sign = -1.0f;
    } else if (x < -1.57079632f) {
        x = -PI - x;
        sign = -1.0f;
    }

    float x2 = x * x;
    return sign * ( 1.0f + x2 * (-0.49999286f + x2 * (0.04163351f + x2 * (-0.00136122f))) );
}

float sinf(float x) 
{
    const float PI = 3.14159265f;
    const float TWO_PI = 6.28318531f;
    float sign = 1.0f;

    /* 1. Argument reduction to [-PI, PI] */
    x = fmodf(x, TWO_PI);
    if (x > PI)  x -= TWO_PI;
    if (x < -PI) x += TWO_PI;

    /* 2. Map quadrants to [-PI/2, PI/2] */
    if (x > 1.57079632f) {
        x = PI - x;
    } else if (x < -1.57079632f) {
        x = -PI - x;
    }

    /* 3. Evaluate 4-term Sinf Minimax Polynomial via Horner's scheme */
    float x2 = x * x;
    return x + x * x2 * (-0.16665724f + x2 * (0.00831348f + x2 * (-0.00018521f)));
}

float tanf(float x) 
{
    const float PI = 3.14159265f;
    const float HALF_PI = 1.57079632f;
    const float QUARTER_PI = 0.78539816f;
    int invert = 0;

    float x_reduced = fmodf(x, PI);
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

    float x2 = x_reduced * x_reduced;
    float num = x_reduced * (15.0f - x2);
    float den = 15.0f - 6.0f * x2;

    if (den == 0.0f) return 0.0f;
    float result = num / den;

    if (invert) {
        if (result == 0.0f) return 0.0f;
        result = 1.0f / result;
    }

    return result;
}

float atan2f(float y, float x) 
{
    const float PI = 3.14159265f;
    const float HALF_PI = 1.57079632f;

    if (x == 0.0f) {
        if (y > 0.0f) return  HALF_PI; 
        if (y < 0.0f) return -HALF_PI; 
        return 0.0f;                   
    }

    float ratio = y / x;

    if (x > 0.0f) {
        return atanf(ratio);
    } else {
        if (y >= 0.0f) {
            return atanf(ratio) + PI; 
        } else {
            return atanf(ratio) - PI; 
        }
    }
}

float asinf(float x) 
{
    const float HALF_PI = 1.57079632f;
    float sign = 1.0f;

    if (x < 0.0f) {
        sign = -1.0f;
        x = -x;
    }

    if (x > 1.0f) return 0.0f;

    if (x > 0.5f) {
        float transform = (float)sqrtf(0.5f * (1.0f - x));
        float sub_asin = asinf(transform);
        return sign * (HALF_PI - 2.0f * sub_asin);
    }

    float x2 = x * x;
    float poly = x + x * x2 * (0.16666752f + x2 * (0.07495300f + x2 * (0.04547002f + x2 * 0.02417036f)));

    return sign * poly;
}

float acosf(float x) 
{
    const float HALF_PI = 1.57079632f;
    return HALF_PI - asinf(x);
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

    /* 1. Direct Sweep Testing: sinf, cosf, tanf, atanf */
    for (i = 0; i < 11; i++) {
        float test_val;
        test_val = angles[i];
        printf( "sin  %f: %f\n", test_val, sinf( test_val ) );
        printf( "cos  %f: %f\n", test_val, cosf( test_val ) );
        printf( "tan  %f: %f\n", test_val, tanf( test_val ) );
        printf( "atan %f: %f\n", test_val, atanf( test_val ) );
    }

    /* 2. Inverse Bound Sweep Testing: asinf, acosf */
    for (i = 0; i < 9; i++) {
        float test_val;
        test_val = inverse_inputs[i];
        printf( "asin %f: %f\n", test_val, asinf( test_val ) );
        printf( "acos %f: %f\n", test_val, acosf( test_val ) );
    }

    /* 3. Coordinate System Testing: atan2f */
    for (i = 0; i < 9; i++) {
        float y, x;
        y = pairs_y[i];
        x = pairs_x[i];
        printf( "atan2f %f %f: %f\n", y, x, atan2f( y, x ) );
    }

    printf( "ttrig completed with great success\n" );
}
