#include <stdio.h>

int ga[4];
int gi;

int main(void)
{
    int la[4];
    int li;

    gi = 1;
    ga[gi++] = 10;

    li = 1;
    la[li++] = 20;

    printf("%d %d %d %d\n", gi, ga[1], li, la[1]);
    return 0;
}
