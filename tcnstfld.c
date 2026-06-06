#include <stdio.h>
#include <stdint.h>

static int failures;
void ck(const char *name, unsigned long got, unsigned long exp) {
    if (got != exp) { printf("FAIL %s got %lu expected %lu\n", name, got, exp); failures++; }
}

int main(void) {
    int a[3];
    unsigned char uc;
    unsigned int ui;
    unsigned long ul;
    long sl;

    ck("add16", 1000 + 2000, 3000UL);
    ck("muldiv", (30 * 10) / 3, 100UL);
    ck("mod", 1000 % 26, 12UL);
    ck("shift", 1UL << 31, 0x80000000UL);
    ck("ushr", 0x80000000UL >> 31, 1UL);
    ck("bit", (0x1234 & 0xff) | 0x100, 0x134UL);
    ck("rel", (100000L > 99999L), 1UL);
    ck("mixeq", (0xffffffffUL == -1L), 1UL);
    ck("mixgt", (0xffffffffUL > -1L), 0UL);
    ck("sizeof", sizeof(a) / sizeof(a[0]), 3UL);
    ck("sizeof expr", sizeof(uc + 1UL), 4UL);

    uc = (unsigned char)(255 + 1); /* folded cast to uchar */
    ck("uchar cast", uc, 0UL);
    ui = (unsigned int)(0xffffUL + 2UL);
    ck("uint cast", ui, 1UL);
    ul = (unsigned long)(100000L + 200000L);
    ck("long add", ul, 300000UL);
    sl = (long)(0 - 100000L);
    ck("long neg", (unsigned long)sl, (unsigned long)-100000L);

    if (failures) { printf("tconstfold %d failure(s)\n", failures); return 1; }
    printf("tcnstfld passed with great success\n");
    return 0;
}
