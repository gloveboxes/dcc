/* tc89fadd.c - C89 float add/sub regression for dcc */
#include <stdio.h>
#include <stdint.h>

static int fails;

static void chk(const char *name, unsigned char *p,
                unsigned int b0, unsigned int b1,
                unsigned int b2, unsigned int b3)
{
    if (p[0] != b0 || p[1] != b1 || p[2] != b2 || p[3] != b3) {
        printf("FAIL %s got %u %u %u %u expected %u %u %u %u\n",
               name, p[0], p[1], p[2], p[3], b0, b1, b2, b3);
        fails++;
    }
}

float fid(float x)
{
    return x;
}

int main(void)
{
    float a, b, c;
    unsigned char *p;

    fails = 0;
    a = 1.0f;
    b = 2.5f;
    c = a + b;
    p = (unsigned char *)&c;
    chk("add", p, 0, 0, 96, 64);       /* 3.5f */

    c = 5.5f - 2.0f;
    p = (unsigned char *)&c;
    chk("sub", p, 0, 0, 96, 64);       /* 3.5f */

    c = 1.25f;
    c += fid(2.25f);
    p = (unsigned char *)&c;
    chk("addeq", p, 0, 0, 96, 64);     /* 3.5f */

    c -= 1.5f;
    p = (unsigned char *)&c;
    chk("subeq", p, 0, 0, 0, 64);      /* 2.0f */

    if (fails) {
        printf("tc89fadd failed: %d\n", fails);
        return 1;
    }
    printf("tc89fadd ok\n");
    return 0;
}
