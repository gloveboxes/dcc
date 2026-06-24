#include <stdio.h>
#include <assert.h>

int main(void)
{
    /* passing assertions: no output, no abort */
    assert(1);
    assert(1 + 1 == 2);

    printf("tassert ok\n");

    /* failing assertion: should print "Assertion failed: 0 at ..." and abort */
    assert(0);
    printf("FAIL assert: returned after assert(0)\n");
    return 1;
}
