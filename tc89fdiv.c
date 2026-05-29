/* tc89fdiv.c - float division tests for dcc */

#include <stdio.h>
#include <stdint.h>

static int fails;

static void chkf(const char *name, float v,
                 unsigned char b0, unsigned char b1,
                 unsigned char b2, unsigned char b3)
{
    unsigned char *p;
    p = (unsigned char *)&v;
    if (p[0] != b0 || p[1] != b1 || p[2] != b2 || p[3] != b3) {
        printf("FAIL %s got %u %u %u %u expected %u %u %u %u\n",
               name, p[0], p[1], p[2], p[3], b0, b1, b2, b3);
        fails++;
    }
}

float fidv(float x)
{
    return x;
}

int main(void)
{
    float a;
    float b;
    float c;

    fails = 0;

    a = 6.0f;
    b = 2.0f;
    c = a / b;
    chkf("div6_2", c, 0, 0, 64, 64);       /* 3.0 */

    a = 7.5f;
    b = 2.5f;
    c = a / b;
    chkf("div75", c, 0, 0, 64, 64);        /* 3.0 */

    a = -6.0f;
    b = 2.0f;
    c = a / b;
    chkf("divn6", c, 0, 0, 64, 192);       /* -3.0 */

    a = 1.0f;
    b = 2.0f;
    c = a / b;
    chkf("div12", c, 0, 0, 0, 63);         /* 0.5 */

    a = 5.0f;
    b = -2.0f;
    c = a / b;
    chkf("div5n2", c, 0, 0, 32, 192);      /* -2.5 */

    c = 9.0f;
    c /= fidv(3.0f);
    chkf("diveq", c, 0, 0, 64, 64);        /* 3.0 */

    c = 1.0f;
    c /= 0.0f;
    chkf("divzero", c, 0, 0, 128, 127);    /* +inf */

    if (fails) {
        printf("tc89fdiv failed: %d\n", fails);
        return 1;
    }

    printf("tc89fdiv ok\n");
    return 0;
}
