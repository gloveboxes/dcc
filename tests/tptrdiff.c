/* pointer difference scaling regression */
#include <stdio.h>
#include <stddef.h>

typedef unsigned int wchar_t;
typedef unsigned long u32;

static unsigned int my_wcslen(const wchar_t *str)
{
    const wchar_t *orig;
    orig = str;
    while (*str != 0)
        str++;
    return str - orig;
}

static int long_dist(const u32 *a, const u32 *b)
{
    return b - a;
}

int main(void)
{
    wchar_t ws[4];
    u32 xs[5];
    int fails;

    fails = 0;
    ws[0] = 'a';
    ws[1] = 'b';
    ws[2] = 0;
    if (my_wcslen(ws) != 2) {
        printf("FAIL wcslen got %d expected 2\n", (int)my_wcslen(ws));
        fails++;
    }
    if (long_dist(xs, xs + 4) != 4) {
        printf("FAIL long_dist got %d expected 4\n", long_dist(xs, xs + 4));
        fails++;
    }
    if (fails) {
        printf("tptrdiff_shift failed: %d\n", fails);
        return 1;
    }
    printf("tptrdiff completed with great success\n" );
    return 0;
}
