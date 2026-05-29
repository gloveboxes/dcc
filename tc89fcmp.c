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

int main(void)
{
    float a = 1.0f;
    float b = 2.5f;
    float c = -3.0f;
    float z = 0.0f;
    float nz = -0.0f;

    fails = 0;

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

    if (fails) {
        printf("tc89fcmp failed: %d\n", fails);
        return 1;
    }

    printf("tc89fcmp ok\n");
    return 0;
}
