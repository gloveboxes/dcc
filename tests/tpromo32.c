/* tpromo32.c - portable 32-bit-equivalent of tpromo2.
 *
 * tpromo2 asserts dcc's 16-bit int balancing and 32-bit long results via
 * (long) casts, so it diverges on a 32-bit-int / 64-bit-long host and is
 * host-skipped.  This version exercises the same promotion, shift, bitwise,
 * arithmetic, comparison, conditional, assignment and compound-assignment
 * paths, but compares every result as the bit pattern truncated back into a
 * fixed-width stdint type (never via (long)).  Truncation makes dcc's 16-bit
 * wrap and the host's 32-bit wrap converge, so it passes identically under dcc
 * (CP/M) and the host baseline compiler.
 *
 * The one operation that cannot be made portable - a bare relational result
 * that depends on uint16_t/int16_t balancing to a 16- vs 32-bit unsigned int
 * (tpromo2's "d > c") - is intentionally omitted.
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
 * sign-extension to int (16-bit on dcc, 32-bit on the host) never leaks and
 * long width (32-bit on dcc, 64-bit on the host) never matters. */
#define B8(x)  ((unsigned long)(uint8_t)(x))
#define B16(x) ((unsigned long)(uint16_t)(x))
#define B32(x) ((unsigned long)(uint32_t)(x))

int main(void)
{
    int8_t   a = -10;          /* 0xF6 */
    uint8_t  b = 200;          /* 0xC8 */
    int16_t  c = -3000;        /* 0xF448 */
    uint16_t d = 50000U;       /* 0xC350 */
    int32_t  e = 123456L;      /* 0x0001E240 */
    uint32_t f = 4000000000UL; /* 0xEE6B2800 */
    int32_t  g = -1000000L;    /* 0xFFF0BDC0 */

    printf("tpromo32 start\n");
    fails = 0;

    /* ---- shifts (dcc/Z80 uses arithmetic signed right shift) ---- */
    cku("c >> 4",  B16(c >> 4),  65348UL);  /* -188 */
    cku("d >> 4",  B16(d >> 4),  3125UL);
    cku("e >> 4",  B32(e >> 4),  7716UL);
    cku("g >> 4",  B32(g >> 4),  4294904796UL); /* -62500 */
    cku("f >> 8",  B32(f >> 8),  15625000UL);
    cku("b << 1",  B16(b << 1),  400UL);
    cku("d << 2",  B16(d << 2),  3392UL);
    cku("a >> 1",  B8(a >> 1),   251UL);     /* -5 */

    /* ---- bitwise with mixed widths, truncated to the result type ---- */
    cku("f & a",   B32(f & a),   4000000000UL); /* a -> 0xFFFFFFF6, masks nothing */
    cku("f | 0xF", B32(f | 0xFUL), 4000000015UL);
    cku("e & m",   B32(e & 0xFF00UL), 57856UL);
    cku("d | a",   B16(d | a),   65526UL);
    cku("d ^ a",   B16(d ^ a),   15526UL);
    cku("~b",      B8(~b),       55UL);
    cku("a & 0xff",B8(a & 0xff), 246UL);

    /* ---- arithmetic with mixed widths, truncated ---- */
    cku("a + a",   B16(a + a),   65516UL);   /* -20 */
    cku("b + a",   B8(b + a),    190UL);
    cku("b * a",   B16(b * a),   63536UL);   /* -2000 */
    cku("d * b",   B16(d * b),   38528UL);
    cku("c / a",   B16(c / a),   300UL);
    cku("e / d",   B16(e / d),   2UL);
    cku("f + d",   B32(f + d),   4000050000UL);
    cku("f + e",   B32(f + e),   4000123456UL);
    cku("b + 300", B16(b + 300), 500UL);
    cku("(b*a)+c", B16((b * a) + c), 60536UL); /* -5000 */

    /* ---- unary ---- */
    cku("+b",  B8(+b),  200UL);
    cku("-b",  B8(-b),  56UL);    /* (uint8_t)-200 */
    cku("-a",  B8(-a),  10UL);
    cku("~a",  B8(~a),  9UL);
    cku("!a",  (unsigned long)(!a), 0UL);
    cku("-f",  B32(-f), 294967296UL);
    cku("~f",  B32(~f), 294967295UL);
    cku("-g",  B32(-g), 1000000UL);
    cku("~e",  B32(~e), 4294843839UL);

    /* ---- comparisons whose result is width-independent ---- */
    cku("a < b", (unsigned long)(a < b), 1UL);
    cku("f > e", (unsigned long)(f > e), 1UL);
    cku("e < f", (unsigned long)(e < f), 1UL);
    cku("g < e", (unsigned long)(g < e), 1UL);

    /* ---- conditional operator balancing ---- */
    cku("1?d:a", B16(1 ? d : a), 50000UL);
    cku("0?a:f", B32(0 ? a : f), 4000000000UL);
    cku("1?f:e", B32(1 ? f : e), 4000000000UL);
    cku("0?e:g", B32(0 ? e : g), 4293967296UL); /* (uint32_t)-1000000 */

    /* ---- assignment conversion (narrowing) ---- */
    {
        uint8_t  u8;
        int8_t   s8;
        uint16_t u16;
        int16_t  s16;
        u8  = a; cku("u8 = a",  B8(u8),  246UL);
        s8  = b; cku("s8 = b",  B8(s8),  200UL);  /* -56 -> 0xC8 */
        u16 = e; cku("u16 = e", B16(u16), 57920UL);
        s16 = f; cku("s16 = f", B16(s16), 10240UL);
    }

    /* ---- compound assignment: lhs type controls the wrap ---- */
    {
        uint8_t  u8  = 250;
        uint16_t u16 = 60000U;
        uint32_t u32 = 0xFFFFFFF0UL;
        u8  += 10;      cku("u8 += 10",   B8(u8),   4UL);
        u16 += 10000U;  cku("u16 += 10k", B16(u16), 4464UL);
        u32 += 0x20UL;  cku("u32 += 0x20",B32(u32), 16UL);
    }

    if (fails) {
        printf("tpromo32 failed: %d\n", fails);
        return 1;
    }

    printf("tpromo32 completed with great success\n");
    return 0;
}
