#include <stddef.h>
#include <stdlib.h>

const void * bsearch( const void * key, const void * vbase, size_t num, size_t size, int (*compare)( const void * a, const void * b ) )
{
    if ( 0 == num )
        return 0;

    const char * base = (const char *) vbase;
    int lo = 0;
    int hi = (int) num - 1;
    do
    {
        int k = ( lo + hi ) / 2;
        const char * here = base + size * k;
        int cmp = ( *compare )( key, here );
        if ( 0 == cmp )
        {
            while ( ( here > base ) && ( ( *compare )( key, here - size ) == 0 ) )
                here -= size;
            return here;
        }

        if ( cmp < 0 )
            hi = k - 1;
        else
            lo = k + 1;
   } while ( hi >= lo );

   return 0;
} //my_bsearch



