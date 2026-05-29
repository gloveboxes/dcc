#include <stdio.h>

#define limit 15

int main()
{
    unsigned long prev2 = 1;
    unsigned long prev1 = 1;

    printf( "should tend towards 1.61803398874989484820458683436563811772030\n" );

    for ( unsigned long i = 1; i <= limit; i++ )
    {
        unsigned long next = prev1 + prev2;
        prev2 = prev1;
        prev1 = next;

        float v = (float) prev1 / (float) prev2;
        printf( "  at %ld iterations: %f\n", i, v );
    }

    printf( "done\n" );
    return 0;
}
