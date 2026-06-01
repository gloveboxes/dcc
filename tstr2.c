#include <stdio.h>
#include <string.h>

/* Helper function to report test status */
void report_test(const char *name, int condition) {
    printf("%-15s: %s\n", name, condition ? "PASS" : "FAIL");
}

/* --- Tests for memchr() --- */
static void test_memchr(void) {
    const char data[] = { 'a', 'b', 0x00, 'c', 'd' };
    void *res;

    /* 1. Basic match */
    res = memchr(data, 'b', sizeof(data));
    report_test("memchr Basic", (res == (void *)&data[1]));

    /* 2. Match with embedded null */
    res = memchr(data, 0x00, sizeof(data));
    report_test("memchr Null", (res == (void *)&data[2]));

    /* 3. Character not found */
    res = memchr(data, 'z', sizeof(data));
    report_test("memchr Missing", (res == NULL));

    /* 4. Match within bounds */
    res = memchr(data, 'a', 2);
    report_test("memchr Bounded", (res == (void *)&data[0]));

    /* 5. Out-of-bounds scan */
    res = memchr(data, 'c', 2);
    report_test("memchr OOB", (res == NULL));
}

/* --- Tests for strcat() --- */
static void test_strcat(void) {
    char dest[20] = "Hello";

    /* 1. Basic concatenation */
    strcat(dest, " World");
    report_test("strcat Basic", (strcmp(dest, "Hello World") == 0));

    /* 2. Concatenating empty string */
    strcat(dest, "");
    report_test("strcat Empty", (strcmp(dest, "Hello World") == 0));
}

/* --- Tests for strncpy() --- */
static void test_strncpy(void) {
    char dest[10];

    /* 1. Standard copy (Source fits exactly) */
    strncpy(dest, "abc", sizeof(dest));
    report_test("strncpy Fit", (strcmp(dest, "abc") == 0));

    /* 2. Truncation (Source exceeds n, no null termination in dest) */
    strncpy(dest, "123456789012", 5);
    report_test("strncpy Trunc", (strncmp(dest, "12345", 5) == 0));

    /* 3. Padding (Source is shorter than n, remaining bytes zero-filled) */
    memset(dest, 'x', sizeof(dest)); /* Fill with garbage */
    strncpy(dest, "abc", sizeof(dest));
    report_test("strncpy Pad1", (dest[3] == '\0' && dest[9] == '\0'));
    report_test("strncpy Pad2", (strcmp(dest, "abc") == 0));
}

int main(void) {
    test_memchr();
    test_strcat();
    test_strncpy();
    printf("tstr2 completed with great success!\n" );
    return 0;
}
