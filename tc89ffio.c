/* tc89ffio.c - float printf smoke test; build dcc with -ffloatio */
#include <stdio.h>

int main(void)
{
    float a;
    float b;
    float c;

    a = 1.5f;
    b = 2.25f;
    c = a + b;

    printf("ffio start\n");
    printf("a=%f\n", a);
    printf("b=%.2f\n", b);
    printf("c=%f\n", c);
    printf("ffio ok\n");
    return 0;
}
