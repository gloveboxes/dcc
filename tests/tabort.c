#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int fails;

static void chki(const char *name, int got, int expected)
{
    if (got != expected) {
        printf("FAIL %s: got %d expected %d\n", name, got, expected);
        fails++;
    }
}

int main(void)
{
    FILE *f;
    char buf[8];

    /* rename: write, rename, verify new name readable, old gone */
    f = fopen("RNOLD.TMP", "w");
    fputs("hello", f);
    fclose(f);

    chki("rename_ret", rename("RNOLD.TMP", "RNNEW.TMP"), 0);

    f = fopen("RNNEW.TMP", "r");
    if (!f) {
        printf("FAIL rename: new file not found\n");
        fails++;
    } else {
        fgets(buf, sizeof(buf), f);
        fclose(f);
        chki("rename_content", strcmp(buf, "hello"), 0);
    }

    f = fopen("RNOLD.TMP", "r");
    if (f) {
        printf("FAIL rename: old file still exists\n");
        fclose(f);
        fails++;
    }
    remove("RNNEW.TMP");

    /* isgraph: printable non-space */
    chki("isgraph_A",    isgraph('A'), 1);
    chki("isgraph_z",    isgraph('z'), 1);
    chki("isgraph_0",    isgraph('0'), 1);
    chki("isgraph_bang", isgraph('!'), 1);
    chki("isgraph_sp",   isgraph(' '), 0);
    chki("isgraph_tab",  isgraph('\t'), 0);
    chki("isgraph_nul",  isgraph('\0'), 0);

    if (fails) {
        printf("tabort FAILED %d\n", fails);
        return 1;
    }

    printf("tabort ok\n");

    /* abort() must terminate before this line executes */
    abort();
    printf("FAIL abort: returned\n");
    return 1;
}
