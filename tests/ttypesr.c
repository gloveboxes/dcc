/* ttypereg.c - focused C89 type-specifier/typedef regression tests */

#include <stdio.h>

static int fails;

typedef long unsigned int tulng, *ptulg;
typedef unsigned short ushort, *pushr;
typedef const unsigned char cuchar;
typedef int iarr4[4], *pintp;
typedef int fnone(int);
typedef int (*pfone)(int);

static iarr4 garr = { 1, 2, 3, 4 };

static void cki(const char *name, int got, int exp)
{
    if (got != exp) {
        printf("FAIL %s got %d expected %d\n", name, got, exp);
        fails++;
    }
}

static void ckul(const char *name, unsigned long got, unsigned long exp)
{
    if (got != exp) {
        printf("FAIL %s got %lu expected %lu\n", name, got, exp);
        fails++;
    }
}

static int add1(int x)
{
    return x + 1;
}

int main(void)
{
    long unsigned int lu;
    unsigned long int ul;
    signed int si;
    unsigned short int usi;
    tulng tv;
    ptulg pp;
    ushort us;
    pushr pus;
    cuchar ch = (cuchar)'A';
    iarr4 la;
    pintp ip;
    pfone fp;
    fnone *fp2;

    lu = 0x12345678UL;
    ul = 0x87654321UL;
    si = -3;
    usi = 0xffffU;
    tv = lu + 1UL;
    pp = &tv;
    us = usi;
    pus = &us;

    la[0] = 5;
    la[1] = 6;
    la[2] = 7;
    la[3] = 8;
    ip = la;

    fp = add1;
    fp2 = add1;

    cki("sizeof tulng", sizeof(tulng), 4);
    cki("sizeof ptulg", sizeof(ptulg), 2);
    cki("sizeof ushort", sizeof(ushort), 2);
    cki("sizeof iarr4 object", sizeof(la), 8);
    cki("array typedef index", la[3], 8);
    cki("global array typedef", garr[2], 3);
    ckul("long unsigned order", lu, 0x12345678UL);
    ckul("unsigned long order", ul, 0x87654321UL);
    ckul("typedef long unsigned", tv, 0x12345679UL);
    ckul("typedef pointer deref", *pp, 0x12345679UL);
    cki("unsigned short order", (int)*pus, 65535);
    cki("const uchar typedef", (int)ch, 65);
    cki("pointer typedef index", ip[1], 6);
    cki("function pointer typedef", fp(10), 11);
    cki("function typedef pointer", fp2(20), 21);
    cki("signed int order", si, -3);

    if (fails) {
        printf("ttypereg: %d failure(s)\n", fails);
        return 1;
    }
    printf("ttypereg: all tests passed\n");
    return 0;
}
