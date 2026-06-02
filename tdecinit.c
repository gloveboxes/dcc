#include <stdio.h>

static int failures;

static void cki(const char *name, int got, int exp)
{
    if (got != exp) {
        printf("FAIL %s got %d expected %d\n", name, got, exp);
        failures++;
    }
}

static int val(int x)
{
    return x;
}

static void t_multi_init(int v)
{
    int B = 0, t = v, ones, i;
    ones = 7;
    i = 9;
    cki("B", B, 0);
    cki("t", t, v);
    cki("ones", ones, 7);
    cki("i", i, 9);
}

static void t_comma_paren(void)
{
    int a = (val(1), val(2)), b = 3;
    cki("paren comma a", a, 2);
    cki("paren comma b", b, 3);
}

static void t_mixed_init(void)
{
    int a = 1, b, c = 3, d;
    b = 2;
    d = 4;
    cki("mixed a", a, 1);
    cki("mixed b", b, 2);
    cki("mixed c", c, 3);
    cki("mixed d", d, 4);
}

int main(void)
{
    t_multi_init(5);
    t_comma_paren();
    t_mixed_init();

    if (failures) {
        printf("tdeclinit: %d failure(s)\n", failures);
        return 1;
    }

    printf("tdeclinit: all tests passed\n");
    return 0;
}
