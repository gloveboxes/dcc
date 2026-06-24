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
    FILE *f;
    char buf[32];
    int c;

    /* ungetc on stdin: push back 'X', getchar should return 'X' */
    chki("ugc_eof_fail", ungetc(EOF, stdin), EOF);
    chki("ugc_ret", ungetc('X', stdin), 'X');
    chki("getchar_pb", getchar(), 'X');

    /* ungetc into a file, then fgetc */
    f = fopen("TUGE0.TMP", "w");
    fputs("abc\n", f);
    fclose(f);

    f = fopen("TUGE0.TMP", "r");
    c = fgetc(f);          /* 'a' */
    chki("fgetc_a", c, 'a');
    chki("ugc_file", ungetc(c, f), 'a');
    c = fgetc(f);          /* pushed-back 'a' */
    chki("fgetc_pb", c, 'a');
    c = fgetc(f);          /* 'b' from file */
    chki("fgetc_b", c, 'b');
    fclose(f);

    /* ungetc then fgets reads pushed-back char */
    f = fopen("TUGE0.TMP", "r");
    c = fgetc(f);          /* 'a' */
    ungetc(c, f);          /* push 'a' back */
    if (!fgets(buf, sizeof(buf), f)) {
        printf("FAIL fgets after ungetc returned NULL\n");
        fails++;
    } else {
        chki("fgets_pb", strcmp(buf, "abc\n"), 0);
    }
    fclose(f);

    remove("TUGE0.TMP");

    if (fails)
        printf("tungetc FAILED %d\n", fails);
    else
        printf("tungetc ok\n");
    return 0;
}
