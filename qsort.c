#include <stddef.h>
#include <stdlib.h>

static void memswap( char *a, char *b, size_t s )
{
    for ( size_t i = 0; i < s; i++ )
    {
        char t = *a;
        *a = *b;
        *b = t;
        a++;
        b++;
    }
} //memswap

void qsort( void * vbase, size_t num, size_t size, int (*compare)( const void * a, const void * b ) )
{
    if ( 0 == num )
        return;

    char * base = (char *) vbase;
    char * max;
    char * last = max = base + ( num - 1 ) * size;
    char * first = base;
    char * key = base + size * ( num >> 1 );
    do
    {
        while ( ( *compare )( first, key ) < 0 )
            first += size;
        while ( ( *compare )( key, last ) < 0 )
            last -= size;

        if ( first <= last )
        {
            if ( first != last )
            {
                memswap( first, last, size );
                if ( first == key )
                    key = last;
                else if ( last == key )
                    key = first;
             }
             first += size;
             last -= size;
        }
    } while ( first <= last );

    if ( base < last )
        qsort( base, ( last - base ) / size + 1, size, compare );
    if ( first < max)
        qsort( first, ( max - first ) / size + 1, size, compare );
} //qsort

