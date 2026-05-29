/* validates some printf behaviors */

#include <stdio.h>
#include <stdint.h>

int main()
{
    /* %d signed decimal */
    printf("%d\n", 0);
    printf("%d\n", 1);
    printf("%d\n", -1);
    printf("%d\n", 12345);
    printf("%d\n", -12345);
    printf("%d\n", 32767);
    printf("%d\n", -32767);

    /* %u unsigned decimal */
    printf("%u\n", 0);
    printf("%u\n", 1);
    printf("%u\n", 12345);
    printf("%u\n", 32767);

    /* %x hex -- always 4 digits, uppercase A-F */
    printf("%x\n", 0);
    printf("%x\n", 255);       /* 00FF */
    printf("%x\n", 291);       /* 0123 */
    printf("%x\n", 2748);      /* 0ABC */
    printf("%x\n", 32767);     /* 7FFF */

    /* %c character */
    printf("%c\n", 65);        /* A */
    printf("%c\n", 97);        /* a */
    printf("%c\n", 48);        /* 0 */

    /* %s string */
    printf("%s\n", "hello");
    printf("%s\n", "world");

    /* %% literal percent */
    printf("100%%\n");

    /* field width: right-justified, space-padded */
    printf("[%6d]\n", 0);      /* [     0] */
    printf("[%6d]\n", 42);     /* [    42] */
    printf("[%6d]\n", -42);    /* [   -42] */
    printf("[%6d]\n", 32767);  /* [ 32767] */
    printf("[%6u]\n", 0);      /* [     0] */
    printf("[%6u]\n", 42);     /* [    42] */
    printf("[%6s]\n", "abc");  /* [   abc] */
    printf("[%6s]\n", "hello");/* [ hello] */
    printf("[%6x]\n", 255);    /* [  00FF] */
    printf("[%6x]\n", 2748);   /* [  0ABC] */

    /* multiple arguments */
    printf("%d %d %d\n", 1, 2, 3);
    printf("%s=%d\n", "ans", 42);
    printf("%c%c%c\n", 65, 66, 67);  /* ABC */

    size_t st = 33333;
    printf( "size_t zu: %zu\n", st );
    printf( "size_t zd: %zd\n", st );
    printf( "size_t zx: %zx\n", st );

    printf("tprintf ok\n");
    return 0;
}
