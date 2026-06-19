#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#if 0
long atol(const char *str)
{
    long result = 0;
    int sign = 1;

    /* 1. Skip leading whitespace characters */
    while (isspace((unsigned char)*str)) {
        str++;
    }

    /* 2. Check for optional plus or minus sign */
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    /* 3. Convert characters to long integer */
    while (isdigit((unsigned char)*str)) {
        result = (result * 10) + (*str - '0');
        str++;
    }

    return sign * result;
} //atol
#endif

uint32_t ulsqrt( uint32_t n )
{
    uint32_t low = 1;
    uint32_t high = n / 2;
    uint32_t result = 0;
    uint32_t mid;

    if ( n <= 1 )
        return n;

    while ( low <= high )
    {
        mid = low + ( ( high - low ) / 2 );
        if ( mid <= ( n / mid ) )
        {
            result = mid;
            low = mid + 1;
        }
        else
            high = mid - 1;
    }

    return result;
}

int main( int argc, char * argv[] )
{
    uint32_t start = 10000;
    uint32_t num_found = 0;
    uint32_t s, i;
    bool is_prime;

    if ( argc >= 2 )
        start = atol( argv[ 1 ] );

    if ( 0 == ( start & 1 ) )
        start++;

    while ( num_found < 10 )
    {
        s = 1 + ulsqrt( start );
        is_prime = true;

        for ( i = 3; i < s; i += 2 )
        {
            if ( 0 == ( start % i ) )
            {
                is_prime = false;
                break;
            }
        }

        if ( is_prime )
        {
            num_found++;
            printf( "%lu\n", start );
        }

        start += 2;
    }

    return 0;
}
