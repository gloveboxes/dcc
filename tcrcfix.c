/*
 * tcrcfix_c89.c -- standalone ANSI C89 regression tests for DCC bugs
 * exposed by crc.c.
 *
 * Expected output:
 *   tcrcfix_c89: all tests passed
 *
 * Expected exit status:
 *   0
 */

#include <stdio.h>
#include <string.h>

#ifndef BUFSIZ
#error BUFSIZ should be available
#endif

#ifndef EOF
#error EOF should be available
#endif

typedef unsigned long crc_t;

static int failures = 0;

static void fail(const char *name, long got, long expected)
{
    printf("FAIL %s got %ld expected %ld\n", name, got, expected);
    failures++;
}

static void check_l(const char *name, long got, long expected)
{
    if (got != expected)
        fail(name, got, expected);
}

static void check_i(const char *name, int got, int expected)
{
    if (got != expected)
        fail(name, (long)got, (long)expected);
}

/*
 * These names collide under M80/L80's short external-name significance if
 * file-scope static functions are incorrectly emitted as PUBLIC C names.
 */
static int cb_is_zero(void) { return 101; }
static int cb_is_one(void)  { return 202; }
static int cb_subtract(void) { return 303; }

static int compute_crc(void) { return 404; }
static int compute_crx(void) { return 505; }

static int crc_update_byte_name(void) { return 606; }
static int crc_update_buffer_name(void) { return 707; }

static int out_err_check_int(void) { return 808; }
static int out_err_check_fputc(void) { return 909; }

static void test_static_name_mangling(void)
{
    check_i("cb_is_zero", cb_is_zero(), 101);
    check_i("cb_is_one", cb_is_one(), 202);
    check_i("cb_subtract", cb_subtract(), 303);
    check_i("compute_crc", compute_crc(), 404);
    check_i("compute_crx", compute_crx(), 505);
    check_i("crc_update_byte_name", crc_update_byte_name(), 606);
    check_i("crc_update_buffer_name", crc_update_buffer_name(), 707);
    check_i("out_err_check_int", out_err_check_int(), 808);
    check_i("out_err_check_fputc", out_err_check_fputc(), 909);
}

#define TRIM_RING 2
#define TRIM_BUFSIZE 16

static void test_multidim_arrays(void)
{
    static char bufs[TRIM_RING][TRIM_BUFSIZE];
    int i;
    int j;
    int sum;

    for (i = 0; i < TRIM_RING; i++) {
        for (j = 0; j < TRIM_BUFSIZE; j++)
            bufs[i][j] = (char)(i * 20 + j);
    }

    sum = 0;
    for (i = 0; i < TRIM_RING; i++) {
        for (j = 0; j < TRIM_BUFSIZE; j++)
            sum += bufs[i][j];
    }

    check_i("multidim static array", sum, 560);
}

/*
 * Similar shape to crc.c's main parameter and progname initializer.
 * This intentionally tests:
 *   char * const argv[]
 *   (char **)0
 *   (char *)0
 *   *argv[0]
 *   argv[0][1]
 */
static int argv_probe(const int argc, char * const argv[])
{
    const char * const progname =
        (((char **)0 != argv && (char *)0 != argv[0] && '\0' != *argv[0])
          ? ('\0' == argv[0][1] ? "crc" : argv[0]) : "crc");

    (void)argc;
    return progname[0] + progname[1];
}

static void test_argv_expr(void)
{
    char a0[3];
    char a1[2];
    char *av[3];

    a0[0] = 'x';
    a0[1] = 'y';
    a0[2] = 0;

    a1[0] = 'z';
    a1[1] = 0;

    av[0] = a0;
    av[1] = a1;
    av[2] = (char *)0;

    check_i("argv expression", argv_probe(2, av), 'x' + 'y');
}

/*
 * This exposed the bug where direct while-condition generation compared
 * only the low 16 bits of a cast-to-unsigned-long expression:
 *
 *   while (0 != (crc_t)x)
 */
static int crc_t_bits_probe(void)
{
    crc_t x = 1;
    int bits = 0;

    while (0 != (crc_t)x) {
        bits++;
        x <<= 1;
    }

    return bits;
}

static void test_long_direct_condition(void)
{
    check_i("crc_t_bits", crc_t_bits_probe(), 32);
}

static crc_t crc_tbl[256];

static void init_crc_tbl(void)
{
    /*
     * First few entries from crc.c's table, plus entries reached by
     * processing "01234567" with crc_update_byte_probe().
     */
    crc_tbl[0] = (crc_t)0x00000000UL;
    crc_tbl[1] = (crc_t)0x51F9D3DEUL;
    crc_tbl[2] = (crc_t)0xA3F3A7BCUL;
    crc_tbl[3] = (crc_t)0xF20A7462UL;
    crc_tbl[4] = (crc_t)0x161E9CA6UL;
    crc_tbl[5] = (crc_t)0x47E74F78UL;
    crc_tbl[6] = (crc_t)0xB5ED3B1AUL;
    crc_tbl[7] = (crc_t)0xE414E8C4UL;

    crc_tbl[48] = (crc_t)0xE88E97A8UL;
    crc_tbl[217] = (crc_t)0x758EB2C8UL;
    crc_tbl[201] = (crc_t)0x2DF4C050UL;
    crc_tbl[7] = (crc_t)0xE414E8C4UL;
    crc_tbl[62] = (crc_t)0x715E95FEUL;
    crc_tbl[88] = (crc_t)0x4457526AUL;
    crc_tbl[148] = (crc_t)0x2E44DD42UL;
    crc_tbl[31] = (crc_t)0x9053A310UL;
}

/*
 * Same shape as crc.c's crc_update_byte. This exercises:
 *   32-bit unsigned right shift
 *   32-bit unsigned left shift
 *   32-bit ^= and &=
 *   long table indexing via tbl[idx]
 *   const crc_t * const parameter declarator
 */
static crc_t crc_update_byte_probe(
    crc_t crc,
    const crc_t * const tbl,
    const crc_t mask32,
    const unsigned char b)
{
    crc_t idx;
    crc_t t;

    t = crc;
    t >>= 24;

    idx = t;
    idx ^= (crc_t)b;
    idx &= (crc_t)0xFF;

    t = crc;
    t <<= 8;

    crc = t;
    crc ^= tbl[idx];
    crc &= mask32;

    return crc;
}

/*
 * Same long-return-with-12-bytes-of-args shape that exposed DE:HL clobbering
 * after call cleanup in crc_update_buffer().
 */
static crc_t call_cleanup_callee(
    crc_t a,
    const crc_t * const tbl,
    const crc_t mask32,
    const unsigned char b)
{
    return crc_update_byte_probe(a, tbl, mask32, b);
}

static crc_t call_cleanup_caller(void)
{
    return call_cleanup_callee(
        (crc_t)0x12345678UL,
        crc_tbl,
        (crc_t)0xFFFFFFFFUL,
        (unsigned char)'0');
}

static void test_crc_update_kernel(void)
{
    crc_t crc;
    int i;
    static unsigned char s[8] = { '0','1','2','3','4','5','6','7' };

    init_crc_tbl();

    crc = 0;
    for (i = 0; i < 8; i++)
        crc = crc_update_byte_probe(crc, crc_tbl, (crc_t)0xFFFFFFFFUL, s[i]);

    check_l("crc_update_kernel", (long)crc, (long)((crc_t)0x78E4E110UL));

    /*
     * idx = ((0x12345678 >> 24) ^ '0') & 0xff = 0x22.
     * tbl[0x22] defaults to zero in this test, so result is 0x34567800.
     * The key property is that the high word 0x3456 survives call cleanup.
     */
    check_l("long return call cleanup", (long)call_cleanup_caller(),
            (long)((crc_t)0x34567800UL));
}


/*
 * This exposed the bug in non-IX-direct 32-bit compound shift stores.
 *
 * In crc.c's 6-bit path, `buf >>= 8` was a local crc_t in a large stack
 * frame, so it lived too far below IX for direct indexed stores.  The old
 * compound-shift store path corrupted the low word while storing DE:HL back
 * through a saved lvalue address.
 */
static crc_t non_ix_shift_store_probe(void)
{
    volatile char pad[160];
    crc_t buf;
    int i;

    for (i = 0; i < (int)sizeof(pad); i++)
        pad[i] = (char)i;

    buf = (crc_t)0x12345678UL;
    buf >>= 8;
    if (buf != (crc_t)0x00123456UL)
        return buf;

    buf >>= 8;
    if (buf != (crc_t)0x00001234UL)
        return buf;

    buf = (crc_t)0x00000031UL;
    buf <<= 6;
    if (buf != (crc_t)0x00000C40UL)
        return buf;

    buf >>= 8;
    return buf;
}

static void test_non_ix_compound_shift_store(void)
{
    check_l("non-IX compound shift store",
            (long)non_ix_shift_store_probe(),
            (long)((crc_t)0x0000000CUL));
}

static void test_stdio_rtl_symbols(void)
{
    char *p;

    clearerr(stdout);
    setbuf(stdout, (char *)0);

    p = strerror(0);
    if ((char *)0 == p || '\0' == *p)
        fail("strerror", 0L, 1L);

    if (EOF == fputc('\n', stdout))
        fail("fputc", 0L, 1L);
}

int main(void)
{
    test_static_name_mangling();
    test_multidim_arrays();
    test_argv_expr();
    test_long_direct_condition();
    test_crc_update_kernel();
    test_non_ix_compound_shift_store();
    test_stdio_rtl_symbols();

    if (failures) {
        printf("tcrcfix_c89: %d failure(s)\n", failures);
        return 1;
    }

    printf("tcrcfix_c89: all tests passed\n");
    return 0;
}
