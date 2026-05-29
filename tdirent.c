#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

int main(void)
{
    DIR *d;
    struct dirent *dir;
    int count;

    d = opendir(".");
    if (d == NULL) {
        perror("opendir");
        return 1;
    }

    count = 0;
    int ok = 0;
    while ((dir = readdir(d)) != NULL) {
        //printf("%s\n", dir->d_name);
        if ( !strcmp( dir->d_name, "TDIRENT.COM" ) )
            ok = 1;
        ++count;
    }

    closedir(d);

    if ( !ok )
    {
        printf( "TDIRENT.COM not found!\n" );
        exit( 1 );
    }

    //printf("%d entries\n", count);
    printf( "tdirent completed with great success\n" );
    return 0;
}
