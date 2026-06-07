/* tc89swjt.c - switch jump table / C89 switch regression */
#include <stdio.h>

static int fails;

enum SwEnum {
    SW_A = 7,
    SW_B = 8,
    SW_C = 10
};

static void chki(const char *n, int g, int e)
{
    if (g != e) {
        printf("FAIL %s got %d expected %d\n", n, g, e);
        fails++;
    }
}

static int swdn(int x)
{
    switch (x) {
    case 2: return 20;
    case 3: return 30;
    case 4: return 40;
    case 5: return 50;
    default: return 99;
    }
}

static int swft(int x)
{
    int r;
    r = 0;
    switch (x) {
    case 1:
        r += 1;
    case 2:
        r += 2;
        break;
    case 3:
        r += 3;
    case 4:
        r += 4;
        break;
    default:
        r = 9;
        break;
    }
    return r;
}

static int swsp(int x)
{
    switch (x) {
    case 10: return 1;
    case 12: return 2;
    case 14: return 3;
    default: return 4;
    }
}

static int swdefmid(int x)
{
    int r;
    r = 0;
    switch (x) {
    case 1:
        r = 10;
        break;
    default:
        r = 90;
        break;
    case 2:
        r = 20;
        break;
    case 3:
        r = 30;
        break;
    }
    return r;
}

static int swdef_fall(int x)
{
    int r;
    r = 1;
    switch (x) {
    default:
        r += 10;
    case 4:
        r += 40;
        break;
    case 5:
        r += 50;
        break;
    }
    return r;
}

static int swneg(int x)
{
    switch (x) {
    case -2: return 22;
    case -1: return 11;
    case 0:  return 100;
    default: return 99;
    }
}

static int swexpr(int x)
{
    switch (x) {
    case 'A': return 1;
    case SW_A + 1: return 2;
    case (1 << 4) | 1: return 3;
    default: return 4;
    }
}

static int swnest(int x, int y)
{
    int r;
    r = 0;
    switch (x) {
    case 1:
        r += 1;
        switch (y) {
        case 2:
            r += 20;
            break;
        default:
            r += 90;
            break;
        }
        r += 100;
        break;
    case 2:
        r = 2;
        break;
    default:
        r = 9;
        break;
    }
    return r;
}

int main(void)
{
    printf("tc89swjt start\n");
    fails = 0;

    chki("dense_low", swdn(2), 20);
    chki("dense_mid", swdn(4), 40);
    chki("dense_high", swdn(5), 50);
    chki("dense_def1", swdn(1), 99);
    chki("dense_def2", swdn(6), 99);

    chki("fall_1", swft(1), 3);
    chki("fall_2", swft(2), 2);
    chki("fall_3", swft(3), 7);
    chki("fall_4", swft(4), 4);
    chki("fall_d", swft(9), 9);

    chki("sparse_10", swsp(10), 1);
    chki("sparse_12", swsp(12), 2);
    chki("sparse_14", swsp(14), 3);
    chki("sparse_d", swsp(11), 4);

    chki("defmid_1", swdefmid(1), 10);
    chki("defmid_2", swdefmid(2), 20);
    chki("defmid_3", swdefmid(3), 30);
    chki("defmid_d", swdefmid(9), 90);

    chki("deffall_4", swdef_fall(4), 41);
    chki("deffall_5", swdef_fall(5), 51);
    chki("deffall_d", swdef_fall(9), 51);

    chki("neg_m2", swneg(-2), 22);
    chki("neg_m1", swneg(-1), 11);
    chki("neg_0", swneg(0), 100);
    chki("neg_d", swneg(1), 99);

    chki("expr_char", swexpr('A'), 1);
    chki("expr_enum", swexpr(8), 2);
    chki("expr_bits", swexpr(17), 3);
    chki("expr_d", swexpr(18), 4);

    chki("nest_hit", swnest(1, 2), 121);
    chki("nest_def", swnest(1, 3), 191);
    chki("nest_outer", swnest(2, 2), 2);

    if (fails) {
        printf("tc89swjt failed: %d\n", fails);
        return 1;
    }
    printf("tc89swjt ok\n");
    return 0;
}
