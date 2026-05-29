/* tc89fmul.c - float multiplication smoke tests */
#include <stdio.h>

static int fails;

static void chk(const char *name, float v, unsigned char b0, unsigned char b1, unsigned char b2, unsigned char b3)
{
    unsigned char *p;
    p = (unsigned char *)&v;
    if (p[0] != b0 || p[1] != b1 || p[2] != b2 || p[3] != b3) {
        printf("FAIL %s got %u %u %u %u expected %u %u %u %u\n",
               name, p[0], p[1], p[2], p[3], b0, b1, b2, b3);
        fails++;
    }
}

int main(void)
{
    float a;
    float b;
    float c;

    fails = 0;
    a = 2.0f;
    b = 3.0f;
    c = a * b;
    chk("mul6", c, 0, 0, 192, 64);      /* 6.0 */

    a = 1.5f;
    b = 2.0f;
    c = a * b;
    chk("mul3", c, 0, 0, 64, 64);       /* 3.0 */

    a = -2.0f;
    b = 3.0f;
    c = a * b;
    chk("muln6", c, 0, 0, 192, 192);    /* -6.0 */

    a = 5.0f;
    a *= 0.5f;
    chk("muleq", a, 0, 0, 32, 64);      /* 2.5 */

    a = 0.0f;
    b = 7.0f;
    c = a * b;
    chk("mul0", c, 0, 0, 0, 0);

    if (fails) {
        printf("tc89fmul failed: %d\n", fails);
        return 1;
    }
    printf("tc89fmul ok\n");
    return 0;
}
