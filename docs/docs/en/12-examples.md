# Worked examples

Short, self-contained programs you can drop into your own project, build with
the helper script (`sh ./ma.sh foo`, or `ma.bat foo` on Windows), and run under
an emulator such as ntvcm. See [Building and linking](02-build-and-link.md) for
the build options and the manual pipeline.

## Sorting and searching an `int` array

`qsort` orders the array, then `bsearch` locates a key with the *same*
comparator. The comparator returns negative / zero / positive — here the
branchless `(x > y) - (x < y)` idiom.

```c
#include <stdio.h>
#include <stdlib.h>

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
```

Output: `found 13 at index 5`.

## Sorting an array of structs by a key field

Any element width works because `qsort` swaps whole elements byte-by-byte. The
comparator reads the field it sorts on — here a string member via `strcmp` — and
`bsearch` reuses it to look a record up by name.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
```

Output:

```text
apples   9
kiwis    2
pears    4
kiwis: 2 in stock
```

## A `printf`-style logging wrapper

Forwarding a `va_list` to `vfprintf` lets you build your own diagnostic helpers
without re-parsing the arguments.

```c
#include <stdio.h>
#include <stdarg.h>

static void logmsg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

int main(void)
{
    logmsg("ready: %d items, %lx flags\n", 3, 0xBEEFL);
    return 0;
}
```

## Reading a text file line by line

```c
#include <stdio.h>

int main(void)
{
    FILE *fp = fopen("DATA.TXT", "r");
    char  line[128];

    if (!fp) {
        perror("DATA.TXT");
        return 1;
    }
    while (fgets(line, sizeof line, fp))
        fputs(line, stdout);
    fclose(fp);
    return 0;
}
```
