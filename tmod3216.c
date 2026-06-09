#include <stdio.h>
#include <stdint.h>

static int fails;

static void chkul(const char *name, uint32_t got, uint32_t exp)
{
    if (got != exp) {
        printf("FAIL %s got %lu expected %lu\n", name, got, exp);
        fails++;
    }
}

static uint32_t mod32_16(uint32_t x, uint16_t m)
{
    return x % m;
}

int main(void)
{
    uint32_t a;
    uint16_t m;

    m = 7;
    a = 65536UL;
    chkul("65536_mod_7", mod32_16(a, m), 2UL);

    m = 257;
    a = 65536UL;
    chkul("65536_mod_257", mod32_16(a, m), 1UL);

    m = 1000;
    a = 123456789UL;
    chkul("123456789_mod_1000", mod32_16(a, m), 789UL);

    m = 65535U;
    a = 0x12345678UL;
    chkul("hex_mod_65535", mod32_16(a, m), 0x68ACUL);

    if (fails) {
        printf("tmod32_16 failed: %d\n", fails);
        return 1;
    }
    printf("tmod32_16 completed\n");
    return 0;
}
