/* texsort.c - worked example: sort + search an int array with qsort/bsearch.
 * Pulled into docs/docs/en/12-examples.md via the snippet markers below. */

#include <stdio.h>
#include <stdlib.h>

/* --8<-- [start:example] */
static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

int main(void)
{
    int v[8];
    int key = 13;
    const int *hit;

    v[0] = 2; v[1] = 8; v[2] = 5; v[3] = 13;
    v[4] = 1; v[5] = 21; v[6] = 3; v[7] = 34;

    qsort(v, 8U, sizeof(int), cmp_int);     /* 1 2 3 5 8 13 21 34 */
    hit = (const int *)bsearch(&key, v, 8U, sizeof(int), cmp_int);
    if (hit)
        printf("found %d at index %d\n", *hit, (int)(hit - v));
    else
        puts("not found");
    return 0;
}
/* --8<-- [end:example] */
