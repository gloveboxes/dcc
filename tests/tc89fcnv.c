/* tc89fcnv.c - float/int conversion tests */
#include <stdio.h>
#include <stdint.h>

static int fails;

static void chki(const char *n, int got, int exp)
{
    if (got != exp) {
        printf("FAIL %s got %d expected %d\n", n, got, exp);
        fails++;
    }
}

static void chku(const char *n, unsigned int got, unsigned int exp)
{
    if (got != exp) {
        printf("FAIL %s got %u expected %u\n", n, got, exp);
        fails++;
    }
}

static void chkf(const char *n, float f, unsigned char b0, unsigned char b1,
                 unsigned char b2, unsigned char b3)
{
    unsigned char *p;
    p = (unsigned char *)&f;
    if (p[0] != b0 || p[1] != b1 || p[2] != b2 || p[3] != b3) {
        printf("FAIL %s got %u %u %u %u\n", n, p[0], p[1], p[2], p[3]);
        fails++;
    }
}

int main(void)
{
    int si;
    unsigned int ui;
    float f;

    fails = 0;

    si = 3;
    f = (float)si;
    chkf("i2f3", f, 0, 0, 64, 64);

    si = -2;
    f = (float)si;
    chkf("i2fn2", f, 0, 0, 0, 192);

    ui = 65535U;
    f = (float)ui;
    chkf("u2f65535", f, 0, 255, 127, 71);

    f = 3.0f;
    si = (int)f;
    chki("f2i3", si, 3);

    f = -2.0f;
    si = (int)f;
    chki("f2in2", si, -2);

    f = 65535.0f;
    ui = (unsigned int)f;
    chku("f2u65535", ui, 65535U);

    if (fails) {
        printf("tc89fcnv failed: %d\n", fails);
        return 1;
    }

    printf("tc89fcnv ok\n");
    return 0;
}
