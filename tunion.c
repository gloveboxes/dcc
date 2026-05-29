/* C89 union regression test */
#include <stdio.h>

union Word {
    unsigned int u;
    struct {
        unsigned char lo;
        unsigned char hi;
    } b;
};

union Mixed {
    long l;
    int i;
    char c[sizeof(long)];
};

int main(void)
{
    union Word w;
    union Mixed m;
    int ok;

    ok = 1;

    w.u = 0;
    w.b.lo = 0x34;
    w.b.hi = 0x12;

    if (sizeof(w) < sizeof(unsigned int)) ok = 0;
    if (w.b.lo != 0x34) ok = 0;
    if (w.b.hi != 0x12) ok = 0;

    m.l = 0;
    m.c[0] = 1;

    if (sizeof(m) != sizeof(long)) ok = 0;
    if (m.c[0] != 1) ok = 0;

    m.i = 1234;
    if (m.i != 1234) ok = 0;

    if (!ok) {
        printf("union test failed\n");
        return 1;
    }

    printf("union test passed with great success\n");
    return 0;
}
