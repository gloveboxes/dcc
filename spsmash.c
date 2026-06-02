/*
   detects stack overflow. this condition is ignored by default.
   concerned coders can follow these patterns to detect overflow.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// memory layout high to low:
//    reserved for the OS
//    sp at app start
//    bottom of stack -- sp at app start minus -stack value from dcc
//    top of malloc() heap (_hlimit)
//    brk (top of currently used malloc() heap) (_brk)
//    bottom of malloc() heap
//    top of bss ( &__bsse )
//    bottom of bss (0-filled memory for app's variables) ( &__bssb )
//    app code and data intermixed as it sees fit (contents of the .com file)
//    base page:
//        default DMA / command tail
//        small fcbs for files in command tail
//        bdos entry vector
//        current drive
//        i/o byte
//        8080/Z80 RST vectors + cp/m vectors/data

// --  If you want to detect if the stack has crossed into the offical end
//     of the malloc() heap, check against _hlimit (per DEFAULT_STACK
//     in dccrtl.mac).
// --  If instead you want to detect if the stack has crossed into the
//     currently utilized heap and/or into BSS, check against _brk

extern size_t _brk;     // top of the utilized malloc() heap
extern size_t _hlimit;  // top of the perhaps unutilized malloc() heap
extern char __bssb;     // bss begin. no _ prefix added by dcc
extern char __bsse;     // bss end. no _ prefix added by dcc

char acbss[ 256 ];      // reserve some bss space

bool spsmash( size_t sp )
{
    sp -= 32; // use the margin of error that makes sense for your app
    return ( sp <= _brk );        // about to smash utilized heap/bss
    // return ( sp <= _hlimit );  // about to smash heap limit
} //spsmash

uint32_t factorial( uint32_t n )
{
    if ( spsmash( (size_t) &n ) )
    {
        #if true
            printf( "intentional stack overflow condition.\n" );
        #else
            printf( "heap top:         %zx\n", _hlimit );
            printf( "brk:              %zx\n", _brk );
            printf( "bss top:          %zx\n", & __bsse );
            printf( "bss bottom:       %zx\n", & __bssb );
            printf( "intentional stack overflow condition. address: %zx, n: %lu\n", &n, n );
        #endif

        exit( 1 );
    }

    if ( 0 == n )
        return 1;

    return n * factorial( n - 1 );
} //factorial

int main( int argc, char * argv[] )
{
    char * buf = (char *) malloc( 256 ); // push space between bss end and brk

#if false
    printf( "current sp about: %zx\n", &argc );
    printf( "heap top:         %zx\n", _hlimit );
    printf( "brk:              %zx\n", _brk );
    printf( "bss top:          %zx\n", & __bsse );
    printf( "bss bottom:       %zx\n", & __bssb );
#endif

    uint32_t n = 4000000000;
    printf( "factorial( %lu ) = %lu\n", n, factorial( n ) );
    return 0;
} //main
