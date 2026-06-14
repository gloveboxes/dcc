/* tc89ptr_fixed.c - pointer arithmetic regression tests; 5-char unique names */

#include <stdio.h>
#include <stdint.h>

struct P2 {
    int a;
    char b;
};

static int fails;

static void cki00(const char *name, int got, int expect)
{
    if (got != expect) {
        printf("FAIL %s got %d expected %d\n", name, got, expect);
        fails++;
    }
}

static void ckl00(const char *name, long got, long expect)
{
    if (got != expect) {
        printf("FAIL %s got %ld expected %ld\n", name, got, expect);
        fails++;
    }
}

static void tpi00(void)
{
    int a[6];
    int *p;
    int *q;

    p = a;
    q = p + 3;
    cki00("int_ptr_plus", q - p, 3);
    cki00("int_ptr_index", &p[4] - p, 4);
    cki00("int_ptr_minus", (q - 1) - p, 2);
    cki00("int_ptr_commute", (2 + p) - p, 2);
}

static void tpl00(void)
{
    long a[6];
    long *p;
    long *q;

    p = a;
    q = p + 3;
    cki00("long_ptr_plus", q - p, 3);
    cki00("long_ptr_index", &p[4] - p, 4);
    cki00("long_ptr_minus", (q - 2) - p, 1);
    cki00("long_ptr_commute", (2 + p) - p, 2);
}

static void tps00(void)
{
    struct P2 a[6];
    struct P2 *p;
    struct P2 *q;

    p = a;
    q = p + 3;
    cki00("struct_ptr_plus", q - p, 3);
    cki00("struct_ptr_index", &p[4] - p, 4);
    cki00("struct_ptr_minus", (q - 1) - p, 2);
}

static void tpv00(void)
{
    int ia[4];
    long la[4];
    struct P2 sa[3];
    int *ip;
    long *lp;
    struct P2 *sp;

    ia[0] = 10; ia[1] = 20; ia[2] = 30; ia[3] = 40;
    la[0] = 1000L; la[1] = 2000L; la[2] = 3000L; la[3] = 4000L;
    sa[0].a = 7; sa[1].a = 8; sa[2].a = 9;
    sa[0].b = 1; sa[1].b = 2; sa[2].b = 3;

    ip = ia;
    lp = la;
    sp = sa;

    cki00("int_value", *(ip + 2), 30);
    ckl00("long_value", *(lp + 2), 3000L);
    cki00("struct_value_a", (sp + 1)->a, 8);
    cki00("struct_value_b", (sp + 2)->b, 3);
}

int main(void)
{
    fails = 0;
    printf("tc89ptr start\n");

    tpi00();
    tpl00();
    tps00();
    tpv00();

    if (fails) {
        printf("tc89ptr failed: %d\n", fails);
        return 1;
    }

    printf("tc89ptr completed with great success\n");
    return 0;
}
