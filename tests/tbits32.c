/* tbits32.c - portable 32-bit-equivalent of tbits.
 *
 * tbits prints bitwise results via %x, whose width-dependent sign-extension to
 * int (fff8 on dcc vs fffffff8 on a 32-bit host) makes it host-skipped.  This
 * version self-checks the bitwise/shift operators on fixed-width stdint types
 * and compares the bit pattern truncated to each type's width, so dcc and the
 * host agree exactly.
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

#define B8(x)  ((unsigned long)(uint8_t)(x))
#define B16(x) ((unsigned long)(uint16_t)(x))
#define B32(x) ((unsigned long)(uint32_t)(x))

int main(void)
{
    uint8_t  a8 = 0xF0U, b8 = 0x3CU;
    int8_t   s8 = -8;                    /* 0xF8 */
    uint16_t a16 = 0xF00FU, b16 = 0x0FF0U;
    int16_t  s16 = -8;                   /* 0xFFF8 */
    uint32_t a32 = 0xF0F0F0F0UL, b32 = 0x0FF00FF0UL;
    int32_t  s32 = 0x12345678L;

    /* 8-bit bitwise */
    cku("u8 and", B8(a8 & b8), 0x30UL);
    cku("u8 or",  B8(a8 | b8), 0xFCUL);
    cku("u8 xor", B8(a8 ^ b8), 0xCCUL);
    cku("u8 ~",   B8(~a8),     0x0FUL);
    cku("s8 ~",   B8(~s8),     0x07UL);  /* ~0xF8 = 0x07 */
    cku("u8 shl", B8(a8 << 1), 0xE0UL);  /* truncated to 8 bits */
    cku("u8 shr", B8(a8 >> 4), 0x0FUL);

    /* 16-bit bitwise */
    cku("u16 and", B16(a16 & b16), 0x0000UL);
    cku("u16 or",  B16(a16 | b16), 0xFFFFUL);
    cku("u16 xor", B16(a16 ^ b16), 0xFFFFUL);
    cku("u16 ~",   B16(~a16),      0x0FF0UL);
    cku("s16 ~",   B16(~s16),      0x0007UL);  /* ~0xFFF8 = 0x0007 */
    cku("u16 shl", B16(a16 << 4),  0x00F0UL);  /* truncated to 16 bits */
    cku("u16 shr", B16(a16 >> 8),  0x00F0UL);

    /* 32-bit bitwise */
    cku("u32 and", B32(a32 & b32), 0x00F000F0UL);
    cku("u32 or",  B32(a32 | b32), 0xFFF0FFF0UL);
    cku("u32 xor", B32(a32 ^ b32), 0xFF00FF00UL);
    cku("u32 ~",   B32(~a32),      0x0F0F0F0FUL);
    cku("u32 shl", B32(a32 << 4),  0x0F0F0F00UL);  /* truncated to 32 bits */
    cku("u32 shr", B32(a32 >> 4),  0x0F0F0F0FUL);
    cku("s32 ~",   B32(~s32),      0xEDCBA987UL);
    cku("s32 shr", B32((uint32_t)0xFFFFFFFFUL >> 28), 0x0000000FUL);

    if (fails) {
        printf("tbits32: %d failure(s)\n", fails);
        return 1;
    }
    printf("tbits32: all tests passed\n");
    return 0;
}
