/* validates argc and argv work */

#include <stdio.h>

int main( int argc, char * argv[] )
{
    printf( "argc: %d\n", argc );

    for ( int i = 0; i < argc; i++ )
        printf( "argv[ %d ]: '%s'\n", i, argv[ i ] );

    printf( "targs complted with great success\n" );
    return 0;
} // main

