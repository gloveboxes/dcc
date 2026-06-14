/* tc89flta_ptr.c */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int fails;

static void chk(const char *name, unsigned char *p, unsigned char b0, unsigned char b1, unsigned char b2, unsigned char b3)
{
    if (p[0] != b0 || p[1] != b1 || p[2] != b2 || p[3] != b3) {
        printf("FAIL %s got %u %u %u %u\n", name, p[0], p[1], p[2], p[3]);
        fails++;
    }
}

float fid(float x)
{
    return x;
}

int main(void)
{
    float fa[4];
    float *fp;
    unsigned char *bp;

    fails = 0;

    fa[0] = 1.0f;
    fa[1] = 2.5f;
    fa[2] = -3.0f;
    fa[3] = fid(fa[1]);

    fp = fa;
    fp++;
    fp[1] = fid(fa[2]);

    bp = (unsigned char *)fa;

    chk("fa0", bp + 0, 0, 0, 128, 63);
    chk("fa1", bp + 4, 0, 0, 32, 64);
    chk("fa2", bp + 8, 0, 0, 64, 192);
    chk("fa3", bp + 12, 0, 0, 32, 64);

    if (fails) {
        printf("tc89flta_ptr failed: %d\n", fails);
        return 1;
    }

    printf("tc89fptr completed with great success\n");
    return 0;
}