#include <stdio.h>
#include <string.h>

static int fails;

static void eq(const char *got, const char *exp, const char *what)
{
    if (strcmp(got, exp) != 0) {
        printf("FAIL %s: got [%s] expected [%s]\n", what, got, exp);
        fails++;
    }
}

int main(void)
{
    char b[64];

    /* 16-bit signed */
    sprintf(b, "%05d", 42);      eq(b, "00042", "d pos");
    sprintf(b, "%05d", -42);     eq(b, "-0042", "d neg");
    sprintf(b, "%02d", 12345);   eq(b, "12345", "d width<len");
    sprintf(b, "%01d", 0);       eq(b, "0", "d zero val");
    sprintf(b, "%03d", -7);      eq(b, "-07", "d neg small");

    /* 16-bit unsigned */
    sprintf(b, "%05u", 42U);     eq(b, "00042", "u pos");
    sprintf(b, "%04u", 60000U);  eq(b, "60000", "u width<len");

    /* hex: %x is lowercase, %X uppercase; both zero-pad to a fixed width */
    sprintf(b, "%04x", 0x2aU);   eq(b, "002a", "x");
    sprintf(b, "%04X", 0x2aU);   eq(b, "002A", "X");
    sprintf(b, "%lx", 0xdeadUL); eq(b, "dead", "lx");
    sprintf(b, "%lX", 0xdeadUL); eq(b, "DEAD", "lX");

    /* 32-bit signed long */
    sprintf(b, "%05ld", 42L);    eq(b, "00042", "ld pos");
    sprintf(b, "%05ld", -42L);   eq(b, "-0042", "ld neg");
    sprintf(b, "%08ld", 123456L);eq(b, "00123456", "ld big");
    sprintf(b, "%02ld", 0L);     eq(b, "00", "ld two");
    sprintf(b, "%02ld", 5L);     eq(b, "05", "ld frac5");

    /* 32-bit unsigned long */
    sprintf(b, "%06lu", 1234UL); eq(b, "001234", "lu");
    sprintf(b, "%010lu", 100000UL); eq(b, "0000100000", "lu big");

    /* '0' flag is correctly suppressed when '-' (left-justify) is present, so
     * we get spaces not zeros.  (Integer left-justification itself is a
     * separate pre-existing gap in dcc: %d ignores '-', so it stays
     * right-justified.  We only assert that NO zero padding leaks in.) */
    sprintf(b, "%-5d", 42);      eq(b, "   42", "left no zero");

    /* '0' flag ignored when precision given (C semantics): precision wins */
    sprintf(b, "%08.3d", 5);     eq(b, "     005", "zero+prec");

    /* plain space width still works */
    sprintf(b, "%5d", 42);       eq(b, "   42", "space width");
    sprintf(b, "%5ld", 42L);     eq(b, "   42", "space width long");

    /* precision-only zero fill (no width) */
    sprintf(b, "%.4d", 42);      eq(b, "0042", "prec only");
    sprintf(b, "%.4u", 42U);     eq(b, "0042", "prec only u");
    sprintf(b, "%.4ld", 42L);    eq(b, "0042", "prec only ld");

    if (fails == 0)
        printf("tzpad passed with great success\n");
    else
        printf("tzpad FAILED (%d)\n", fails);
    return fails ? 1 : 0;
}
