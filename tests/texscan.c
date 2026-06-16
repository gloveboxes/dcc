/* texscan.c - worked example: parse input with the scanf family (sscanf).
 * Pulled into docs/docs/en/12-examples.md via the snippet markers below. */

#include <stdio.h>

/* --8<-- [start:example] */
int main(void)
{
    int  value;
    char word[16];
    int  hexval;
    long big;

    sscanf("-12 hello 0x2a", "%d %s %i", &value, word, &hexval);
    printf("value=%d word=%s hexval=%d\n", value, word, hexval);

    sscanf("123456", "%ld", &big);
    printf("big=%ld\n", big);
    return 0;
}
/* --8<-- [end:example] */
