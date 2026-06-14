/* tc89fmat.c - float math helper regression */

#include <stdio.h>
#include <stdint.h>
#include <math.h>

static int fails;

static void chkf(const char *name, float f, unsigned char b0, unsigned char b1, unsigned char b2, unsigned char b3)
{
    unsigned char *p;
    p = (unsigned char *)&f;
    if (p[0] != b0 || p[1] != b1 || p[2] != b2 || p[3] != b3) {
        printf("FAIL %s got %u %u %u %u expected %u %u %u %u\n",
               name, p[0], p[1], p[2], p[3], b0, b1, b2, b3);
        fails++;
    }
}

int main(void)
{
    float a, b;

    fails = 0;

    a = -2.5f;
    b = fabsf(a);
    chkf("fabs", b, 0, 0, 32, 64);       /* 2.5 */

    a = 2.75f;
    b = floorf(a);
    chkf("floorp", b, 0, 0, 0, 64);      /* 2.0 */

    a = -2.25f;
    b = floorf(a);
    chkf("floorn", b, 0, 0, 64, 192);    /* -3.0 */

    a = 2.25f;
    b = ceilf(a);
    chkf("ceilp", b, 0, 0, 64, 64);      /* 3.0 */

    a = -2.75f;
    b = ceilf(a);
    chkf("ceiln", b, 0, 0, 0, 192);      /* -2.0 */

    a = 4.0f;
    b = sqrtf(a);
    chkf("sqrt4", b, 0, 0, 0, 64);       /* 2.0 */

    a = 9.0f;
    b = sqrtf(a);
    chkf("sqrt9", b, 0, 0, 64, 64);      /* 3.0 */

    if (fails) {
        printf("tc89fmat failed: %d\n", fails);
        return 1;
    }

    printf("tc89fmat ok\n");
    return 0;
}
