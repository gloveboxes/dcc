/* tc89flng.c - long/float conversion/codegen tests */
#include <stdio.h>

static int fails;

static void chk(const char *n, int ok)
{
    if (!ok) {
        printf("FAIL %s\n", n);
        fails++;
    }
}

static float idf(float f)
{
    return f;
}

int main(void)
{
    long sl;
    unsigned long ul;
    int si;
    unsigned int ui;
    float f;
    float g;

    fails = 0;

    sl = -12345L;
    ul = 123456UL;
    f = (float)sl;
    chk("sl_to_f", (long)f == -12345L);
    f = (float)ul;
    chk("ul_to_f", (unsigned long)f == 123456UL);

    f = -321.0f;
    sl = (long)f;
    chk("f_to_sl", sl == -321L);
    f = 65000.0f;
    ul = (unsigned long)f;
    chk("f_to_ul", ul == 65000UL);

    sl = 100000L;
    f = sl;
    chk("imp_sl_f", (long)f == 100000L);
    ul = 40000UL;
    f = ul;
    chk("imp_ul_f", (unsigned long)f == 40000UL);

    si = 3;
    sl = 4L;
    g = 2.0f;
    f = g + sl;
    chk("f_add_l", (long)f == 6L);
    f = sl + g;
    chk("l_add_f", (long)f == 6L);
    f = sl * g;
    chk("l_mul_f", (long)f == 8L);
    f = sl / g;
    chk("l_div_f", (long)f == 2L);
    f += sl;
    chk("f_addeq_l", (long)f == 6L);

    ui = 7U;
    f = idf(ui);
    chk("arg_ui_f", (unsigned int)f == 7U);
    f = idf(sl);
    chk("arg_l_f", (long)f == 4L);

    chk("cmp_l_f", sl == g + 2.0f);

    if (fails) {
        printf("tc89flng failed: %d\n", fails);
        return 1;
    }

    printf("tc89flng ok\n");
    return 0;
}
