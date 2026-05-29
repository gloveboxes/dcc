/* tc89fnty.c - typedef function type tests */
#include <stdio.h>

typedef int fnya(int);
typedef int (*fnyb)(int);

extern fnya extf;

static int fails;

static int adda(int x) { return x + 1; }
static int mulb(int x) { return x * 2; }
static int subc(int x) { return x - 3; }

static void chki(const char *name, int got, int expect)
{
    if (got != expect) {
        printf("FAIL %s got %d expected %d\n", name, got, expect);
        fails++;
    }
}

static int aply(fnya *fn, int x)
{
    return fn(x);
}

int main(void)
{
    fnya *fp;
    fnyb gp;
    fnya *tab[3];

    printf("tc89fnty start\n");
    fails = 0;

    fp = adda;
    gp = mulb;
    tab[0] = adda;
    tab[1] = mulb;
    tab[2] = subc;

    chki("fn_type_ptr", fp(10), 11);
    chki("fp_typedef", gp(10), 20);
    chki("fn_param", aply(subc, 10), 7);
    chki("fn_array0", tab[0](20), 21);
    chki("fn_array1", tab[1](20), 40);
    chki("fn_array2", tab[2](20), 17);

    if (fails) {
        printf("tc89fnty failed: %d\n", fails);
        return 1;
    }

    printf("tc89fnty completed with great success\n");
    return 0;
}
