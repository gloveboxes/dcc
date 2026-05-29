/* tc89pp.c - C89 preprocessor regression test */
#include <stdio.h>

static int fails;

static void ckpi(const char *name, int got, int exp)
{
    if (got != exp) {
        printf("FAIL %s got %d expected %d\n", name, got, exp);
        fails++;
    }
}

#define FOO 1
#define BAR 0

#if defined(FOO) && !defined(NO_SUCH_SYMBOL)
#define VDEF 11
#else
#define VDEF 99
#endif

#if BAR
#define VELI 1
#elif defined(FOO) && (FOO == 1)
#define VELI 22
#else
#define VELI 99
#endif

#undef FOO

#if defined(FOO)
#define VUND 99
#elif !defined(FOO)
#define VUND 33
#else
#define VUND 88
#endif

#if 0
#error inactive error should not fire
#endif

#if defined(BAR) && (BAR != 0)
#error wrong branch should not fire
#elif !defined(FOO)
#define VERR 44
#else
#error elif handling failed
#endif

int main(void)
{
    printf("tc89pp start\n");
    fails = 0;
    ckpi("defined", VDEF, 11);
    ckpi("elif", VELI, 22);
    ckpi("undef", VUND, 33);
    ckpi("error", VERR, 44);

    if (fails) {
        printf("tc89pp failed: %d\n", fails);
        return 1;
    }
    printf("tc89pp completed with great success\n");
    return 0;
}
