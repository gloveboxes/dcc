#include <stdio.h>

void main()
{
    printf("__DATE__ %s\n", __DATE__);
    printf("__TIME__ %s\n", __TIME__);
    printf("__FILE__ %s\n", __FILE__);
    printf("__LINE__ %d\n", __LINE__);
    printf("__STDC__ %d\n", __STDC__);

    printf( "test tstdc completed with great success\n" );
}

