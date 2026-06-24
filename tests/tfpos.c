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

static void chkstr(const char *name, const char *got, const char *expected)
{
    if (strcmp(got, expected) != 0) {
        printf("FAIL %s: got [%s] expected [%s]\n", name, got, expected);
        fails++;
    }
}

int main(void)
{
    FILE *f;
    fpos_t p1, p2;
    char buf[16];
    int i, r;

    f = fopen("TFPS0.TMP", "w");
    fputs("hello world", f);
    fclose(f);

    f = fopen("TFPS0.TMP", "r");

    /* read "hello" then snapshot position */
    for (i = 0; i < 5; i++) buf[i] = fgetc(f);
    buf[5] = '\0';
    chkstr("first5", buf, "hello");

    r = fgetpos(f, &p1);
    chki("fgetpos_ret", r, 0);

    /* read " world" then snapshot end position */
    for (i = 0; i < 6; i++) buf[i] = fgetc(f);
    buf[6] = '\0';
    chkstr("next6", buf, " world");

    r = fgetpos(f, &p2);
    chki("fgetpos2_ret", r, 0);

    /* fsetpos back to after "hello", re-read " world" */
    r = fsetpos(f, &p1);
    chki("fsetpos_ret", r, 0);
    for (i = 0; i < 6; i++) buf[i] = fgetc(f);
    buf[6] = '\0';
    chkstr("reread6", buf, " world");

    /* fsetpos to p2 (end), fgetpos should give same value back */
    r = fsetpos(f, &p2);
    chki("fsetpos2_ret", r, 0);
    r = fgetpos(f, &p2);
    chki("fgetpos3_ret", r, 0);

    fclose(f);
    remove("TFPS0.TMP");

    if (fails)
        printf("tfpos FAILED %d\n", fails);
    else
        printf("tfpos ok\n");
    return 0;
}
