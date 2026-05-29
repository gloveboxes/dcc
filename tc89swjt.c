/* tc89swjt.c - switch jump table regression */
#include <stdio.h>

static int fails;

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

    if (fails) {
        printf("tc89swjt failed: %d\n", fails);
        return 1;
    }
    printf("tc89swjt ok\n");
    return 0;
}
