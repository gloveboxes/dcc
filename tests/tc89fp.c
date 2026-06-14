#include <stdio.h>

static int fails;

static int adone(int x) { return x + 1; }
static int subtw(int x) { return x - 2; }
static int multh(int x) { return x * 3; }

static void chki(const char *name, int got, int expect)
{
    if (got != expect) {
        printf("FAIL %s got %d expected %d\n", name, got, expect);
        fails++;
    }
}

int main(void)
{
    int (*fp)(int);
    int (*tab[3])(int);
    int v;

    fails = 0;

    fp = adone;
    chki("fp_direct", fp(10), 11);
    chki("fp_star", (*fp)(20), 21);

    fp = subtw;
    chki("fp_reassign", fp(10), 8);

    tab[0] = adone;
    tab[1] = subtw;
    tab[2] = multh;

    chki("tab0", tab[0](5), 6);
    chki("tab1", tab[1](5), 3);
    chki("tab2", tab[2](5), 15);

    fp = tab[2];
    v = fp(7);
    chki("tab_to_fp", v, 21);

    if (fails) {
        printf("tc89fp failed %d\n", fails);
        return 1;
    }

    printf("tc89fp complted with great success\n");
    return 0;
}
