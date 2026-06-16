/* texstrct.c - worked example: sort an array of structs by a key field.
 * Pulled into docs/docs/en/12-examples.md via the snippet markers below. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --8<-- [start:example] */
struct item {
    char name[8];
    int  qty;
};

static int by_name(const void *a, const void *b)
{
    return strcmp(((const struct item *)a)->name,
                  ((const struct item *)b)->name);
}

int main(void)
{
    struct item items[3];
    struct item key;
    const struct item *hit;
    int i;

    strcpy(items[0].name, "pears");  items[0].qty = 4;
    strcpy(items[1].name, "apples"); items[1].qty = 9;
    strcpy(items[2].name, "kiwis");  items[2].qty = 2;

    qsort(items, 3U, sizeof(struct item), by_name);
    for (i = 0; i < 3; i++)
        printf("%-8s %d\n", items[i].name, items[i].qty);

    strcpy(key.name, "kiwis");
    hit = (const struct item *)bsearch(&key, items, 3U,
                                       sizeof(struct item), by_name);
    if (hit)
        printf("%s: %d in stock\n", hit->name, hit->qty);
    return 0;
}
/* --8<-- [end:example] */
