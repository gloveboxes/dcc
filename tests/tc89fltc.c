/* tc89fltc.c - float constant storage test */

#include <stdio.h>

static int fails;
static float gf = 1.0f;
static float ga[] = { 1.0f, 2.5f, -3.0f };

static void chkb(const char *name, unsigned char got, unsigned char expect)
{
    if (got != expect) {
        printf("FAIL %s got %u expected %u\n", name, (unsigned int)got, (unsigned int)expect);
        fails++;
    }
}

static void chkword(const char *name, unsigned char *p, unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
    chkb(name, p[0], a);
    chkb(name, p[1], b);
    chkb(name, p[2], c);
    chkb(name, p[3], d);
}

int main(void)
{
    unsigned char *p;
    float lf = 1.25f;
    float la[2] = { 0.5f, -2.0f };

    fails = 0;
    p = (unsigned char *)&gf;
    chkword("gf", p, 0, 0, 128, 63);

    p = (unsigned char *)&ga[0];
    chkword("ga0", p, 0, 0, 128, 63);
    p = (unsigned char *)&ga[1];
    chkword("ga1", p, 0, 0, 32, 64);
    p = (unsigned char *)&ga[2];
    chkword("ga2", p, 0, 0, 64, 192);

    p = (unsigned char *)&lf;
    chkword("lf", p, 0, 0, 160, 63);
    p = (unsigned char *)&la[0];
    chkword("la0", p, 0, 0, 0, 63);
    p = (unsigned char *)&la[1];
    chkword("la1", p, 0, 0, 0, 192);

    if (sizeof(float) != 4)
        fails++;
    if (sizeof(ga) != 12)
        fails++;

    if (fails) {
        printf("tc89fltc failed: %d\n", fails);
        return 1;
    }
    printf("tc89fltc ok\n");
    return 0;
}
