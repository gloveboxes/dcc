/* tfloat4.c - DCC-oriented C89 4-byte float/runtime regression test.
 *
 * Assumptions:
 * - dcc has a 2-byte int.
 * - dcc has a 4-byte float.
 * - dcc does not support double, so this test avoids double entirely.
 *
 * Expected output:
 * tfloat4 start
 * PASS
 */

#include <stdio.h>
#include <math.h>

struct FPair {
    float a;
    float b;
};

struct FNode {
    int tag;
    float v[4];
    long lv;
};

static int fails;
static float gfa[8];
static struct FPair gpair;
static struct FNode gnode;

static void check_int(name, got, exp)
char *name;
int got;
int exp;
{
    if (got != exp) {
        printf("FAIL %s got %d expected %d\n", name, got, exp);
        fails++;
    }
}

static void check_uint(name, got, exp)
char *name;
unsigned got;
unsigned exp;
{
    if (got != exp) {
        printf("FAIL %s got %u expected %u\n", name, got, exp);
        fails++;
    }
}

static void check_long(name, got, exp)
char *name;
long got;
long exp;
{
    if (got != exp) {
        printf("FAIL %s got %ld expected %ld\n", name, got, exp);
        fails++;
    }
}

static void check_ulong(name, got, exp)
char *name;
unsigned long got;
unsigned long exp;
{
    if (got != exp) {
        printf("FAIL %s got %lu expected %lu\n", name, got, exp);
        fails++;
    }
}

static float absf(x)
float x;
{
    if (x < 0.0)
        return -x;
    return x;
}

static void check_float(name, got, exp, tol)
char *name;
float got;
float exp;
float tol;
{
    float diff;

    diff = absf(got - exp);
    if (diff > tol) {
        printf("FAIL %s got %.6f expected %.6f diff %.6f\n",
               name, got, exp, diff);
        fails++;
    }
}

static float identf(x)
float x;
{
    return x;
}

static float add3(a, b, c)
float a;
float b;
float c;
{
    return a + b + c;
}

static float muladd(a, b, c)
float a;
float b;
float c;
{
    return a * b + c;
}

static int add_if(a, b)
int a;
float b;
{
    return (int)((float)a + b);
}

static unsigned add_uf(a, b)
unsigned a;
float b;
{
    return (unsigned)((float)a + b);
}

static long add_lf(a, b)
long a;
float b;
{
    return (long)((float)a + b);
}

static unsigned long add_ul_f(a, b)
unsigned long a;
float b;
{
    return (unsigned long)((float)a + b);
}

static void test_constants()
{
    check_float("const1", (float)1.25, (float)1.25, (float)0.0001);
    check_float("const2", (float)-123.75, (float)-123.75, (float)0.0001);
    check_float("const3", (float)1000000.0, (float)1000000.0, (float)1.0);
    check_float("const4", (float)0.000001, (float)0.000001, (float)0.000001);
    check_float("const5", (float)-0.000001, (float)-0.000001, (float)0.000001);
}

static void test_basic()
{
    float a;
    float b;
    float c;

    a = (float)1.5;
    b = (float)2.25;

    check_float("add", a + b, (float)3.75, (float)0.0001);
    check_float("sub", b - a, (float)0.75, (float)0.0001);
    check_float("mul", a * b, (float)3.375, (float)0.0001);
    check_float("div", b / a, (float)1.5, (float)0.0001);

    c = ((a + b) * (float)4.0 - (float)3.0) / (float)2.0;
    check_float("expr1", c, (float)6.0, (float)0.0001);

    c = -a * b + (float)10.0;
    check_float("expr2", c, (float)6.625, (float)0.0001);

    check_float("unary_minus", -((float)-3.5), (float)3.5, (float)0.0001);
    check_float("return_float", identf((float)7.25), (float)7.25, (float)0.0001);
    check_float("param3", add3((float)1.0, (float)2.0, (float)3.0),
                (float)6.0, (float)0.0001);
    check_float("muladd", muladd((float)3.0, (float)4.0, (float)5.0),
                (float)17.0, (float)0.0001);
}

static void test_conversions()
{
    char sc;
    unsigned char uc;
    int si;
    unsigned ui;
    long sl;
    unsigned long ul;
    float f;

    sc = -12;
    uc = (unsigned char)250;
    si = -12345;
    ui = 60000U;
    sl = -1234567L;
    ul = 3456789UL;

    check_float("char_to_float", (float)sc, (float)-12.0, (float)0.0001);
    check_float("uchar_to_float", (float)uc, (float)250.0, (float)0.0001);
    check_float("int_to_float", (float)si, (float)-12345.0, (float)0.5);
    check_float("uint_to_float", (float)ui, (float)60000.0, (float)1.0);
    check_float("long_to_float", (float)sl, (float)-1234567.0, (float)4.0);
    check_float("ulong_to_float", (float)ul, (float)3456789.0, (float)8.0);

    f = (float)123.75;
    check_int("float_to_int_pos", (int)f, 123);
    check_uint("float_to_uint_pos", (unsigned)f, 123U);
    check_long("float_to_long_pos", (long)f, 123L);
    check_ulong("float_to_ulong_pos", (unsigned long)f, 123UL);

    f = (float)-123.75;
    check_int("float_to_int_neg", (int)f, -123);
    check_long("float_to_long_neg", (long)f, -123L);

    f = (float)32760.75;
    check_int("float_to_int_16hi", (int)f, 32760);
    check_uint("float_to_uint_16hi", (unsigned)f, 32760U);

    f = (float)65530.75;
    check_uint("float_to_uint_16uhi", (unsigned)f, 65530U);

    f = (float)123456.75;
    check_long("float_to_long_large", (long)f, 123456L);
    check_ulong("float_to_ulong_large", (unsigned long)f, 123456UL);

    check_int("mixed_int_float", add_if(-100, (float)25.75), -74);
    check_uint("mixed_uint_float", add_uf(60000U, (float)12.5), 60012U);
    check_long("mixed_long_float", add_lf(-100000L, (float)12.75), -99987L);
    check_ulong("mixed_ulong_float", add_ul_f(100000UL, (float)99.75), 100099UL);
}

static void test_arrays()
{
    float la[8];
    int i;
    float sum;

    for (i = 0; i < 8; i++) {
        gfa[i] = (float)i + (float)0.5;
        la[i] = (float)(i * 2) + (float)0.25;
    }

    check_float("garray0", gfa[0], (float)0.5, (float)0.0001);
    check_float("garray7", gfa[7], (float)7.5, (float)0.0001);
    check_float("larray0", la[0], (float)0.25, (float)0.0001);
    check_float("larray7", la[7], (float)14.25, (float)0.0001);

    gfa[2] = gfa[0] + gfa[1] + gfa[7];
    check_float("garray_expr", gfa[2], (float)9.5, (float)0.0001);

    *(la + 3) = *(la + 1) * (float)2.0;
    check_float("larray_ptr", la[3], (float)4.5, (float)0.0001);

    sum = (float)0.0;
    for (i = 0; i < 8; i++)
        sum += gfa[i];
    check_float("garray_sum", sum, (float)39.0, (float)0.0001);
}

static void test_structs()
{
    struct FPair lp;
    struct FNode ln;
    struct FNode *p;

    gpair.a = (float)1.25;
    gpair.b = (float)2.75;
    check_float("gstruct_pair", gpair.a + gpair.b, (float)4.0, (float)0.0001);

    lp.a = (float)-5.5;
    lp.b = (float)2.0;
    check_float("lstruct_pair", lp.a / lp.b, (float)-2.75, (float)0.0001);

    gnode.tag = 11;
    gnode.lv = 100000L;
    gnode.v[0] = (float)1.0;
    gnode.v[1] = (float)2.0;
    gnode.v[2] = (float)3.0;
    gnode.v[3] = (float)4.0;
    check_float("gstruct_array", gnode.v[0] + gnode.v[3], (float)5.0, (float)0.0001);

    ln.tag = 22;
    ln.lv = -200000L;
    ln.v[0] = (float)10.0;
    ln.v[1] = (float)20.0;
    ln.v[2] = (float)30.0;
    ln.v[3] = (float)40.0;
    p = &ln;
    check_float("lstruct_ptr", p->v[2] - p->v[1], (float)10.0, (float)0.0001);
    check_float("lstruct_longmix", (float)p->lv + p->v[0],
                (float)-199990.0, (float)1.0);
}

static void test_long_float_mix()
{
    long l;
    unsigned long ul;
    float f;

    l = 100000L;
    ul = 200000UL;

    check_float("longmix1", (float)l + (float)0.5,
                (float)100000.5, (float)1.0);
    check_float("ulongmix1", (float)ul + (float)0.25,
                (float)200000.25, (float)1.0);

    f = (float)12345.75;
    check_long("longmix2", (long)f, 12345L);

    f = (float)-12345.75;
    check_long("longmix3", (long)f, -12345L);

    l = (long)((float)40000.0 + (float)123.75);
    check_long("longmix4", l, 40123L);
}

static void test_math()
{
    float pi;
    float halfpi;
    float qtrpi;
    float e1;

    pi = (float)3.1415926535;
    halfpi = pi / (float)2.0;
    qtrpi = pi / (float)4.0;
    e1 = (float)2.7182818284;

    check_float("sin0", sin((float)0.0), (float)0.0, (float)0.0002);
    check_float("sin_halfpi", sin(halfpi), (float)1.0, (float)0.0020);
    check_float("sin_neg", sin(-halfpi), (float)-1.0, (float)0.0020);

    check_float("cos0", cos((float)0.0), (float)1.0, (float)0.0002);
    check_float("cos_pi", cos(pi), (float)-1.0, (float)0.0020);
    check_float("cos_halfpi", cos(halfpi), (float)0.0, (float)0.0020);

    check_float("tan0", tan((float)0.0), (float)0.0, (float)0.0002);
    check_float("tan_qtrpi", tan(qtrpi), (float)1.0, (float)0.0030);

    check_float("asin0", asin((float)0.0), (float)0.0, (float)0.0002);
    check_float("asin1", asin((float)1.0), halfpi, (float)0.0030);
    check_float("acos1", acos((float)1.0), (float)0.0, (float)0.0002);
    check_float("acos0", acos((float)0.0), halfpi, (float)0.0030);
    check_float("atan1", atan((float)1.0), qtrpi, (float)0.0030);
    check_float("atan2_11", atan2((float)1.0, (float)1.0), qtrpi, (float)0.0030);

    check_float("sqrt9", sqrt((float)9.0), (float)3.0, (float)0.0002);
    check_float("sqrt2", sqrt((float)2.0), (float)1.4142135, (float)0.0020);

    check_float("exp1", exp((float)1.0), e1, (float)0.0060);
    check_float("log_e", log(e1), (float)1.0, (float)0.0060);
    check_float("log10_100", log10((float)100.0), (float)2.0, (float)0.0020);
    check_float("pow_2_8", pow((float)2.0, (float)8.0), (float)256.0, (float)0.05);
    check_float("pow_9_half", pow((float)9.0, (float)0.5), (float)3.0, (float)0.0030);

    check_float("fabs", fabs((float)-12.25), (float)12.25, (float)0.0002);
    check_float("floor_pos", floor((float)12.75), (float)12.0, (float)0.0002);
    check_float("floor_neg", floor((float)-12.25), (float)-13.0, (float)0.0002);
    check_float("ceil_pos", ceil((float)12.25), (float)13.0, (float)0.0002);
    check_float("ceil_neg", ceil((float)-12.75), (float)-12.0, (float)0.0002);
    check_float("fmod", fmod((float)17.5, (float)3.0), (float)2.5, (float)0.0002);
}

static void test_comparisons()
{
    float a;
    float b;
    float c;
    int count;

    a = (float)1.25;
    b = (float)2.5;
    c = (float)1.25;
    count = 0;

    if (a < b) count++;
    if (b > a) count++;
    if (a <= c) count++;
    if (a >= c) count++;
    if (a == c) count++;
    if (a != b) count++;
    if ((a + b == (float)3.75) && (b - a == (float)1.25)) count++;
    if ((a > b) || (c == a)) count++;

    check_int("float_cmp_count", count, 8);
}

int main()
{
    printf("tfloat4 start\n");

    test_constants();
    test_basic();
    test_conversions();
    test_arrays();
    test_structs();
    test_long_float_mix();
    test_math();
    test_comparisons();

    if (fails) {
        printf("FAILED %d\n", fails);
        return 1;
    }

    printf("PASS\n");
    return 0;
}
