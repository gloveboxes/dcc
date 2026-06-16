/* texfile.c - worked example: read a text file line by line.
 * Reads the DATA.TXT fixture (tests/DATA.TXT, staged into the build dir).
 * Pulled into docs/docs/en/12-examples.md via the snippet markers below. */

#include <stdio.h>

/* --8<-- [start:example] */
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
/* --8<-- [end:example] */
