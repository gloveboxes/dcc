/* C89 goto regression test */
#include <stdio.h>

int main(void)
{
    int i;
    int sum;
    int ok;

    i = 0;
    sum = 0;
    ok = 1;

loop:
    if (i >= 10)
        goto done;

    if (i == 5)
        goto skip;

    sum = sum + i;

skip:
    i = i + 1;
    goto loop;

done:
    if (sum != 40)
        ok = 0;

    i = 0;

outer:
    if (i == 3)
        goto finished;

    i = i + 1;
    goto outer;

finished:
    if (i != 3)
        ok = 0;

    if (!ok) {
        printf("goto test failed\n");
        return 1;
    }

    printf("goto test passed with great success\n");
    return 0;
}
