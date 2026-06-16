#include <stdio.h>
#include <math.h>

/*
 * Exercise the dcc runtime trigonometric functions directly and print their
 * results. Output is compared against tests/baselines/trig.txt by the harness.
 */

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

    /* sinf, cosf, tanf, atanf over a sweep of angles */
    for (i = 0; i < 11; i++) {
        float x = angles[i];
        printf("sinf  %f: %f\n", x, sinf(x));
        printf("cosf  %f: %f\n", x, cosf(x));
        printf("tanf  %f: %f\n", x, tanf(x));
        printf("atanf %f: %f\n", x, atanf(x));
    }

    /* asinf, acosf over the valid [-1, 1] domain */
    for (i = 0; i < 9; i++) {
        float x = inverse_inputs[i];
        printf("asinf %f: %f\n", x, asinf(x));
        printf("acosf %f: %f\n", x, acosf(x));
    }

    /* atan2f over all quadrants */
    for (i = 0; i < 9; i++) {
        float y = pairs_y[i];
        float x = pairs_x[i];
        printf("atan2f %f %f: %f\n", y, x, atan2f(y, x));
    }

    /* sinhf, coshf, tanhf over a sweep of angles */
    for (i = 0; i < 11; i++) {
        float x = angles[i];
        printf("sinhf %f: %f\n", x, sinhf(x));
        printf("coshf %f: %f\n", x, coshf(x));
        printf("tanhf %f: %f\n", x, tanhf(x));
    }

    printf("trig completed with great success\n");
    return 0;
}
