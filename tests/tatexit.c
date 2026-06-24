#include <stdio.h>
#include <stdlib.h>

/* Verify LIFO order and that all handlers run before output completes. */

static void h1(void) { printf("h1\n"); }
static void h2(void) { printf("h2\n"); }
static void h3(void) { printf("h3\n"); }

/* Registration order: h1, h2, h3  ->  call order: h3, h2, h1 */
int main(void)
{
    int r1, r2, r3;

    r1 = atexit(h1);
    r2 = atexit(h2);
    r3 = atexit(h3);

    if (r1 || r2 || r3)
        printf("FAIL atexit returned nonzero\n");
    else
        printf("tatexit ok\n");

    return 0;
    /* h3, h2, h1 print after this */
}
