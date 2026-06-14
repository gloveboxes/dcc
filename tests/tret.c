#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

int8_t a_i8_fun( void )
{
    return -1;
}

int8_t b_i8_fun( void )
{
    return 69;
}

int16_t a_i16_fun( void )
{
    return -1;
}

int16_t b_i16_fun( void )
{
    return 69;
}

int32_t a_i32_fun( void )
{
    return -1;
}

int32_t b_i32_fun( void )
{
    return 69;
}

uint8_t a_ui8_fun( void )
{
    return -1;
}

uint8_t b_ui8_fun( void )
{
    return 69;
}

uint16_t a_ui16_fun( void )
{
    return -1;
}

uint16_t b_ui16_fun( void )
{
    return 69;
}

uint32_t a_ui32_fun( void )
{
    return -1;
}

uint32_t b_ui32_fun( void )
{
    return 69;
}

int main( int argc, char * argv[] )
{
    // signed 8 to signed
    printf( "signed 8 to\n" );

    int8_t i8v = a_i8_fun();
    printf( "    i8_t a: %d\n", i8v );

    i8v = b_i8_fun();
    printf( "    i8_t b: %d\n", i8v );

    int16_t i16v = a_i8_fun();
    printf( "    i16_t a: %d\n", i16v );

    i16v = b_i8_fun();
    printf( "    i16_t b: %d\n", i16v );

    int32_t i32v = a_i8_fun();
    printf( "    i32_t a: %ld\n", i32v );

    i32v = b_i8_fun();
    printf( "    i32_t b: %ld\n", i32v );

    // signed 16 to signed
    printf( "signed 16 to\n" );

    int8_t i8v = a_i16_fun();
    printf( "    i8_t a: %d\n", i8v );

    i8v = b_i16_fun();
    printf( "    i8_t b: %d\n", i8v );

    int16_t i16v = a_i16_fun();
    printf( "    i16_t a: %d\n", i16v );

    i16v = b_i16_fun();
    printf( "    i16_t b: %d\n", i16v );

    int32_t i32v = a_i16_fun();
    printf( "    i32_t a: %ld\n", i32v );

    i32v = b_i16_fun();
    printf( "    i32_t b: %ld\n", i32v );

    // signed 32 to signed
    printf( "signed 32 to\n" );

    int8_t i8v = a_i32_fun();
    printf( "    i8_t a: %d\n", i8v );

    i8v = b_i32_fun();
    printf( "    i8_t b: %d\n", i8v );

    int16_t i16v = a_i32_fun();
    printf( "    i16_t a: %d\n", i16v );

    i16v = b_i32_fun();
    printf( "    i16_t b: %d\n", i16v );

    int32_t i32v = a_i32_fun();
    printf( "    i32_t a: %ld\n", i32v );

    i32v = b_i32_fun();
    printf( "    i32_t b: %ld\n", i32v );

    //////////////////////////
    // signed 8 to unsigned
    printf( "signed 8 to\n" );

    uint8_t ui8v = a_i8_fun();
    printf( "    ui8_t a: %u\n", ui8v );

    ui8v = b_i8_fun();
    printf( "    ui8_t b: %u\n", ui8v );

    uint16_t ui16v = a_i8_fun();
    printf( "    ui16_t a: %u\n", ui16v );

    ui16v = b_i8_fun();
    printf( "    ui16_t b: %u\n", ui16v );

    uint32_t ui32v = a_i8_fun();
    printf( "    ui32_t a: %lu\n", ui32v );

    ui32v = b_i8_fun();
    printf( "    ui32_t b: %lu\n", ui32v );

    // signed 16 to unsigned
    printf( "signed 16 to\n" );

    uint8_t ui8v = a_i16_fun();
    printf( "    ui8_t a: %u\n", ui8v );

    ui8v = b_i16_fun();
    printf( "    ui8_t b: %u\n", ui8v );

    uint16_t ui16v = a_i16_fun();
    printf( "    ui16_t a: %u\n", ui16v );

    ui16v = b_i16_fun();
    printf( "    ui16_t b: %u\n", ui16v );

    uint32_t ui32v = a_i16_fun();
    printf( "    ui32_t a: %lu\n", ui32v );

    ui32v = b_i16_fun();
    printf( "    ui32_t b: %lu\n", ui32v );

    // signed 32 to unsigned
    printf( "signed 32 to\n" );

    int8_t ui8v = a_i32_fun();
    printf( "    ui8_t a: %u\n", ui8v );

    ui8v = b_i32_fun();
    printf( "    ui8_t b: %u\n", ui8v );

    int16_t ui16v = a_i32_fun();
    printf( "    ui16_t a: %u\n", ui16v );

    ui16v = b_i32_fun();
    printf( "    ui16_t b: %u\n", ui16v );

    int32_t ui32v = a_i32_fun();
    printf( "    ui32_t a: %lu\n", ui32v );

    ui32v = b_i32_fun();
    printf( "    ui32_t b: %lu\n", ui32v );

///////////////////////////////

    // unsigned 8 to signed
    printf( "unsigned 8 to\n" );

    int8_t i8v = a_ui8_fun();
    printf( "    i8_t a: %d\n", i8v );

    i8v = b_ui8_fun();
    printf( "    i8_t b: %d\n", i8v );

    int16_t i16v = a_ui8_fun();
    printf( "    i16_t a: %d\n", i16v );

    i16v = b_ui8_fun();
    printf( "    i16_t b: %d\n", i16v );

    int32_t i32v = a_ui8_fun();
    printf( "    i32_t a: %ld\n", i32v );

    i32v = b_ui8_fun();
    printf( "    i32_t b: %ld\n", i32v );

    // unsigned 16 to signed
    printf( "unsigned 16 to\n" );

    int8_t i8v = a_ui16_fun();
    printf( "    i8_t a: %d\n", i8v );

    i8v = b_ui16_fun();
    printf( "    i8_t b: %d\n", i8v );

    int16_t i16v = a_ui16_fun();
    printf( "    i16_t a: %d\n", i16v );

    i16v = b_ui16_fun();
    printf( "    i16_t b: %d\n", i16v );

    int32_t i32v = a_ui16_fun();
    printf( "    i32_t a: %ld\n", i32v );

    i32v = b_ui16_fun();
    printf( "    i32_t b: %ld\n", i32v );

    // unsigned 32 to signed
    printf( "unsigned 32 to\n" );

    int8_t i8v = a_ui32_fun();
    printf( "    i8_t a: %d\n", i8v );

    i8v = b_ui32_fun();
    printf( "    i8_t b: %d\n", i8v );

    int16_t i16v = a_ui32_fun();
    printf( "    i16_t a: %d\n", i16v );

    i16v = b_ui32_fun();
    printf( "    i16_t b: %d\n", i16v );

    int32_t i32v = a_ui32_fun();
    printf( "    i32_t a: %ld\n", i32v );

    i32v = b_ui32_fun();
    printf( "    i32_t b: %ld\n", i32v );

    //////////////////////////
    // unsigned 8 to unsigned
    printf( "unsigned 8 to\n" );

    uint8_t ui8v = a_ui8_fun();
    printf( "    ui8_t a: %u\n", ui8v );

    ui8v = b_ui8_fun();
    printf( "    ui8_t b: %u\n", ui8v );

    uint16_t ui16v = a_ui8_fun();
    printf( "    ui16_t a: %u\n", ui16v );

    ui16v = b_ui8_fun();
    printf( "    ui16_t b: %u\n", ui16v );

    uint32_t ui32v = a_ui8_fun();
    printf( "    ui32_t a: %lu\n", ui32v );

    ui32v = b_ui8_fun();
    printf( "    ui32_t b: %lu\n", ui32v );

    // unsigned 16 to unsigned
    printf( "unsigned 16 to\n" );

    uint8_t ui8v = a_ui16_fun();
    printf( "    ui8_t a: %u\n", ui8v );

    ui8v = b_ui16_fun();
    printf( "    ui8_t b: %u\n", ui8v );

    uint16_t ui16v = a_ui16_fun();
    printf( "    ui16_t a: %u\n", ui16v );

    ui16v = b_ui16_fun();
    printf( "    ui16_t b: %u\n", ui16v );

    uint32_t ui32v = a_ui16_fun();
    printf( "    ui32_t a: %lu\n", ui32v );

    ui32v = b_ui16_fun();
    printf( "    ui32_t b: %lu\n", ui32v );

    // unsigned 32 to unsigned
    printf( "unsigned 32 to\n" );

    uint8_t ui8v = a_ui32_fun();
    printf( "    ui8_t a: %u\n", ui8v );

    ui8v = b_ui32_fun();
    printf( "    ui8_t b: %u\n", ui8v );

    uint16_t ui16v = a_ui32_fun();
    printf( "    ui16_t a: %u\n", ui16v );

    ui16v = b_ui32_fun();
    printf( "    ui16_t b: %u\n", ui16v );

    uint32_t ui32v = a_ui32_fun();
    printf( "    ui32_t a: %lu\n", ui32v );

    ui32v = b_ui32_fun();
    printf( "    ui32_t b: %lu\n", ui32v );

    printf( "    tret completed with great success\n" );
    return 0;
}

