/* C89 goto regression test */
#include <stdio.h>

static int fails;

static void chki(const char *n, int g, int e)
{
    if (g != e) {
        printf("FAIL %s got %d expected %d\n", n, g, e);
        fails++;
    }
}

static int gt_basic(void)
{
    int i;
    int sum;

    i = 0;
    sum = 0;

loop:
    if (i >= 10)
        goto done;

    if (i == 5)
        goto skip;

    sum = sum + i;

skip:
    i = i + 1;
    goto loop;

done:
    return sum;
}

static int gt_forward(int x)
{
    int r;

    r = 0;
    if (x)
        goto yes;
    r = 1;
    goto done;

yes:
    r = 2;

done:
    return r;
}

static int gt_empty(void)
{
    goto lab;
    return 1;

lab:
    ;
    return 2;
}

static int gt_block_label(void)
{
    int r;

    r = 0;
    goto block;

block:
    {
        r = 7;
    }
    return r;
}

static int gt_out_block(int x)
{
    int r;

    r = 0;
    if (x)
        goto out;

    {
        r = 10;
        goto done;
    }

out:
    r = 20;

done:
    return r;
}

static int gt_loop_out(void)
{
    int i;
    int j;
    int c;

    c = 0;
    for (i = 0; i < 3; i = i + 1) {
        for (j = 0; j < 3; j = j + 1) {
            c = c + 1;
            if (i == 1 && j == 1)
                goto out;
        }
    }

out:
    return c;
}

static int gt_switch(int x)
{
    int r;

    r = 0;
    switch (x) {
    case 1:
        goto one;
    case 2:
        r = 2;
        break;
    default:
        goto done;
    }
    goto done;

one:
    r = 11;

done:
    return r;
}

static int gt_multi_label(void)
{
    int r;

    r = 0;
    goto b;

a:
b:
    r = r + 3;
    goto d;

c:
    r = r + 100;

d:
    return r;
}

int main(void)
{
    int i;

    printf("tgoto start\n");
    fails = 0;

    chki("basic", gt_basic(), 40);
    chki("forward0", gt_forward(0), 1);
    chki("forward1", gt_forward(1), 2);
    chki("empty", gt_empty(), 2);
    chki("block_label", gt_block_label(), 7);
    chki("out_block0", gt_out_block(0), 10);
    chki("out_block1", gt_out_block(1), 20);
    chki("loop_out", gt_loop_out(), 5);
    chki("switch1", gt_switch(1), 11);
    chki("switch2", gt_switch(2), 2);
    chki("switch3", gt_switch(3), 0);
    chki("multi_label", gt_multi_label(), 3);

    i = 0;

outer:
    if (i == 3)
        goto finished;

    i = i + 1;
    goto outer;

finished:
    chki("main_forward_backward", i, 3);

    if (fails) {
        printf("tgoto failed: %d\n", fails);
        return 1;
    }

    printf("tgoto passed with great success\n");
    return 0;
}
