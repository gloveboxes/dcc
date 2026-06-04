#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

char ac[ 4096 ];
char other[ 4096 ];
char zeroes[ 4096 ]; // will be put in bss and guaranteed to be 0 by C89 and later

#define _countof( X ) ( sizeof( X ) / sizeof( X[0] ) )

#define TEST_WCHAR_T

#ifdef TEST_WCHAR_T

typedef unsigned int wchar_t;

wchar_t wc[ 4096 ];
wchar_t wc_other[ 4096 ];
wchar_t wc_zeroes[ 4096 ];

wchar_t *wcscpy(wchar_t * dest, const wchar_t * src)
{
    wchar_t * r = dest;
    while ( *src )
        *dest++ = *src++;
    *dest = 0;
    return r;
}

wchar_t * wcschr(const wchar_t *str, int c) 
{
    while (*str != 0) 
    {
        if (*str == c)
            return (wchar_t *)str; 
        str++;
    }

    return NULL;
}

wchar_t * wcsrchr(const wchar_t *str, int c) 
{
    const wchar_t *last_occurrence = NULL;
    while (*str != 0) 
    {
        if (*str == c)
            last_occurrence = str;
        str++;
    }
    return (wchar_t *)last_occurrence;
}

wchar_t * wcsstr(const wchar_t *haystack, const wchar_t *needle) 
{
    if (!*needle)
        return (wchar_t *)haystack;

    while (*haystack) 
    {
        const wchar_t *h = haystack;
        const wchar_t *n = needle;

        while (*h && *n && *h == *n) 
        {
            h++;
            n++;
        }

        if (!*n)
            return (wchar_t *)haystack;

        haystack++;
    }
    return NULL;
}

size_t wcslen(const wchar_t *str) 
{
    size_t len = 0;

    while (*str != 0) 
    {
        len++;
        str++; 
    }
    return len;
}

void test_wide()
{
    for ( int i = 0; i < _countof( wc ); i++ )
        wc[ i ] = ( L'a' + ( i % 26 ) );

    printf( "testing wcslen\n" );
    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int end = 1 + start + ( (unsigned int) rand() % 3000 );
        int len = end - start;
        wchar_t orig = wc[ end ];
        wc[ end ] = 0;
        int slen = wcslen( wc + start );
        if ( len != slen )
        {
            printf( "wcslen failed: iteration %d, len %d, wcslen %d, start %d, end %d\n", i, len, slen, start, end );
            exit( 1 );
        }
        wc[ end ] = orig;
    }

    printf( "testing wcschr and wcsrchr\n" );
    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int end = 1 + start + ( (unsigned int) rand() % 70 );
        int len = end - start;
        wchar_t orig = wc[ end ];
        wc[ end ] = L'!';
        wchar_t * pbang = wcschr( wc + start, L'!' );
        if ( !pbang )
        {
            printf( "wcschr failed to find char: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        if ( pbang != ( wc + end ) )
        {
            printf( "wcschr offset incorrect: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        pbang = wcsrchr( wc + start, L'!' );
        if ( !pbang )
        {
            printf( "wcsrchr failed to find char: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }

        if ( pbang != ( wc + end ) )
        {
            printf( "wcsrchr offset incorrect: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        wc[ end ] = orig;
    }

    printf( "testing wcsstr\n" );
    wchar_t alpha[27];
    wcscpy( alpha, L"abcdefghijklmnopqrstuvwxyz" );
    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int offset = ( (unsigned int) rand() % 26 );
        int len = 1 + ( (unsigned int) rand() % ( 26 - offset ) );
        if ( ( offset + len ) > 26 )
        {
            printf( "test bug offset %d, len %d\n", offset, len );
            exit( 1 );
        }
        const wchar_t * pattern = alpha + offset;
        const wchar_t * p = wcsstr( wc + start, pattern );
        if ( !p )
        {
            printf( "wcsstr pattern not found iteration %d, start %d, offset %d, len %d, pattern %ls\n", i, start, offset, len, pattern );
            exit( 1 );
        }
        if ( memcmp( p, pattern, len * sizeof( wchar_t ) ) )
        {
            printf( "wcsstr found the wrong pattern iteration %d, start %d, offset %d, len %d, pattern %ls\n", i, start, offset, len, pattern );
            exit( 1 );
        }
    }

    printf( "testing printf with wide strings\n" );
    for ( i = 0; i < 20; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int end = 1 + start + ( ( (unsigned int) rand() ) % 70 );
        int len = end - start;
        char orig = wc[ end ];
        wc[ end ] = 0;

        int l = wcslen( wc + start );
        printf( "%2d (%2d): %ls\n", len, l, wc + start );
        wc[ end ] = orig;
    }
} //test_wide

#endif

int main( int argc, char * argv[] )
{
    char alpha[27];
    int loops, i, slen;
    int loop_count = ( argc > 1 ) ? atoi( argv[ 1 ] ) : 1;

    for ( loops = 0; loops < loop_count; loops++ )
    {
        srand( 317 );
        for ( i = 0; i < sizeof( ac ); i++ )
            ac[ i ] = ( 'a' + ( i % 26 ) );

        printf( "testing strlen\n" );
        for ( i = 0; i < 1000; i++ )
        {
            int start = ( (unsigned int) rand() % 300 );
            int end = 1 + start + ( (unsigned int) rand() % 3000 );
            int len = end - start;
            char orig = ac[ end ];
            ac[ end ] = 0;
            slen = strlen( ac + start );
            if ( len != slen )
            {
                printf( "strlen failed: iteration %d, len %d, strlen %d, start %d, end %d\n", i, len, slen, start, end );
                exit( 1 );
            }
            ac[ end ] = orig;
        }

        printf( "testing strchr and strrchr\n" );
        for ( i = 0; i < 1000; i++ )
        {
            char * pbang;
            int start = ( (unsigned int) rand() % 300 ); 
            int end = 1 + start + ( (unsigned int) rand() % 70 );
            int len = end - start;
            char orig = ac[ end ];
            ac[ end ] = '!';
            pbang = strchr( ac + start, '!' );
            if ( !pbang )
            {
                printf( "strchr failed to find char: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
                exit( 1 );
            }
            if ( pbang != ( ac + end ) )
            {
                printf( "strchr offset incorrect: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
                exit( 1 );
            }
            pbang = strrchr( ac + start, '!' );
            if ( !pbang )
            {
                printf( "strrchr failed to find char: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
                exit( 1 );
            }
            if ( pbang != ( ac + end ) )
            {
                printf( "strrchr offset incorrect: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
                exit( 1 );
            }
            if ( strchr( ac + start, '$' ) )
            {
                printf( "strrchr somehow found $: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
                exit( 1 );
            }
            ac[ end ] = orig;
        }

        printf( "testing strstr\n" );
        strcpy( alpha, "abcdefghijklmnopqrstuvwxyz" );
        for ( i = 0; i < 1000; i++ )
        {
            const char * pattern, *p;
            int start = ( (unsigned int) rand() % 300 );
            int offset = ( (unsigned int) rand() % 26 );
            int len = 1 + ( (unsigned int) rand() % ( 26 - offset ) );
            if ( ( offset + len ) > 26 )
            {
                printf( "test bug offset %d, len %d\n", offset, len );
                exit( 1 );
            }
            pattern = alpha + offset;
            p = strstr( ac + start, pattern );
            if ( !p )
            {
                printf( "strstr pattern not found iteration %d, start %d, offset %d, len %d, pattern %s\n", i, start, offset, len, pattern );
                exit( 1 );
            }
            if ( memcmp( p, pattern, len ) )
            {
                printf( "strstr found the wrong pattern iteration %d, start %d, offset %d, len %d, pattern %s\n", i, start, offset, len, pattern );
                exit( 1 );
            }
            if ( strstr( ac + start, "gfe" ) )
            {
                printf( "strstr somehow found gfe. iteration %d, start %d, offset %d\n", i, start, offset );
                exit( 1 );
            }
        }

        printf( "testing memcpy and memcmp\n" );
        for ( i = 0; i < 1000; i++ )
        {
            int start = ( (unsigned int) rand() % 300 );
            int end = 1 + start + ( (unsigned int) rand() % 3000 );
            int len = end - start;

            memcpy( other + start, ac + start, len );
            if ( memcmp( other + start, ac + start, len ) )
            {
                printf( "memcmp of memcpy'ed memory failed to find match, iteration %d, len %d, start %d, end %d\n", i, len, start, end );
                exit( 1 );
            }
            memset( other + start, 0, len );
            if ( memcmp( other + start, zeroes + start, len ) )
            {
                printf( "zeroes not found in zero-filled memory, iteration %d, len %d, start %d, end %d\n", i, len, start, end );
                exit( 1 );
            }
        }

        printf( "testing printf\n" );
        for ( i = 0; i < 20; i++ )
        {
            int start = ( (unsigned int) rand() % 300 );
            int end = 1 + start + ( ( (unsigned int) rand() ) % 70 );
            int len = end - start;
            char orig = ac[ end ];
            ac[ end ] = 0;

            int l = strlen( ac + start );
            printf( "%2d (%2d): %s\n", len, l, ac + start );
            ac[ end ] = orig;
        }

        #ifdef TEST_WCHAR_T
            test_wide();
        #endif
    }

    printf( "tstr completed with great success\n" );
    return 0;
} //main
