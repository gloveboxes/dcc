#include <stdio.h>
#include <stdlib.h>

static int failures;

static void chki(const char *name, int got, int expected)
{
    if (got != expected) {
        printf("FAIL %s: got %d expected %d\n", name, got, expected);
        failures++;
    }
}

int main(void)
{
    /* Print a selection of exactly-representable values for baseline. */
    printf("%f\n", (float)atof("0"));
    printf("%f\n", (float)atof("1"));
    printf("%f\n", (float)atof("-1"));
    printf("%f\n", (float)atof("3.5"));
    printf("%f\n", (float)atof("-2.5"));
    printf("%f\n", (float)atof("0.25"));
    printf("%f\n", (float)atof("100"));
    printf("%f\n", (float)atof("1e3"));
    printf("%f\n", (float)atof("1.5e2"));
    printf("%f\n", (float)atof("  +3.5"));
    printf("%f\n", (float)atof("+0.5e1"));

    /* Integer-cast checks. */
    chki("zero",      (int)atof("0"),       0);
    chki("pos_int",   (int)atof("42"),      42);
    chki("neg_int",   (int)atof("-17"),     -17);
    chki("large",     (int)atof("1000"),    1000);
    chki("neg_large", (int)atof("-1000"),   -1000);
    chki("frac2",     (int)(atof("3.5") * 2.0f), 7);
    chki("exp2",      (int)atof("1e2"),     100);
    chki("neg_exp",   (int)(atof("5e-1") * 2.0f), 1);

    if (failures)
        printf("tatof FAILED %d\n", failures);
    else
        printf("tatof ok\n");
    return failures ? 1 : 0;
}
