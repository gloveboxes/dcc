/* qsort.c - sample: sorting with the runtime qsort.
 *
 * qsort is now part of the dcc runtime (DCCRTL.MAC).  This file used to supply
 * a C implementation of qsort for programs to link against, back when the
 * runtime did not provide one; it now simply demonstrates calling the built-in
 * function.  See tqsort.c for the full unit test and the "Searching and
 * sorting" section of the dcc C89 reference guide.
 */

#include <stdio.h>
#include <stdlib.h>

static int cmp_int_asc(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;

    if (x < y)
        return -1;
    if (x > y)
        return 1;
    return 0;
}

static int cmp_int_desc(const void *a, const void *b)
{
    return cmp_int_asc(b, a);
}

static void show(const char *label, const int *v, int n)
{
    int i;

    printf("%s", label);
    for (i = 0; i < n; i++)
        printf(" %d", v[i]);
    putchar('\n');
}

int main(void)
{
    int v[10];
    int i;

    /* unsorted input */
    v[0] = 5; v[1] = 2; v[2] = 8; v[3] = 1; v[4] = 9;
    v[5] = 3; v[6] = 7; v[7] = 0; v[8] = 6; v[9] = 4;
    show("unsorted:  ", v, 10);

    /* sort ascending with the runtime qsort */
    qsort(v, 10U, sizeof(int), cmp_int_asc);
    show("ascending: ", v, 10);
    for (i = 1; i < 10; i++)
        if (v[i - 1] > v[i])
            printf("ERROR: not sorted at %d\n", i);

    /* the comparator alone flips the order */
    qsort(v, 10U, sizeof(int), cmp_int_desc);
    show("descending:", v, 10);

    return 0;
} /* main */

