#include <stdio.h>
#include <stdint.h>

/*
 * DCC-compatible C89 integer promotion regression test.
 * Avoids printf flags/width/zero-padding in the test harness so failures
 * reflect promotion/codegen bugs, not printf format-support limits.
 */
static int fails;

static void chkl(const char *name, long got, long expected)
{
    if (got != expected) {
        printf("FAIL %s got %ld expected %ld\n", name, got, expected);
        fails++;
    } else {
        printf("PASS %s got %ld\n", name, got);
    }
}

int main(void)
{
    int8_t   a = -10;          /* 0xF6 */
    uint8_t  b = 200;          /* 0xC8 */
    int16_t  c = -3000;        /* 0xF448 */
    uint16_t d = 50000U;       /* 0xC350 */
    int32_t  e = 123456L;      /* 0x0001E240 */
    uint32_t f = 4000000000UL; /* 0xEE6B2800 */

    printf("tpromo2 start\n");
    fails = 0;

    /* signed right shift is implementation-defined in C89.
       DCC/Z80 currently uses arithmetic signed right shift. */
    chkl("c >> 4", (long)(c >> 4), -188L);

    chkl("d >> 4", (long)(d >> 4), 3125L);

    /* uint32_t & int8_t:
       a promotes to int (-10), then converts to uint32_t 0xfffffff6.
       0xee6b2800 & 0xfffffff6 == 0xee6b2800, displayed as signed long. */
    chkl("f & a", (long)(f & a), -294967296L);

    chkl("a + a", (long)(a + a), -20L);
    chkl("b + a", (long)(b + a), 190L);
    chkl("b * a", (long)(b * a), -2000L);
    chkl("d * b", (long)(d * b), 38528L);
    chkl("d | a", (long)(d | a), 65526L);
    chkl("d ^ a", (long)(d ^ a), 15526L);
    chkl("d << 2", (long)(d << 2), 3392L);
    chkl("c / a", (long)(c / a), 300L);
    chkl("e / d", (long)(e / d), 2L);
    chkl("f + d", (long)(f + d), -294917296L);
    chkl("f + e", (long)(f + e), -294843840L);
    chkl("~b", (long)(~b), -201L);
    chkl("(b * a) + c", (long)((b * a) + c), -5000L);

    /* unary operators */
    chkl("+b", (long)(+b), 200L);
    chkl("-b", (long)(-b), -200L);
    chkl("~a", (long)(~a), 9L);
    chkl("!a", (long)(!a), 0L);
    
    /* comparisons after usual arithmetic conversions */
    chkl("a < b", (long)(a < b), 1L);
    /* uint16_t and int16_t balance to unsigned int on a 16-bit-int target. */
    chkl("d > c", (long)(d > c), 0L);
    chkl("f > e", (long)(f > e), 1L);
    chkl("e < f", (long)(e < f), 1L);
    
    /* conditional operator balancing */
    chkl("cond uint16/int8", (long)(1 ? d : a), 50000L);
    chkl("cond int8/uint32", (long)(0 ? a : f), -294967296L);
    
    /* assignment conversion */
    {
        uint8_t u8;
        int8_t s8;
        u8 = a;
        s8 = b;
        chkl("assign int8 to uint8", (long)u8, 246L);
        chkl("assign uint8 to int8", (long)s8, -56L);
    }
    
    /* compound assignment: lhs type matters */
    {
        uint8_t u8;
        u8 = 250;
        u8 += 10;
        chkl("u8 += 10", (long)u8, 4L);
    }
    
    /* shifts: promoted lhs, rhs does not choose result type */
    chkl("b << 1", (long)(b << 1), 400L);
    chkl("a >> 1", (long)(a >> 1), -5L);
    
    /* constants interacting with narrow types */
    chkl("b + 300", (long)(b + 300), 500L);
    chkl("a & 0xff", (long)(a & 0xff), 246L);

    if (fails) {
        printf("tpromo2 failed: %d\n", fails);
        return 1;
    }

    printf("tpromo2 completed with great success\n");
    return 0;
}
