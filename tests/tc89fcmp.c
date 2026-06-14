/* tc89fcmp.c - float comparison codegen test */
#include <stdio.h>

static int fails;

static void chki(const char *name, int got, int expect)
{
    if (got != expect) {
        printf("FAIL %s got %d expected %d\n", name, got, expect);
        fails++;
    }
}

float fidf(float x)
{
    return x;
}

/* Construct a float from its raw IEEE-754 little-endian bytes so the test can
 * build values (NaN, +/-Inf, -0) that have no portable C89 literal form. */
union fb { float f; unsigned char b[4]; };

static float mkf(unsigned char b0, unsigned char b1,
                 unsigned char b2, unsigned char b3)
{
    union fb u;
    u.b[0] = b0; u.b[1] = b1; u.b[2] = b2; u.b[3] = b3;
    return u.f;
}

int main(void)
{
    float a = 1.0f;
    float b = 2.5f;
    float c = -3.0f;
    float z = 0.0f;
    float nz = -0.0f;
    float pinf, ninf, fnan, big;

    fails = 0;

    pinf = mkf(0x00, 0x00, 0x80, 0x7f);  /* +Inf  0x7F800000 */
    ninf = mkf(0x00, 0x00, 0x80, 0xff);  /* -Inf  0xFF800000 */
    fnan = mkf(0x00, 0x00, 0xc0, 0x7f);  /* qNaN  0x7FC00000 */
    big  = 1.0e30f;

    chki("lt_ab", a < b, 1);
    chki("gt_ba", b > a, 1);
    chki("le_aa", a <= fidf(a), 1);
    chki("ge_aa", a >= fidf(a), 1);
    chki("eq_aa", a == fidf(a), 1);
    chki("ne_ab", a != b, 1);

    chki("lt_ca", c < a, 1);
    chki("gt_ac", a > c, 1);
    chki("lt_ac", a < c, 0);
    chki("gt_ca", c > a, 0);

    chki("eq_z", z == nz, 1);
    chki("ne_z", z != nz, 0);
    chki("le_z", z <= nz, 1);
    chki("ge_z", z >= nz, 1);

    /* NaN: every ordered comparison is false, == is false, != is true. */
    chki("nan_lt_a", fnan <  a,    0);
    chki("nan_gt_a", fnan >  a,    0);
    chki("nan_le_a", fnan <= a,    0);
    chki("nan_ge_a", fnan >= a,    0);
    chki("a_lt_nan", a    <  fnan, 0);
    chki("a_gt_nan", a    >  fnan, 0);
    chki("a_le_nan", a    <= fnan, 0);
    chki("a_ge_nan", a    >= fnan, 0);
    chki("nan_eq",   fnan == fnan, 0);
    chki("nan_ne",   fnan != fnan, 1);

    /* +Inf is greater than any finite and equal to itself. */
    chki("pinf_gt_big", pinf >  big,  1);
    chki("pinf_ge_big", pinf >= big,  1);
    chki("pinf_lt_big", pinf <  big,  0);
    chki("big_lt_pinf", big  <  pinf, 1);
    chki("pinf_eq",     pinf == pinf, 1);
    chki("pinf_le",     pinf <= pinf, 1);
    chki("pinf_ge",     pinf >= pinf, 1);

    /* -Inf is less than any finite, and ordered against +Inf. */
    chki("ninf_lt_c",   ninf <  c,    1);
    chki("ninf_le_c",   ninf <= c,    1);
    chki("ninf_gt_c",   ninf >  c,    0);
    chki("c_gt_ninf",   c    >  ninf, 1);
    chki("ninf_lt_inf", ninf <  pinf, 1);
    chki("pinf_gt_ninf",pinf >  ninf, 1);

    /* Both-negative ordering: raw magnitude order is reversed. */
    chki("c_lt_n2",  c     <  -2.0f, 1);  /* -3 < -2 */
    chki("n2_gt_c",  -2.0f >  c,     1);  /* -2 > -3 */
    chki("c_gt_n2",  c     >  -2.0f, 0);
    chki("c_ge_c",   c     >= -3.0f, 1);
    chki("c_le_c",   c     <= -3.0f, 1);

    /* Mixed-sign and signed-zero vs non-zero. */
    chki("pz_gt_c",  z  >  c, 1);   /* 0  > -3 */
    chki("nz_gt_c",  nz >  c, 1);   /* -0 > -3 */
    chki("pz_lt_a",  z  <  a, 1);   /* 0  <  1 */
    chki("c_lt_pz",  c  <  z, 1);   /* -3 <  0 */

    if (fails) {
        printf("tc89fcmp failed: %d\n", fails);
        return 1;
    }

    printf("tc89fcmp ok\n");
    return 0;
}
