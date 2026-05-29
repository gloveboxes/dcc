#include <stdio.h>
#include <limits.h>

int main() {
    int passed = 0;
    int total = 0;

    printf("Starting C89 Limits Validation...\n\n");

    /* 1. Validate CHAR (8-bit) */
    total++;
    printf("Test 1: char (8-bit) limits... ");
    if (CHAR_BIT == 8 && SCHAR_MIN == -128 && SCHAR_MAX == 127) {
        /* Verify math: UCHAR_MAX + 1 should wrap to 0 */
        if ((unsigned char)(UCHAR_MAX + 1) == 0) {
            printf("PASSED\n");
            passed++;
        } else printf("FAILED (Math mismatch)\n");
    } else printf("FAILED (Constants mismatch)\n");

    /* 2. Validate 2-byte Constants (16-bit short) */
    total++;
    printf("Test 2: 2-byte (16-bit) short limits... ");
    if (SHRT_MIN == -32768 && SHRT_MAX == 32767) {
        /* Verify math: USHRT_MAX + 1 should wrap to 0 */
        if ((unsigned short)(USHRT_MAX + 1) == 0) {
            printf("PASSED\n");
            passed++;
        } else printf("FAILED (Math mismatch)\n");
    } else printf("FAILED (Constants mismatch)\n");

    /* 3. Validate 4-byte Constants (32-bit long) */
    total++;
    printf("Test 3: 4-byte (32-bit) long limits... ");
    /* C89 signed long min is typically defined this way to avoid overflow literals */
    if (LONG_MIN == -2147483647L - 1L && LONG_MAX == 2147483647L) {
        /* Verify math: ULONG_MAX + 1 should wrap to 0 */
        if ((unsigned long)(ULONG_MAX + 1L) == 0) {
            printf("PASSED\n");
            passed++;
        } else printf("FAILED (Math mismatch)\n");
    } else printf("FAILED (Constants mismatch)\n");

    /* Summary */
    printf("\nResults: %d/%d tests passed.\n", passed, total);
    return (passed == total) ? 0 : 1;
}
