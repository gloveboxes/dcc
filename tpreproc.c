#include <stdio.h>
#include <string.h>

static int g_failed = 0;

void verify_int(int actual, int expected, const char *test_name) {
    if (actual == expected) {
        printf("PASS: %s\n", test_name);
    } else {
        printf("FAIL: %s\n", test_name);
        g_failed++;
    }
}

void verify_str(const char *actual, const char *expected, const char *test_name) {
    if (strcmp(actual, expected) == 0) {
        printf("PASS: %s\n", test_name);
    } else {
        printf("FAIL: %s\n", test_name);
        g_failed++;
    }
}

/* Feature 1 & 3 Setup: Token Pasting & Function-Like Macro Isolation */
#define GLUE(a, b) a ## b

/* A function-like macro hides '100' from premature evaluation in MSVC */
#define SUB_MACRO_VAL() 100

/* Pastes the token 'PREFIX_' directly onto whatever text token is passed */
#define EVALUATE_TEST(x) GLUE(PREFIX_, x)

/* Feature 4 Setup: Hierarchy Macros */
#define TEST_VALUE 99
#define STR_IMMEDIATE(x) #x
#define STR_DEFERRED(x) STR_IMMEDIATE(x)

int main(void) {
    /* Define the target variable name using the function-like token wrapper name */
    int PREFIX_SUB_MACRO_VAL = 777;

    printf("Running Universal C89 Preprocessor Tests...\n\n");

    /* 1. Test Token Pasting (##) */
    /* Pastes 'PREFIX_' and 'SUB_MACRO_VAL' literal into the variable identifier */
    verify_int(GLUE(PREFIX_, SUB_MACRO_VAL), 777, "1. Token Pasting (##)");

    /* 2. Test Automatic String Literal Concatenation */
    verify_str("Part A " "and Part B", "Part A and Part B", "2. String Concatenation");

    /* 3. Test Function-like Macro Isolation during Pasting */
    /* If isolation fails, 'SUB_MACRO_VAL' evaluates, generating a call or error. */
    /* Because it works properly, it joins the exact token names into 'PREFIX_SUB_MACRO_VAL' */
    verify_int(EVALUATE_TEST(SUB_MACRO_VAL), 777, "3. Macro Isolation during Pasting");

    /* 4. Test Macro Argument Substitution Hierarchy */
    verify_str(STR_IMMEDIATE(TEST_VALUE), "TEST_VALUE", "4a. Immediate Stringify");
    verify_str(STR_DEFERRED(TEST_VALUE), "99", "4b. Deferred Stringify");

    printf("\n----------------------------------------\n");
    if (g_failed == 0) {
        printf("ALL COMBINED PREPROCESSOR TESTS PASSED WITH GREAT SUCCESS!\n");
        return 0;
    } else {
        return 1;
    }
}
