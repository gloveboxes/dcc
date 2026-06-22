/* ttype32.c - portable 32-bit-equivalent of the type-size tests
 * (ttype2 / ttypesr / tc89flt struct sizing).
 *
 * Those tests assert dcc's 16-bit int and 16-bit pointer sizes, which no host
 * ABI matches, so they are host-skipped.  This version asserts only sizes that
 * are identical on dcc and the host: the fixed-width stdint types, arrays of
 * them, and structs composed solely of one fixed-width type (which require no
 * inter-member padding on either ABI).  Pointer and mixed-alignment struct
 * sizes are intentionally omitted because they are inherently ABI-specific.
 */

#include <stdio.h>
#include <stdint.h>

static int fails;

static void cksz(const char *name, unsigned long got, unsigned long exp)
{
    if (got != exp) {
        printf("FAIL %s got %lu expected %lu\n", name, got, exp);
        fails++;
    }
}

typedef int32_t  I32A4[4];
typedef uint32_t U32A2x3[2][3];
typedef int16_t  S16A2x3x4[2][3][4];
typedef int32_t  I32A2x3x2x2[2][3][2][2];
typedef uint8_t  U8A2x2x2x2x3[2][2][2][2][3];

struct TwoI32 {
    int32_t a;
    int32_t b;
};

struct FourU8 {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
};

int main(void)
{
    cksz("sizeof int8_t",   (unsigned long)sizeof(int8_t),   1UL);
    cksz("sizeof uint8_t",  (unsigned long)sizeof(uint8_t),  1UL);
    cksz("sizeof int16_t",  (unsigned long)sizeof(int16_t),  2UL);
    cksz("sizeof uint16_t", (unsigned long)sizeof(uint16_t), 2UL);
    cksz("sizeof int32_t",  (unsigned long)sizeof(int32_t),  4UL);
    cksz("sizeof uint32_t", (unsigned long)sizeof(uint32_t), 4UL);

    cksz("sizeof int32_t[4]",    (unsigned long)sizeof(int32_t[4]),    16UL);
    cksz("sizeof uint32_t[2][3]",(unsigned long)sizeof(uint32_t[2][3]),24UL);
    cksz("sizeof I32A4",         (unsigned long)sizeof(I32A4),         16UL);

    /* Multidimensional array typedefs flatten to element_size * product of
     * all dimensions (regression: dcc previously kept only the first dim). */
    cksz("sizeof U32A2x3",       (unsigned long)sizeof(U32A2x3),       24UL);
    cksz("sizeof S16A2x3x4",     (unsigned long)sizeof(S16A2x3x4),     48UL);
    cksz("sizeof I32A2x3x2x2",   (unsigned long)sizeof(I32A2x3x2x2),   96UL);
    cksz("sizeof U8A2x2x2x2x3",  (unsigned long)sizeof(U8A2x2x2x2x3),  48UL);

    /* Structs of a single fixed-width type need no padding on any ABI. */
    cksz("sizeof struct TwoI32", (unsigned long)sizeof(struct TwoI32), 8UL);
    cksz("sizeof struct FourU8", (unsigned long)sizeof(struct FourU8), 4UL);

    if (fails) {
        printf("ttype32: %d failure(s)\n", fails);
        return 1;
    }
    printf("ttype32: all tests passed\n");
    return 0;
}
