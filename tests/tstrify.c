#include <stdio.h>
#include <string.h>

/* The stringification macro under test */
#define STR(x) #x

/* Global counter to track test failures */
static int g_failed = 0;

/* Helper function to validate the stringification results */
void test_stringify(const char *actual, const char *expected, const char *test_name) {
    if (strcmp(actual, expected) == 0) {
        printf("PASS: %s -> \"%s\"\n", test_name, actual);
    } else {
        printf("FAIL: %s\n", test_name);
        printf("  Expected: \"%s\"\n", expected);
        printf("  Actual:   \"%s\"\n", actual);
        g_failed++;
    }
}

int main(void) {
    printf("Running C89 Stringification (#) Operator Tests...\n\n");

    /* Test 1: Basic integer expression */
    test_stringify(STR(1 + 1), "1 + 1", "Basic expression");

    /* Test 2: Whitespace normalization */
    /* C89 specifies that multiple whitespaces collapse to a single space, */
    /* and leading/trailing whitespace is deleted. */
    test_stringify(STR(  a    +   b  ), "a + b", "Whitespace normalization");

    /* Test 3: Variable names and logical operators */
    test_stringify(STR(x && y || !z), "x && y || !z", "Logical operators");

    /* Test 4: Internal quotes and escape sequences */
    /* Internal double quotes and backslashes must automatically be escaped */
    test_stringify(STR("hello\n"), "\"hello\\n\"", "String literal escape");

    /* Final report */
    printf("\n----------------------------------------\n");
    if (g_failed == 0) {
        printf("ALL TESTS PASSED WITH GREAT SUCCESS!\n");
        return 0;
    } else {
        printf("TEST SUITE FAILED: %d failure(s) detected.\n", g_failed);
        return 1;
    }
}
