/* ts32.c - portable 32-bit-equivalent of the shift/comparison test ts.
 *
 * ts asserts dcc's exact widths by printing sizeof(long) (4 on dcc, 8 on a
 * 64-bit host) and by printing %x of a plain int (16-bit ffe5 on dcc, 32-bit
 * ffffffe5 on the host), so it is host-skipped.  This version exercises the
 * same shift matrix - simple and compound (>>=, <<=), arithmetic (signed) and
 * logical (unsigned) right shifts, and left-shift truncation across 8-, 16-
 * and 32-bit operands - but compares every result as its bit pattern masked
 * back into a fixed-width stdint type (never via sizeof or a (long)/int print).
 * Masking makes dcc's 16-bit int / 32-bit long and the host's 32-bit int /
 * 64-bit long converge, so it passes identically under dcc (CP/M) and the host
 * baseline compiler.
 *
 * The operations that cannot be made portable - bare relational results that
 * depend on uint16_t/int16_t balancing to a 16- vs 32-bit unsigned int
 * (ts's "i16 == ui16"), and signed-long vs unsigned-long relations that depend
 * on the unsigned-long width (ts's "i32 == ui32") - are intentionally omitted.
 */

#include <stdio.h>
#include <stdint.h>

static int fails;

/* Compare the raw bit pattern of a result, masked to its declared width, so
 * sign-extension to int and long width never affect the comparison. */
#define B8(x)  ((unsigned long)(uint8_t)(x))
#define B16(x) ((unsigned long)(uint16_t)(x))
#define B32(x) ((unsigned long)(uint32_t)(x))

static void cku(const char *name, unsigned long got, unsigned long exp)
{
    if (got != exp) {
        printf("FAIL %s got %lu expected %lu\n", name, got, exp);
        fails++;
    }
}

static void ckb(const char *name, int got, int exp)
{
    if (got != exp) {
        printf("FAIL %s got %d expected %d\n", name, got, exp);
        fails++;
    }
}

int main(void)
{
    int8_t   i8;
    uint8_t  ui8;
    int16_t  i16;
    uint16_t ui16;
    long     i32;
    unsigned long ui32;

    printf("ts32 start\n");
    fails = 0;

    /* ---- right shifts: simple and compound, signed (arithmetic) ---- */
    i8 = -1; i8 = i8 >> 1;   cku("i8 >>1",   B8((uint8_t)i8),   255UL);
    i8 = -1; i8 >>= 1;       cku("i8 >>=1",  B8((uint8_t)i8),   255UL);
    ui8 = 0xff; ui8 = ui8 >> 1; cku("ui8 >>1",  B8(ui8), 127UL);
    ui8 = 0xff; ui8 >>= 1;      cku("ui8 >>=1", B8(ui8), 127UL);

    i16 = -1; i16 = i16 >> 1; cku("i16 >>1",  B16((uint16_t)i16), 65535UL);
    i16 = -1; i16 >>= 1;      cku("i16 >>=1", B16((uint16_t)i16), 65535UL);
    ui16 = 0xffff; ui16 = ui16 >> 1; cku("ui16 >>1",  B16(ui16), 32767UL);
    ui16 = 0xffff; ui16 >>= 1;       cku("ui16 >>=1", B16(ui16), 32767UL);

    i32 = -1L; i32 = i32 >> 1; cku("i32 >>1",  B32(i32), 4294967295UL);
    i32 = -1L; i32 >>= 1;      cku("i32 >>=1", B32(i32), 4294967295UL);
    ui32 = 0xffffffffUL; ui32 = ui32 >> 1; cku("ui32 >>1",  B32(ui32), 2147483647UL);
    ui32 = 0xffffffffUL; ui32 >>= 1;       cku("ui32 >>=1", B32(ui32), 2147483647UL);

    /* ---- left shifts: truncation back into the lhs width ---- */
    i8 = (int8_t)0xff; i8 = i8 << 1;  cku("i8 <<1",   B8((uint8_t)i8),  254UL);
    i8 = (int8_t)0xff; i8 <<= 1;      cku("i8 <<=1",  B8((uint8_t)i8),  254UL);
    ui8 = 0xff; ui8 = ui8 << 1;       cku("ui8 <<1",  B8(ui8), 254UL);
    ui8 = 0xff; ui8 <<= 1;            cku("ui8 <<=1", B8(ui8), 254UL);

    i16 = (int16_t)0xffff; i16 = i16 << 1; cku("i16 <<1",  B16((uint16_t)i16), 65534UL);
    i16 = (int16_t)0xffff; i16 <<= 1;      cku("i16 <<=1", B16((uint16_t)i16), 65534UL);
    ui16 = 0xffff; ui16 = ui16 << 1;       cku("ui16 <<1",  B16(ui16), 65534UL);
    ui16 = 0xffff; ui16 <<= 1;             cku("ui16 <<=1", B16(ui16), 65534UL);

    i32 = 0xffffffffL; i32 = i32 << 1; cku("i32 <<1",  B32(i32), 4294967294UL);
    i32 = 0xffffffffL; i32 <<= 1;      cku("i32 <<=1", B32(i32), 4294967294UL);
    ui32 = 0xffffffffUL; ui32 = ui32 << 1; cku("ui32 <<1",  B32(ui32), 4294967294UL);
    ui32 = 0xffffffffUL; ui32 <<= 1;       cku("ui32 <<=1", B32(ui32), 4294967294UL);

    /* ---- portable comparisons (operands fit identically as int on dcc and
     * the host, so the relational result does not depend on int width) ---- */
    i8 = -2; ui8 = 254;
    ckb("i8 == (int8_t)ui8", i8 == (int8_t)ui8, 1);
    ckb("i8 > ui8",          i8 > ui8,          0);
    ckb("i8 >= (int8_t)ui8", i8 >= (int8_t)ui8, 1);
    ckb("i8 < (int8_t)ui8",  i8 < (int8_t)ui8,  0);
    ckb("i8 <= ui8",         i8 <= ui8,         1);

    i16 = -2;
    ckb("i8 == i16", i8 == i16, 1);
    ckb("i8 > i16",  i8 > i16,  0);
    ckb("i8 >= i16", i8 >= i16, 1);
    ckb("i8 < i16",  i8 < i16,  0);
    ckb("i8 <= i16", i8 <= i16, 1);

    ckb("i8 == 16", i8 == 16, 0);
    ckb("i8 > 16",  i8 > 16,  0);
    ckb("i8 >= 16", i8 >= 16, 0);
    ckb("i8 < 16",  i8 < 16,  1);
    ckb("i8 <= 16", i8 <= 16, 1);

    ckb("i16 == 32", i16 == 32, 0);
    ckb("i16 > 32",  i16 > 32,  0);
    ckb("i16 >= 32", i16 >= 32, 0);
    ckb("i16 < 32",  i16 < 32,  1);
    ckb("i16 <= 32", i16 <= 32, 1);

    i32 = -2L;
    ckb("i32 == 32L", i32 == 32L, 0);
    ckb("i32 > 32L",  i32 > 32L,  0);
    ckb("i32 >= 32L", i32 >= 32L, 0);
    ckb("i32 < 32L",  i32 < 32L,  1);
    ckb("i32 <= 32L", i32 <= 32L, 1);

    if (fails) {
        printf("ts32 failed: %d\n", fails);
        return 1;
    }

    printf("ts32 completed with great success\n");
    return 0;
}
