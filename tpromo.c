/*
 * tpromo.c - integer/float promotion and demotion regression test for dcc
 *
 * Exercises:
 *   - assignment conversions among int8_t/uint8_t/int16_t/uint16_t/int32_t/uint32_t/float
 *   - integer promotions for 8-bit operands
 *   - usual arithmetic conversions involving signed/unsigned 16-bit and 32-bit operands
 *   - truncation/wrap on demotion
 *   - sign/zero extension on promotion
 *   - float conversions to/from integer types
 */

#include <stdio.h>
#include <stdint.h>

static int failures = 0;

static void ck_i(const char *name, long got, long exp)
{
    if (got != exp) {
        printf("FAIL %s got %ld expected %ld\n", name, got, exp);
        failures++;
    }
}

static void ck_u(const char *name, unsigned long got, unsigned long exp)
{
    if (got != exp) {
        printf("FAIL %s got %lu expected %lu\n", name, got, exp);
        failures++;
    }
}

static void ck_f(const char *name, float got, float exp)
{
    float d;

    d = got - exp;
    if (d < 0.0f)
        d = -d;

    if (d > 0.001f) {
        printf("FAIL %s got %f expected %f\n", name, got, exp);
        failures++;
    }
}

static void test_assignment_conversions(void)
{
    int8_t s8;
    uint8_t u8;
    int16_t s16;
    uint16_t u16;
    int32_t s32;
    uint32_t u32;
    float f;

    printf("assignment conversions\n");

    s8 = -1;
    ck_i("s8=-1", s8, -1);

    u8 = s8;
    ck_u("s8->u8", u8, 255UL);

    s16 = s8;
    ck_i("s8->s16", s16, -1);

    u16 = s8;
    ck_u("s8->u16", u16, 65535UL);

    s32 = s8;
    ck_i("s8->s32", s32, -1);

    u32 = s8;
    ck_u("s8->u32", u32, 4294967295UL);

    u8 = 255;
    ck_u("u8=255", u8, 255UL);

    s8 = u8;
    ck_i("u8->s8", s8, -1);

    s16 = u8;
    ck_i("u8->s16", s16, 255);

    u16 = u8;
    ck_u("u8->u16", u16, 255UL);

    s32 = u8;
    ck_i("u8->s32", s32, 255);

    u32 = u8;
    ck_u("u8->u32", u32, 255UL);

    s16 = -32768;
    ck_i("s16=-32768", s16, -32768L);

    u16 = s16;
    ck_u("s16->u16", u16, 32768UL);

    s32 = s16;
    ck_i("s16->s32", s32, -32768L);

    u32 = s16;
    ck_u("s16->u32", u32, 4294934528UL);

    u16 = 65535U;
    ck_u("u16=65535", u16, 65535UL);

    s16 = u16;
    ck_i("u16->s16", s16, -1L);

    s32 = u16;
    ck_i("u16->s32", s32, 65535L);

    u32 = u16;
    ck_u("u16->u32", u32, 65535UL);

    s32 = -2147483647L - 1L;
    ck_i("s32 min", s32, -2147483647L - 1L);

    u32 = s32;
    ck_u("s32min->u32", u32, 2147483648UL);

    u32 = 4294967295UL;
    ck_u("u32 max", u32, 4294967295UL);

    s32 = u32;
    ck_i("u32max->s32", s32, -1L);

    s8 = 0x1234;
    ck_i("int->s8 trunc", s8, 52);

    u8 = 0x1234;
    ck_u("int->u8 trunc", u8, 52UL);

    s16 = 0x12345678L;
    ck_i("long->s16 trunc", s16, 22136L);

    u16 = 0x12345678UL;
    ck_u("ulong->u16 trunc", u16, 22136UL);

    f = 123.0f;
    s8 = f;
    ck_i("float->s8", s8, 123);

    f = 255.0f;
    u8 = f;
    ck_u("float->u8", u8, 255UL);

    f = -12345.0f;
    s16 = f;
    ck_i("float->s16", s16, -12345L);

    f = 65535.0f;
    u16 = f;
    ck_u("float->u16", u16, 65535UL);

    f = -123456.0f;
    s32 = f;
    ck_i("float->s32", s32, -123456L);

    f = 300000.0f;
    u32 = f;
    ck_u("float->u32", u32, 300000UL);

    s16 = -300;
    f = s16;
    ck_f("s16->float", f, -300.0f);

    u16 = 60000U;
    f = u16;
    ck_f("u16->float", f, 60000.0f);

    s32 = -100000L;
    f = s32;
    ck_f("s32->float", f, -100000.0f);

    u32 = 200000UL;
    f = u32;
    ck_f("u32->float", f, 200000.0f);
}

static void test_integer_promotions(void)
{
    int8_t s8a;
    int8_t s8b;
    uint8_t u8a;
    uint8_t u8b;
    int16_t s16;
    uint16_t u16;

    printf("integer promotions\n");

    s8a = 120;
    s8b = 10;
    ck_i("s8+s8 promotes", s8a + s8b, 130L);

    s8a = -120;
    s8b = -10;
    ck_i("negative s8+s8 promotes", s8a + s8b, -130L);

    u8a = 250;
    u8b = 10;
    ck_i("u8+u8 promotes", u8a + u8b, 260L);

    u8a = 255;
    ck_i("u8+1 promotes", u8a + 1, 256L);
    ck_u("u8+1UL promotes", u8a + 1UL, 256UL);

    s8a = -1;
    u8a = 255;
    ck_i("s8+u8 promotes", s8a + u8a, 254L);

    u8a = 200;
    s16 = -300;
    ck_i("u8+s16", u8a + s16, -100L);

    u16 = 65535U;
    ck_u("u16+1U wraps 16", (uint16_t)(u16 + 1U), 0UL);
    ck_u("u16+1UL promotes long", u16 + 1UL, 65536UL);
}

static void test_usual_arithmetic_conversions(void)
{
    int16_t s16;
    uint16_t u16;
    int32_t s32;
    uint32_t u32;
    float f;

    printf("usual arithmetic conversions\n");

    s16 = -1;
    u16 = 1U;
    ck_u("s16<u16 unsigned compare", (unsigned long)(s16 < u16), 0UL);
    ck_u("s16>u16 unsigned compare", (unsigned long)(s16 > u16), 1UL);
    ck_u("s16+u16 unsigned result", s16 + u16, 0UL);

    s32 = -1L;
    u32 = 1UL;
    ck_u("s32<u32 unsigned compare", (unsigned long)(s32 < u32), 0UL);
    ck_u("s32>u32 unsigned compare", (unsigned long)(s32 > u32), 1UL);
    ck_u("s32+u32 unsigned result", s32 + u32, 0UL);

    u32 = 0x80000000UL;
    s32 = 1L;
    ck_u("u32>s32 high bit", (unsigned long)(u32 > s32), 1UL);

    u16 = 65535U;
    s32 = 1L;
    ck_i("u16+s32 to signed long", u16 + s32, 65536L);

    s16 = -2;
    u32 = 1UL;
    ck_u("s16+u32 to ulong", s16 + u32, 4294967295UL);

    s32 = -100000L;
    f = s32 + 0.5f;
    ck_f("s32+float", f, -99999.5f);

    u16 = 60000U;
    f = u16 + 0.25f;
    ck_f("u16+float", f, 60000.25f);

    u32 = 200000UL;
    f = u32 + 0.75f;
    ck_f("u32+float", f, 200000.75f);
}

static void test_demotions_after_operations(void)
{
    int8_t s8;
    uint8_t u8;
    int16_t s16;
    uint16_t u16;
    int32_t s32;
    uint32_t u32;

    printf("demotions after operations\n");

    s8 = 120 + 10;
    ck_i("130->s8", s8, -126L);

    u8 = 250 + 10;
    ck_u("260->u8", u8, 4UL);

    s16 = 32760 + 10;
    ck_i("32770->s16", s16, -32766L);

    u16 = 65530U + 10U;
    ck_u("65540->u16", u16, 4UL);

    s32 = 2147483647L + 1UL;
    ck_i("2147483648->s32", s32, -2147483647L - 1L);

    u32 = -1L;
    ck_u("-1L->u32", u32, 4294967295UL);

    s8 = (int8_t)(uint32_t)0xffffffffUL;
    ck_i("u32max cast s8", s8, -1L);

    u8 = (uint8_t)(int32_t)-129L;
    ck_u("-129 cast u8", u8, 127UL);
}

static void test_function_arguments_and_returns(void)
{
    int8_t s8;
    uint8_t u8;
    int16_t s16;
    uint16_t u16;
    int32_t s32;
    uint32_t u32;
    float f;

    printf("vararg/function conversion sanity\n");

    s8 = -1;
    u8 = 255;
    s16 = -1;
    u16 = 65535U;
    s32 = -1L;
    u32 = 4294967295UL;
    f = 12.5f;

    ck_i("vararg s8", s8, -1L);
    ck_i("vararg u8 promotes int", u8, 255L);
    ck_i("vararg s16", s16, -1L);
    ck_u("vararg u16", u16, 65535UL);
    ck_i("vararg s32", s32, -1L);
    ck_u("vararg u32", u32, 4294967295UL);
    ck_f("float value", f, 12.5f);
}

int main(void)
{
    test_assignment_conversions();
    test_integer_promotions();
    test_usual_arithmetic_conversions();
    test_demotions_after_operations();
    test_function_arguments_and_returns();

    if (failures) {
        printf("tpromo: %d failure(s)\n", failures);
        return 1;
    }

    printf("tpromo completed with great success\n");
    return 0;
}
