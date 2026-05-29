#include <stdio.h>

static int add1(int x)
{
    return x + 1;
}

static int callit(int (*fp)(int), int x)
{
    return fp(x);
}

int main(void)
{
    int (*fp)(int);
    char msg[] = "core";

    fp = add1;
    printf("%s %d %d\n", msg, fp(41), callit(fp, 9));
    return 0;
}
