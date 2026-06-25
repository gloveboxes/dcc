/* tln2.c: C89 integer spigot-ish ln(2), 100 fractional digits */
/* 0.6931471805599453094172321214581765680755001343602552541206800094933936219696947156058633269964186875 */
#include <stdio.h>

#define NDIG 100
#define GUARD 16
#define BASE 10000L
#define WIDTH 4
#define NBLOCKS ((NDIG + GUARD + WIDTH - 1) / WIDTH)

static void add(long a[], const long b[])
{
    int i;
    long carry = 0;

    for (i = NBLOCKS - 1; i >= 0; --i) {
        long v = a[i] + b[i] + carry;
        if (v >= BASE) {
            v -= BASE;
            carry = 1;
        } else {
            carry = 0;
        }
        a[i] = v;
    }
}

static int is_zero(const long a[])
{
    int i;
    for (i = 0; i < NBLOCKS; ++i)
        if (a[i] != 0)
            return 0;
    return 1;
}

static void mul_div(long a[], long mul, long div)
{
    int i;
    long carry = 0;
    long rem = 0;

    for (i = NBLOCKS - 1; i >= 0; --i) {
        long v = a[i] * mul + carry;
        a[i] = v % BASE;
        carry = v / BASE;
    }

    for (i = 0; i < NBLOCKS; ++i) {
        long v = rem * BASE + a[i];
        a[i] = v / div;
        rem = v % div;
    }
}

int main(void)
{
    long sum[NBLOCKS], term[NBLOCKS];
    int i, k, printed;

    for (i = 0; i < NBLOCKS; ++i) {
        sum[i] = 0;
        term[i] = 0;
    }

    /* term = 2/3 */
    {
        long rem = 2;
        for (i = 0; i < NBLOCKS; ++i) {
            rem *= BASE;
            term[i] = rem / 3;
            rem %= 3;
        }
    }

    /* ln(2) = 2 * atanh(1/3)
       = 2 * (1/3 + 1/(3*3^3) + 1/(5*3^5) + ...) */
    k = 0;
    while (!is_zero(term)) {
        add(sum, term);
        mul_div(term, 2L * k + 1L, 9L * (2L * k + 3L));
        ++k;
    }

    printf("0.");
    printed = 0;
    for (i = 0; i < NBLOCKS && printed < NDIG; ++i) {
        int d;
        long p = BASE / 10;
        while (p > 0 && printed < NDIG) {
            d = (int)((sum[i] / p) % 10);
            putchar('0' + d);
            p /= 10;
            ++printed;
        }
    }
    putchar('\n');

    return 0;
}