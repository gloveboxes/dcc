#include <stdio.h>
#include <string.h>

int main(void)
{
    FILE *fp;
    char buf[32];
    int i;

    /* create two small test files */
    fp = fopen("FRA.TMP", "w");
    if (!fp) { printf("FAIL create FRA\n"); return 1; }
    fputs("alpha\n", fp);
    fclose(fp);

    fp = fopen("FRB.TMP", "w");
    if (!fp) { printf("FAIL create FRB\n"); return 1; }
    fputs("beta\n", fp);
    fclose(fp);

    /* open FRA, read first line, then freopen FRB on same stream */
    fp = fopen("FRA.TMP", "r");
    if (!fp) { printf("FAIL open FRA\n"); return 1; }
    if (!fgets(buf, sizeof(buf), fp)) { printf("FAIL read FRA\n"); return 1; }
    for (i = 0; buf[i] && buf[i] != '\n' && buf[i] != '\r'; i++) ;
    buf[i] = '\0';
    if (strcmp(buf, "alpha") != 0) { printf("FAIL FRA content: %s\n", buf); return 1; }

    fp = freopen("FRB.TMP", "r", fp);
    if (!fp) { printf("FAIL freopen FRB\n"); return 1; }
    if (!fgets(buf, sizeof(buf), fp)) { printf("FAIL read FRB\n"); return 1; }
    for (i = 0; buf[i] && buf[i] != '\n' && buf[i] != '\r'; i++) ;
    buf[i] = '\0';
    if (strcmp(buf, "beta") != 0) { printf("FAIL FRB content: %s\n", buf); return 1; }
    fclose(fp);

    /* freopen of nonexistent file should return NULL */
    fp = fopen("FRA.TMP", "r");
    if (fp) {
        FILE *fp2 = freopen("NOSUCHF.TMP", "r", fp);
        if (fp2 != NULL) { printf("FAIL freopen nonexist returned non-NULL\n"); return 1; }
    }

    remove("FRA.TMP");
    remove("FRB.TMP");

    printf("tfreopen ok\n");
    return 0;
}
