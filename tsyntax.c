/*
 * tsyntax.c -- DCC compiler regression tests
 *
 *   1. Parenthesized constant expressions in static initializers:
 *        static long x = (MAXDIST * 2);
 *
 *   2. Parenthesized constant expressions in array bounds:
 *        static unsigned char buf[(MAXDIST * 2)];
 *
 *   3. Recursive nested braces in static scalar array initializers:
 *        static const unsigned short fix[][2] = { {a,b}, ... };
 *
 *   4. Subscript expressions with additive constant expressions:
 *        rcore[LZ_RCORE_SRCV + 1]
 *
 *   5. Casted function-pointer calls:
 *        ((void (*)(void))fn)();
 *
 * Expected output:
 *   tsyntax: all tests passed
 *
 * Expected exit status:
 *   0
 */

#include <stdio.h>
#include <string.h>

#define MAXDIST 8192
#define WIN_MAX (MAXDIST * 2)
#define SMALL_A 5
#define SMALL_B 7
#define ARRAY_N ((SMALL_A + SMALL_B) * 2)

#define RCORE_SRCV 3
#define RCORE_DSTV 8
#define RCORE_FIX_N 6

typedef unsigned char uchar;
typedef unsigned short ushort;

static int failures = 0;

static long s_win_start = WIN_MAX;
static uchar s_win[(MAXDIST * 2)];
static int expr_array[ARRAY_N];

static const ushort decomprz80_fix[][2] = {
    { 0x19,  0x90 },
    { 0x32,  0x90 },
    { 0x3b,  0x90 },
    { 0x48,  0x90 },
    { 0x5a,   0x0c },
    { 0x66,  0x90 },
};

static int nested3[2][2][2] = {
    { { 1, 2 }, { 3, 4 } },
    { { 5, 6 }, { 7, 8 } },
};

static uchar rcore[16];
static int called_count = 0;

static void fail(const char *name, long got, long expected)
{
    printf("FAIL %s got %ld expected %ld\n", name, got, expected);
    failures++;
}

static void fail_s(const char *name, const char *got, const char *expected)
{
    printf("FAIL %s got '%s' expected '%s'\n", name, got, expected);
    failures++;
}

static void check_l(const char *name, long got, long expected)
{
    if (got != expected)
        fail(name, got, expected);
}

static void check_s(const char *name, const char *got, const char *expected)
{
    if (strcmp(got, expected) != 0)
        fail_s(name, got, expected);
}

static void target_func(void)
{
    called_count++;
}

static void test_constexpr_static_init_and_bounds(void)
{
    int i;
    long sum;

    check_l("static long constexpr init", s_win_start, 16384L);

    s_win[0] = 11;
    s_win[WIN_MAX - 1] = 22;
    check_l("constexpr array first", s_win[0], 11);
    check_l("constexpr array last", s_win[WIN_MAX - 1], 22);

    sum = 0;
    for (i = 0; i < ARRAY_N; i++) {
        expr_array[i] = i;
        sum += expr_array[i];
    }

    check_l("constexpr array bound", (long)ARRAY_N, 24L);
    check_l("constexpr array sum", sum, 276L);
}

static void test_nested_static_initializers(void)
{
    check_l("nested pair 0 0", decomprz80_fix[0][0], 0x19L);
    check_l("nested pair 0 1", decomprz80_fix[0][1], 0x90L);
    check_l("nested pair 4 0", decomprz80_fix[4][0], 0x5aL);
    check_l("nested pair 4 1", decomprz80_fix[4][1], 0x0cL);
    check_l("nested pair 5 1", decomprz80_fix[5][1], 0x90L);

    check_l("nested3 0 0 0", nested3[0][0][0], 1L);
    check_l("nested3 0 1 1", nested3[0][1][1], 4L);
    check_l("nested3 1 0 1", nested3[1][0][1], 6L);
    check_l("nested3 1 1 1", nested3[1][1][1], 8L);
}

static void test_additive_subscripts(void)
{
    int i;

    for (i = 0; i < 16; i++)
        rcore[i] = (uchar)(i + 1);

    rcore[RCORE_SRCV + 0] = 0xa0;
    rcore[RCORE_SRCV + 1] = 0xa1;
    rcore[RCORE_DSTV + 0] = 0xb0;
    rcore[RCORE_DSTV + 1] = 0xb1;

    check_l("subscript plus src0", rcore[RCORE_SRCV + 0], 0xa0L);
    check_l("subscript plus src1", rcore[RCORE_SRCV + 1], 0xa1L);
    check_l("subscript plus dst0", rcore[RCORE_DSTV + 0], 0xb0L);
    check_l("subscript plus dst1", rcore[RCORE_DSTV + 1], 0xb1L);

    for (i = 0; i < RCORE_FIX_N; i++) {
        int off;
        int val;
        off = decomprz80_fix[i][0] & 0x0f;
        val = decomprz80_fix[i][1] & 0xff;
        rcore[off + 1] = (uchar)val;
    }

    check_l("subscript plus loop 10", rcore[(0x19 & 0x0f) + 1], 0x90L);
    check_l("subscript plus loop 11", rcore[(0x5a & 0x0f) + 1], 0x0cL);
}

static void test_casted_function_pointer_call(void)
{
    void (*fp)(void);

    fp = target_func;

    ((void (*)(void))fp)();
    ((void (*)(void))target_func)();

    check_l("casted function pointer call", called_count, 2L);
}

int main(void)
{
    test_constexpr_static_init_and_bounds();
    test_nested_static_initializers();
    test_additive_subscripts();
    test_casted_function_pointer_call();

    if (failures) {
        printf("tsyntax: %d failure(s)\n", failures);
        return 1;
    }

    printf("tsyntax: all tests passed\n");
    return 0;
}
