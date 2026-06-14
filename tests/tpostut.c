#include <stdio.h>

static int failures;

static void ck(const char *name, int got, int exp)
{
    if (got != exp) {
        printf("FAIL %s got %d expected %d\n", name, got, exp);
        failures++;
    }
}

int main(void)
{
    unsigned p;
    unsigned lo;
    int dep;
    int n;

    p = 0;
    lo = 0;
    n = 0;
    while (p-- > lo)
        n++;
    ck("unsigned p-- at zero", n, 0);

    p = 3;
    lo = 0;
    n = 0;
    while (p-- > lo)
        n++;
    ck("unsigned p-- count", n, 3);

    dep = 3;
    n = 0;
    while (dep-- > 0)
        n++;
    ck("signed dep-- count", n, 3);

    if (failures) {
        printf("tpostutype2: %d failure(s)\n", failures);
        return 1;
    }
    printf("tpostut all tests passed\n");
    return 0;
}
