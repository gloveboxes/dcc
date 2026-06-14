/* basic long arithmetic and I/O */

#include <stdio.h>
#include <stdint.h>

long g_s = 0;
unsigned long g_u = 0;

void tbasic(void)
{
    long a, b;
    unsigned long ua, ub;

    printf("basic arithmetic\n");
    a = 100000L;
    b = 200000L;
    printf("%ld\n", a + b);        /* 300000 */
    printf("%ld\n", b - a);        /* 100000 */
    printf("%ld\n", a - b);        /* -100000 */
    printf("%ld\n", a * 3L);       /* 300000 */
    printf("%ld\n", b / 5L);       /* 40000 */
    printf("%ld\n", b % 7L);       /* 200000 % 7 = 4 */

    ua = 100000UL;
    ub = 200000UL;
    printf("%lu\n", ua + ub);      /* 300000 */
    printf("%lu\n", ub - ua);      /* 100000 */
    printf("%lu\n", ua * 3UL);     /* 300000 */
    printf("%lu\n", ub / 5UL);     /* 40000 */
    printf("%lu\n", ub % 7UL);     /* 200000 % 7 = 4 */
}

void tneg(void)
{
    long a, b;
    printf("negative values\n");
    a = -1L;
    b = -100000L;
    printf("%ld\n", a);            /* -1 */
    printf("%ld\n", b);            /* -100000 */
    printf("%ld\n", -a);           /* 1 */
    printf("%ld\n", -b);           /* 100000 */
    printf("%ld\n", a + b);        /* -100001 */
    printf("%ld\n", a - b);        /* 99999 */
    printf("%ld\n", b * -2L);      /* 200000 */
    printf("%ld\n", b / -1L);      /* 100000 */
    printf("%ld\n", b % 3L);       /* -100000 % 3 = -1 */
}

void tbitw(void)
{
    long a;
    unsigned long ua;
    printf("bitwise\n");
    a = 0x00FF00FFL;
    printf("%ld\n", a & 0x0000FFFFL);    /* 255 */
    printf("%ld\n", a | 0xFF000000L);    /* -16711681 */
    printf("%ld\n", a ^ 0xFFFFFFFFL);    /* -16711936 */
    printf("%ld\n", ~a);                  /* -16711937 */
    ua = 0x00FF00FFU;
    printf("%lu\n", ua & 0x0000FFFFUL);  /* 255 */
    printf("%lu\n", ua | 0xFF000000UL);  /* 4278255615 */
    printf("%lu\n", ~ua);                 /* 4278255360 */
}

void tshft(void)
{
    long a;
    unsigned long ua;
    printf("shifts\n");
    a = 1L;
    printf("%ld\n", a << 0);    /* 1 */
    printf("%ld\n", a << 1);    /* 2 */
    printf("%ld\n", a << 15);   /* 32768 */
    printf("%ld\n", a << 16);   /* 65536 */
    printf("%ld\n", a << 31);   /* -2147483648 */
    a = -2147483648L;
    printf("%ld\n", a >> 1);    /* -1073741824 */
    printf("%ld\n", a >> 31);   /* -1 */
    ua = 2147483648UL;
    printf("%lu\n", ua >> 1);   /* 1073741824 */
    printf("%lu\n", ua >> 31);  /* 1 */
}

void tcomp(void)
{
    long a, b;
    unsigned long ua, ub;
    printf("comparisons\n");
    a = 100000L;  b = 200000L;
    printf("%d\n", a < b);    /* 1 */
    printf("%d\n", a > b);    /* 0 */
    printf("%d\n", a == b);   /* 0 */
    printf("%d\n", a != b);   /* 1 */
    printf("%d\n", a <= b);   /* 1 */
    printf("%d\n", a >= b);   /* 0 */
    a = -1L;  b = 1L;
    printf("%d\n", a < b);    /* 1 (signed: -1 < 1) */
    printf("%d\n", a > b);    /* 0 */
    ua = 0xFFFF0000UL;  ub = 1UL;
    printf("%d\n", ua < ub);  /* 0 (unsigned: big > small) */
    printf("%d\n", ua > ub);  /* 1 */
}

void tglob(void)
{
    printf("globals\n");
    g_s = -123456L;
    g_u = 654321UL;
    printf("%ld\n", g_s);        /* -123456 */
    printf("%lu\n", g_u);        /* 654321 */
    g_s = g_s + 1L;
    g_u = g_u + 1UL;
    printf("%ld\n", g_s);        /* -123455 */
    printf("%lu\n", g_u);        /* 654322 */
}

long ident(long x) { return x; }
unsigned long uident(unsigned long x) { return x; }

long lsum(long a, long b) { return a + b; }

void tcall(void)
{
    printf("funcall\n");
    printf("%ld\n", ident(100000L));    /* 100000 */
    printf("%ld\n", ident(-1L));        /* -1 */
    printf("%lu\n", uident(200000UL));  /* 200000 */
    printf("%ld\n", lsum(100000L, 200000L));  /* 300000 */
}

void tasgn(void)
{
    long a;
    unsigned long ua;
    printf("assign ops\n");
    a = 100000L;
    a += 50000L;   printf("%ld\n", a);   /* 150000 */
    a -= 20000L;   printf("%ld\n", a);   /* 130000 */
    a *= 2L;       printf("%ld\n", a);   /* 260000 */
    a /= 4L;       printf("%ld\n", a);   /* 65000 */
    a %= 7L;       printf("%ld\n", a);   /* 65000 % 7 = 2 */
    a = 0xFFFFFFFL;
    a &= 0x0FFFFFFL; printf("%ld\n", a); /* 268435455 */
    a |= 0x10000000L; printf("%ld\n", a); /* 536870911 */
    a = 0x0F0F0F0FL;
    a ^= 0xF0F0F0F0L; printf("%ld\n", a); /* -1 */
    a = 1L;
    a <<= 16;  printf("%ld\n", a);       /* 65536 */
    a >>= 8;   printf("%ld\n", a);       /* 256 */
}

int main()
{
    tbasic();
    tneg();
    tbitw();
    tshft();
    tcomp();
    tglob();
    tcall();
    tasgn();
    printf("tlong done\n");
    return 0;
}
