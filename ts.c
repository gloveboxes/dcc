/* tests shift functionality in C89 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

void shi8( int8_t x ) { printf( "sizeof int8_t: %d, result %x\n", sizeof( x ), x ); }
void shui8( uint8_t x ) { printf( "sizeof uint8_t: %d, result %x\n", sizeof( x ), x ); }
void shi16( int16_t x ) { printf( "sizeof int16_t: %d, result %x\n", sizeof( x ), x ); }
void shui16( uint16_t x ) { printf( "sizeof uint16_t: %d, result %x\n", sizeof( x ), x ); }
void shi32( long x ) { printf( "sizeof long: %d, result %lx\n", sizeof( x ), x ); }
void shui32( unsigned long x ) { printf( "sizeof ulong: %d, result %lx\n", sizeof( x ), x ); }
const char bc( bool x ) { return x ? 't' : 'f'; }
void shbool( bool a, bool b, bool c, bool d, bool e )
{
    printf( "%c, %c, %c, %c, %c\n", bc( a ), bc( b ), bc( c ), bc( d ), bc( e ) );
}

int main()
{
    int8_t i8;
    uint8_t ui8;
    int16_t i16;
    uint16_t ui16;
    long i32;
    unsigned long ui32;
    bool f0, f1, f2, f3, f4;
    printf( "top of app\n" );

    printf( "print an int %d\n", (int16_t) 27 );

    i8 = -1;
    i8 = i8 >> 1;
    shui8( (uint8_t) i8 );

    i8 = -1;
    i8 >>= 1;
    shui8( (uint8_t) i8 );

    ui8 = 0xff;
    ui8 = ui8 >> 1;
    shui8( ui8 );

    ui8 = 0xff;
    ui8 >>= 1;
    shui8( ui8 );

    i16 = -1;
    i16 = i16 >> 1;
    shui16( (uint16_t) i16 );

    i16 = -1;
    i16 >>= 1;
    shui16( (uint16_t) i16 );

    ui16 = 0xffff;
    ui16 = ui16 >> 1;
    shui16( ui16 );

    ui16 = 0xffff;
    ui16 >>= 1;
    shui16( ui16 );

    i32 = -1L;
    i32 = i32 >> 1;
    shi32( i32 );

    i32 = -1L;
    i32 >>= 1;
    shi32( i32 );

    ui32 = 0xffffffffUL;
    ui32 = ui32 >> 1;
    shui32( ui32 );

    ui32 = 0xffffffffUL;
    ui32 >>= 1;
    shui32( ui32 );

    printf( "now test left shifts\n" );

    i8 = 0xff;
    i8 = i8 << 1;
    shui8( (uint8_t) i8 );

    i8 = 0xff;
    i8 <<= 1;
    shui8( (uint8_t) i8 );

    ui8 = 0xff;
    ui8 = ui8 << 1;
    shui8( ui8 );

    ui8 = 0xff;
    ui8 <<= 1;
    shui8( ui8 );

    i16 = 0xffff;
    i16 = i16 << 1;
    shui16( (uint16_t) i16 );

    i16 = 0xffff;
    i16 <<= 1;
    shui16( (uint16_t) i16 );

    ui16 = 0xffff;
    ui16 = ui16 << 1;
    shui16( ui16 );

    ui16 = 0xffff;
    ui16 <<= 1;
    shui16( ui16 );

    i32 = 0xffffffffL;
    i32 = i32 << 1;
    shi32( i32 );

    i32 = 0xffffffffL;
    i32 <<= 1;
    shi32( i32 );

    ui32 = 0xffffffffUL;
    ui32 = ui32 << 1;
    shui32( ui32 );

    ui32 = 0xffffffffUL;
    ui32 <<= 1;
    shui32( ui32 );

    printf( "now test comparisons\n" );

    f0 = i8 == (int8_t) ui8; /*aztec requires a cast to work! */
    f1 = i8 > ui8;
    f2 = i8 >= (int8_t) ui8; /*aztec requires a cast to work! */
    f3 = i8 < (int8_t) ui8;  /*aztec requires a cast to work! */
    f4 = i8 <= ui8;
    shbool( f0, f1, f2, f3, f4 );

    f0 = i16 == ui16;
    f1 = i16 > ui16;
    f2 = i16 >= ui16;
    f3 = i16 < ui16;
    f4 = i16 <= ui16;
    shbool( f0, f1, f2, f3, f4 );

    f0 = i8 == i16;
    f1 = i8 > i16;
    f2 = i8 >= i16;
    f3 = i8 < i16;
    f4 = i8 <= i16;
    shbool( f0, f1, f2, f3, f4 );

    f0 = i8 == 16;
    f1 = i8 > 16;
    f2 = i8 >= 16;
    f3 = i8 < 16;
    f4 = i8 <= 16;
    shbool( f0, f1, f2, f3, f4 );

    f0 = i16 == 32;
    f1 = i16 > 32;
    f2 = i16 >= 32;
    f3 = i16 < 32;
    f4 = i16 <= 32;
    shbool( f0, f1, f2, f3, f4 );

    f0 = i32 == ui32;
    f1 = i32 > ui32;
    f2 = i32 >= ui32;
    f3 = i32 < ui32;
    f4 = i32 <= ui32;
    shbool( f0, f1, f2, f3, f4 );

    f0 = i32 == 32L;
    f1 = i32 > 32L;
    f2 = i32 >= 32L;
    f3 = i32 < 32L;
    f4 = i32 <= 32L;
    shbool( f0, f1, f2, f3, f4 );

    printf( "testing printf\n" );

    printf( "  string: '%s'\n", "hello" );
    printf( "  char: '%c'\n", 'h' );
    printf( "  int: %d, %x\n", 27, 27 );
    printf( "  negative int: %d, %x\n", -27, -27 );

    printf( "ts completed with great success\n" );
    return 0;
} 


