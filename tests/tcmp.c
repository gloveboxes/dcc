/* tests comparisons of various types */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>

void i8cmp( int8_t a, int8_t b )
{
    bool gt = ( a > b );
    bool lt = ( a < b );
    bool eq = ( a == b );
    bool le = ( a <= b );
    bool ge = ( a >= b );
    printf( "  lt %d le %d eq %d ge %d gt %d\n", lt, le, eq, ge, gt );
} //i8cmp

void ui8cmp( uint8_t a, uint8_t b )
{
    bool gt = ( a > b );
    bool lt = ( a < b );
    bool eq = ( a == b );
    bool le = ( a <= b );
    bool ge = ( a >= b );
    printf( "  lt %d le %d eq %d ge %d gt %d\n", lt, le, eq, ge, gt );
} //ui8cmp

void i16cmp( int16_t a, int16_t b )
{
    bool gt = ( a > b );
    bool lt = ( a < b );
    bool eq = ( a == b );
    bool le = ( a <= b );
    bool ge = ( a >= b );
    printf( "  lt %d le %d eq %d ge %d gt %d\n", lt, le, eq, ge, gt );
} //i16cmp

void ui16cmp( uint16_t a, uint16_t b )
{
    bool gt = ( a > b );
    bool lt = ( a < b );
    bool eq = ( a == b );
    bool le = ( a <= b );
    bool ge = ( a >= b );
    printf( "  lt %d le %d eq %d ge %d gt %d\n", lt, le, eq, ge, gt );
} //ui16cmp

void i32cmp( long a, long b )
{
    bool gt = ( a > b );
    bool lt = ( a < b );
    bool eq = ( a == b );
    bool le = ( a <= b );
    bool ge = ( a >= b );
    printf( "  lt %d le %d eq %d ge %d gt %d\n", lt, le, eq, ge, gt );
} //i32cmp

void ui32cmp( unsigned long a, unsigned long b )
{
    bool gt = ( a > b );
    bool lt = ( a < b );
    bool eq = ( a == b );
    bool le = ( a <= b );
    bool ge = ( a >= b );
    printf( "  lt %d le %d eq %d ge %d gt %d\n", lt, le, eq, ge, gt );
} //ui32cmp

void cmp_float( float a, float b )
{
    float diff;
    diff = a - b;
    float abs_diff;
    abs_diff = fabsf( diff );
    bool gt = ( diff > 0.0 && abs_diff > FLT_EPSILON );
    bool lt = ( diff < 0.0 && abs_diff > FLT_EPSILON );
    bool eq = ( abs_diff < FLT_EPSILON );
    bool le = ( diff <= 0.0 || abs_diff < FLT_EPSILON );
    bool ge = ( diff >= 0.0 || abs_diff < FLT_EPSILON );
    printf( "  lt %d le %d eq %d ge %d gt %d\n", lt, le, eq, ge, gt );
} //cmp_float

int main( int argc, char * argv[] )
{
    printf( "uint8_t:\n" );
    ui8cmp( (uint8_t) 1, (uint8_t) 3 );
    ui8cmp( (uint8_t) 1, (uint8_t) -3 );
    ui8cmp( (uint8_t) -1, (uint8_t) 3 );
    ui8cmp( (uint8_t) -1, (uint8_t) -3 );
    ui8cmp( (uint8_t) -1, (uint8_t) -1 );
    ui8cmp( (uint8_t) 1, (uint8_t) -1 );
    ui8cmp( (uint8_t) 247, (uint8_t) 3 );
    ui8cmp( (uint8_t) 247, (uint8_t) -3 );
    ui8cmp( (uint8_t) -247, (uint8_t) 3 );
    ui8cmp( (uint8_t) -247, (uint8_t) -3 );
    ui8cmp( (uint8_t) 0xf1, (uint8_t) 0xf2 );
    ui8cmp( (uint8_t) 0xf3, (uint8_t) 0xf2 );
    ui8cmp( (uint8_t) 0xe1, (uint8_t) 0xe2 );
    ui8cmp( (uint8_t) 0xe3, (uint8_t) 0xe2 );
    ui8cmp( (uint8_t) 0x81, (uint8_t) 0x78 );
    ui8cmp( (uint8_t) 0, (uint8_t) 0x80 );
    ui8cmp( (uint8_t) 0x7f, (uint8_t) 0x80 );

    printf( "int8_t:\n" );
    i8cmp( (int8_t) 1, (int8_t) 3 );
    i8cmp( (int8_t) 1, (int8_t) -3 );
    i8cmp( (int8_t) -1, (int8_t) 3 );
    i8cmp( (int8_t) -1, (int8_t) -3 );
    i8cmp( (int8_t) -1, (int8_t) -1 );
    i8cmp( (int8_t) 1, (int8_t) -1 );
    i8cmp( (int8_t) 247, (int8_t) 3 );
    i8cmp( (int8_t) 247, (int8_t) -3 );
    i8cmp( (int8_t) -247, (int8_t) 3 );
    i8cmp( (int8_t) -247, (int8_t) -3 );
    i8cmp( (int8_t) 0xf1, (int8_t) 0xf2 );
    i8cmp( (int8_t) 0xf3, (int8_t) 0xf2 );
    i8cmp( (int8_t) 0xe1, (int8_t) 0xe2 );
    i8cmp( (int8_t) 0xe3, (int8_t) 0xe2 );
    i8cmp( (int8_t) 0x81, (int8_t) 0x78 );
    i8cmp( (int8_t) 0, (int8_t) 0x80 );
    i8cmp( (int8_t) 0x7f, (int8_t) 0x80 );

    printf( "uint16_t:\n" );
    ui16cmp( (uint16_t) 1, (uint16_t) 3 );
    ui16cmp( (uint16_t) 1, (uint16_t) -3 );
    ui16cmp( (uint16_t) -1, (uint16_t) 3 );
    ui16cmp( (uint16_t) -1, (uint16_t) -3 );
    ui16cmp( (uint16_t) -1, (uint16_t) -1 );
    ui16cmp( (uint16_t) 1, (uint16_t) -1 );
    ui16cmp( (uint16_t) 247, (uint16_t) 3 );
    ui16cmp( (uint16_t) 247, (uint16_t) -3 );
    ui16cmp( (uint16_t) -247, (uint16_t) 3 );
    ui16cmp( (uint16_t) -247, (uint16_t) -3 );
    ui16cmp( (uint16_t) 0xff11, (uint16_t) 0xff22 );
    ui16cmp( (uint16_t) 0xff33, (uint16_t) 0xff22 );
    ui16cmp( (uint16_t) 0xef11, (uint16_t) 0xef22 );
    ui16cmp( (uint16_t) 0xef33, (uint16_t) 0xef22 );
    ui16cmp( (uint16_t) 0x8001, (uint16_t) 0x7ff8 );
    ui16cmp( (uint16_t) 0, (uint16_t) 0x8000 );
    ui16cmp( (uint16_t) 0x7fff, (uint16_t) 0x8000 );

    printf( "int16_t:\n" );
    i16cmp( (int16_t) 1, (int16_t) 3 );
    i16cmp( (int16_t) 1, (int16_t) -3 );
    i16cmp( (int16_t) -1, (int16_t) 3 );
    i16cmp( (int16_t) -1, (int16_t) -3 );
    i16cmp( (int16_t) -1, (int16_t) -1 );
    i16cmp( (int16_t) 1, (int16_t) -1 );
    i16cmp( (int16_t) 247, (int16_t) 3 );
    i16cmp( (int16_t) 247, (int16_t) -3 );
    i16cmp( (int16_t) -247, (int16_t) 3 );
    i16cmp( (int16_t) -247, (int16_t) -3 );
    i16cmp( (int16_t) 0xff11, (int16_t) 0xff22 );
    i16cmp( (int16_t) 0xff33, (int16_t) 0xff22 );
    i16cmp( (int16_t) 0xef11, (int16_t) 0xef22 );
    i16cmp( (int16_t) 0xef33, (int16_t) 0xef22 );
    i16cmp( (int16_t) 0x8001, (int16_t) 0x7ff8 );
    i16cmp( (int16_t) 0, (int16_t) 0x8000 );
    i16cmp( (int16_t) 0x7fff, (int16_t) 0x8000 );

    printf( "uint32_t:\n" );
    ui32cmp( 1UL, 3UL );
    ui32cmp( 1UL, (unsigned long) -3L );
    ui32cmp( (unsigned long) -1L, 3UL );
    ui32cmp( (unsigned long) -1L, (unsigned long) -3L );
    ui32cmp( (unsigned long) -1L, (unsigned long) -1L );
    ui32cmp( 1UL, (unsigned long) -1L );
    ui32cmp( 247UL, 3UL );
    ui32cmp( 247UL, (unsigned long) -3L );
    ui32cmp( (unsigned long) -247L, 3UL );
    ui32cmp( (unsigned long) -247L, (unsigned long) -3L );
    ui32cmp( 0xffff1111UL, 0xffff2222UL );
    ui32cmp( 0xffff3333UL, 0xffff2222UL );
    ui32cmp( 0xefff1111UL, 0xefff2222UL );
    ui32cmp( 0xefff3333UL, 0xefff2222UL );
    ui32cmp( 0x80000001UL, 0x7ffffff8UL );
    ui32cmp( 0UL, 0x80000000UL );
    ui32cmp( 0x7fffffffUL, 0x80000000UL );

    printf( "int32_t:\n" );
    i32cmp( 1L, 3L );
    i32cmp( 1L, -3L );
    i32cmp( -1L, 3L );
    i32cmp( -1L, -3L );
    i32cmp( -1L, -1L );
    i32cmp( 1L, -1L );
    i32cmp( 247L, 3L );
    i32cmp( 247L, -3L );
    i32cmp( -247L, 3L );
    i32cmp( -247L, -3L );
    i32cmp( 0xffff1111L, 0xffff2222L );
    i32cmp( 0xffff3333L, 0xffff2222L );
    i32cmp( 0xefff1111L, 0xefff2222L );
    i32cmp( 0xefff3333L, 0xefff2222L );
    i32cmp( 0x80000001L, 0x7ffffff8L );
    i32cmp( 0L, 0x80000000L );
    i32cmp( 0x7fffffffL, 0x80000000L );

    printf( "floating point:\n" );
    float f = -0.5f;
    for ( int i = 0; i < 10; i++ )
    {
        cmp_float( f, 0.2f );
        f += 0.1f;
    }

    printf( "tcmp completed with great success\n" );
    return 0;
} //main
