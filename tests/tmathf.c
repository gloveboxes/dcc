/*
 * tmathf.c - verification of the single-precision <math.h> functions added
 * to the dcc runtime (expf logf log10f powf sinhf coshf tanhf sinf cosf tanf
 * atanf atan2f asinf acosf frexpf ldexpf modff).
 *
 * Each result is checked against a reference value (computed with host libm)
 * using a combined absolute/relative tolerance, so the output is deterministic
 * regardless of the last bits of the approximation.  A PASS/FAIL summary is
 * printed at the end and main() returns non-zero on any failure.
 *
 * Build: bash ./ma.sh tmathf peep   (then: ntvcm build/TMATHF)
 */

#include <stdio.h>
#include <math.h>

static int checks = 0;
static int failures = 0;

/* core check: pass when |got-want| <= tol */
static void chkt(const char *name, float got, float want, float tol)
{
    float diff;

    checks++;
    diff = got - want;
    if (diff < 0.0f) diff = -diff;

    if (diff > tol) {
        failures++;
        printf("  [FAIL] %-8s got=%f want=%f (diff=%f)\n", name, got, want, diff);
    }
}

/* transcendental approximations: ~1e-3 absolute + relative band */
static void chk(const char *name, float got, float want)
{
    float a = (want < 0.0f) ? -want : want;
    chkt(name, got, want, 0.001f + 0.001f * a);
}

/* bit-exact decomposition (frexpf/ldexpf/modff): values are exactly
 * representable, so demand a tight, essentially exact match */
static void chkx(const char *name, float got, float want)
{
    chkt(name, got, want, 0.0001f);
}

int main(void)
{
    int e;
    float m;
    float ip;
    float fr;

    printf("=== dcc math.h verification ===\n");

    /* expf */
    chk("expf",  expf(0.0f),   1.000000f);
    chk("expf",  expf(1.0f),   2.718282f);
    chk("expf",  expf(-1.0f),  0.367879f);
    chk("expf",  expf(2.0f),   7.389056f);
    chk("expf",  expf(0.5f),   1.648721f);
    chk("expf",  expf(-3.0f),  0.049787f);

    /* logf */
    chk("logf",  logf(1.0f),      0.000000f);
    chk("logf",  logf(2.718282f), 1.000000f);
    chk("logf",  logf(10.0f),     2.302585f);
    chk("logf",  logf(0.5f),     -0.693147f);
    chk("logf",  logf(100.0f),    4.605170f);

    /* log10f */
    chk("log10f", log10f(10.0f),   1.000000f);
    chk("log10f", log10f(100.0f),  2.000000f);
    chk("log10f", log10f(1000.0f), 3.000000f);
    chk("log10f", log10f(2.0f),    0.301030f);

    /* powf */
    chk("powf", powf(2.0f, 10.0f), 1024.000000f);
    chk("powf", powf(2.0f, 0.5f),  1.414214f);
    chk("powf", powf(9.0f, 0.5f),  3.000000f);
    chk("powf", powf(2.0f, -1.0f), 0.500000f);
    chk("powf", powf(-2.0f, 3.0f), -8.000000f);
    chk("powf", powf(5.0f, 0.0f),  1.000000f);

    /* sinhf / coshf / tanhf */
    chk("sinhf", sinhf(0.0f),  0.000000f);
    chk("sinhf", sinhf(1.0f),  1.175201f);
    chk("sinhf", sinhf(-1.0f), -1.175201f);
    chk("sinhf", sinhf(2.0f),  3.626860f);
    chk("coshf", coshf(0.0f),  1.000000f);
    chk("coshf", coshf(1.0f),  1.543081f);
    chk("coshf", coshf(2.0f),  3.762196f);
    chk("tanhf", tanhf(0.0f),  0.000000f);
    chk("tanhf", tanhf(1.0f),  0.761594f);
    chk("tanhf", tanhf(-1.0f), -0.761594f);
    chk("tanhf", tanhf(2.0f),  0.964028f);

    /* sinf / cosf / tanf */
    chk("sinf", sinf(0.0f),       0.000000f);
    chk("sinf", sinf(1.0f),       0.841471f);
    chk("sinf", sinf(-1.0f),      -0.841471f);
    chk("sinf", sinf(3.0f),       0.141120f);
    chk("sinf", sinf(0.523599f),  0.500000f);
    chk("sinf", sinf(1.570796f),  1.000000f);
    chk("cosf", cosf(0.0f),       1.000000f);
    chk("cosf", cosf(1.0f),       0.540302f);
    chk("cosf", cosf(3.0f),       -0.989992f);
    chk("cosf", cosf(1.047198f),  0.500000f);
    chk("cosf", cosf(1.570796f),  0.000000f);
    chk("tanf", tanf(0.0f),       0.000000f);
    chk("tanf", tanf(1.0f),       1.557408f);
    chk("tanf", tanf(-1.0f),      -1.557408f);
    chk("tanf", tanf(0.785398f),  1.000000f);

    /* quadrant boundaries near pi (range-reduction edges) */
    chk("sinf", sinf(3.141593f),  0.000000f);
    chk("cosf", cosf(3.141593f),  -1.000000f);
    chk("tanf", tanf(3.141593f),  0.000000f);

    /* odd/even symmetry (sin odd, cos even) */
    chk("sinf", sinf(-3.0f),      -0.141120f);
    chk("cosf", cosf(-3.0f),      -0.989992f);

    /* atanf / atan2f */
    chk("atanf", atanf(0.0f),  0.000000f);
    chk("atanf", atanf(1.0f),  0.785398f);
    chk("atanf", atanf(-1.0f), -0.785398f);
    chk("atanf", atanf(2.0f),  1.107149f);
    chk("atan2f", atan2f(1.0f, 1.0f),   0.785398f);
    chk("atan2f", atan2f(1.0f, 0.0f),   1.570796f);
    chk("atan2f", atan2f(1.0f, -1.0f),  2.356194f);
    chk("atan2f", atan2f(-1.0f, -1.0f), -2.356194f);
    chk("atan2f", atan2f(0.0f, 1.0f),   0.000000f);
    /* atan2f quadrant transitions (different code branches) */
    chk("atan2f", atan2f(-1.0f, 1.0f),  -0.785398f);
    chk("atan2f", atan2f(-1.0f, 0.0f),  -1.570796f);

    /* asinf / acosf */
    chk("asinf", asinf(0.0f),  0.000000f);
    chk("asinf", asinf(0.5f),  0.523599f);
    chk("asinf", asinf(-0.5f), -0.523599f);
    chk("asinf", asinf(0.8f),  0.927295f);
    chk("asinf", asinf(1.0f),  1.570796f);
    chk("asinf", asinf(-1.0f), -1.570796f);
    chk("acosf", acosf(1.0f),  0.000000f);
    chk("acosf", acosf(0.5f),  1.047198f);
    chk("acosf", acosf(0.0f),  1.570796f);
    chk("acosf", acosf(-0.5f), 2.094395f);
    chk("acosf", acosf(-1.0f), 3.141593f);

    /* frexpf: x = m * 2^e, 0.5 <= |m| < 1 (bit-exact) */
    m = frexpf(8.0f, &e);
    chkx("frexpf.m", m, 0.5f);
    chkx("frexpf.e", (float)e, 4.0f);
    m = frexpf(0.75f, &e);
    chkx("frexpf.m", m, 0.75f);
    chkx("frexpf.e", (float)e, 0.0f);
    m = frexpf(1.0f, &e);
    chkx("frexpf.m", m, 0.5f);
    chkx("frexpf.e", (float)e, 1.0f);
    m = frexpf(-8.0f, &e);
    chkx("frexpf.m", m, -0.5f);
    chkx("frexpf.e", (float)e, 4.0f);

    /* ldexpf: x * 2^n (bit-exact) */
    chkx("ldexpf", ldexpf(0.5f, 4), 8.0f);
    chkx("ldexpf", ldexpf(1.0f, 3), 8.0f);
    chkx("ldexpf", ldexpf(3.0f, 2), 12.0f);
    chkx("ldexpf", ldexpf(-0.5f, 4), -8.0f);

    /* modff: integer + fractional parts (bit-exact) */
    fr = modff(3.75f, &ip);
    chkx("modff.fr", fr, 0.75f);
    chkx("modff.ip", ip, 3.0f);
    fr = modff(-3.75f, &ip);
    chkx("modff.fr", fr, -0.75f);
    chkx("modff.ip", ip, -3.0f);

    printf("-------------------------------\n");
    printf("checks=%d failures=%d\n", checks, failures);
    if (failures == 0) {
        printf("RESULT: PASS\n");
        return 0;
    }
    printf("RESULT: FAIL\n");
    return 1;
}
