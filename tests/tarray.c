/* tests various array patterns */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define _countof( X ) ( sizeof( X ) / sizeof( X[0] ) )

#pragma pack( push, 1 )
struct SMany
{
    uint8_t ui8;
    uint16_t ui16;
    uint32_t ui32;
    int8_t i8;
    int16_t i16;
    int32_t i32;
};

static struct SMany abefore[ 10 ];
static struct SMany amany[ 20 ];
static struct SMany aafter[ 10 ];

static uint8_t au8[ 8 ] = { 10, 11, 12, 13, 14, 15, 16, 17 };
static uint16_t au16[ 8 ] = { 10, 11, 12, 13, 14, 15, 16, 17 };
static uint32_t au32[ 8 ] = { 10, 11, 12, 13, 14, 15, 16, 17 };

static int8_t a8[ 8 ] = { -10, -11, -12, -13, -14, -15, -16, -17 };
static int16_t a16[ 8 ] = { -10, -11, -12, -13, -14, -15, -16, -17 };
static int32_t a32[ 8 ] = { -10, -11, -12, -13, -14, -15, -16, -17 };

static char ac[ 8 ] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h' };

static char start_board[65] =
    "RNBQKBNR"
    "PPPPPPPP"
    "........"
    "........"
    "........"
    "........"
    "pppppppp"
    "rnbqkbnr";

static char * astr[ 8 ] =
{
    "anteater", "bear", "cat", "dog", "earthworm", "fruitbat", "goat", "horse"
};

static char * aHexNibble( char * p, uint8_t val )
{
    *p++ = ( val <= 9 ) ? val + '0' : val - 10 + 'a';
    return p;
} //aHexNibble

static char * aHexByte( char * p, uint8_t val )
{
    p = aHexNibble( p, ( val >> 4 ) & 0xf );
    p = aHexNibble( p, val & 0xf );
    return p;
} //aBexByte

static char * aHexWord( char * p, uint16_t val )
{
    p = aHexByte( p, ( val >> 8 ) & 0xff );
    p = aHexByte( p, val & 0xff );
    return p;
} //aHexWord

#define bytesPerRow 32
static uint8_t buf[ bytesPerRow ];
static char acLine[ 200 ];

void ShowBinaryData( uint8_t * pData, size_t length, size_t indent )
{
    size_t offset = 0;
    size_t beyond = length;

    while ( offset < beyond )
    {
        char * pline = acLine;

        for ( size_t i = 0; i < indent; i++ )
            *pline++ = ' ';

        pline = aHexWord( pline, (size_t) offset );
        *pline++ = ' ';
        *pline++ = ' ';

        int32_t end_of_row = offset + bytesPerRow;
        int32_t cap = ( end_of_row > beyond ) ? beyond : end_of_row;
        int32_t toread = ( ( offset + bytesPerRow ) > beyond ) ? ( length % bytesPerRow ) : bytesPerRow;
        memcpy( buf, pData + offset, toread );
        uint32_t extraSpace = 2;

        for ( size_t o = offset; o < cap; o++ )
        {
            pline = aHexByte( pline, buf[ o - offset ] );
            *pline++ = ' ';
            if ( ( bytesPerRow > 16 ) && ( o == ( offset + 15 ) ) )
            {
                *pline++ = ':';
                *pline++ = ' ';
                extraSpace = 0;
            }
        }

        uint32_t spaceNeeded = extraSpace + ( ( bytesPerRow - ( cap - offset ) ) * 3 );

        for ( size_t sp = 0; sp < ( 1 + spaceNeeded ); sp++ )
            *pline++ = ' ';

        for ( size_t o = offset; o < cap; o++ )
        {
            char ch = buf[ o - offset ];

            if ( (int8_t) ch < ' ' || 127 == ch )
                ch = '.';

            *pline++ = ch;
        }

        offset += bytesPerRow;
        *pline = 0;
        printf( "%s\n", acLine );
    }
} //ShowBinaryData

void test_many()
{
    memset( abefore, 0, sizeof( abefore ) );
    memset( aafter, 0, sizeof( aafter ) );

    for ( size_t i = 0; i < _countof( amany ); i++ )
    {
        struct SMany * m = & amany[ i ];
        m->ui8 = (uint8_t) i;
        m->ui16 = (uint16_t) i * 2;
        m->ui32 = (uint32_t) i * 4;
        m->i8 = - (int8_t) i;
        m->i16 = - (int16_t) i * 2;
        m->i32 = - (int32_t) i * 4;
    }

    memset( abefore, 0, sizeof( abefore ) );
    memset( aafter, 0, sizeof( aafter ) );

    for ( size_t i = 0; i < _countof( amany ); i++ )
    {
        struct SMany * m = & amany[ i ];

        if ( m->ui8 != i )
            printf( "error: i %u, ui8 is %lu, not %lu\n", (int) i, (uint32_t) m->ui8, (uint32_t) i );
        if ( m->ui16 != (uint16_t) i * 2 )
            printf( "error: i %u, ui16 is %lu, not %lu\n", (int) i, (uint32_t) m->ui16, (uint32_t) i * 2 );
        if ( m->ui32 != (uint32_t) i * 4 )
            printf( "error: i %u, ui32 is %lu, not %lu\n", (int) i, (uint32_t) m->ui32, (uint32_t) i * 4 );

        if ( m->i8 != - (int8_t) i )
            printf( "error: i %u, i8 is %ld, not %ld\n", (int) i, (int32_t) m->i8, - (int32_t) i );
        if ( m->i16 != - (int16_t) i * 2 )
            printf( "error: i %u, i16 is %ld, not %ld\n", (int) i, (int32_t) m->ui16, - (int32_t) i * 2 );
        if ( m->i32 != - (int32_t) i * 4 )
            printf( "error: i %u, i32 is %ld, not %ld\n", (int) i, (int32_t) m->ui32, - (int32_t) i * 4 );
    }

    ShowBinaryData( (uint8_t *) amany, sizeof( amany ), 4 );
} //test_many

int main( int argc, char * argv[] )
{
    if ( ( _countof( au8 ) != _countof( a8 ) ) ||
         ( _countof( au8 ) != _countof( au16 ) ) ||
         ( _countof( au8 ) != _countof( a16 ) ) ||
         ( _countof( au8 ) != _countof( au32 ) ) ||
         ( _countof( au8 ) != _countof( a32 ) ) )
    {
        printf( "array sizes aren't what is expected\n" );
        exit( 1 );
    }

    for ( int i = 0; i < _countof( au8 ); i++ )
    {
        printf( "index %u, au8 %u, au16 %u, au32 %lu\n", i, au8[ i ], au16[ i ], au32[ i ] );
        printf( "index %u, a8 %d, a16 %d, a32 %ld\n", i, a8[ i ], a16[ i ], a32[ i ] );
    }

    for ( int i = 0; i < 8; i++ )
    {
        au32[i] = i + 20;
        au16[i] = i + 20;
        au8[i] = i + 20;

        a32[i] = (long) i - 20;
        a16[i] = i - 20;
        a8[i] = i - 20;
    }

    for ( int i = 0; i < 8; i++ )
    {
        printf( "index %u, au8 %u, au16 %u, au32 %lu\n", i, au8[ i ], au16[ i ], au32[ i ] );
        printf( "index %u, a8 %d, a16 %d, a32 %ld\n", i, a8[ i ], a16[ i ], a32[ i ] );
    }

    printf( "ac: %c %c %c %c %c %c %c %c\n", ac[ 0 ], ac[ 1 ], ac[ 2 ], ac[ 3 ], ac[ 4 ], ac[ 5 ], ac[ 6 ], ac[ 7 ] );

    for ( int r = 0; r < 8; r++ )
    {
        for ( int c = 0; c < 8; c++ )
            printf( "%c ", start_board[ r * 8 + c ] );

        printf( "\n" );
    }

    for ( int a = 0; a < sizeof( astr ) / sizeof( astr[ 0 ] ); a++ )
        printf( "animal %d: %s\n", a, astr[ a ] );

    test_many();

    printf( "tarray completed with great success\n" );
    return 0;
} // main



