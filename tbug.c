/* validates a variety of switch behaviors */

#include <stdio.h>
#include <stdlib.h>

int g_ok;
int g_ng;

void chk(int ok, char *msg)
{
    if (ok)
        g_ok++;
    else {
        printf("FAIL: %s\n", msg);
        g_ng++;
    }
}

/* bug: case 1: case 2: body -- x==1 re-evaluated case 2 and failed */
int swft(int x)
{
    int r;
    r = 0;
    switch (x) {
        case 1:
        case 2:
            r = 42;
            break;
        case 3:
            r = 99;
            break;
    }
    return r;
}

/* bug: continue inside switch inside for jumped to switch cleanup, not loop */
int swfc()
{
    int i;
    int n;
    n = 0;
    for (i = 0; i < 5; i++) {
        switch (i & 1) {
            case 0:
                continue;
        }
        n++;
    }
    return n;
}

/* same continue bug, while variant */
int swwc()
{
    int i;
    int n;
    i = 0;
    n = 0;
    while (i < 6) {
        i++;
        switch (i % 3) {
            case 0:
                continue;
        }
        n++;
    }
    return n;
}

/* break in switch must not break the enclosing for loop */
int swbr()
{
    int i;
    int n;
    n = 0;
    for (i = 0; i < 4; i++) {
        switch (i) {
            case 2:
                n = n + 10;
                break;
            default:
                n = n + 1;
                break;
        }
    }
    return n;
}

/* three consecutive empty fall-throughs */
int swmf(int x)
{
    int r;
    r = 0;
    switch (x) {
        case 1:
        case 2:
        case 3:
            r = 7;
            break;
        case 4:
            r = 4;
            break;
    }
    return r;
}

/* switch with default case */
int swdf(int x)
{
    int r;
    r = 0;
    switch (x) {
        case 1:
            r = 10;
            break;
        case 2:
            r = 20;
            break;
        default:
            r = 99;
            break;
    }
    return r;
}

int main()
{
    g_ok = 0;
    g_ng = 0;

    /* fall-through: case 1 empty body must reach case 2 body */
    chk(swft(1) == 42, "ft1");
    chk(swft(2) == 42, "ft2");
    chk(swft(3) == 99, "ft3");
    chk(swft(4) == 0,  "ft4");

    /* continue in for loop inside switch -- only odd i (1,3) reach n++ */
    chk(swfc() == 2, "sfc");

    /* continue in while loop inside switch -- i%3==0 skips n++, rest (4 of 6) don't */
    chk(swwc() == 4, "swc");

    /* break exits switch, loop continues: 0+1+1+10+1 = 13 */
    chk(swbr() == 13, "sbr");

    /* three consecutive fall-throughs */
    chk(swmf(1) == 7, "mf1");
    chk(swmf(2) == 7, "mf2");
    chk(swmf(3) == 7, "mf3");
    chk(swmf(4) == 4, "mf4");
    chk(swmf(5) == 0, "mf5");

    /* default case */
    chk(swdf(1) == 10, "df1");
    chk(swdf(2) == 20, "df2");
    chk(swdf(5) == 99, "df3");

    printf("%d pass, %d fail\n", g_ok, g_ng);
    return 0;
}
