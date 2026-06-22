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

    /* right-justify (space) padding: sign counts toward the field width */
    sprintf(b, "%5d", 42);       eq(b, "   42", "rj d");
    sprintf(b, "%6d", -42);      eq(b, "   -42", "rj d neg");
    sprintf(b, "%3d", -42);      eq(b, "-42", "rj d width=len");
    sprintf(b, "%2d", -42);      eq(b, "-42", "rj d width<len");
    sprintf(b, "%5u", 42U);      eq(b, "   42", "rj u");
    sprintf(b, "%6x", 0x2aU);    eq(b, "    2a", "rj x");
    sprintf(b, "%6X", 0x2aU);    eq(b, "    2A", "rj X");
    sprintf(b, "%5ld", 42L);     eq(b, "   42", "rj ld");
    sprintf(b, "%8ld", -42L);    eq(b, "     -42", "rj ld neg");
    sprintf(b, "%6lu", 42UL);    eq(b, "    42", "rj lu");
    sprintf(b, "%6lx", 0x2aUL);  eq(b, "    2a", "rj lx");
    sprintf(b, "%5d", 0);        eq(b, "    0", "rj zero val");

    /* '-' left-justifies integers; sign counts toward the field width */
    sprintf(b, "%-5d", 42);      eq(b, "42   ", "lj d");
    sprintf(b, "%-6d", -42);     eq(b, "-42   ", "lj d neg");
    sprintf(b, "%-3d", -42);     eq(b, "-42", "lj d width=len");
    sprintf(b, "%-2d", -42);     eq(b, "-42", "lj d width<len");
    sprintf(b, "%-5u", 42U);     eq(b, "42   ", "lj u");
    sprintf(b, "%-5x", 0x2aU);   eq(b, "2a   ", "lj x");
    sprintf(b, "%-5X", 0x2aU);   eq(b, "2A   ", "lj X");
    sprintf(b, "%-5ld", 42L);    eq(b, "42   ", "lj ld");
    sprintf(b, "%-8ld", -42L);   eq(b, "-42     ", "lj ld neg");
    sprintf(b, "%-5lu", 42UL);   eq(b, "42   ", "lj lu");
    sprintf(b, "%-5lx", 0x2aUL); eq(b, "2a   ", "lj lx");
    sprintf(b, "%-5d", 0);       eq(b, "0    ", "lj zero val");

    /* '-' suppresses the '0' flag, including for negative values */
    sprintf(b, "%-05d", 42);     eq(b, "42   ", "lj 0 ignored");
    sprintf(b, "%-06d", -42);    eq(b, "-42   ", "lj 0 ignored neg");
    sprintf(b, "%-08ld", -42L);  eq(b, "-42     ", "lj 0 ignored ld");

    /* width + precision: precision zero-fills, width space-pads around it */
    sprintf(b, "%08.3d", 5);     eq(b, "     005", "rj 0 ignored by prec");
    sprintf(b, "%6.3d", 5);      eq(b, "   005", "rj width+prec");
    sprintf(b, "%6.3d", -5);     eq(b, "  -005", "rj width+prec neg");
    sprintf(b, "%-6.3d", 5);     eq(b, "005   ", "lj width+prec");
    sprintf(b, "%-6.3d", -5);    eq(b, "-005  ", "lj width+prec neg");
    sprintf(b, "%8.4lx", 0x2aUL);eq(b, "    002a", "rj width+prec lx");
    sprintf(b, "%-8.4lx", 0x2aUL); eq(b, "002a    ", "lj width+prec lx");

    /* precision of zero with a zero value emits no digits (C89 7.19.6.1) */
    sprintf(b, "%.0d", 0);       eq(b, "", "prec0 zero");
    sprintf(b, "%3.0d", 0);      eq(b, "   ", "prec0 zero rj");
    sprintf(b, "%-3.0d", 0);     eq(b, "   ", "prec0 zero lj");
    sprintf(b, "%.0d", 7);       eq(b, "7", "prec0 nonzero");

    /* precision-only zero fill (no width) */
    sprintf(b, "%.4d", 42);      eq(b, "0042", "prec only");
    sprintf(b, "%.4u", 42U);     eq(b, "0042", "prec only u");
    sprintf(b, "%.4ld", 42L);    eq(b, "0042", "prec only ld");
    sprintf(b, "%.4x", 0x2aU);   eq(b, "002a", "prec only x");
    sprintf(b, "%.4lx", 0x2aUL); eq(b, "002a", "prec only lx");

    if (fails == 0)
        printf("tzpad passed with great success\n");
    else
        printf("tzpad FAILED (%d)\n", fails);
    return fails ? 1 : 0;
}
