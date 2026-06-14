/*
 * trtl_lzpack.c -- regression tests for RTL routines added for lzpack.c.
 *
 * Tests:
 *   putc
 *   remove
 *   strrchr
 *   memcmp
 *   memmove
 *   strncat
 *
 * Intentionally does not test getc.
 *
 * Expected output:
 *   trtl_lzpack: all tests passed
 *
 * Expected exit status:
 *   0
 */

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void fail(const char *name, int got, int expected)
{
    printf("FAIL %s got %d expected %d\n", name, got, expected);
    failures++;
}

static void fail_ptr(const char *name)
{
    printf("FAIL %s\n", name);
    failures++;
}

static void check_i(const char *name, int got, int expected)
{
    if (got != expected)
        fail(name, got, expected);
}

static void check_s(const char *name, const char *got, const char *expected)
{
    if (strcmp(got, expected) != 0) {
        printf("FAIL %s got '%s' expected '%s'\n", name, got, expected);
        failures++;
    }
}

static void test_strrchr(void)
{
    char s[] = "a/b/c.txt";
    char *p;

    p = strrchr(s, '/');
    if (p == 0)
        fail_ptr("strrchr slash null");
    else
        check_s("strrchr slash", p, "/c.txt");

    p = strrchr(s, '.');
    if (p == 0)
        fail_ptr("strrchr dot null");
    else
        check_s("strrchr dot", p, ".txt");

    p = strrchr(s, 'z');
    if (p != 0)
        fail_ptr("strrchr missing");

    p = strrchr(s, '\0');
    if (p != s + strlen(s))
        fail_ptr("strrchr nul");
}

static void test_memcmp(void)
{
    unsigned char a[5];
    unsigned char b[5];

    a[0] = 1; a[1] = 2; a[2] = 3; a[3] = 4; a[4] = 5;
    b[0] = 1; b[1] = 2; b[2] = 3; b[3] = 4; b[4] = 5;

    check_i("memcmp equal", memcmp(a, b, 5), 0);
    check_i("memcmp zero length", memcmp(a, b, 0), 0);

    b[2] = 9;
    if (memcmp(a, b, 5) >= 0)
        fail_ptr("memcmp less-than");

    b[2] = 0;
    if (memcmp(a, b, 5) <= 0)
        fail_ptr("memcmp greater-than");
}

static void test_memmove(void)
{
    char s1[16];
    char s2[16];
    char *r;

    strcpy(s1, "abcdef");
    r = memmove(s1 + 2, s1, 4);
    if (r != s1 + 2)
        fail_ptr("memmove return overlap high");
    check_s("memmove overlap high", s1, "ababcd");

    strcpy(s2, "abcdef");
    r = memmove(s2, s2 + 2, 4);
    if (r != s2)
        fail_ptr("memmove return overlap low");
    check_s("memmove overlap low", s2, "cdefef");

    r = memmove(s2, s2, 6);
    if (r != s2)
        fail_ptr("memmove return same");
    check_s("memmove same", s2, "cdefef");

    r = memmove(s2, s1, 0);
    if (r != s2)
        fail_ptr("memmove return zero");
}

static void test_strncat(void)
{
    char s[32];
    char *r;

    strcpy(s, "abc");
    r = strncat(s, "def", 2);
    if (r != s)
        fail_ptr("strncat return");
    check_s("strncat partial", s, "abcde");

    r = strncat(s, "XYZ", 0);
    if (r != s)
        fail_ptr("strncat zero return");
    check_s("strncat zero", s, "abcde");

    r = strncat(s, "f", 10);
    if (r != s)
        fail_ptr("strncat long return");
    check_s("strncat stops at nul", s, "abcdef");
}

static void test_putc_and_remove(void)
{
    FILE *f;
    int rc;

    f = fopen("TRTLTMP.$$$", "wb");
    if (f == 0) {
        fail_ptr("fopen temp");
        return;
    }

    if (putc('A', f) == EOF)
        fail_ptr("putc A");
    if (putc('\n', f) == EOF)
        fail_ptr("putc newline");

    rc = fclose(f);
    check_i("fclose temp", rc, 0);

    rc = remove("TRTLTMP.$$$");
    check_i("remove temp", rc, 0);

    rc = remove("TRTLTMP.$$$");
    if (rc == 0)
        fail_ptr("remove missing should fail");
}

int main(void)
{
    test_strrchr();
    test_memcmp();
    test_memmove();
    test_strncat();
    test_putc_and_remove();

    if (failures) {
        printf("trtl_lzpack: %d failure(s)\n", failures);
        return 1;
    }

    printf("trtl_lzpack: all tests passed\n");
    return 0;
}
