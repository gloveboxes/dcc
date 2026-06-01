
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

    if (failures) {
        printf("tlongaudit: %d failure(s)\n", failures);
        return 1;
    }
    printf("tlongaud: all tests passed with great success\n");
    return 0;
}
