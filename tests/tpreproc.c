#include <stdio.h>
#include <string.h>

static int g_failed = 0;

static void verify_int(int actual, int expected, const char *test_name) {
    if (actual == expected) {
        printf("PASS: %s\n", test_name);
    } else {
        printf("FAIL: %s\n", test_name);
        g_failed++;
    }
}

static void verify_str(const char *actual, const char *expected, const char *test_name) {
    if (strcmp(actual, expected) == 0) {
        printf("PASS: %s\n", test_name);
    } else {
        printf("FAIL: %s\n", test_name);
        g_failed++;
    }
}

/* Token pasting and function-like macro isolation. */
#define GLUE(a, b) a ## b
#define SUB_MACRO_VAL() 100
#define EVALUATE_TEST(x) GLUE(PREFIX_, x)

/* Stringification and argument pre-expansion. */
#define TEST_VALUE 99
#define STR_IMMEDIATE(x) #x
#define STR_DEFERRED(x) STR_IMMEDIATE(x)

/* Function-like macro expansion through object macros and macro arguments. */
#define ADD1(x) ((x)+1)
#define VALUE_FROM_CALL ADD1(4)
#define STR_DEFERRED_CALL(x) STR_IMMEDIATE(x)

/* Pasted tokens must be rescanned as normal preprocessing tokens. */
#define VALUE_7 123
#define MAKE_VALUE(n) GLUE(VALUE_, n)

/* Conditional expression handling. */
#define PP_A 3
#define PP_B 4
#if defined(PP_A) && !defined(PP_MISSING) && ((PP_A + PP_B * 2) == 11)
#define CONDITIONAL_VALUE 456
#else
#define CONDITIONAL_VALUE -1
#endif

#undef PP_B
#ifdef PP_B
#define UNDEF_VALUE -1
#elif defined(PP_A)
#define UNDEF_VALUE 789
#else
#define UNDEF_VALUE -2
#endif

int main(void) {
    int PREFIX_SUB_MACRO_VAL = 777;

    printf("Running Universal C89 Preprocessor Tests...\n\n");

    verify_int(GLUE(PREFIX_, SUB_MACRO_VAL), 777, "1. Token Pasting (##)");
    verify_str("Part A " "and Part B", "Part A and Part B", "2. String Concatenation");
    verify_int(EVALUATE_TEST(SUB_MACRO_VAL), 777, "3. Macro Isolation during Pasting");
    verify_str(STR_IMMEDIATE(TEST_VALUE), "TEST_VALUE", "4a. Immediate Stringify");
    verify_str(STR_DEFERRED(TEST_VALUE), "99", "4b. Deferred Stringify");

    verify_int(VALUE_FROM_CALL, 5, "5. Object Macro Rescans Function-like Macro");
    verify_str(STR_DEFERRED(ADD1(4)), "((4)+1)", "6. Function-like Macro Argument Pre-expansion");
    verify_int(MAKE_VALUE(7), 123, "7. Pasted Token Rescan");
    verify_int(CONDITIONAL_VALUE, 456, "8. #if defined/expression");
    verify_int(UNDEF_VALUE, 789, "9. #undef and #elif");

    printf("\n----------------------------------------\n");
    if (g_failed == 0) {
        printf("ALL COMBINED PREPROCESSOR TESTS PASSED WITH GREAT SUCCESS!\n");
        return 0;
    } else {
        return 1;
    }
}
