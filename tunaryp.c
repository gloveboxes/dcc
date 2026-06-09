#include <stdio.h>

typedef unsigned char uint8_t;
typedef signed char int8_t;

static int fails;
static void chkl(const char *name, long got, long expected)
{
    if (got != expected) {
        printf("FAIL %s got %ld expected %ld\n", name, got, expected);
        fails++;
    }
}

int main(void)
{
    uint8_t b;
    int8_t a;
    b = 200;
    a = -10;
    chkl("~b", (long)(~b), -201L);
    chkl("+b", (long)(+b), 200L);
    chkl("-b", (long)(-b), -200L);
    chkl("~a", (long)(~a), 9L);
    if (fails) return 1;
    printf("tunary_promote completed\n");
    return 0;
}
