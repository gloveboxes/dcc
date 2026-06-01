/* focused initializer regression tests for dcc */

#include <stdio.h>

static int fails;

static int garr1[5] = { 1, 2 };
static int gmat1[2][3] = { { 1, 2 }, { 3 } };
static char gstr1[8] = "abc";
static char gstr2[] = "ab" "cd";
static long glng1[3] = { 0x00010000L };
static int gscl1 = { 1234 };

static void cki(const char *n, int g, int e)
{
    if (g != e) {
        printf("FAIL %s got %d expected %d\n", n, g, e);
        fails++;
    }
}

static void ckul(const char *n, unsigned long g, unsigned long e)
{
    if (g != e) {
        printf("FAIL %s got %lu expected %lu\n", n, g, e);
        fails++;
    }
}

static void tglob(void)
{
    cki("garr1 0", garr1[0], 1);
    cki("garr1 1", garr1[1], 2);
    cki("garr1 2", garr1[2], 0);
    cki("garr1 4", garr1[4], 0);

    cki("gmat1 00", gmat1[0][0], 1);
    cki("gmat1 01", gmat1[0][1], 2);
    cki("gmat1 02", gmat1[0][2], 0);
    cki("gmat1 10", gmat1[1][0], 3);
    cki("gmat1 11", gmat1[1][1], 0);
    cki("gmat1 12", gmat1[1][2], 0);

    cki("gstr1 nul", gstr1[3], 0);
    cki("gstr1 tail", gstr1[7], 0);
    cki("gstr2 size", sizeof(gstr2), 5);
    cki("gstr2 d", gstr2[3], 'd');
    cki("gstr2 nul", gstr2[4], 0);

    ckul("glng1 0", (unsigned long)glng1[0], 0x00010000UL);
    ckul("glng1 1", (unsigned long)glng1[1], 0UL);
    cki("gscl1", gscl1, 1234);
}

static void tauto(void)
{
    int a[5] = { 1, 2 };
    int m[2][3] = { { 1, 2 }, { 3 } };
    char s[8] = "abc";
    char t[] = "xy" "z";
    long l[3] = { 0x00010000L };
    int x = { 4321 };

    cki("a 0", a[0], 1);
    cki("a 1", a[1], 2);
    cki("a 2", a[2], 0);
    cki("a 4", a[4], 0);

    cki("m 00", m[0][0], 1);
    cki("m 01", m[0][1], 2);
    cki("m 02", m[0][2], 0);
    cki("m 10", m[1][0], 3);
    cki("m 11", m[1][1], 0);
    cki("m 12", m[1][2], 0);

    cki("s nul", s[3], 0);
    cki("s tail", s[7], 0);
    cki("t size", sizeof(t), 4);
    cki("t z", t[2], 'z');
    cki("t nul", t[3], 0);

    ckul("l 0", (unsigned long)l[0], 0x00010000UL);
    ckul("l 1", (unsigned long)l[1], 0UL);
    cki("x", x, 4321);
}

int main(void)
{
    tglob();
    tauto();

    if (fails) {
        printf("tinitreg: %d failure(s)\n", fails);
        return 1;
    }

    printf("tinitreg: all tests passed\n");
    return 0;
}
