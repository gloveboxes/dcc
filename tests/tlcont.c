#include <stdio.h>
#include <string.h>

/* Global counter to track test failures */
static int g_failed = 0;

/* Helper function to validate string results */
static void verify_string(const char *actual, const char *expected, const char *test_name) {
    if (strcmp(actual, expected) == 0) {
        printf("PASS: %s\n", test_name);
    } else {
        printf("FAIL: %s\n", test_name);
        printf("  Expected: \"%s\"\n", expected);
        printf("  Actual:   \"%s\"\n", actual);
        g_failed++;
    }
}

/* Helper function to validate integer results */
static void verify_int(int actual, int expected, const char *test_name) {
    if (actual == expected) {
        printf("PASS: %s\n", test_name);
    } else {
        printf("FAIL: %s\n", test_name);
        printf("  Expected: %d\n", expected);
        printf("  Actual:   %d\n", actual);
        g_failed++;
    }
}

/* Test 2 macro: Splitting a macro definition across lines */
#define MULTI_LINE_MACRO(a, b) \
    ((a) + \
     (b))

int main(void) {
    /* Test 3 variables: Splitting keywords, identifiers, and assignments */
    int long_v\
ariable_n\
ame = 40;

    int second_val = 2;

    printf("Running C89 Line Continuation (\\) Tests...\n\n");

    /* Test 1: Splicing within a string literal */
    /* Note: There must be NO whitespace after the backslash on line 45 */
    verify_string("Hello \
World", "Hello World", "String literal line continuation");

    /* Test 2: Multi-line macro execution */
    verify_int(MULTI_LINE_MACRO(10, 5), 15, "Macro line continuation");

    /* Test 3: Splicing token identifiers and expressions */
    /* The assignment below splices the operator '+=' across lines */
    long_v\
ariable_name \
+=\
    second_val;

    verify_int(long_variable_name, 42, "Identifier and operator line continuation");

    /* Final report */
    printf("\n----------------------------------------\n");
    if (g_failed == 0) {
        printf("ALL LINE CONTINUATION TESTS PASSED!\n");
        return 0;
    } else {
        printf("TEST SUITE FAILED: %d failure(s) detected.\n", g_failed);
        return 1;
    }
}
