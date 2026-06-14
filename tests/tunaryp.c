#include <stdio.h>
#include <stdint.h>

int fails = 0;

void chkl(const char *expr, long result, long expected) {
    if (result != expected) {
        printf("FAIL: %s = %ld (expected %ld)\n", expr, result, expected);
        fails = 1;
    } else {
        printf("PASS: %s = %ld\n", expr, expected);
    }
}

void chki(const char *expr, int result, int expected) {
    if (result != expected) {
        printf("FAIL: %s = %d (expected %d)\n", expr, result, expected);
        fails = 1;
    } else {
        printf("PASS: %s = %d\n", expr, expected);
    }
}

void chku(const char *expr, unsigned int result, unsigned int expected) {
    if (result != expected) {
        printf("FAIL: %s = %u (expected %u)\n", expr, result, expected);
        fails = 1;
    } else {
        printf("PASS: %s = %u\n", expr, expected);
    }
}

int main(void)
{
    // 8-bit types
    uint8_t b;
    int8_t a;
    
    // 16-bit signed types
    int c;
    int d;
    
    // 16-bit unsigned types
    uint16_t e;
    uint16_t f;
    
    // Explicit promotion variables
    int int_b;
    int int_a;
    unsigned int uint_b;
    
    b = 200;
    a = -10;
    c = -20000;
    d = 20000;
    e = 45000;
    f = 200;
    
    int_b = b; 
    int_a = a; 
    uint_b = b;

    // ==========================================
    // LONG TESTS (32-bit Promotions)
    // ==========================================
    
    // uint8_t (b = 200) -> promoted to int, then cast to long
    chkl("~b (long)", (long)(~b), -201L);
    chkl("+b (long)", (long)(+b), 200L);
    chkl("-b (long)", (long)(-b), -200L);
    chkl("!b (long)", (long)(!b), 0L);
    
    // int8_t (a = -10) -> promoted to int, then cast to long
    chkl("~a (long)", (long)(~a), 9L);
    chkl("+a (long)", (long)(+a), -10L);
    chkl("-a (long)", (long)(-a), 10L);
    chkl("!a (long)", (long)(!a), 0L);

    // 16-bit signed int (c = -20000) -> cast to long
    chkl("~c (long)", (long)(~c), 19999L);
    chkl("+c (long)", (long)(+c), -20000L);
    chkl("-c (long)", (long)(-c), 20000L);
    chkl("!c (long)", (long)(!c), 0L);

    // 16-bit signed int (d = 20000) -> cast to long
    chkl("~d (long)", (long)(~d), -20001L);
    chkl("+d (long)", (long)(+d), 20000L);
    chkl("-d (long)", (long)(-d), -20000L);
    chkl("!d (long)", (long)(!d), 0L);

    // 16-bit unsigned int (e = 45000) -> cast to long
    chkl("~e (long)", (long)(~e), 20535L);
    chkl("+e (long)", (long)(+e), 45000L);
    chkl("-e (long)", (long)(-e), 20536L);  // <-- Changed from -45000L
    chkl("!e (long)", (long)(!e), 0L);

    // 16-bit unsigned int (f = 200) -> cast to long
    chkl("~f (long)", (long)(~f), 65335L);
    chkl("+f (long)", (long)(+f), 200L);
    chkl("-f (long)", (long)(-f), 65336L);  // <-- Changed from -200L
    chkl("!f (long)", (long)(!f), 0L);

    // ==========================================
    // INT TESTS (16-bit Signed Promotions)
    // ==========================================

    // uint8_t (b = 200) expressions
    chki("~b", ~b, -201);
    chki("+b", +b, 200);
    chki("-b", -b, -200);
    chki("!b", !b, 0);
    
    // int8_t (a = -10) expressions
    chki("~a", ~a, 9);
    chki("+a", +a, -10);
    chki("-a", -a, 10);
    chki("!a", !a, 0);

    // Explicitly promoted uint8_t -> int (int_b = 200)
    chki("~int_b", ~int_b, -201);
    chki("+int_b", +int_b, 200);
    chki("-int_b", -int_b, -200);
    chki("!int_b", !int_b, 0);

    // Explicitly promoted int8_t -> int (int_a = -10)
    chki("~int_a", ~int_a, 9);
    chki("+int_a", +int_a, -10);
    chki("-int_a", -int_a, 10);
    chki("!int_a", !int_a, 0);

    // 16-bit signed int (c = -20000)
    chki("~c", ~c, 19999);
    chki("+c", +c, -20000);
    chki("-c", -c, 20000);
    chki("!c", !c, 0);

    // 16-bit signed int (d = 20000)
    chki("~d", ~d, -20001);
    chki("+d", +d, 20000);
    chki("-d", -d, -20000);
    chki("!d", !d, 0);

    // ==========================================
    // UNSIGNED INT TESTS (16-bit Unsigned Evaluations)
    // ==========================================

    // Explicitly promoted uint8_t -> unsigned int (uint_b = 200)
    chku("~uint_b", ~uint_b, 65335U);
    chku("+uint_b", +uint_b, 200U);
    chku("-uint_b", -uint_b, 65336U);
    chku("!uint_b", !uint_b, 0U);

    // 16-bit unsigned int (e = 45000)
    chku("~e", ~e, 20535U);
    chku("+e", +e, 45000U);
    chku("-e", -e, 20536U);
    chku("!e", !e, 0U);

    // 16-bit unsigned int (f = 200)
    chku("~f", ~f, 65335U);
    chku("+f", +f, 200U);
    chku("-f", -f, 65336U);
    chku("!f", !f, 0U);

    if (fails) return 1;
    
    printf("tunaryp completed with great success\n");
    return 0;
}

