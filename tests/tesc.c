/*
 * tesc.c -- DCC regression tests
 *
 * 1. Chained assignment: a = b = c = d = 0 for global struct members.
 *    The dccpeep pass_store_word_const_hl peephole must not destroy DE
 *    when subsequent chained stores rely on it.
 *
 * 2. C89 escape sequences in string and character literals:
 *    \n \r \t \b \f \v \a \\ \' \" \? \0 \033 \x1b
 *
 * Expected output:
 *   tesc: all tests passed
 *
 * Expected exit status:
 *   0
 */

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void fail(name, got, expected)
const char *name;
int got;
int expected;
{
    printf("FAIL %s got %d expected %d\n", name, got, expected);
    failures++;
}

static void fail_s(name, got, expected)
const char *name;
const char *got;
const char *expected;
{
    printf("FAIL %s got '%s' expected '%s'\n", name, got, expected);
    failures++;
}

static void check(name, got, expected)
const char *name;
int got;
int expected;
{
    if (got != expected)
        fail(name, got, expected);
}

static void check_s(name, got, expected)
const char *name;
const char *got;
const char *expected;
{
    if (strcmp(got, expected) != 0)
        fail_s(name, got, expected);
}

/* ------------------------------------------------------------------ */
/* Test 1: chained assignment                                           */
/* ------------------------------------------------------------------ */

struct state {
    int a;
    int b;
    int c;
    int d;
    int e;
};

static struct state S;

static void reset_state()
{
    S.a = 99;
    S.b = 99;
    S.c = 99;
    S.d = 99;
    S.e = 99;
}

static void test_chained_assign()
{
    int x;
    int y;
    int z;

    /* Chained assignment to global struct members. */
    reset_state();
    S.a = S.b = S.c = S.d = 0;
    check("chain4 S.a", S.a, 0);
    check("chain4 S.b", S.b, 0);
    check("chain4 S.c", S.c, 0);
    check("chain4 S.d", S.d, 0);
    check("chain4 S.e", S.e, 99); /* untouched */

    /* Three-way chain. */
    reset_state();
    S.a = S.b = S.c = 7;
    check("chain3 S.a", S.a, 7);
    check("chain3 S.b", S.b, 7);
    check("chain3 S.c", S.c, 7);
    check("chain3 S.d", S.d, 99); /* untouched */

    /* Two-way chain. */
    reset_state();
    S.a = S.b = 3;
    check("chain2 S.a", S.a, 3);
    check("chain2 S.b", S.b, 3);
    check("chain2 S.c", S.c, 99); /* untouched */

    /* Chained local int assignment. */
    x = y = z = 0;
    check("local chain x", x, 0);
    check("local chain y", y, 0);
    check("local chain z", z, 0);

    x = y = z = 42;
    check("local chain42 x", x, 42);
    check("local chain42 y", y, 42);
    check("local chain42 z", z, 42);
}

/* ------------------------------------------------------------------ */
/* Test 2: C89 escape sequences                                         */
/* ------------------------------------------------------------------ */

static void test_escapes()
{
    /* Named escapes as character literals. */
    check("esc \\n",  '\n', 10);
    check("esc \\r",  '\r', 13);
    check("esc \\t",  '\t',  9);
    check("esc \\b",  '\b',  8);
    check("esc \\f",  '\f', 12);
    check("esc \\v",  '\v', 11);
    check("esc \\a",  '\a',  7);
    check("esc \\\\", '\\', 92);
    check("esc \\'",  '\'', 39);
    check("esc \\\"", '\"', 34);
    check("esc \\?",  '\?', 63);

    /* Octal: \0 (nul), \033 (ESC), \177 (DEL). */
    check("esc \\0",   '\0',   0);
    check("esc \\033", '\033', 27);
    check("esc \\177", '\177', 127);
    check("esc \\012", '\012', 10); /* \n */
    check("esc \\07",  '\07',   7); /* \a, 1-digit octal padded */

    /* Hex: \x1b (ESC), \x7f (DEL). */
    check("esc \\x1b", '\x1b', 27);
    check("esc \\x7f", '\x7f', 127);
    check("esc \\x41", '\x41', 65); /* 'A' */

    /* Escape sequences in string literals. */
    check_s("str \\n\\r",   "\n\r",   "\012\015");
    check_s("str \\t\\b",   "\t\b",   "\011\010");
    check_s("str \\033[H",  "\033[H", "\x1b[H");
    check_s("str \\x1b[H",  "\x1b[H", "\033[H");
    check_s("str mix",      "A\033Z", "A\x1bZ");
    check_s("str \\0 term", "\060",   "0");   /* \060 = '0' = 48 */
}

int main()
{
    test_chained_assign();
    test_escapes();

    if (failures) {
        printf("tesc: %d failure(s)\n", failures);
        return 1;
    }

    printf("tesc: all tests passed\n");
    return 0;
}
