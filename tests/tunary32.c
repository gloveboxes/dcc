/* tunary32.c - portable 32-bit-equivalent of tunaryp.
 *
 * tunaryp asserts dcc's 16-bit int unary-operator results, which differ from a
 * host's 32-bit int, so it is host-skipped.  This version exercises the same
 * operators but compares the value truncated back into a fixed-width stdint
 * type, so dcc's 16-bit wrap and a host's 32-bit wrap converge to the same
 * answer.  It therefore passes identically under dcc (CP/M) and the host
 * compiler used for baseline validation.
 */

#include <stdio.h>
#include <stdint.h>

static int fails;

static void cku(const char *name, unsigned long got, unsigned long exp)
{
    if (got != exp) {
        printf("FAIL %s got %lu expected %lu\n", name, got, exp);
        fails++;
    }
}

/* Compare the raw bit pattern of a result, masked to its declared width, so
 * sign-extension to int (16-bit on dcc, 32-bit on the host) never leaks. */
#define B8(x)  ((unsigned long)(uint8_t)(x))
#define B16(x) ((unsigned long)(uint16_t)(x))
#define B32(x) ((unsigned long)(uint32_t)(x))

int main(void)
{
    uint8_t  u8  = 200;
    int8_t   s8  = -10;
    uint16_t u16 = 45000U;
    int16_t  s16 = -20000;
    uint32_t u32 = 3000000000UL;
    int32_t  s32 = -1000000L;

    /* uint8_t: results wrap mod 256 */
    cku("~u8", B8(~u8), 55UL);          /* ~0xC8 = 0x37 */
    cku("-u8", B8(-u8), 56UL);          /* 256 - 200 */
    cku("+u8", B8(+u8), 200UL);
    cku("!u8", (unsigned long)(!u8), 0UL);

    /* int8_t: result stored in the 8-bit object wraps mod 256 */
    cku("~s8", B8(~s8), 9UL);           /* ~-10 = 9 */
    cku("-s8", B8(-s8), 10UL);
    cku("+s8", B8(+s8), 246UL);         /* (uint8_t)-10 */
    cku("!s8", (unsigned long)(!s8), 0UL);

    /* uint16_t: wrap mod 65536 (the classic dcc 16-bit case) */
    cku("~u16", B16(~u16), 20535UL);    /* ~0xAFC8 = 0x5037 */
    cku("-u16", B16(-u16), 20536UL);    /* 65536 - 45000 */
    cku("+u16", B16(+u16), 45000UL);
    cku("!u16", (unsigned long)(!u16), 0UL);

    /* int16_t */
    cku("~s16", B16(~s16), 19999UL);    /* ~-20000 = 19999 */
    cku("-s16", B16(-s16), 20000UL);
    cku("+s16", B16(+s16), 45536UL);    /* (uint16_t)-20000 */
    cku("!s16", (unsigned long)(!s16), 0UL);

    /* uint32_t: wrap mod 2^32 */
    cku("~u32", B32(~u32), 1294967295UL);   /* 2^32-1 - 3000000000 */
    cku("-u32", B32(-u32), 1294967296UL);   /* 2^32 - 3000000000 */
    cku("+u32", B32(+u32), 3000000000UL);
    cku("!u32", (unsigned long)(!u32), 0UL);

    /* int32_t */
    cku("~s32", B32(~s32), 999999UL);       /* ~-1000000 = 999999 */
    cku("-s32", B32(-s32), 1000000UL);
    cku("+s32", B32(+s32), 4293967296UL);   /* (uint32_t)-1000000 */
    cku("!s32", (unsigned long)(!s32), 0UL);

    if (fails) {
        printf("tunary32: %d failure(s)\n", fails);
        return 1;
    }
    printf("tunary32: all tests passed\n");
    return 0;
}
