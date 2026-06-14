#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* helpers to print values with type context */
void shi8( int8_t x ) { printf( "%d\n" , (int16_t) x ); }
void shui8( uint8_t x ) { printf( "%d\n" , (uint16_t) x ); }
void shi16( int16_t x ) { printf( "%d\n" , x ); }

/* New helpers for 32-bit types */
void shi32( int32_t x ) { printf( "%ld\n" , (long) x ); }
void shui32( uint32_t x ) { printf( "%lu\n" , (unsigned long) x ); }

int main() {
    int8_t i8;
    uint8_t ui8;
    int16_t i16;
    uint16_t ui16;
    
    /* New variables for 32-bit types */
    int32_t i32;
    uint32_t ui32;

   printf( "unary minus\n" );
    i8 = 5;    shi8( -i8 );       /* -5      */
    i8 = -5;   shi8( -i8 );       /*  5      */
    i8 = -1;   shi8( -i8 );       /*  1      */
    i8 = 0;    shi8( -i8 );       /*  0      */
    i16 = 1000;  shi16( -i16 );   /* -1000   */
    i16 = -1000; shi16( -i16 );   /*  1000   */
    i16 = 0;     shi16( -i16 );   /*  0      */

    printf( "bitwise not\n" );
    ui8 = 0x00; shui8( ~ui8 );    /* 255     */
    ui8 = 0xff; shui8( ~ui8 );    /*   0     */
    ui8 = 0x55; shui8( ~ui8 );    /* 170     */
    ui8 = 0xaa; shui8( ~ui8 );    /*  85     */
    i8 = 0;    shi8( ~i8 );       /*  -1     */
    i8 = -1;   shi8( ~i8 );       /*   0     */
    i8 = 1;    shi8( ~i8 );       /*  -2     */
    ui16 = 0x0000; shi16( ~ui16 );  /* -1    */
    ui16 = 0xffff; shi16( ~ui16 );  /*  0    */
    ui16 = 0x00ff; shi16( ~ui16 );  /* -256  */
    i16 = 0;    shi16( ~i16 );    /* -1      */
    i16 = -1;   shi16( ~i16 );    /*  0      */
    i16 = 1;    shi16( ~i16 );    /* -2      */

    printf( "logical not\n" );
    i16 = 0;    shi16( !i16 );    /*  1      */
    i16 = 1;    shi16( !i16 );    /*  0      */
    i16 = -1;   shi16( !i16 );    /*  0      */
    i16 = 100;  shi16( !i16 );    /*  0      */
    i8 = 0;     shi16( !i8 );     /*  1      */
    i8 = 5;     shi16( !i8 );     /*  0      */
    ui8 = 0;    shi16( !ui8 );    /*  1      */
    ui8 = 1;    shi16( !ui8 );    /*  0      */

    printf( "pre-increment\n" );
    i8  = 5;   ++i8;   shi8( i8 );   /*  6  */
    i8  = -1;  ++i8;   shi8( i8 );   /*  0  */
    ui8 = 5;   ++ui8;  shui8( ui8 ); /*  6  */
    ui8 = 255; ++ui8;  shui8( ui8 ); /*  0  */
    i16 = 100; ++i16;  shi16( i16 ); /* 101 */
    i16 = -1;  ++i16;  shi16( i16 ); /*  0  */

    printf( "pre-decrement\n" );
    i8  = 5;   --i8;   shi8( i8 );   /*  4  */
    i8  = 0;   --i8;   shi8( i8 );   /* -1  */
    ui8 = 5;   --ui8;  shui8( ui8 ); /*  4  */
    ui8 = 0;   --ui8;  shui8( ui8 ); /* 255 */
    i16 = 100; --i16;  shi16( i16 ); /*  99 */
    i16 = 0;   --i16;  shi16( i16 ); /*  -1 */

    printf( "post-increment\n" );
    i8  = 5;   i8++;   shi8( i8 );   /*  6  */
    i8  = -1;  i8++;   shi8( i8 );   /*  0  */
    ui8 = 5;   ui8++;  shui8( ui8 ); /*  6  */
    ui8 = 255; ui8++;  shui8( ui8 ); /*  0  */
    i16 = 100; i16++;  shi16( i16 ); /* 101 */
    i16 = -1;  i16++;  shi16( i16 ); /*  0  */

    printf( "post-decrement\n" );
    i8  = 5;   i8--;   shi8( i8 );   /*  4  */
    i8  = 0;   i8--;   shi8( i8 );   /* -1  */
    ui8 = 5;   ui8--;  shui8( ui8 ); /*  4  */
    ui8 = 0;   ui8--;  shui8( ui8 ); /* 255 */
    i16 = 100; i16--;  shi16( i16 ); /*  99 */
    i16 = 0;   i16--;  shi16( i16 ); /*  -1 */

    printf( "ternary basic\n" );
    shi16( 1 ? 10 : 20 );            /*  10  constant-true */
    shi16( 0 ? 10 : 20 );            /*  20  constant-false */
    i16 = 7;
    shi16( i16 ? 10 : 20 );          /*  10  variable-true */
    i16 = 0;
    shi16( i16 ? 10 : 20 );          /*  20  variable-false */

    printf( "ternary comparisons\n" );
    i8 =  5; shi16( i8 > 0 ?  1 : -1 ); /*  1 */
    i8 = -5; shi16( i8 > 0 ?  1 : -1 ); /* -1 */
    i8 =  0; shi16( i8 > 0 ?  1 : -1 ); /* -1 */
    i8 =  5; shi16( i8 == 5 ? 99 :  0 ); /* 99 */
    i8 =  4; shi16( i8 == 5 ? 99 :  0 ); /*  0 */

    printf( "ternary variable branches\n" );
    i8 =  3; shi16( i8 >= 0 ? (int16_t)i8 : -(int16_t)i8 ); /*  3 abs */
    i8 = -3; shi16( i8 >= 0 ? (int16_t)i8 : -(int16_t)i8 ); /*  3 abs */
    i8 =  0; shi16( i8 >= 0 ? (int16_t)i8 : -(int16_t)i8 ); /*  0 abs */

    printf( "ternary nested\n" );     /* signum */
    i16 =  5; shi16( i16 < 0 ? -1 : i16 > 0 ? 1 : 0 ); /*  1 */
    i16 = -5; shi16( i16 < 0 ? -1 : i16 > 0 ? 1 : 0 ); /* -1 */
    i16 =  0; shi16( i16 < 0 ? -1 : i16 > 0 ? 1 : 0 ); /*  0 */

    printf( "ternary 8-bit condition\n" );
    ui8 = 0; shi16( ui8 ? 100 : -100 ); /* -100 */
    ui8 = 1; shi16( ui8 ? 100 : -100 ); /*  100 */
    i8  = 0; shi16( i8  ? 100 : -100 ); /* -100 */
    i8  = -1; shi16( i8 ? 100 : -100 ); /*  100 */

    printf( "ternary in expression\n" );
    i16 = 3;
    shi16( (i16 > 0 ? i16 : -i16) + 1 );  /*  4 */
    i16 = -3;
    shi16( (i16 > 0 ? i16 : -i16) + 1 );  /*  4 */

    /* 32-bit Bitwise NOT */
    ui32 = 0x00000000; shui32( ~ui32 ); /* 4294967295 */
    ui32 = 0xffffffff; shui32( ~ui32 ); /* 0 */
    ui32 = 0x0000ffff; shui32( ~ui32 ); /* 4294901760 */
    i32 = 0; shi32( ~i32 ); /* -1 */
    i32 = -1; shi32( ~i32 ); /* 0 */
    i32 = 1; shi32( ~i32 ); /* -2 */

    /* 32-bit Logical NOT */
    i32 = 0; shi16( !i32 ); /* 1 */
    i32 = 999999; shi16( !i32 ); /* 0 */
    ui32 = 0; shi16( !ui32 ); /* 1 */
    ui32 = 555555; shi16( !ui32 ); /* 0 */
    i32 = 100000; ++i32; shi32( i32 ); /* 100001 */
    ui32 = 500000; ++ui32; shui32( ui32 ); /* 500001 */

    printf( "tunary completed\n" );
    return 0;
}
