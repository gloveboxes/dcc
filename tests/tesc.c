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

static void check_f(name, got, expected)
const char *name;
float got;
float expected;
{
    if (got != expected) {
        printf("FAIL %s\n", name);
        failures++;
    }
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

static void test_char_consts()
{
    /* Plain printable character literals. */
    check("plain ' '", ' ',  32);
    check("plain '!'", '!',  33);
    check("plain '0'", '0',  48);
    check("plain '9'", '9',  57);
    check("plain 'A'", 'A',  65);
    check("plain 'Z'", 'Z',  90);
    check("plain 'a'", 'a',  97);
    check("plain 'z'", 'z', 122);
    check("plain '~'", '~', 126);

    /* Octal: all single digits 1-7. */
    check("oct \\1", '\1', 1);
    check("oct \\2", '\2', 2);
    check("oct \\3", '\3', 3);
    check("oct \\4", '\4', 4);
    check("oct \\5", '\5', 5);
    check("oct \\6", '\6', 6);
    check("oct \\7", '\7', 7);

    /* Octal: two-digit forms. */
    check("oct \\10", '\10',  8);   /* BS */
    check("oct \\11", '\11',  9);   /* HT */
    check("oct \\15", '\15', 13);   /* CR */
    check("oct \\17", '\17', 15);
    check("oct \\20", '\20', 16);
    check("oct \\37", '\37', 31);
    check("oct \\40", '\40', 32);   /* space */
    check("oct \\57", '\57', 47);   /* '/' */
    check("oct \\60", '\60', 48);   /* '0' */
    check("oct \\71", '\71', 57);   /* '9' */
    check("oct \\77", '\77', 63);   /* '?' */

    /* Octal: three-digit forms. */
    check("oct \\100", '\100', 64);  /* '@' */
    check("oct \\101", '\101', 65);  /* 'A' */
    check("oct \\141", '\141', 97);  /* 'a' */
    check("oct \\176", '\176', 126); /* '~' */

    /* Hex: single-digit forms 0-9. */
    check("hex \\x0", '\x0',  0);
    check("hex \\x1", '\x1',  1);
    check("hex \\x7", '\x7',  7);
    check("hex \\x8", '\x8',  8);
    check("hex \\x9", '\x9',  9);

    /* Hex: lowercase a-f. */
    check("hex \\xa", '\xa', 10);
    check("hex \\xb", '\xb', 11);
    check("hex \\xc", '\xc', 12);
    check("hex \\xd", '\xd', 13);
    check("hex \\xe", '\xe', 14);
    check("hex \\xf", '\xf', 15);

    /* Hex: uppercase A-F. */
    check("hex \\xA", '\xA', 10);
    check("hex \\xB", '\xB', 11);
    check("hex \\xC", '\xC', 12);
    check("hex \\xD", '\xD', 13);
    check("hex \\xE", '\xE', 14);
    check("hex \\xF", '\xF', 15);

    /* Hex: two-digit forms. */
    check("hex \\x10", '\x10', 16);
    check("hex \\x20", '\x20', 32);  /* space */
    check("hex \\x30", '\x30', 48);  /* '0' */
    check("hex \\x4A", '\x4A', 74);  /* 'J' */
    check("hex \\x61", '\x61', 97);  /* 'a' */
    check("hex \\x7e", '\x7e', 126); /* '~' */

    /* Octal 3-digit boundary in strings: parser must stop at 3 digits. */
    check_s("str \\0772", "\0772", "?2");  /* \077='?' then '2' */
    check_s("str \\1001", "\1001", "@1");  /* \100='@' then '1' */
}

static void test_int_consts()
{
    /* Decimal integer constants. */
    check("idec 0",    0,    0);
    check("idec 1",    1,    1);
    check("idec 7",    7,    7);
    check("idec 8",    8,    8);
    check("idec 9",    9,    9);
    check("idec 10",  10,   10);
    check("idec 27",  27,   27);
    check("idec 63",  63,   63);
    check("idec 65",  65,   65);
    check("idec 97",  97,   97);
    check("idec 127", 127, 127);

    /* Octal integer constants (leading zero). */
    check("ioct 00",   00,    0);
    check("ioct 01",   01,    1);
    check("ioct 07",   07,    7);
    check("ioct 010",  010,   8);
    check("ioct 011",  011,   9);
    check("ioct 012",  012,  10);
    check("ioct 015",  015,  13);
    check("ioct 017",  017,  15);
    check("ioct 020",  020,  16);
    check("ioct 032",  032,  26);  /* not 32 -- octal gotcha */
    check("ioct 033",  033,  27);
    check("ioct 040",  040,  32);
    check("ioct 077",  077,  63);
    check("ioct 0100", 0100, 64);
    check("ioct 0101", 0101, 65);
    check("ioct 0141", 0141, 97);
    check("ioct 0177", 0177, 127);

    /* Hexadecimal constants: 0x prefix, lowercase digits. */
    check("ihex 0x0",  0x0,   0);
    check("ihex 0x1",  0x1,   1);
    check("ihex 0x7",  0x7,   7);
    check("ihex 0x8",  0x8,   8);
    check("ihex 0x9",  0x9,   9);
    check("ihex 0xa",  0xa,  10);
    check("ihex 0xb",  0xb,  11);
    check("ihex 0xc",  0xc,  12);
    check("ihex 0xd",  0xd,  13);
    check("ihex 0xe",  0xe,  14);
    check("ihex 0xf",  0xf,  15);
    check("ihex 0x10", 0x10, 16);
    check("ihex 0x1b", 0x1b, 27);
    check("ihex 0x3f", 0x3f, 63);
    check("ihex 0x41", 0x41, 65);
    check("ihex 0x61", 0x61, 97);
    check("ihex 0x7f", 0x7f, 127);

    /* Hexadecimal constants: 0x prefix, uppercase digits. */
    check("ihex 0xA",  0xA,  10);
    check("ihex 0xB",  0xB,  11);
    check("ihex 0xC",  0xC,  12);
    check("ihex 0xD",  0xD,  13);
    check("ihex 0xE",  0xE,  14);
    check("ihex 0xF",  0xF,  15);
    check("ihex 0x1B", 0x1B, 27);
    check("ihex 0x4A", 0x4A, 74);
    check("ihex 0x7F", 0x7F, 127);

    /* Hexadecimal constants: 0X prefix (upper-case X). */
    check("ihex 0X1b", 0X1b, 27);
    check("ihex 0X7f", 0X7f, 127);
    check("ihex 0X1B", 0X1B, 27);
    check("ihex 0X7F", 0X7F, 127);

    /* Cross-check: same value in all three bases. */
    check("cross dec 27",  27,   27);
    check("cross oct 033", 033,  27);
    check("cross hex 0x1b",0x1b, 27);
    check("cross dec 127",  127,   127);
    check("cross oct 0177", 0177,  127);
    check("cross hex 0x7f", 0x7f,  127);
    check("cross dec 65",   65,    65);
    check("cross oct 0101", 0101,  65);
    check("cross hex 0x41", 0x41,  65);
}

static void test_float_consts()
{
    /* Decimal forms: trailing dot, leading dot, both. */
    check_f("flt 1.0",  1.0,  1.0);
    check_f("flt 1.",   1.,   1.0);
    check_f("flt .5",   .5,   0.5);
    check_f("flt 0.5",  0.5,  0.5);
    check_f("flt 1.5",  1.5,  1.5);
    check_f("flt 0.25", 0.25, 0.25);

    /* Exponent: e vs E. */
    check_f("flt 1.0e0", 1.0e0, 1.0);
    check_f("flt 1.0E0", 1.0E0, 1.0);
    check_f("flt 5.0e1", 5.0e1, 50.0);
    check_f("flt 5.0E1", 5.0E1, 50.0);

    /* Exponent: explicit + sign. */
    check_f("flt 1.0e+0", 1.0e+0, 1.0);
    check_f("flt 1.0E+0", 1.0E+0, 1.0);
    check_f("flt 5.0e+1", 5.0e+1, 50.0);
    check_f("flt 5.0E+1", 5.0E+1, 50.0);

    /* Exponent: negative. */
    check_f("flt 1.25e-1", 1.25e-1, 0.125);
    check_f("flt 1.25E-1", 1.25E-1, 0.125);
    check_f("flt 5.0e-1",  5.0e-1,  0.5);
    check_f("flt 5.0E-1",  5.0E-1,  0.5);

    /* Trailing-dot form with exponent. */
    check_f("flt 1.e0",  1.e0,  1.0);
    check_f("flt 5.e1",  5.e1,  50.0);
    check_f("flt 5.e+1", 5.e+1, 50.0);
    check_f("flt 5.e-1", 5.e-1, 0.5);

    /* float suffix: f and F. */
    check_f("flt 1.0f",   1.0f,   1.0);
    check_f("flt 1.0F",   1.0F,   1.0);
    check_f("flt 0.5f",   0.5f,   0.5);
    check_f("flt 50.0f",  50.0f,  50.0);
    check_f("flt 5.e1f",  5.e1f,  50.0);
    check_f("flt 5.e+1F", 5.e+1F, 50.0);

    /* Cross-check: same value, different notation. */
    check_f("flt cross 50.0 a", 5.0e1,   50.0);
    check_f("flt cross 50.0 b", 5.0E+1,  50.0);
    check_f("flt cross 50.0 c", 50.,     50.0);
    check_f("flt cross 0.5 a",  .5,      0.5);
    check_f("flt cross 0.5 b",  5.0e-1,  0.5);
    check_f("flt cross 0.5 c",  5.0E-1,  0.5);
    check_f("flt cross 0.125 a", .125,    0.125);
    check_f("flt cross 0.125 b", 1.25e-1, 0.125);
    check_f("flt cross 0.125 c", 1.25E-1, 0.125);
}

int main()
{
    test_chained_assign();
    test_escapes();
    test_char_consts();
    test_int_consts();
    test_float_consts();

    if (failures) {
        printf("tesc: %d failure(s)\n", failures);
        return 1;
    }

    printf("tesc: all tests passed\n");
    return 0;
}
