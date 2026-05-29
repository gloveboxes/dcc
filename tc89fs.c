/* tc89fs.c - float fields in structs and arrays of structs */

#include <stdio.h>
#include <stdint.h>

struct FS {
    int id;
    float f;
    int tail;
};

static int fails;

static void ckbi(const char *name, int got, int exp)
{
    if (got != exp) {
        printf("FAIL %s got %d expected %d\n", name, got, exp);
        fails++;
    }
}

static void ckbf(const char *name, float *fp,
                 unsigned char b0, unsigned char b1,
                 unsigned char b2, unsigned char b3)
{
    unsigned char *p;
    p = (unsigned char *)fp;
    if (p[0] != b0 || p[1] != b1 || p[2] != b2 || p[3] != b3) {
        printf("FAIL %s got %u %u %u %u\n", name, p[0], p[1], p[2], p[3]);
        fails++;
    }
}

float fidf(float x)
{
    return x;
}

int main(void)
{
    struct FS a[3];
    struct FS *p;

    fails = 0;

    a[0].id = 10;
    a[0].f = 1.0f;
    a[0].tail = 20;

    a[1].id = 11;
    a[1].f = fidf(a[0].f);
    a[1].tail = 21;

    a[2].id = 12;
    a[2].f = 2.5f;
    a[2].tail = 22;

    p = a;
    p++;
    p->f = fidf(a[2].f);

    ckbi("id0", a[0].id, 10);
    ckbi("tl0", a[0].tail, 20);
    ckbf("f0", &a[0].f, 0, 0, 128, 63);

    ckbi("id1", a[1].id, 11);
    ckbi("tl1", a[1].tail, 21);
    ckbf("f1", &a[1].f, 0, 0, 32, 64);

    ckbi("id2", a[2].id, 12);
    ckbi("tl2", a[2].tail, 22);
    ckbf("f2", &a[2].f, 0, 0, 32, 64);

    if (fails) {
        printf("tc89fs failed: %d\n", fails);
        return 1;
    }

    printf("tc89fs ok\n");
    return 0;
}
