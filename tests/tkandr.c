#include <stdio.h>

int add(a, b)
int a;
int b;
{
    return a + b;
}

int default_int(a, b)
{
    return a * 10 + b;
}

int ptrsum(p, n)
char *p;
int n;
{
    int i;
    int s;

    s = 0;
    for (i = 0; i < n; ++i)
        s += p[i];
    return s;
}

long ladd(a, b, c)
long a;
int b;
long c;
{
    return a + b + c;
}

int uchar_mix(a, b, c)
char a;
unsigned char b;
int c;
{
    return a + b + c;
}

int main()
{
    char s[4];
    int fails;
    long lv;

    fails = 0;
    s[0] = 1;
    s[1] = 2;
    s[2] = 3;
    s[3] = 4;

    if (add(7, 5) != 12) {
        printf("FAIL add\n");
        fails++;
    }
    if (default_int(3, 4) != 34) {
        printf("FAIL default_int\n");
        fails++;
    }
    if (ptrsum(s, 4) != 10) {
        printf("FAIL ptrsum\n");
        fails++;
    }
    lv = ladd(100000L, 23, 4000L);
    if (lv != 104023L) {
        printf("FAIL ladd\n");
        fails++;
    }
    if (uchar_mix(-1, 2, 3) != 4) {
        printf("FAIL uchar_mix\n");
        fails++;
    }

    if (fails) {
        printf("tkandr failed: %d\n", fails);
        return 1;
    }

    printf("tkandr completed with great success\n");
    return 0;
}
