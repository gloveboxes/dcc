#include <stdio.h>
#include <stdint.h>

void tui8_bits( uint8_t a, uint8_t b )
{
    uint8_t r_and = ( a & b );
    uint8_t r_or = ( a | b );
    uint8_t r_xor = ( a ^ b );
    uint8_t r_nota = ( ~a );
    uint8_t r_notb = ( ~b );

    printf( "  and %x, or %x, xor %x, nota %x, notb %x\n", r_and, r_or, r_xor, r_nota, r_notb );
}

void ti8_bits( int8_t a, int8_t b )
{
    int8_t r_and = ( a & b );
    int8_t r_or = ( a | b );
    int8_t r_xor = ( a ^ b );
    int8_t r_nota = ( ~a );
    int8_t r_notb = ( ~b );

    printf( "  and %x, or %x, xor %x, nota %x, notb %x\n", r_and, r_or, r_xor, r_nota, r_notb );
}

void tui16_bits( uint16_t a, uint16_t b )
{
    uint16_t r_and = ( a & b );
    uint16_t r_or = ( a | b );
    uint16_t r_xor = ( a ^ b );
    uint16_t r_nota = ( ~a );
    uint16_t r_notb = ( ~b );

    printf( "  and %x, or %x, xor %x, nota %x, notb %x\n", r_and, r_or, r_xor, r_nota, r_notb );
}

void ti16_bits( int16_t a, int16_t b )
{
    int16_t r_and = ( a & b );
    int16_t r_or = ( a | b );
    int16_t r_xor = ( a ^ b );
    int16_t r_nota = ( ~a );
    int16_t r_notb = ( ~b );

    printf( "  and %x, or %x, xor %x, nota %x, notb %x\n", r_and, r_or, r_xor, r_nota, r_notb );
}

void tui32_bits( uint32_t a, uint32_t b )
{
    uint32_t r_and = ( a & b );
    uint32_t r_or = ( a | b );
    uint32_t r_xor = ( a ^ b );
    uint32_t r_nota = ( ~a );
    uint32_t r_notb = ( ~b );

    printf( "  and %lx, or %lx, xor %lx, nota %lx, notb %lx\n", r_and, r_or, r_xor, r_nota, r_notb );
}

void ti32_bits( int32_t a, int32_t b )
{
    int32_t r_and = ( a & b );
    int32_t r_or = ( a | b );
    int32_t r_xor = ( a ^ b );
    int32_t r_nota = ( ~a );
    int32_t r_notb = ( ~b );

    printf( "  and %lx, or %lx, xor %lx, nota %lx, notb %lx\n", r_and, r_or, r_xor, r_nota, r_notb );
}

int main( int argc, char * argv[] )
{
    printf( "uint8_t:\n" );
    tui8_bits( (uint8_t) 7, (uint8_t) 3 );
    tui8_bits( (uint8_t) 7, (uint8_t) -3 );
    tui8_bits( (uint8_t) -7, (uint8_t) 3 );
    tui8_bits( (uint8_t) -7, (uint8_t) -3 );
    tui8_bits( (uint8_t) -1, (uint8_t) -1 );
    tui8_bits( (uint8_t) 247, (uint8_t) 3 );
    tui8_bits( (uint8_t) 247, (uint8_t) -3 );
    tui8_bits( (uint8_t) -247, (uint8_t) 3 );
    tui8_bits( (uint8_t) -247, (uint8_t) -247 );

    printf( "int8_t:\n" );
    ti8_bits( (int8_t) 7, (int8_t) 3 );   
    ti8_bits( (int8_t) 7, (int8_t) -3 );
    ti8_bits( (int8_t) -7, (int8_t) 3 );
    ti8_bits( (int8_t) -7, (int8_t) -3 );
    ti8_bits( (int8_t) -1, (int8_t) -1 );
    ti8_bits( (int8_t) 247, (int8_t) 3 );
    ti8_bits( (int8_t) 247, (int8_t) -3 );
    ti8_bits( (int8_t) -247, (int8_t) 3 );
    ti8_bits( (int8_t) -247, (int8_t) -247 );

    printf( "uint16_t:\n" );
    tui16_bits( (uint16_t) 7, (uint16_t) 3 );
    tui16_bits( (uint16_t) 7, (uint16_t) -3 );
    tui16_bits( (uint16_t) -7, (uint16_t) 3 );
    tui16_bits( (uint16_t) -7, (uint16_t) -3 );
    tui16_bits( (uint16_t) -1, (uint16_t) -1 );
    tui16_bits( (uint16_t) 247, (uint16_t) 3 );
    tui16_bits( (uint16_t) 247, (uint16_t) -3 );
    tui16_bits( (uint16_t) -247, (uint16_t) 3 );
    tui16_bits( (uint16_t) -247, (uint16_t) -247 );

    printf( "int16_t:\n" );
    ti16_bits( (int16_t) 7, (int16_t) 3 );
    ti16_bits( (int16_t) 7, (int16_t) -3 );
    ti16_bits( (int16_t) -7, (int16_t) 3 );
    ti16_bits( (int16_t) -7, (int16_t) -3 );
    ti16_bits( (int16_t) -1, (int16_t) -1 );
    ti16_bits( (int16_t) 247, (int16_t) 3 );
    ti16_bits( (int16_t) 247, (int16_t) -3 );
    ti16_bits( (int16_t) -247, (int16_t) 3 );
    ti16_bits( (int16_t) -247, (int16_t) -247 );

    printf( "uint32_t:\n" );
    tui32_bits( (uint32_t) 7, (uint32_t) 3 );
    tui32_bits( (uint32_t) 7, (uint32_t) -3 );
    tui32_bits( (uint32_t) -7, (uint32_t) 3 );
    tui32_bits( (uint32_t) -7, (uint32_t) -3 );
    tui32_bits( (uint32_t) -1, (uint32_t) -1 );
    tui32_bits( (uint32_t) 247, (uint32_t) 3 );
    tui32_bits( (uint32_t) 247, (uint32_t) -3 );
    tui32_bits( (uint32_t) -247, (uint32_t) 3 );
    tui32_bits( (uint32_t) -247, (uint32_t) -247 );

    printf( "int32_t:\n" );
    ti32_bits( (int32_t) 7, (int32_t) 3 );
    ti32_bits( (int32_t) 7, (int32_t) -3 );
    ti32_bits( (int32_t) -7, (int32_t) 3 );
    ti32_bits( (int32_t) -7, (int32_t) -3 );
    ti32_bits( (int32_t) -1, (int32_t) -1 );
    ti32_bits( (int32_t) 247, (int32_t) 3 );
    ti32_bits( (int32_t) 247, (int32_t) -3 );
    ti32_bits( (int32_t) -247, (int32_t) 3 );
    ti32_bits( (int32_t) -247, (int32_t) -247 );

    printf( "tbits completed with great success\n" );
} //main

