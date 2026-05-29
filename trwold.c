#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

extern int errno;

void show_error( char * str )
{
    printf( "error: %s, errno: %d\n", str, errno );
    exit( 1 );
}

#define BUF_ELEMENTS 32
static int buf[ BUF_ELEMENTS ];
#define BUF_SIZE ( sizeof( buf ) )

int creat( const char * name, int flags )
{
    return open("trw.dat", O_CREAT | O_RDWR | O_TRUNC, 0);
}

int main()
{
    int i, j, buf_bytes, result, fd;
    long int seekoffset, fileoffset;

    fd = creat( "trw.dat", O_CREAT | O_RDWR );
    if ( -1 == fd )
        show_error( "unable to create data file" );

    buf_bytes = BUF_SIZE;

    for ( i = 0; i < 4096; i++ )
    {
        for ( j = 0; j < BUF_ELEMENTS; j++ )
            buf[ j ] = i;

        result = write( fd, buf, buf_bytes );
        if ( buf_bytes != result )
            show_error( "unable to write to file" );
    }

    close( fd );

    fd = open( "trw.dat", O_RDONLY );
    if ( -1 == fd )
        show_error( "unable to open data file read only" );

    for ( i = 0; i < 4096; i++ )
    {
        result = read( fd, buf, buf_bytes );
        if ( buf_bytes != result )
            show_error( "unable to read from file" );

        for ( j = 0; j < BUF_ELEMENTS; j++ )
            if ( buf[ j ] != i )
                show_error( "data read from file isn't what was expected" );
    }

    close( fd );

    fd = open( "trw.dat", O_WRONLY );
    if ( -1 == fd )
        show_error( "unable to open data file write only" );

    for ( i = 0; i < 4096; i++ )
    {
        if ( 0 == ( i % 8 ) )
        {
            seekoffset = (long int) i * BUF_SIZE;
            fileoffset = lseek( fd, seekoffset, 0 );
            if ( fileoffset != seekoffset )
            {
                printf( "fileoffset %ld, seekoffset %ld\n", fileoffset, seekoffset );
                show_error( "lseek location not as expected" );
            }

            for ( j = 0; j < BUF_ELEMENTS; j++ )
                buf[ j ] = i + 0x4000;

            result = write( fd, buf, buf_bytes );
            if ( buf_bytes != result )
                show_error( "unable to write to file after lseek" );
        }
    }

    close( fd );

    fd = open( "trw.dat", O_RDONLY );
    if ( -1 == fd )
        show_error( "unable to open data file read only" );

    for ( i = 4095; i >= 0; i-- )
    {
        seekoffset = (long int) i * BUF_SIZE;
        fileoffset = lseek( fd, seekoffset, 0 );
        if ( fileoffset != seekoffset )
        {
            printf( "fileoffset %ld, seekoffset %ld\n", fileoffset, seekoffset );
            show_error( "lseek location not as expected" );
        }

        result = read( fd, buf, buf_bytes );
        if ( buf_bytes != result )
            show_error( "unable to write to file after lseek" );

        for ( j = 0; j < BUF_ELEMENTS; j++ )
        {
            if ( 0 == ( i % 8 ) )
            {
                if ( buf[ j ] != i + 0x4000 )
                    show_error( "data read from file isn't what was expected" );
            }
            else
            {
                if ( buf[ j ] != i )
                    show_error( "data read from file isn't what was expected" );
            }
        }
    }

    close( fd );

    result = unlink( "trw.dat" );
    if ( 0 != result )
        show_error( "can't unlink test file\n" );

    printf( "trwold completed with great success\n" );
    return 0;
}

