/* tc89flt.c - syntax/storage-only float smoke test */
#include <stdio.h>

struct SFlt {
    char c;
    float f;
    int i;
};

typedef float f32;

static float gf;
static float ga[3];
static struct SFlt gs;

static int fails;

static void chki(const char *name, int got, int expect)
{
    if (got != expect) {
        printf("FAIL %s got %d expected %d\n", name, got, expect);
        fails++;
    }
}

extern int usef(float *p, f32 *q);

int main(void)
{
    float lf;
    float la[2];
    struct SFlt ls;


    chki("sizeof_float", sizeof(float), 4);
    chki("sizeof_typedef", sizeof(f32), 4);
    chki("sizeof_array", sizeof(la), 8);
    chki("sizeof_struct", sizeof(struct SFlt), 7);
    chki("sizeof_field", sizeof(ls.f), 4);

    if (fails) {
        printf("tc89flt failed: %d\n", fails);
        return 1;
    }
    printf("tc89flt ok\n");
    return 0;
}
