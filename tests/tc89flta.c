/* tc89flta.c - float assignment/copy/call/return storage tests */
#include <stdio.h>

static int fails;

static void chkb(const char *name, unsigned char got, unsigned char expect)
{
    if (got != expect) {
        printf("FAIL %s got %u expected %u\n", name, (unsigned int)got, (unsigned int)expect);
        fails++;
    }
}

static void chk4(const char *name, float *pf, unsigned char b0, unsigned char b1, unsigned char b2, unsigned char b3)
{
    unsigned char *p;
    p = (unsigned char *)pf;
    chkb(name, p[0], b0);
    chkb(name, p[1], b1);
    chkb(name, p[2], b2);
    chkb(name, p[3], b3);
}

struct Sflt { int tag; float f; };

static float gf;
static float gg;
static float ga[2];

static float f_id(float x)
{
    return x;
}

static float f_gv(void)
{
    return ga[1];
}

static void f_st(float x)
{
    gg = x;
}

int main(void)
{
    float lf;
    float lg;
    float lh;
    float la[2];
    struct Sflt s;

    fails = 0;
    printf("tc89flta start\n");

    gf = 1.0f;
    chk4("gf", &gf, 0, 0, 128, 63);

    gg = gf;
    chk4("gg", &gg, 0, 0, 128, 63);

    lf = 2.5f;
    chk4("lf", &lf, 0, 0, 32, 64);

    lg = lf;
    chk4("lg", &lg, 0, 0, 32, 64);

    ga[0] = 1.0f;
    ga[1] = -3.0f;
    chk4("ga0", &ga[0], 0, 0, 128, 63);
    chk4("ga1", &ga[1], 0, 0, 64, 192);

    la[0] = 2.0f;
    la[1] = la[0];
    chk4("la0", &la[0], 0, 0, 0, 64);
    chk4("la1", &la[1], 0, 0, 0, 64);

    s.f = 1.0f;
    chk4("sf", &s.f, 0, 0, 128, 63);

    lh = f_id(la[1]);
    chk4("fid", &lh, 0, 0, 0, 64);

    lh = f_gv();
    chk4("fgv", &lh, 0, 0, 64, 192);

    f_st(lf);
    chk4("fst", &gg, 0, 0, 32, 64);

    if (fails) {
        printf("tc89flta failed: %d\n", fails);
        return 1;
    }
    printf("tc89flta ok\n");
    return 0;
}
