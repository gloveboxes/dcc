#include <stdio.h>

typedef unsigned lzpos_t;
static int failures;

static void ck(const char *name, int got, int exp)
{
    if (got != exp) {
        printf("FAIL %s got %d expected %d\n", name, got, exp);
        failures++;
    }
}

static int gt_post(lzpos_t a, lzpos_t b)
{
    lzpos_t p;
    p = a;
    return p-- > b;
}

static int ge_post(lzpos_t a, lzpos_t b)
{
    lzpos_t p;
    p = a;
    return p-- >= b;
}

int main(void)
{
    ck("post unsigned gt high", gt_post((lzpos_t)0x8001U, (lzpos_t)0x8000U), 1);
    ck("post unsigned gt low", gt_post((lzpos_t)0x8000U, (lzpos_t)1U), 1);
    ck("post unsigned ge eq", ge_post((lzpos_t)0x8000U, (lzpos_t)0x8000U), 1);
    ck("post unsigned gt false", gt_post((lzpos_t)1U, (lzpos_t)0x8000U), 0);
    if (failures) {
        printf("tbug2: %d failure(s)\n", failures);
        return 1;
    }
    printf("tbug2 all tests passed\n");
    return 0;
}
