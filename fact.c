/* finds the factorial of a given number. tests recursion */

#include <stdint.h>
#include <stdio.h>

int32_t factorial( int32_t n )
{
    if ( 0 == n )
        return 1;

    return n * factorial( n - 1 );
} //factorial

int main()
{
    int32_t n = 15;
    printf( "factorial( %lu ) = %lu\n", n, factorial( n ) );
} //main
