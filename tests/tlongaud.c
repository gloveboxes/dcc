
#include <stdio.h>

static int failures;

void ck(const char *name, unsigned long got, unsigned long exp)
{
    if (got != exp) {
        printf("FAIL %s got %lu expected %lu\n", name, got, exp);
        failures++;
    }
}

int main(void)
{
    unsigned long ul;
    long sl;
    int r;

    ul = 0x00010000UL;   /* HL low word is zero, DE high word nonzero */
    r = (ul && 1);
    ck("land lhs high-only", (unsigned long)r, 1UL);

    r = (1 && ul);
    ck("land rhs high-only", (unsigned long)r, 1UL);

    r = (ul || 0);
    ck("lor lhs high-only", (unsigned long)r, 1UL);

    r = (0 || ul);
    ck("lor rhs high-only", (unsigned long)r, 1UL);

    ul = 0x0000ffffUL;
    ++ul;
    ck("preinc carry", ul, 0x00010000UL);

    --ul;
    ck("predec borrow", ul, 0x0000ffffUL);


    ul = 0x00010000UL;
    if (ul)
        ck("if high-only", 1UL, 1UL);
    else
        ck("if high-only", 0UL, 1UL);

    {
        int cnt;
        cnt = 0;
        while (ul && cnt == 0)
            cnt++;
        ck("while high-only", (unsigned long)cnt, 1UL);
    }

    {
        int cnt;
        cnt = 0;
        for (; ul && cnt == 0; )
            cnt++;
        ck("for high-only", (unsigned long)cnt, 1UL);
    }

    {
        int cnt;
        cnt = 0;
        do {
            cnt++;
            ul = 0UL;
        } while (ul);
        ck("do high-only exits", (unsigned long)cnt, 1UL);
    }

    ul = 0xffffffffUL;
    ++ul;
    ck("preinc wrap", ul, 0UL);

    --ul;
    ck("predec wrap", ul, 0xffffffffUL);

    sl = -1L;
    ++sl;
    ck("preinc signed", (unsigned long)sl, 0UL);

    /* mixed signed/unsigned conversions */

    ck("ul==-1L",
       (unsigned long)(0xffffffffUL == -1L),
       1UL);

    ck("ul>-1L",
       (unsigned long)(0xffffffffUL > -1L),
       0UL);

    ck("high-bit unsigned cmp",
       (unsigned long)(0x80000000UL > 0x7fffffffUL),
       1UL);

    /* unsigned divide/mod */

    ul = 0x80000000UL;
    ck("udiv high", ul / 2UL, 0x40000000UL);

    ul = 0x80000001UL;
    ck("umod high", ul % 2UL, 1UL);

    /* unsigned wrap */

    ul = 0xffffffffUL;
    ul += 2UL;
    ck("ul add wrap", ul, 1UL);

    /* unsigned shifts */

    ul = 0x80000000UL;
    ul >>= 31;
    ck("ul shr assign", ul, 1UL);

    ul = 1UL;
    ul <<= 31;
    ck("ul shl assign", ul, 0x80000000UL);

    /* signed/unsigned casts */

    ck("cast -2 to ul",
       (unsigned long)-2L,
       0xfffffffeUL);

    ck("cast ul high preserve",
       (unsigned long)(long)0x80000000UL,
       0x80000000UL);

    ul = 0x80000000UL;
    if (ul > 0L)
        ck("mixed ul>0L", 1UL, 1UL);
    else
        ck("mixed ul>0L", 0UL, 1UL);

    ck("cf add", 100000L + 200000L, 300000L);
    ck("cf sub", 100000L - 200000L, (unsigned long)-100000L);
    ck("cf mul", 30000L * 10L, 300000L);
    ck("cf shl", 1UL << 31, 0x80000000UL);

    unsigned char uc = 255;
    unsigned int ui = 65535;
    unsigned long ul;

    ul = uc;
    ck("uc->ul", ul, 255UL);

    ul = ui;
    ck("ui->ul", ul, 65535UL);

    ck("uc+ul", uc + 1UL, 256UL);
    ck("ui+ul", ui + 1UL, 65536UL);

    if (failures) {
        printf("tlongaudit: %d failure(s)\n", failures);
        return 1;
    }
    printf("tlongaud: all tests passed with great success\n");
    return 0;
}
