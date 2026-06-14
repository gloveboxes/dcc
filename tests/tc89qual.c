#include <stdio.h>

static int fails;

static void chk(const char *name, int got, int exp)
{
    if (got != exp) {
        printf("FAIL %s got %d expected %d\n", name, got, exp);
        fails++;
    }
}

static int addq(const volatile int *p, const int *q)
{
    return *p + *q;
}

int main(void)
{
    const int ci = 7;
    volatile int vi = 5;
    const char *s = "abc";
    char buf[4];
    char * const cp = buf;
    volatile char *vp;
    const volatile int *pvi;

    fails = 0;
    cp[0] = 'x';
    cp[1] = 0;
    vp = cp;
    pvi = &vi;

    chk("const_int", ci, 7);
    chk("volatile_int", vi, 5);
    chk("const_ptr", s[1], 'b');
    chk("ptr_const", cp[0], 'x');
    chk("volatile_ptr", vp[0], 'x');
    chk("const_volatile_ptr", *pvi, 5);
    chk("qual_param", addq(&vi, &ci), 12);

    if (fails) {
        printf("tc89qual failed: %d\n", fails);
        return 1;
    }
    printf("tc89qual completed with great success\n");
    return 0;
}
