/* tvariad.c - C89 variadic function regression test for dcc */
#include <stdio.h>
#include <stdarg.h>

static int sum_i(int n, ...)
{
    va_list ap;
    int i;
    int total;

    total = 0;
    va_start(ap, n);
    for (i = 0; i < n; ++i)
        total = total + va_arg(ap, int);
    va_end(ap);
    return total;
}

static long sum_l(int n, ...)
{
    va_list ap;
    int i;
    long total;

    total = 0L;
    va_start(ap, n);
    for (i = 0; i < n; ++i)
        total = total + va_arg(ap, long);
    va_end(ap);
    return total;
}

int main(void)
{
    int si;
    long sl;
    int ok;

    ok = 1;
    si = sum_i(5, 1, 2, 3, 4, 5);
    if (si != 15) ok = 0;

    sl = sum_l(4, 100000L, 200000L, -30000L, 7L);
    if (sl != 270007L) ok = 0;

    if (!ok) {
        printf("variadic test failed %d %ld\n", si, sl);
        return 1;
    }

    printf("variadic test passed with great success\n");
    return 0;
}
