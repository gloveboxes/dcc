/*
 * tctxops.c - regression for binary-operator type prediction across contexts.
 *
 * peek_simple_unary_type() only inspects the first operand token of the RHS,
 * so a *parenthesized* or otherwise compound RHS such as (fa * fb) or (la + lb)
 * used to be mis-predicted as 16-bit int.  That truncated long results (high
 * word dropped) and corrupted float results in arithmetic, comparison, and the
 * direct if/while branch paths.  These tests pin the fixed behaviour:
 *   - peek recurses into a non-cast parenthesized expression, and
 *   - the direct-branch condition scanner treats a float operand at ANY paren
 *     depth as needing the general (float) comparison path.
 */
#include <stdio.h>
#include <stdint.h>

static int fails;
static void chk(long got, long want, char *name)
{
    if (got != want) { printf("FAIL %s got=%ld want=%ld\n", name, got, want); fails++; }
}
static void chku(unsigned long got, unsigned long want, char *name)
{
    if (got != want) { printf("FAIL %s got=%lu want=%lu\n", name, got, want); fails++; }
}
static void chkf(float got, float want, char *name)
{
    float d = got - want;
    if (d < 0) d = -d;
    if (d > 0.001) { printf("FAIL %s got=%f want=%f\n", name, got, want); fails++; }
}

/* ---- compound long RHS (first token 16-bit) across contexts ------------- */
static long ca_pluseq(long s, int y, long la) { s += (y + la); return s; }
static long ca_muleq(long s, int y, long la)  { s *= (y + la); return s; }
static long ca_diveq(long s, int y, long la)  { s /= (y + la); return s; }
static long ca_modeq(long s, int y, long la)  { s %= (y + la); return s; }
static long ca_andeq(long s, int y, long la)  { s &= (y + la); return s; }
static long ca_tern(int c, int y, long la, long lb) { return c ? (y + la) : (y + lb); }
static long ca_sink(long v) { return v + 1L; }
static long ca_callarg(int y, long la) { return ca_sink(y + la); }
static long ca_ret(int y, long la) { return y + la; }
static long ca_arr[8];
static long ca_index(int y, long lb) { return ca_arr[(int)(y + lb)]; }
static long ca_switch(int y, long la)
{
    switch (y + la) { case 100000L: return 1; case 100001L: return 2; default: return 9; }
}
static long ca_init(int y, long la) { long x = y + la; return x; }
static unsigned long ca_upluseq(unsigned long s, unsigned int y, unsigned long la)
{ s += (y + la); return s; }

/* ---- compound float RHS (peek predicted 16-bit) ------------------------- */
static float fa, fb;
static float cf_add(int x)  { return x + (fa * fb); }
static float cf_sub(int x)  { return x - (fa * fb); }
static float cf_mul(int x)  { return x * (fa + fb); }
static float cf_addp1(int x){ return x + (fa); }
static float cf_addl(long lx) { return lx + (fa * fb); }
/* float compare inside an if-condition (direct-branch path) */
static int cf_lt(int x) { if (x < (fa * fb)) return 1; return 0; }
static int cf_gt(int x) { if (x > (fa * fb)) return 1; return 0; }
static int cf_eq(int x) { if (x == (fa + fb)) return 1; return 0; }
static int cf_le(int x) { if (x <= (fa * fb)) return 1; return 0; }

/* ---- shift / unsigned div with compound long ---------------------------- */
static long sh_shl(int y, long la)  { return (y + la) << 2; }
static long sh_shr(int y, long la)  { return (y + la) >> 2; }
static unsigned long sh_ushr(unsigned int y, unsigned long la) { return (y + la) >> 2; }
static int sh_cmp(int y, long la) { if (((y + la) >> 1) > 50000L) return 1; return 0; }
static unsigned long sh_udiv(unsigned int y, unsigned long la) { return (y + la) / 7UL; }
static unsigned long sh_umod(unsigned int y, unsigned long la) { return (y + la) % 7UL; }

int main(void)
{
    int i;
    for (i = 0; i < 8; i++) ca_arr[i] = (long)i * 100000L;

    chk(ca_pluseq(1000000L, 5, 100000L), 1100005L, "pluseq");
    chk(ca_muleq(3L, 5, 100000L), 300015L, "muleq");
    chk(ca_diveq(1000000L, 0, 1000L), 1000L, "diveq");
    chk(ca_modeq(1000003L, 0, 1000L), 3L, "modeq");
    chk(ca_andeq(0x1FFFFL, 0, 0x10001L), 0x10001L, "andeq");
    chk(ca_tern(1, 5, 100000L, 200000L), 100005L, "tern true");
    chk(ca_tern(0, 5, 100000L, 200000L), 200005L, "tern false");
    chk(ca_callarg(5, 100000L), 100006L, "callarg");
    chk(ca_ret(5, 100000L), 100005L, "ret");
    chk(ca_index(1, 2L), 300000L, "index");
    chk(ca_switch(0, 100000L), 1, "switch0");
    chk(ca_switch(1, 100000L), 2, "switch1");
    chk(ca_switch(5, 100000L), 9, "switch default");
    chk(ca_init(5, 100000L), 100005L, "init");
    chku(ca_upluseq(1000000UL, 5U, 100000UL), 1100005UL, "upluseq");

    fa = 3.0f; fb = 4.0f;   /* fa*fb=12, fa+fb=7 */
    chkf(cf_add(5), 17.0f, "f add");
    chkf(cf_sub(5), -7.0f, "f sub");
    chkf(cf_mul(5), 35.0f, "f mul");
    chkf(cf_addp1(5), 8.0f, "f add paren1");
    chkf(cf_addl(100000L), 100012.0f, "f add long");
    chk(cf_lt(5), 1, "f lt");
    chk(cf_lt(20), 0, "f lt no");
    chk(cf_gt(20), 1, "f gt");
    chk(cf_eq(7), 1, "f eq");
    chk(cf_eq(8), 0, "f eq no");
    chk(cf_le(12), 1, "f le");

    chk(sh_shl(0, 100000L), 400000L, "shl");
    chk(sh_shr(0, 100000L), 25000L, "shr");
    chku(sh_ushr(0U, 4000000000UL), 1000000000UL, "ushr");
    chk(sh_cmp(0, 200000L), 1, "shcmp");
    chk(sh_cmp(0, 80000L), 0, "shcmp no");
    chku(sh_udiv(1U, 99UL), 14UL, "udiv");
    chku(sh_umod(1U, 99UL), 2UL, "umod");

    if (fails == 0) printf("tctxops passed with great success\n");
    else printf("tctxops FAILED: %d\n", fails);
    return fails != 0;
}
