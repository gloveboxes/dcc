/* C89 setjmp/longjmp regression test */
#include <stdio.h>
#include <setjmp.h>

static jmp_buf env;
static int marker;

static void jump1(void)
{
    marker = 2;
    longjmp(env, 7);
}

int main(void)
{
    int r;
    int ok;

    marker = 1;
    ok = 1;

    r = setjmp(env);

    if (r == 0) {
        if (marker != 1) ok = 0;
        jump1();
        ok = 0;
    } else {
        if (r != 7) ok = 0;
        if (marker != 2) ok = 0;
    }

    if (!ok) {
        printf("setjmp test failed\n");
        return 1;
    }

    printf("setjmp test passed with great success\n");
    return 0;
}