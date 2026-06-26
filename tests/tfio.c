/* tfio.c - test f* I/O routines */
#include <stdio.h>
#include <string.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

int main(void) {
    FILE *f;
    char buf[64];
    int n;

    /* --- write test --- */
    f = fopen("tfio.tmp", "w");
    if (!f) { puts("fopen write failed"); return 1; }
    fputs("hello\n", f);
    fputs("world\n", f);
    fprintf(f, "line %d\n", 3);
    fclose(f);

    /* --- read test --- */
    f = fopen("tfio.tmp", "r");
    if (!f) { puts("fopen read failed"); return 1; }
    n = 0;
    while (fgets(buf, sizeof(buf), f)) {
        printf("%d: %s", ++n, buf);
    }
    fclose(f);

    /* --- sprintf test --- */
    sprintf(buf, "value=%d", 42);
    puts(buf);

    unlink("tfio.tmp");

    puts("tfio completed with great success");
    return 0;
}
