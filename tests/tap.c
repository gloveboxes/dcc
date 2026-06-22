#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define __max( a, b ) (a) > (b) ? a : b
#define __min( a, b ) (a) < (b) ? a : b

uint32_t gcd( uint32_t m, uint32_t n )
{
    uint32_t a = 0;
    uint32_t b = __max( m, n );
    uint32_t r = __min( m, n );

    while ( 0 != r )
    {
        a = b;
        b = r;
        r = a % b;
    }

    return b;
}

uint32_t rand_ui32()
{
    return rand() | ( ( (uint32_t) rand() ) << 16 );
} 

/* https://en.wikipedia.org/wiki/Ap%C3%A9ry%27s_theorem */

void first_implementation()
{
    uint32_t total = 1000;
    uint32_t i, iq;
    float sofar = 0.0;
    uint32_t prev = 1;

    for ( i = 1; i <= total; i++ )
    {
        iq = i * i * i;
        sofar += (float) 1.0 / (float) ( iq );
        /*printf( "i, iq, and sofar: %lu, %lu, %lf\n", i, iq, sofar );*/

        if ( i == ( prev * 10 ) )
        {
            prev = i;
            printf( "  at %12lu iterations: %f\n", i, sofar );
            fflush( stdout );
        }
    }
} 

int main()
{
    printf( "starting, should tend towards 1.2020569031595942854...\n" );

    printf( "first implementation...\n" );
    first_implementation();

    printf( "second implementation...\n" );

    const uint32_t totalEntries = 1000; /* 100000 is too slow; */
    uint32_t totalCoprimes = 0;
    uint32_t prev = 1;
    uint32_t rsf = 0;

    for ( uint32_t i = 1; i <= totalEntries; i++ )
    {
        uint32_t a = rand_ui32();
        uint32_t b = rand_ui32();
        uint32_t c = rand_ui32();

        uint32_t greatest = gcd( a, gcd( b, c ) );
        if ( 1 == greatest )
            totalCoprimes++;

        if ( i == ( prev * 10 ) )
        {
            prev = i;
            float v = (float) i / (float) totalCoprimes;
            printf( "  at %12lu iterations: %f\n", i, v );
            fflush( stdout );
        }
    }

    printf( "done\n" );
    return 1202;
} 
