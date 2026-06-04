/* regression test for 'register' keyword — allocates 2-byte locals to BC */

#include <stdio.h>
#include <string.h>

int g_ok;
int g_ng;

void chk(int ok, char *msg)
{
    if (ok)
        g_ok++;
    else {
        printf("FAIL: %s\n", msg);
        g_ng++;
    }
}

/* 1. Basic byte-pointer walk: inc bc / ld a,(hl) after ld l,c; ld h,b */
static void test_walk(void)
{
    static unsigned char buf[8];
    register unsigned char *p;
    int i;

    for (i = 0; i < 8; i++)
        buf[i] = (unsigned char)(i * 3);

    p = buf;
    for (i = 0; i < 8; i++) {
        chk(*p == (unsigned char)(i * 3), "walk value");
        p++;
    }
    chk(p == buf + 8, "walk end ptr");
}

/* 2. Byte scan with early exit — exercises conditional branch on deref */
static void test_scan(void)
{
    static unsigned char buf[6];
    register unsigned char *p;
    register unsigned char *q;  /* second register — falls back to stack */
    int i;

    buf[0] = 1; buf[1] = 2; buf[2] = 0; buf[3] = 3; buf[4] = 4; buf[5] = 5;

    p = buf;
    q = buf;
    i = 0;
    while (*p != 0) {
        i++;
        p++;
    }
    chk(i == 2, "scan stops at zero");
    chk(*p == 0, "scan ptr at zero byte");

    /* q still valid (was stack-allocated since p got BC) */
    chk(q == buf, "second reg var unchanged");
}

/* helper: sums a byte array, exercises spill/reload of caller's BC */
static int sumarray(unsigned char *a, int n)
{
    int s;
    int i;
    s = 0;
    for (i = 0; i < n; i++)
        s += a[i];
    return s;
}

/* 3. Function call inside loop — spills BC before call, reloads after */
static void test_call_spill(void)
{
    static unsigned char buf[5];
    register unsigned char *p;
    int total;
    int i;

    for (i = 0; i < 5; i++)
        buf[i] = (unsigned char)(i + 1);   /* 1,2,3,4,5 */

    p = buf;
    total = 0;

    for (i = 0; i < 5; i++) {
        /* sumarray call spills/reloads p (BC) */
        total += sumarray(p, 1);
        p++;
    }
    chk(total == 15, "sum via spill/reload loop");
    chk(p == buf + 5, "ptr correct after spill/reload loop");
}

/* 4. Call before and after pointer arithmetic */
static void test_call_around(void)
{
    static unsigned char buf[4];
    register unsigned char *p;
    int s;

    buf[0] = 10; buf[1] = 20; buf[2] = 30; buf[3] = 40;

    p = buf;
    s = sumarray(p, 4);   /* spill/reload */
    chk(s == 100, "sum before advance");
    chk(*p == 10, "ptr intact after call");

    p += 2;
    s = sumarray(p, 2);   /* spill/reload after advance */
    chk(s == 70, "sum after advance");
    chk(*p == 30, "ptr correct after advance+call");
}

/* 5. register int is silently treated as a plain local (pointer-only feature) */
static void test_register_int(void)
{
    register int i;
    int sum;

    sum = 0;
    for (i = 1; i <= 10; i++)
        sum += i;
    chk(sum == 55, "register int sum 1..10");
}

/* 6. Post-increment dereference: *p++ */
static void test_postinc(void)
{
    static unsigned char src[5];
    static unsigned char dst[5];
    register unsigned char *p;
    unsigned char *q;
    int i;

    for (i = 0; i < 5; i++)
        src[i] = (unsigned char)(10 + i);

    p = src;
    q = dst;
    for (i = 0; i < 5; i++)
        *q++ = *p++;

    for (i = 0; i < 5; i++)
        chk(dst[i] == (unsigned char)(10 + i), "postinc copy");
    chk(p == src + 5, "postinc end ptr");
}

/* 7. memset-style loop: write through register pointer */
static void test_write(void)
{
    static unsigned char buf[10];
    register unsigned char *p;
    int i;

    p = buf;
    for (i = 0; i < 10; i++) {
        *p = (unsigned char)i;
        p++;
    }

    for (i = 0; i < 10; i++)
        chk(buf[i] == (unsigned char)i, "write through reg ptr");
}

int main(void)
{
    test_walk();
    test_scan();
    test_call_spill();
    test_call_around();
    test_register_int();
    test_postinc();
    test_write();

    if (g_ng == 0)
        printf("success\n");
    else
        printf("%d failures\n", g_ng);

    return g_ng ? 1 : 0;
}
