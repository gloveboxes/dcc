/* ttype2.c - sizeof(type) and cast declarator regression tests */

#include <stdio.h>

typedef char C4[4];
typedef long L2[2];
typedef unsigned long UL;
typedef char *CP;
typedef int (*IFP)(int);

static int fails;
static char buf[16];

static void ckul(const char *name, unsigned long got, unsigned long exp)
{
    if (got != exp) {
        printf("FAIL %s got %lu expected %lu\n", name, got, exp);
        fails++;
    }
}

static int fadd(int x)
{
    return x + 1;
}

static void tsiz2(void)
{
    ckul("sizeof int[4]", (unsigned long)sizeof(int[4]), 8UL);
    ckul("sizeof char *", (unsigned long)sizeof(char *), 2UL);
    ckul("sizeof char *[3]", (unsigned long)sizeof(char *[3]), 6UL);
    ckul("sizeof char (*)[4]", (unsigned long)sizeof(char (*)[4]), 2UL);
    ckul("sizeof long[2][3]", (unsigned long)sizeof(long[2][3]), 24UL);
    ckul("sizeof typedef array", (unsigned long)sizeof(C4), 4UL);
    ckul("sizeof typedef long array", (unsigned long)sizeof(L2), 8UL);
    ckul("sizeof typedef ptr", (unsigned long)sizeof(CP), 2UL);
    ckul("sizeof func ptr", (unsigned long)sizeof(IFP), 2UL);
}

static void tcast(void)
{
    unsigned long ul;
    long sl;
    char *p;
    IFP fp;

    ul = (long unsigned int)0x8000U;
    ckul("cast long unsigned int", ul, 0x00008000UL);

    ul = (unsigned long)(unsigned int)0xffffU;
    ckul("cast uint to ulong", ul, 0x0000ffffUL);

    sl = (signed long int)(unsigned int)0xffffU;
    ckul("cast uint to signed long", (unsigned long)sl, 0x0000ffffUL);

    p = (char *)buf;
    p[0] = 'a';
    p = (CP)p;
    p[1] = 'b';
    ckul("cast typedef ptr", (unsigned long)buf[1], (unsigned long)'b');

    fp = (IFP)fadd;
    ckul("func ptr cast size", (unsigned long)sizeof(fp), 2UL);
}

int main(void)
{
    tsiz2();
    tcast();

    if (fails) {
        printf("ttype2: %d failure(s)\n", fails);
        return 1;
    }
    printf("ttype2: all tests passed\n");
    return 0;
}
