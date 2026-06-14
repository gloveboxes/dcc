#include <stdio.h>
#include <stdlib.h>

#define MAXR 16

struct REnt { int kind; int a; int b; };
struct State { struct REnt *s_rs; int s_rp; };
static struct State *G;

#define rs G->s_rs
#define rp G->s_rp

static void fail(const char *s, int a, int b)
{
    printf("FAIL %s got %d expected %d\n", s, a, b);
    exit(1);
}

static void direct_push(int kind, int a, int b)
{
    rs[rp].kind = kind;
    rs[rp].a = a;
    rs[rp].b = b;
    rp++;
}

static int direct_return_to_call(int base_rp, int *pcp)
{
    int k;

    while (rp > base_rp) {
        rp--;
        k = rs[rp].kind;
        if (k == 1) {
            *pcp = rs[rp].a;
            return 1;
        }
    }
    return 0;
}

static void helper_push(int kind, int a, int b)
{
    struct REnt *re;
    re = rs + rp;
    re->kind = kind;
    re->a = a;
    re->b = b;
    rp++;
}

static int helper_return_to_call(int base_rp, int *pcp)
{
    struct REnt *re;
    int k;

    while (rp > base_rp) {
        rp--;
        re = rs + rp;
        k = re->kind;
        if (k == 1) {
            *pcp = re->a;
            return 1;
        }
    }
    return 0;
}

static void run_direct(void)
{
    int pc;
    int ok;

    rp = 0;
    direct_push(2, 10, 20);
    direct_push(1, 1234, 0);
    pc = 0;
    ok = direct_return_to_call(0, &pc);
    if (!ok) fail("direct ok", ok, 1);
    if (pc != 1234) fail("direct pc", pc, 1234);
    if (rp != 1) fail("direct rp after call", rp, 1);
    if (rs[0].kind != 2) fail("direct loop kind", rs[0].kind, 2);
    if (rs[0].a != 10) fail("direct loop a", rs[0].a, 10);
}

static void run_helper(void)
{
    int pc;
    int ok;

    rp = 0;
    helper_push(2, 10, 20);
    helper_push(1, 1234, 0);
    pc = 0;
    ok = helper_return_to_call(0, &pc);
    if (!ok) fail("helper ok", ok, 1);
    if (pc != 1234) fail("helper pc", pc, 1234);
    if (rp != 1) fail("helper rp after call", rp, 1);
    if (rs[0].kind != 2) fail("helper loop kind", rs[0].kind, 2);
    if (rs[0].a != 10) fail("helper loop a", rs[0].a, 10);
}

int main(void)
{
    int i;
    G = (struct State *)calloc(1, sizeof(struct State));
    if (!G) return 1;
    rs = (struct REnt *)calloc(MAXR, sizeof(struct REnt));
    if (!rs) return 1;

    for (i = 0; i < 50; i++) {
        run_direct();
        run_helper();
    }
    printf("PASS tstretst\n");
    return 0;
}
