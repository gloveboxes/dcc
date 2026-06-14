#include <stdio.h>

struct S {
    unsigned char a;
    int guard;
    unsigned char b;
};

static struct S s[2];
static int fails;

static void checki(const char *name, int got, int exp)
{
    if (got != exp) {
        printf("FAIL %s got %d expected %d\n", name, got, exp);
        fails++;
    }
}

int main(void)
{
    int old;

    s[0].a = 250;
    s[0].guard = 0x1234;
    s[0].b = 10;
    s[1].a = 1;
    s[1].guard = 0x5678;
    s[1].b = 20;

    old = s[0].a++;
    checki("old_a", old, 250);
    checki("new_a", s[0].a, 251);
    checki("guard0", s[0].guard, 0x1234);
    checki("b0", s[0].b, 10);
    checki("next_a", s[1].a, 1);

    old = s[0].b++;
    checki("old_b", old, 10);
    checki("new_b", s[0].b, 11);
    checki("guard0b", s[0].guard, 0x1234);
    checki("next_guard", s[1].guard, 0x5678);

    old = s[0].a--;
    checki("old_ad", old, 251);
    checki("new_ad", s[0].a, 250);
    checki("guard0d", s[0].guard, 0x1234);

    if (fails) {
        printf("tpostfld failed: %d\n", fails);
        return 1;
    }
    printf("tpostfld passed\n");
    return 0;
}
