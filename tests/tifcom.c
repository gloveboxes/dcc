#include <stdio.h>

#if 1 /* active branch must remain active */
#define PICK() 123
#else
static int PICK(void)
{
    return 456;
}
#endif

int main(void)
{
    printf("%d\n", PICK());
    return 0;
}
