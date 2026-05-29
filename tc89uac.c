/* tc89uac_check.c */

#include <stdio.h>
#include <stdint.h>

static int fails;

static void chk_i(const char *name, int got, int expect)
{
    if (got != expect) {
        printf("FAIL %s got %d expected %d\n", name, got, expect);
        fails++;
    }
}

static void chk_u(const char *name, unsigned int got, unsigned int expect)
{
    if (got != expect) {
        printf("FAIL %s got %u expected %u\n", name, got, expect);
        fails++;
    }
}

static void chk_l(const char *name, long got, long expect)
{
    if (got != expect) {
        printf("FAIL %s got %ld expected %ld\n", name, got, expect);
        fails++;
    }
}

int main(void)
{
    int si;
    unsigned int ui;
    long sl;
    char ch;

    fails = 0;

    si = -1;
    ui = 1;
    chk_u("int_plus_uint", (unsigned int)(si + ui), 0U);

    si = 5;
    sl = 100000L;
    chk_l("int_plus_long", si + sl, 100005L);

    ui = 65535U;
    sl = 1L;
    chk_l("uint_plus_long", ui + sl, 65536L);

    ui = 65535U;
    si = -1;
    chk_i("uint_gt_int", ui > si, 0);
    chk_i("int_lt_uint", si < ui, 0);

    ch = -1;
    si = 1;
    chk_i("char_plus_int", ch + si, 0);

    if (fails) {
        printf("tc89uac failed: %d\n", fails);
        return 1;
    }

    printf("tc89uac completed with great success\n");
    return 0;
}