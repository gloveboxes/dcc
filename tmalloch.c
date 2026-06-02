#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    unsigned char *p;
    unsigned char *q;
    unsigned int i;
    unsigned long sum;

    p = (unsigned char *)malloc(32768U);
    if (!p) {
        printf("tmallochi2: malloc32768 failed\n");
        return 1;
    }

    p[0] = 0x12;
    p[32767U] = 0x34;
    sum = (unsigned long)p[0] + (unsigned long)p[32767U];
    if (sum != 0x46UL) {
        printf("FAIL large block sum got %lu expected %lu\n", sum, 0x46UL);
        return 1;
    }

    free(p);

    q = (unsigned char *)malloc(32U);
    if (!q) {
        printf("FAIL malloc after large free returned null\n");
        return 1;
    }

    for (i = 0; i < 32U; i++)
        q[i] = (unsigned char)i;

    if (q[0] != 0U || q[31] != 31U) {
        printf("FAIL small block after large free\n");
        return 1;
    }

    free(q);
    printf("tmallochi2: all tests passed\n");
    return 0;
}
