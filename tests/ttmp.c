#include <stdio.h>
#include <string.h>

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
    char buf[L_tmpnam];
    char *p1, *p2;
    FILE *tf;
    char rbuf[32];

    /* tmpnam(NULL) uses internal buffer, returns non-NULL pointer */
    p1 = tmpnam(NULL);
    if (!p1) {
        printf("FAIL tmpnam(NULL) returned NULL\n");
        fails++;
    } else {
        printf("tmpnam1: %s\n", p1);
    }

    /* tmpnam(buf) writes into buf and returns buf */
    p2 = tmpnam(buf);
    chki("tmpnam_ret", (p2 == buf), 1);
    if (p2 == buf)
        printf("tmpnam2: %s\n", buf);

    /* two successive calls produce different names */
    if (p1 && p2 && strcmp(p1, buf) == 0) {
        printf("FAIL tmpnam returned same name twice\n");
        fails++;
    }

    /* tmpfile: create, write, rewind, read back */
    tf = tmpfile();
    if (!tf) {
        printf("FAIL tmpfile returned NULL\n");
        fails++;
    } else {
        fputs("hello\n", tf);
        rewind(tf);
        if (!fgets(rbuf, sizeof(rbuf), tf)) {
            printf("FAIL fgets after rewind\n");
            fails++;
        } else {
            chki("content", strcmp(rbuf, "hello\n"), 0);
        }
        fclose(tf);
    }

    if (fails)
        printf("ttmp FAILED %d\n", fails);
    else
        printf("ttmp ok\n");
    return 0;
}
