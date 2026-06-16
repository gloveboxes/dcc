#include <stdio.h>

int main( int argc, char * argv[] )
{
    while ( !kbhit() )
        continue;
    int x = getch();
    if ( 'x' != x )
    {
        printf( "this test requires an 'x' for the test to pass and you pressed '%c'\n", x );
        return 1;
    }
    printf( "tkbd completed with great success\n" );
    return 0;
}

