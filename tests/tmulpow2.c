/* tmul_index_pow2.c - power-of-two multiply/index codegen regression */
#include <stdio.h>

struct Pair {
    long a;
    long b;
};

static unsigned umul_rhs(unsigned x)
{
    return x * 32U;
}

static unsigned umul_lhs(unsigned x)
{
    return 64U * x;
}

static int idx_int(int *p, int i)
{
    return p[i];
}

static long idx_long(long *p, int i)
{
    return p[i];
}

static long idx_pair(struct Pair *p, int i)
{
    return p[i].b;
}

int main(void)
{
    int ai[4];
    long al[4];
    struct Pair ap[3];
    int fails;

    fails = 0;

    ai[0] = 11;
    ai[1] = 22;
    ai[2] = 33;
    ai[3] = 44;

    al[0] = 1000L;
    al[1] = 2000L;
    al[2] = 3000L;
    al[3] = 4000L;

    ap[0].a = 1L; ap[0].b = 10L;
    ap[1].a = 2L; ap[1].b = 20L;
    ap[2].a = 3L; ap[2].b = 30L;

    if (umul_rhs(7U) != 224U) fails++;
    if (umul_lhs(3U) != 192U) fails++;
    if (idx_int(ai, 2) != 33) fails++;
    if (idx_long(al, 3) != 4000L) fails++;
    if (idx_pair(ap, 2) != 30L) fails++;

    if (fails) {
        printf("tmul_index_pow2 failed: %d\n", fails);
        return 1;
    }

    printf("tmul_index_pow2 ok\n");
    return 0;
}
